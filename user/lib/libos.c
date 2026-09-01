#include <env.h>
#include <lib.h>
#include <mmu.h>
#include <syscall.h>

/*
 * Exit a user process with an integer status code.
 */
void exit(int status) {
	// After fs is ready (lab5), all our open files should be closed before dying.
#if !defined(LAB) || LAB >= 5
	close_all();
#endif

	// Call the destroy syscall, but for the current environment (envid=0).
	// The kernel will interpret this as a self-termination and will use
	// the provided 'status' as the exit code, transitioning the process
	// into a zombie state.
	syscall_env_destroy(0, status);

	// This part should not be reached if the syscall works correctly.
	user_panic("unreachable code from exit");
}

const volatile struct Env *env;
extern int main(int, char **);

/*
 * The main entry point for user-space programs.
 */
void libmain(int argc, char **argv) {
	// set env to point at our env structure in envs[].
	env = &envs[ENVX(syscall_getenvid())];

	// call user main routine and CAPTURE its return value
	int ret_val = main(argc, argv);

	// exit gracefully, passing the return value from main as the exit status
	exit(ret_val);
}