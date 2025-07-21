#ifndef PATH_H
#define PATH_H

#include <types.h>

// Resolves a relative or absolute path into a canonical absolute path.
int resolve_path(const char *path, char *resolved_path, size_t max_len);

#endif