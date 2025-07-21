#include <env.h>
#include <lib.h>
#include <syscall.h>

/*
 * The updated wait() function.
 * It polls for a child to become a zombie, retrieves its exit status,
 * and then calls the modified syscall_env_destroy to reap it.
 */
int wait(u_int envid) {
	const volatile struct Env *e;

	e = &envs[ENVX(envid)];

	// Poll until the child is no longer running.
	while (e->env_id == envid && (e->env_status == ENV_RUNNABLE || e->env_status == ENV_NOT_RUNNABLE)) {
		syscall_yield();
	}

	// Check if the child is a zombie.
	if (e->env_id == envid && e->env_status == ENV_ZOMBIE) {
		int exit_status = e->env_exit_status;

		// "Reap" the zombie. We pass a dummy status '0' because the kernel
		// only cares about the status when a process exits by itself (envid=0).
		syscall_env_destroy(envid, 0); // MODIFIED: Added second argument

		return exit_status;
	}

	return -1;
}