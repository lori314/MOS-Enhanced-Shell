#include <env.h>
#include <io.h>
#include <mmu.h>
#include <pmap.h>
#include <printk.h>
#include <sched.h>
#include <syscall.h>
#include <string.h> // strcpy and other functions are still available

extern struct Env *curenv;

/* Overview:
 * 	This function is used to print a character on screen.
 *
 * Pre-Condition:
 * 	`c` is the character you want to print.
 */
void sys_putchar(int c) {
	printcharc((char)c);
	return;
}

/* Overview:
 * 	This function is used to print a string of bytes on screen.
 *
 * Pre-Condition:
 * 	`s` is base address of the string, and `num` is length of the string.
 */
int sys_print_cons(const void *s, u_int num) {
	if (((u_int)s + num) > UTOP || ((u_int)s) >= UTOP || (s > s + num)) {
		return -E_INVAL;
	}
	u_int i;
	for (i = 0; i < num; i++) {
		printcharc(((char *)s)[i]);
	}
	return 0;
}

/* Overview:
 *	This function provides the environment id of current process.
 *
 * Post-Condition:
 * 	return the current environment id
 */
u_int sys_getenvid(void) {
	return curenv->env_id;
}

/* Overview:
 *   Give up remaining CPU time slice for 'curenv'.
 *
 * Post-Condition:
 *   Another env is scheduled.
 *
 * Hint:
 *   This function will never return.
 */
// void sys_yield(void);
void __attribute__((noreturn)) sys_yield(void) {
	// Hint: Just use 'schedule' with 'yield' set.
	/* Exercise 4.7: Your code here. */
	schedule(1);
}

// --- MODIFICATION START ---
/*
 * New system call for destroying an environment. This function now handles
 * both self-destruction (becoming a zombie) and reaping by a parent.
 *
 * @param envid The ID of the environment to affect. If 0, affects the current env.
 * @param status The exit status, only used when an env destroys itself.
 */
int sys_env_destroy(u_int envid, int status) {
	struct Env *e;

	// Use envid2env to get the target Env struct.
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}

	if (e == curenv) {
		// Case 1: The current environment is destroying itself (exit(status)).
		// It becomes a zombie.
		printk("[%08x] exiting with status %d, becoming zombie\n", curenv->env_id, status);
		e->env_exit_status = status;
		e->env_status = ENV_ZOMBIE;
		// This process will be cleaned up by its parent later. We just need to
		// make sure it never runs again.
		TAILQ_REMOVE(&env_sched_list, e, env_sched_link);
		schedule(1); // Should not return.
		return 0; // Unreachable
	} else {
		// Case 2: The current environment is reaping a child (called from wait()).
		// The child must be a ZOMBIE and a direct child of the current process.
		if (e->env_parent_id != curenv->env_id || e->env_status != ENV_ZOMBIE) {
			printk("sys_env_destroy: bad reap target %08x\n", e->env_id);
			return -E_BAD_ENV;
		}
		
		printk("[%08x] reaping zombie %08x\n", curenv->env_id, e->env_id);
		
		// CORRECTED: Call the globally available env_destroy function.
		// It will handle the full cleanup, including calling the static
		// functions `env_free`, `asid_free`, and manipulating `env_free_list`.
		env_destroy(e);
	}
	
	return 0;
}
// --- MODIFICATION END ---

/* Overview:
 *   Register the entry of user space TLB Mod handler of 'envid'.
 *
 * Post-Condition:
 *   The 'envid''s TLB Mod exception handler entry will be set to 'func'.
 *   Returns 0 on success.
 *   Returns the original error if underlying calls fail.
 */
int sys_set_tlb_mod_entry(u_int envid, u_int func) {
	struct Env *env;

	/* Step 1: Convert the envid to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.12: Your code here. (1/2) */
	try(envid2env(envid,&env,1));
	/* Step 2: Set its 'env_user_tlb_mod_entry' to 'func'. */
	/* Exercise 4.12: Your code here. (2/2) */
	env->env_user_tlb_mod_entry = func;
	return 0;
}

/* Overview:
 *   Check 'va' is illegal or not, according to include/mmu.h
 */
static inline int is_illegal_va(u_long va) {
	return va < UTEMP || va >= UTOP;
}

static inline int is_illegal_va_range(u_long va, u_int len) {
	if (len == 0) {
		return 0;
	}
	return va + len < va || va < UTEMP || va + len > UTOP;
}

/* Overview:
 *   Allocate a physical page and map 'va' to it with 'perm' in the address space of 'envid'.
 *   If 'va' is already mapped, that original page is sliently unmapped.
 *   'envid2env' should be used with 'checkperm' set, like in most syscalls, to ensure the target is
 * either the caller or its child.
 *
 * Post-Condition:
 *   Return 0 on success.
 *   Return -E_BAD_ENV: 'checkperm' of 'envid2env' fails for 'envid'.
 *   Return -E_INVAL:   'va' is illegal (should be checked using 'is_illegal_va').
 *   Return the original error: underlying calls fail (you can use 'try' macro).
 *
 * Hint:
 *   You may want to use the following functions:
 *   'envid2env', 'page_alloc', 'page_insert', 'try' (macro)
 */
int sys_mem_alloc(u_int envid, u_int va, u_int perm) {
	struct Env *env;
	struct Page *pp;

	/* Step 1: Check if 'va' is a legal user virtual address using 'is_illegal_va'. */
	/* Exercise 4.4: Your code here. (1/3) */
	if (is_illegal_va(va)) {
		return -E_INVAL;
	}
	/* Step 2: Convert the envid to its corresponding 'struct Env *' using 'envid2env'. */
	/* Hint: **Always** validate the permission in syscalls! */
	/* Exercise 4.4: Your code here. (2/3) */
	try(envid2env(envid,&env,1));
	/* Step 3: Allocate a physical page using 'page_alloc'. */
	/* Exercise 4.4: Your code here. (3/3) */
	try(page_alloc(&pp));
	/* Step 4: Map the allocated page at 'va' with permission 'perm' using 'page_insert'. */
	return page_insert(env->env_pgdir, env->env_asid, pp, va, perm);
}

/* Overview:
 *   Find the physical page mapped at 'srcva' in the address space of env 'srcid', and map 'dstid''s
 *   'dstva' to it with 'perm'.
 *
 * Post-Condition:
 *   Return 0 on success.
 *   Return -E_BAD_ENV: 'checkperm' of 'envid2env' fails for 'srcid' or 'dstid'.
 *   Return -E_INVAL: 'srcva' or 'dstva' is illegal, or 'srcva' is unmapped in 'srcid'.
 *   Return the original error: underlying calls fail.
 *
 * Hint:
 *   You may want to use the following functions:
 *   'envid2env', 'page_lookup', 'page_insert'
 */
int sys_mem_map(u_int srcid, u_int srcva, u_int dstid, u_int dstva, u_int perm) {
	struct Env *srcenv;
	struct Env *dstenv;
	struct Page *pp;

	/* Step 1: Check if 'srcva' and 'dstva' are legal user virtual addresses using
	 * 'is_illegal_va'. */
	/* Exercise 4.5: Your code here. (1/4) */
	if (is_illegal_va(srcva) || is_illegal_va(dstva)) {
		return -E_INVAL;
	}
	/* Step 2: Convert the 'srcid' to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.5: Your code here. (2/4) */
	try(envid2env(srcid, &srcenv, 1));
	/* Step 3: Convert the 'dstid' to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.5: Your code here. (3/4) */
	try(envid2env(dstid, &dstenv, 1));
	/* Step 4: Find the physical page mapped at 'srcva' in the address space of 'srcid'. */
	/* Return -E_INVAL if 'srcva' is not mapped. */
	/* Exercise 4.5: Your code here. (4/4) */
	pp = page_lookup(srcenv->env_pgdir, srcva, NULL);
	if (pp == NULL) {
		return -E_INVAL;
	}
	/* Step 5: Map the physical page at 'dstva' in the address space of 'dstid'. */
	return page_insert(dstenv->env_pgdir, dstenv->env_asid, pp, dstva, perm);
}

/* Overview:
 *   Unmap the physical page mapped at 'va' in the address space of 'envid'.
 *   If no physical page is mapped there, this function silently succeeds.
 *
 * Post-Condition:
 *   Return 0 on success.
 *   Return -E_BAD_ENV: 'checkperm' of 'envid2env' fails for 'envid'.
 *   Return -E_INVAL:   'va' is illegal.
 *   Return the original error when underlying calls fail.
 */
int sys_mem_unmap(u_int envid, u_int va) {
	struct Env *e;

	/* Step 1: Check if 'va' is a legal user virtual address using 'is_illegal_va'. */
	/* Exercise 4.6: Your code here. (1/2) */
	if (is_illegal_va(va)) {
		return -E_INVAL;
	}
	/* Step 2: Convert the envid to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.6: Your code here. (2/2) */
	try(envid2env(envid, &e, 1));
	/* Step 3: Unmap the physical page at 'va' in the address space of 'envid'. */
	page_remove(e->env_pgdir, e->env_asid, va);
	return 0;
}

/* Overview:
 *   Allocate a new env as a child of 'curenv'.
 *
 * Post-Condition:
 *   Returns the child's envid on success, and
 *   - The new env's 'env_tf' is copied from the kernel stack, except for $v0 set to 0 to indicate
 *     the return value in child.
 *   - The new env's 'env_status' is set to 'ENV_NOT_RUNNABLE'.
 *   - The new env's 'env_pri' is copied from 'curenv'.
 *   Returns the original error if underlying calls fail.
 *
 * Hint:
 *   This syscall works as an essential step in user-space 'fork' and 'spawn'.
 */
int sys_exofork(void) {
	struct Env *e;

	/* Step 1: Allocate a new env using 'env_alloc'. */
	/* Exercise 4.9: Your code here. (1/4) */
	try(env_alloc(&e, curenv->env_id));
	/* Step 2: Copy the current Trapframe below 'KSTACKTOP' to the new env's 'env_tf'. */
	/* Exercise 4.9: Your code here. (2/4) */
	e->env_tf = *((struct Trapframe *)KSTACKTOP - 1);
	/* Step 3: Set the new env's 'env_tf.regs[2]' to 0 to indicate the return value in child. */
	/* Exercise 4.9: Your code here. (3/4) */
	e->env_tf.regs[2] = 0;
	/* Step 4: Set up the new env's 'env_status' and 'env_pri'.  */
	/* Exercise 4.9: Your code here. (4/4) */
	e->env_status = ENV_NOT_RUNNABLE;
	e->env_pri = curenv->env_pri;
	return e->env_id;
}

/* Overview:
 *   Set 'envid''s 'env_status' to 'status' and update 'env_sched_list'.
 *
 * Post-Condition:
 *   Returns 0 on success.
 *   Returns -E_INVAL if 'status' is neither 'ENV_RUNNABLE' nor 'ENV_NOT_RUNNABLE'.
 *   Returns the original error if underlying calls fail.
 *
 * Hint:
 *   The invariant that 'env_sched_list' contains and only contains all runnable envs should be
 *   maintained.
 */
int sys_set_env_status(u_int envid, u_int status) {
	struct Env *env;

	/* Step 1: Check if 'status' is valid. */
	/* Exercise 4.14: Your code here. (1/3) */
	if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE) {
		return -E_INVAL;
	}
	/* Step 2: Convert the envid to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.14: Your code here. (2/3) */
	try(envid2env(envid,&env,1));
	/* Step 3: Update 'env_sched_list' if the 'env_status' of 'env' is being changed. */
	/* Exercise 4.14: Your code here. (3/3) */
	if (status == ENV_RUNNABLE && env->env_status != ENV_RUNNABLE) {
		TAILQ_INSERT_TAIL(&env_sched_list, env, env_sched_link);
	} else if (status == ENV_NOT_RUNNABLE && env->env_status != ENV_NOT_RUNNABLE) {
		TAILQ_REMOVE(&env_sched_list, env, env_sched_link);
	}
	/* Step 4: Set the 'env_status' of 'env'. */
	env->env_status = status;

	/* Step 5: Use 'schedule' with 'yield' set if ths 'env' is 'curenv'. */
	if (env == curenv) {
		schedule(1);
	}
	return 0;
}

/* Overview:
 *  Set envid's trap frame to 'tf'.
 *
 * Post-Condition:
 *  The target env's context is set to 'tf'.
 *  Returns 0 on success (except when the 'envid' is the current env, so no value could be
 * returned).
 *  Returns -E_INVAL if the environment cannot be manipulated or 'tf' is invalid.
 *  Returns the original error if other underlying calls fail.
 */
int sys_set_trapframe(u_int envid, struct Trapframe *tf) {
	if (is_illegal_va_range((u_long)tf, sizeof *tf)) {
		return -E_INVAL;
	}
	struct Env *env;
	try(envid2env(envid, &env, 1));
	if (env == curenv) {
		*((struct Trapframe *)KSTACKTOP - 1) = *tf;
		// return `tf->regs[2]` instead of 0, because return value overrides regs[2] on
		// current trapframe.
		return tf->regs[2];
	} else {
		env->env_tf = *tf;
		return 0;
	}
}

/* Overview:
 * 	Kernel panic with message `msg`.
 *
 * Post-Condition:
 * 	This function will halt the system.
 */
void sys_panic(char *msg) {
	panic("%s", TRUP(msg));
}

/* Overview:
 *   Wait for a message (a value, together with a page if 'dstva' is not 0) from other envs.
 *   'curenv' is blocked until a message is sent.
 *
 * Post-Condition:
 *   Return 0 on success.
 *   Return -E_INVAL: 'dstva' is neither 0 nor a legal address.
 */
int sys_ipc_recv(u_int dstva) {
	/* Step 1: Check if 'dstva' is either zero or a legal address. */
	if (dstva != 0 && is_illegal_va(dstva)) {
		return -E_INVAL;
	}

	/* Step 2: Set 'curenv->env_ipc_recving' to 1. */
	/* Exercise 4.8: Your code here. (1/8) */
	curenv->env_ipc_recving = 1;
	/* Step 3: Set the value of 'curenv->env_ipc_dstva'. */
	/* Exercise 4.8: Your code here. (2/8) */
	curenv->env_ipc_dstva = dstva;
	/* Step 4: Set the status of 'curenv' to 'ENV_NOT_RUNNABLE' and remove it from
	 * 'env_sched_list'. */
	/* Exercise 4.8: Your code here. (3/8) */
	curenv->env_status = ENV_NOT_RUNNABLE;
	TAILQ_REMOVE(&env_sched_list, curenv, env_sched_link);
	/* Step 5: Give up the CPU and block until a message is received. */
	((struct Trapframe *)KSTACKTOP - 1)->regs[2] = 0;
	schedule(1);
}

/* Overview:
 *   Try to send a 'value' (together with a page if 'srcva' is not 0) to the target env 'envid'.
 *
 * Post-Condition:
 *   Return 0 on success, and the target env is updated as follows:
 *   - 'env_ipc_recving' is set to 0 to block future sends.
 *   - 'env_ipc_from' is set to the sender's envid.
 *   - 'env_ipc_value' is set to the 'value'.
 *   - 'env_status' is set to 'ENV_RUNNABLE' again to recover from 'ipc_recv'.
 *   - if 'srcva' is not NULL, map 'env_ipc_dstva' to the same page mapped at 'srcva' in 'curenv'
 *     with 'perm'.
 *
 *   Return -E_IPC_NOT_RECV if the target has not been waiting for an IPC message with
 *   'sys_ipc_recv'.
 *   Return the original error when underlying calls fail.
 */
int sys_ipc_try_send(u_int envid, u_int value, u_int srcva, u_int perm) {
	struct Env *e;
	struct Page *p;

	/* Step 1: Check if 'srcva' is either zero or a legal address. */
	/* Exercise 4.8: Your code here. (4/8) */
	if (srcva != 0 && is_illegal_va(srcva)) {
		return -E_INVAL;
	}
	/* Step 2: Convert 'envid' to 'struct Env *e'. */
	/* This is the only syscall where the 'envid2env' should be used with 'checkperm' UNSET,
	 * because the target env is not restricted to 'curenv''s children. */
	/* Exercise 4.8: Your code here. (5/8) */
	try(envid2env(envid,&e,0));
	/* Step 3: Check if the target is waiting for a message. */
	/* Exercise 4.8: Your code here. (6/8) */
	if (e->env_ipc_recving != 1) {
		return -E_IPC_NOT_RECV;
	}
	/* Step 4: Set the target's ipc fields. */
	e->env_ipc_value = value;
	e->env_ipc_from = curenv->env_id;
	e->env_ipc_perm = PTE_V | perm;
	e->env_ipc_recving = 0;

	/* Step 5: Set the target's status to 'ENV_RUNNABLE' again and insert it to the tail of
	 * 'env_sched_list'. */
	/* Exercise 4.8: Your code here. (7/8) */
	e->env_status = ENV_RUNNABLE;
	TAILQ_INSERT_TAIL(&env_sched_list, e, env_sched_link);
	/* Step 6: If 'srcva' is not zero, map the page at 'srcva' in 'curenv' to 'e->env_ipc_dstva'
	 * in 'e'. */
	/* Return -E_INVAL if 'srcva' is not zero and not mapped in 'curenv'. */
	if (srcva != 0) {
		/* Exercise 4.8: Your code here. (8/8) */
		p = page_lookup(curenv->env_pgdir, srcva, NULL);
		if (p == NULL) {
			return -E_INVAL;
		}
		try(page_insert(e->env_pgdir, e->env_asid, p, e->env_ipc_dstva, perm));
	}
	return 0;
}

// XXX: kernel does busy waiting here, blocking all envs
int sys_cgetc(void) {
	int ch;
	while ((ch = scancharc()) == 0) {
	}
	return ch;
}

/* Overview:
 *  This function is used to write data at 'va' with length 'len' to a device physical address
 *  'pa'. Remember to check the validity of 'va' and 'pa' (see Hint below);
 *
 *  'va' is the starting address of source data, 'len' is the
 *  length of data (in bytes), 'pa' is the physical address of
 *  the device (maybe with a offset).
 *
 * Pre-Condition:
 *  'len' must be 1, 2 or 4, otherwise return -E_INVAL.
 *
 * Post-Condition:
 *  Data within [va, va+len) is copied to the physical address 'pa'.
 *  Return 0 on success.
 *  Return -E_INVAL on bad address.
 *
 * Hint:
 *  You can use 'is_illegal_va_range' to validate 'va'.
 *  You may use the unmapped and uncached segment in kernel address space (KSEG1)
 *  to perform MMIO by assigning a corresponding-lengthed data to the address,
 *  or you can just simply use the io function defined in 'include/io.h',
 *  such as 'iowrite32', 'iowrite16' and 'iowrite8'.
 *
 *  All valid device and their physical address ranges:
 *	* ---------------------------------*
 *	|   device   | start addr | length |
 *	* -----------+------------+--------*
 *	|  console   | 0x180003f8 | 0x20   |
 *	|  IDE disk  | 0x180001f0 | 0x8    |
 *	* ---------------------------------*
 */
int valid_addr_space_num = 2;
unsigned int valid_addr_start[2] = {0x180003f8, 0x180001f0};
unsigned int valid_addr_end[2] = {0x180003f8 + 0x20, 0x180001f8};

static inline int is_illegal_dev_range(u_long pa, u_long len) {
	if ((pa % 4 != 0 && len != 1 && len != 2) || (pa % 2 != 0 && len != 1)) {
		return 1;
	}
	int i;
	u_int target_start = pa;
	u_int target_end = pa + len;
	for (i = 0; i < valid_addr_space_num; i++) {
		if (target_start >= valid_addr_start[i] && target_end <= valid_addr_end[i]) {
			return 0;
		}
	}
	return 1;
}

int sys_write_dev(u_int va, u_int pa, u_int len) {
	/* Exercise 5.1: Your code here. (1/2) */
	if (is_illegal_va_range(va, len) || is_illegal_dev_range(pa, len) || va % len != 0) {
		return -E_INVAL;
	}
	if (len == 4) {
		iowrite32(*(uint32_t *)va, pa);
	} else if (len == 2) {
		iowrite16(*(uint16_t *)va, pa);
	} else if (len == 1) {
		iowrite8(*(uint8_t *)va, pa);
	} else {
		return -E_INVAL;
	}
	return 0;
}

/* Overview:
 *  This function is used to read data from a device physical address.
 *
 * Pre-Condition:
 *  'len' must be 1, 2 or 4, otherwise return -E_INVAL.
 *
 * Post-Condition:
 *  Data at 'pa' is copied from device to [va, va+len).
 *  Return 0 on success.
 *  Return -E_INVAL on bad address.
 *
 * Hint:
 *  You can use 'is_illegal_va_range' to validate 'va'.
 *  You can use function 'ioread32', 'ioread16' and 'ioread8' to read data from device.
 */
int sys_read_dev(u_int va, u_int pa, u_int len) {
	/* Exercise 5.1: Your code here. (2/2) */
	if (is_illegal_va_range(va, len) || is_illegal_dev_range(pa, len) || va % len != 0) {
		return -E_INVAL;
	}
	if (len == 4) {
		*(uint32_t *)va = ioread32(pa);
	} else if (len == 2) {
		*(uint16_t *)va = ioread16(pa);
	} else if (len == 1) {
		*(uint8_t *)va = ioread8(pa);
	} else {
		return -E_INVAL;
	}
	return 0;
}

// Lab 6 Shell: Implementation of SYS_getcwd
int sys_getcwd(char *buf, u_int len) {
    // Validate user-provided buffer address and ensure it's writable
    if (is_illegal_va_range((u_long)buf, len)) {
        return -E_INVAL; // Invalid user virtual address range or length
    }

    // Check if the provided buffer is large enough for the current CWD including null terminator
    size_t cwd_len = strlen(curenv->env_cwd);
    if (len < cwd_len + 1) { // +1 for null terminator
        return -E_NO_MEM; // Buffer too small
    }

    // Copy the current working directory from kernel space to user space
    strcpy(buf, curenv->env_cwd);
    return 0; // Success
}

// Lab 6 Shell: Implementation of SYS_chdir
int sys_chdir(const char *user_path) {
    char kpath[MAXPATHLEN];
    size_t len = 0;

    // Safely copy user_path from user space to kernel buffer and determine length
    // This loop ensures we don't read beyond MAXPATHLEN or into unmapped user pages.
    while (len < MAXPATHLEN - 1) {
        // Check if the current byte's address is valid in user space
        if (is_illegal_va((u_long)user_path + len)) {
            return -E_INVAL; // User path goes into unmapped/illegal region
        }
        kpath[len] = user_path[len];
        if (kpath[len] == '\0') {
            break; // Found null terminator
        }
        len++;
    }
    // If loop finished because len reached MAXPATHLEN-1, check if the last char is null.
    // If not, the path is too long.
    if (len == MAXPATHLEN - 1 && user_path[len] != '\0') {
        return -E_INVAL; // Path too long (does not fit with null terminator)
    }
    kpath[len] = '\0'; // Ensure kernel buffer is null-terminated

    // Now kpath holds a safe copy of the user-provided path.
    // Update the current environment's working directory.
    // The path validation (existence, directory type) is assumed to be handled by user-space shell.
    strcpy(curenv->env_cwd, kpath); // strcpy is safe here, operating on kernel memory
    return 0; // Success
}

// --- START OF MODIFIED SECTION ---

// Lab 6 Shell Challenge: Find a variable by name in the current environment.
// Returns a pointer to the struct Var if found, otherwise NULL.
static struct Var *find_var(const char *name) {
	for (int i = 0; i < MAX_VARS; i++) {
		if ((curenv->env_vars[i].flags & VAR_USED) && 
			strcmp(curenv->env_vars[i].name, name) == 0) {
			return &curenv->env_vars[i];
		}
	}
	return NULL;
}

// Lab 6 Shell Challenge: Set or create a variable.
int sys_set_var(const char *user_name, const char *user_value, u_int flags) {
	char name[MAX_VAR_NAME_LEN + 1];
	char value[MAX_VAR_VALUE_LEN + 1];
	int i;
	
	// Safely copy name from user space, byte by byte.
	for (i = 0; i < MAX_VAR_NAME_LEN; i++) {
		if (is_illegal_va((u_long)user_name + i)) return -E_INVAL;
		name[i] = user_name[i];
		if (name[i] == '\0') break;
	}
	name[i] = '\0'; // Ensure null termination

	// Safely copy value from user space, byte by byte.
	for (i = 0; i < MAX_VAR_VALUE_LEN; i++) {
		if (is_illegal_va((u_long)user_value + i)) return -E_INVAL;
		value[i] = user_value[i];
		if (value[i] == '\0') break;
	}
	value[i] = '\0'; // Ensure null termination

	struct Var *var = find_var(name);

	if (var) { // Variable exists
		if (var->flags & VAR_READONLY) {
			return -E_PERM; // Cannot modify a read-only variable
		}
		strcpy(var->value, value);
		var->flags |= (flags & (VAR_EXPORT | VAR_READONLY)); // Update flags (can add but not remove readonly/export)
	} else { // New variable, find a free slot
		int free_slot = -1;
		for (i = 0; i < MAX_VARS; i++) {
			if (!(curenv->env_vars[i].flags & VAR_USED)) {
				free_slot = i;
				break;
			}
		}
		if (free_slot == -1) {
			return -E_NO_MEM; // No space for new variables
		}
		var = &curenv->env_vars[free_slot];
		strcpy(var->name, name);
		strcpy(var->value, value);
		var->flags = VAR_USED | flags;
	}
	return 0;
}

// Lab 6 Shell Challenge: Get a variable. Note: The user passes a struct Var pointer.
int sys_get_var(const char *user_name, struct Var *user_buf) {
    char name[MAX_VAR_NAME_LEN + 1];
	int i;

    // Safely copy name from user space
    for (i = 0; i < MAX_VAR_NAME_LEN; i++) {
		if (is_illegal_va((u_long)user_name + i)) return -E_INVAL;
		name[i] = user_name[i];
		if (name[i] == '\0') break;
	}
	name[i] = '\0';

    if (is_illegal_va_range((u_long)user_buf, sizeof(struct Var))) return -E_INVAL;

    struct Var *var = find_var(name);
    if (!var) {
        return -E_VAR_NOT_FOUND; // Using a new error code
    }
    
    *user_buf = *var; // Copy the entire struct to user space
    return 0;
}

// Lab 6 Shell Challenge: Unset a variable.
int sys_unset_var(const char *user_name) {
    char name[MAX_VAR_NAME_LEN + 1];
    int i;
    
    // Safely copy name from user space
    for (i = 0; i < MAX_VAR_NAME_LEN; i++) {
		if (is_illegal_va((u_long)user_name + i)) return -E_INVAL;
		name[i] = user_name[i];
		if (name[i] == '\0') break;
	}
	name[i] = '\0';

    struct Var *var = find_var(name);
    if (var) {
        if (var->flags & VAR_READONLY) {
            return -E_PERM; // Cannot unset a read-only variable
        }
        var->flags = 0; // Mark as not used
    }
    // If not found, it's not an error, just succeed silently.
    return 0;
}

// Lab 6 Shell Challenge: Get a variable by its index in the env_vars array.
int sys_get_var_by_index(u_int index, struct Var *user_buf) {
    // Check if the index is valid.
    if (index >= MAX_VARS) {
        return -E_INVAL;
    }

    // Check if the user-provided buffer address is valid.
    if (is_illegal_va_range((u_long)user_buf, sizeof(struct Var))) {
        return -E_INVAL;
    }

    // Check if the variable at the given index is actually in use.
    if (!(curenv->env_vars[index].flags & VAR_USED)) {
        return -E_VAR_NOT_FOUND;
    }

    // Copy the entire variable struct to the user-provided buffer.
    *user_buf = curenv->env_vars[index];

    return 0; // Success
}

// --- END OF MODIFIED SECTION ---

void *syscall_table[MAX_SYSNO] = {
    [SYS_putchar] = sys_putchar,
    [SYS_print_cons] = sys_print_cons,
    [SYS_getenvid] = sys_getenvid,
    [SYS_yield] = sys_yield,
    [SYS_env_destroy] = sys_env_destroy,
    [SYS_set_tlb_mod_entry] = sys_set_tlb_mod_entry,
    [SYS_mem_alloc] = sys_mem_alloc,
    [SYS_mem_map] = sys_mem_map,
    [SYS_mem_unmap] = sys_mem_unmap,
    [SYS_exofork] = sys_exofork,
    [SYS_set_env_status] = sys_set_env_status,
    [SYS_set_trapframe] = sys_set_trapframe,
    [SYS_panic] = sys_panic,
    [SYS_ipc_try_send] = sys_ipc_try_send,
    [SYS_ipc_recv] = sys_ipc_recv,
    [SYS_cgetc] = sys_cgetc,
    [SYS_write_dev] = sys_write_dev,
    [SYS_read_dev] = sys_read_dev,
    // Lab 6 Shell: Add new syscalls to the table
    [SYS_getcwd] = sys_getcwd,
    [SYS_chdir] = sys_chdir,
	// Lab 6 Shell Challenge: Add new variable syscalls to the table
    [SYS_set_var] = sys_set_var,
    [SYS_get_var] = sys_get_var,
    [SYS_unset_var] = sys_unset_var,
	[SYS_get_var_by_index] = sys_get_var_by_index,
};

/* Overview:
 *   Call the function in 'syscall_table' indexed at 'sysno' with arguments from user context and
 * stack.
 *
 * Hint:
 *   Use sysno from $a0 to dispatch the syscall.
 *   The possible arguments are stored at $a1, $a2, $a3, [$sp + 16 bytes], [$sp + 20 bytes] in
 *   order.
 *   Number of arguments cannot exceed 5.
 */
void do_syscall(struct Trapframe *tf) {
	int (*func)(u_int, u_int, u_int, u_int, u_int);
	int sysno = tf->regs[4];
	if (sysno < 0 || sysno >= MAX_SYSNO) {
		tf->regs[2] = -E_NO_SYS;
		return;
	}

	/* Step 1: Add the EPC in 'tf' by a word (size of an instruction). */
	/* Exercise 4.2: Your code here. (1/4) */
	tf->cp0_epc += 4;
	/* Step 2: Use 'sysno' to get 'func' from 'syscall_table'. */
	/* Exercise 4.2: Your code here. (2/4) */
	func = syscall_table[sysno];
	/* Step 3: First 3 args are stored in $a1, $a2, $a3. */
	u_int arg1 = tf->regs[5];
	u_int arg2 = tf->regs[6];
	u_int arg3 = tf->regs[7];

	/* Step 4: Last 2 args are stored in stack at [$sp + 16 bytes], [$sp + 20 bytes]. */
	u_int arg4, arg5;
	/* Exercise 4.2: Your code here. (3/4) */
	arg4 = *((u_int *)tf->regs[29] + 4);
	arg5 = *((u_int *)tf->regs[29] + 5);
	/* Step 5: Invoke 'func' with retrieved arguments and store its return value to $v0 in 'tf'.
	 */
	/* Exercise 4.2: Your code here. (4/4) */
	tf->regs[2] = func(arg1, arg2, arg3, arg4, arg5);
}