#include <elf.h>
#include <env.h>
#include <lib.h>
#include <mmu.h>

#define debug 0

int init_stack(u_int child, char **argv, u_int *init_sp) {
	int argc, i, r, tot;
	char *strings;
	u_int *args;

	// Count the number of arguments (argc)
	// and the total amount of space needed for strings (tot)
	tot = 0;
	for (argc = 0; argv[argc]; argc++) {
		tot += strlen(argv[argc]) + 1;
	}

	// Make sure everything will fit in the initial stack page
	if (ROUND(tot, 4) + 4 * (argc + 3) > PAGE_SIZE) {
		return -E_NO_MEM;
	}

	// Determine where to place the strings and the args array
	strings = (char *)(UTEMP + PAGE_SIZE) - tot;
	args = (u_int *)(UTEMP + PAGE_SIZE - ROUND(tot, 4) - 4 * (argc + 1));

	if ((r = syscall_mem_alloc(0, (void *)UTEMP, PTE_D)) < 0) {
		return r;
	}

	// Copy the argument strings into the stack page at 'strings'
	char *ctemp, *argv_temp;
	u_int j;
	ctemp = strings;
	for (i = 0; i < argc; i++) {
		argv_temp = argv[i];
		for (j = 0; j < strlen(argv[i]); j++) {
			*ctemp = *argv_temp;
			ctemp++;
			argv_temp++;
		}
		*ctemp = 0;
		ctemp++;
	}

	// Initialize args[0..argc-1] to be pointers to these strings
	// that will be valid addresses for the child environment
	// (for whom this page will be at USTACKTOP-PAGE_SIZE!).
	ctemp = (char *)(USTACKTOP - UTEMP - PAGE_SIZE + (u_int)strings);
	for (i = 0; i < argc; i++) {
		args[i] = (u_int)ctemp;
		ctemp += strlen(argv[i]) + 1;
	}

	// Set args[argc] to 0 to null-terminate the args array.
	ctemp--;
	args[argc] = (u_int)ctemp;

	// Push two more words onto the child's stack below 'args',
	// containing the argc and argv parameters to be passed
	// to the child's main() function.
	u_int *pargv_ptr;
	pargv_ptr = args - 1;
	*pargv_ptr = USTACKTOP - UTEMP - PAGE_SIZE + (u_int)args;
	pargv_ptr--;
	*pargv_ptr = argc;

	// Set *init_sp to the initial stack pointer for the child
	*init_sp = USTACKTOP - UTEMP - PAGE_SIZE + (u_int)pargv_ptr;

	if ((r = syscall_mem_map(0, (void *)UTEMP, child, (void *)(USTACKTOP - PAGE_SIZE), PTE_D)) <
	    0) {
		goto error;
	}
	if ((r = syscall_mem_unmap(0, (void *)UTEMP)) < 0) {
		goto error;
	}

	return 0;

error:
	syscall_mem_unmap(0, (void *)UTEMP);
	return r;
}

static int spawn_mapper(void *data, u_long va, size_t offset, u_int perm, const void *src,
			size_t len) {
	u_int child_id = *(u_int *)data;
	try(syscall_mem_alloc(child_id, (void *)va, perm));
	if (src != NULL) {
		int r = syscall_mem_map(child_id, (void *)va, 0, (void *)UTEMP, perm | PTE_D);
		if (r) {
			syscall_mem_unmap(child_id, (void *)va);
			return r;
		}
		memcpy((void *)(UTEMP + offset), src, len);
		return syscall_mem_unmap(0, (void *)UTEMP);
	}
	return 0;
}

int spawn(char *prog, char **argv) {
	// Step 1: Open the file 'prog', trying with and without a '.b' suffix.
	char prog_buf[MAXPATHLEN];
	int fd;

	// Try opening the name as is.
	if ((fd = open(prog, O_RDONLY)) < 0) {
		// If that fails, try appending ".b".

		// Check for buffer overflow before concatenating. +3 for '.', 'b', '\0'
		if (strlen(prog) + 3 > MAXPATHLEN) {
			return -E_INVAL; // Path too long to append suffix.
		}

		// Manually create "prog.b" using strcpy, which is available.
		strcpy(prog_buf, prog);
		strcpy(prog_buf + strlen(prog), ".b");

		if ((fd = open(prog_buf, O_RDONLY)) < 0) {
			// If both fail, return the error from the second attempt.
			return fd;
		}
	}

	// Step 2: Read the ELF header.
	int r;
	u_char elfbuf[512];
	if ((r = readn(fd, elfbuf, sizeof(Elf32_Ehdr))) != sizeof(Elf32_Ehdr)) {
		goto err;
	}
	const Elf32_Ehdr *ehdr = elf_from(elfbuf, sizeof(Elf32_Ehdr));
	if (!ehdr) {
		r = -E_NOT_EXEC;
		goto err;
	}
	u_long entrypoint = ehdr->e_entry;

	// Step 3: Create a child process.
	u_int child;
	if ((child = syscall_exofork()) < 0) {
		r = child;
		goto err;
	}
	
	// Step 4: Initialize the child's stack.
	u_int sp;
	if ((r = init_stack(child, argv, &sp))) {
		goto err1;
	}
	
	// Step 5: Load the ELF segments into the child's memory.
	size_t ph_off;
	ELF_FOREACH_PHDR_OFF (ph_off, ehdr) {
		if (seek(fd, ph_off) < 0) {
			r = -E_INVAL;
			goto err1;
		}
		if (readn(fd, elfbuf, ehdr->e_phentsize) != ehdr->e_phentsize) {
			r = -E_INVAL;
			goto err1;
		}
		Elf32_Phdr *ph = (Elf32_Phdr *)elfbuf;
		if (ph->p_type == PT_LOAD) {
			void *bin;
			if ((r = read_map(fd, ph->p_offset, &bin)) != 0) {
				goto err1;
			}
			if ((r = elf_load_seg(ph, bin, spawn_mapper, &child)) != 0) {
				goto err1;
			}
		}
	}
	close(fd);
	fd = -1; // Mark file as closed

	// Step 6: Set up the child's trap frame (registers).
	struct Trapframe tf = envs[ENVX(child)].env_tf;
	tf.cp0_epc = entrypoint;
	tf.regs[29] = sp;
	if ((r = syscall_set_trapframe(child, &tf)) != 0) {
		goto err2;
	}

	// Step 7: Share library pages with the child.
	for (u_int pdeno = 0; pdeno <= PDX(USTACKTOP); pdeno++) {
		if (!(vpd[pdeno] & PTE_V)) continue;
		for (u_int pteno = 0; pteno <= PTX(~0); pteno++) {
			u_int pn = (pdeno << 10) + pteno;
			u_int perm = vpt[pn] & ((1 << PGSHIFT) - 1);
			if ((perm & PTE_V) && (perm & PTE_LIBRARY)) {
				void *va = (void *)(pn << PGSHIFT);
				if ((r = syscall_mem_map(0, va, child, va, perm)) < 0) {
					goto err2;
				}
			}
		}
	}

	// Step 8: Set the child to runnable.
	if ((r = syscall_set_env_status(child, ENV_RUNNABLE)) < 0) {
		goto err2;
	}

	return child;

err2:
	syscall_env_destroy(child, 0); 
	return r;
err1:
	syscall_env_destroy(child, 0); 
err:
	if (fd >= 0) {
		close(fd);
	}
	return r;
}

int spawnl(char *prog, char *args, ...) {
	return spawn(prog, &args);
}