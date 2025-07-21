#ifndef SYSCALL_H
#define SYSCALL_H

#ifndef __ASSEMBLER__

enum {
	SYS_putchar,
	SYS_print_cons,
	SYS_getenvid,
	SYS_yield,
	SYS_env_destroy,
	SYS_set_tlb_mod_entry,
	SYS_mem_alloc,
	SYS_mem_map,
	SYS_mem_unmap,
	SYS_exofork,
	SYS_set_env_status,
	SYS_set_trapframe,
	SYS_panic,
	SYS_ipc_try_send,
	SYS_ipc_recv,
	SYS_cgetc,
	SYS_write_dev,
	SYS_read_dev,
	// Lab 6 Shell: New syscalls for Current Working Directory
	SYS_getcwd, // Get current working directory
	SYS_chdir,  // Change current working directory
	// Lab 6 Shell Challenge: New syscalls for environment variables
	SYS_set_var,   // Set/update a variable
	SYS_get_var,   // Get a variable's value by name
	SYS_unset_var, // Unset a variable
	SYS_get_var_by_index, // Get a variable by its index
	MAX_SYSNO,
};

#endif

#endif