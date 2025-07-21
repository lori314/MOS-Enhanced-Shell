#ifndef _ENV_H_
#define _ENV_H_

#include <mmu.h>
#include <queue.h>
#include <trap.h>
#include <types.h> // This still provides u_int etc.

// Lab 6 Shell: Define MAXPATHLEN directly for kernel-side usage in Env struct
// Keeping it in sync with user/include/fs.h's definition (1024)
#define MAXPATHLEN 1024

#define LOG2NENV 10
#define NENV (1 << LOG2NENV)
#define ENVX(envid) ((envid) & (NENV - 1))

// All possible values of 'env_status' in 'struct Env'.
#define ENV_FREE 0
#define ENV_RUNNABLE 1
#define ENV_NOT_RUNNABLE 2
// Lab 6 Shell Challenge: Add a status for zombie processes.
#define ENV_ZOMBIE 3

// Lab 6 Shell Challenge: Environment Variables Constants
#define MAX_VARS 64 // Maximum number of variables per environment
#define MAX_VAR_NAME_LEN 16
#define MAX_VAR_VALUE_LEN 16

// Variable flags
#define VAR_USED      0x1   // Flag to indicate if the slot is in use
#define VAR_EXPORT    0x2   // Flag for environment variable (exportable, -x)
#define VAR_READONLY  0x4   // Flag for read-only variable (-r)

// Structure to hold a single variable's data
struct Var {
	char name[MAX_VAR_NAME_LEN + 1];
	char value[MAX_VAR_VALUE_LEN + 1];
	u_int flags;
};

// Control block of an environment (process).
struct Env {
	struct Trapframe env_tf;	 // saved context (registers) before switching
	LIST_ENTRY(Env) env_link;	 // intrusive entry in 'env_free_list'
	u_int env_id;			 // unique environment identifier
	u_int env_asid;			 // ASID of this env
	u_int env_parent_id;		 // env_id of this env's parent
	u_int env_status;		 // status of this env
	Pde *env_pgdir;			 // page directory
	TAILQ_ENTRY(Env) env_sched_link; // intrusive entry in 'env_sched_list'
	u_int env_pri;			 // schedule priority

	// Lab 4 IPC
	u_int env_ipc_value;   // the value sent to us
	u_int env_ipc_from;    // envid of the sender
	u_int env_ipc_recving; // whether this env is blocked receiving
	u_int env_ipc_dstva;   // va at which the received page should be mapped
	u_int env_ipc_perm;    // perm in which the received page should be mapped

	// Lab 4 fault handling
	u_int env_user_tlb_mod_entry; // userspace TLB Mod handler

	// Lab 6 scheduler counts
	u_int env_runs; // number of times we've been env_run'ed

	// Lab 6 Shell: Current Working Directory (CWD)
	char env_cwd[MAXPATHLEN]; // Stores the current working directory for the environment.

	// Lab 6 Shell Challenge: Exit status for wait()
	int env_exit_status; // Stores the exit status of the environment for the parent to retrieve.

	// Lab 6 Shell Challenge: Environment and local variables
	// FIX: This is now a pointer. Memory will be allocated in env_alloc.
	struct Var *env_vars;
};

LIST_HEAD(Env_list, Env);
TAILQ_HEAD(Env_sched_list, Env);
extern struct Env *curenv;		     // the current env
extern struct Env_sched_list env_sched_list; // runnable env list

void env_init(void);
int env_alloc(struct Env **e, u_int parent_id);
void env_free(struct Env *);
struct Env *env_create(const void *binary, size_t size, int priority);
void env_destroy(struct Env *e);

int envid2env(u_int envid, struct Env **penv, int checkperm);
void env_run(struct Env *e) __attribute__((noreturn));

void env_check(void);
void envid2env_check(void);

#define ENV_CREATE_PRIORITY(x, y)                                                                  \
	({                                                                                         \
		extern u_char binary_##x##_start[];                                                \
		extern u_int binary_##x##_size;                                                    \
		env_create(binary_##x##_start, (u_int)binary_##x##_size, y);                       \
	})

#define ENV_CREATE(x)                                                                              \
	({                                                                                         \
		extern u_char binary_##x##_start[];                                                \
		extern u_int binary_##x##_size;                                                    \
		env_create(binary_##x##_start, (u_int)binary_##x##_size, 1);                       \
	})

#endif // !_ENV_H_