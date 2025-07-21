#ifndef _STRING_H_
#define _STRING_H_

#include <types.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
const char *strchr(const char *s, int c);
int strcmp(const char *p, const char *q);

// Add declarations for the missing functions
void *memmove(void *dst, const void *src, size_t n);
char *strncpy(char *dst, const char *src, size_t n);

#endif