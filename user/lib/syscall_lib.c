#include <env.h>
#include <lib.h>
#include <mmu.h>
#include <syscall.h>
#include <trap.h>

void syscall_putchar(int ch) {
	msyscall(SYS_putchar, ch);
}

int syscall_print_cons(const void *str, u_int num) {
	return msyscall(SYS_print_cons, str, num);
}

u_int syscall_getenvid(void) {
	return msyscall(SYS_getenvid);
}

void syscall_yield(void) {
	msyscall(SYS_yield);
}

// --- MODIFICATION START ---
// The definition is updated to match the declaration in lib.h.
// It now accepts a status and passes it to the kernel via msyscall.
int syscall_env_destroy(u_int envid, int status) {
	return msyscall(SYS_env_destroy, envid, status);
}
// --- MODIFICATION END ---

int syscall_set_tlb_mod_entry(u_int envid, void (*func)(struct Trapframe *)) {
	return msyscall(SYS_set_tlb_mod_entry, envid, func);
}

int syscall_mem_alloc(u_int envid, void *va, u_int perm) {
	return msyscall(SYS_mem_alloc, envid, va, perm);
}

int syscall_mem_map(u_int srcid, void *srcva, u_int dstid, void *dstva, u_int perm) {
	return msyscall(SYS_mem_map, srcid, srcva, dstid, dstva, perm);
}

int syscall_mem_unmap(u_int envid, void *va) {
	return msyscall(SYS_mem_unmap, envid, va);
}

int syscall_set_env_status(u_int envid, u_int status) {
	return msyscall(SYS_set_env_status, envid, status);
}

int syscall_set_trapframe(u_int envid, struct Trapframe *tf) {
	return msyscall(SYS_set_trapframe, envid, tf);
}

void syscall_panic(const char *msg) {
	int r = msyscall(SYS_panic, msg);
	user_panic("SYS_panic returned %d", r);
}

int syscall_ipc_try_send(u_int envid, u_int value, const void *srcva, u_int perm) {
	return msyscall(SYS_ipc_try_send, envid, value, srcva, perm);
}

int syscall_ipc_recv(void *dstva) {
	return msyscall(SYS_ipc_recv, dstva);
}

int syscall_cgetc() {
	return msyscall(SYS_cgetc);
}

int syscall_write_dev(void *va, u_int dev, u_int size) {
	return msyscall(SYS_write_dev, va, dev, size);
}

int syscall_read_dev(void *va, u_int dev, u_int size) {
	return msyscall(SYS_read_dev, va, dev, size);
}

int syscall_getcwd(char *buf, u_int len) {
	return msyscall(SYS_getcwd, buf, len);
}

int syscall_chdir(const char *path) {
	return msyscall(SYS_chdir, path);
}

// Shell Challenge: Implementations for environment variable syscall wrappers
int syscall_set_var(const char *name, const char *value, u_int flags) {
    return msyscall(SYS_set_var, name, value, flags);
}

int syscall_get_var(const char *name, struct Var *buf) {
    return msyscall(SYS_get_var, name, buf);
}

int syscall_unset_var(const char *name) {
    return msyscall(SYS_unset_var, name);
}

int syscall_get_var_by_index(u_int index, struct Var *buf) {
    return msyscall(SYS_get_var_by_index, index, buf);
}