#include <lib.h>
#include <fs.h>

// A general-purpose path resolver.
// It is designed to be used by any user-space program.
int resolve_path(const char *path, char *resolved_path, size_t max_len) {
	char cwd[MAXPATHLEN];
	char component[MAXNAMELEN];

	if (syscall_getcwd(cwd, sizeof(cwd)) < 0) {
		return -1; // Failed to get current working directory
	}
	
	const char *p = path;

	if (path == NULL || *path == '\0') {
		strcpy(resolved_path, "/");
		return 0;
	}

	if (path[0] == '/') {
		// Absolute path, start from root.
		if (max_len < 2) return -1;
		strcpy(resolved_path, "/");
		p = path + 1;
	} else {
		// Relative path, start from current working directory.
		if (strlen(cwd) >= max_len) return -1;
		strcpy(resolved_path, cwd);
	}

	// Process each component of the path.
	while (*p != '\0') {
		while (*p == '/') p++; // Skip leading slashes
		if (*p == '\0') break;

		const char *end = strchr(p, '/');
		size_t comp_len = (end == NULL) ? strlen(p) : (end - p);

		if (comp_len >= MAXNAMELEN) return -1;
		memcpy(component, p, comp_len);
		component[comp_len] = '\0';
		p += comp_len;

		if (strcmp(component, ".") == 0) {
			continue; // Ignore '.'
		}
		if (strcmp(component, "..") == 0) {
			// Go up one directory.
			size_t len = strlen(resolved_path);
			if (len > 1) { // Can't go up from root "/"
				size_t i = len - 1;
				if (resolved_path[i] == '/') i--; // Handle trailing slash
				while (i > 0 && resolved_path[i] != '/') i--;
				resolved_path[i > 0 ? i : 1] = '\0';
			}
		} else {
			// Append the new component.
			size_t len = strlen(resolved_path);
			// Add a separator slash if not at root.
			if (len > 1) {
				if (len + 1 >= max_len) return -1;
				resolved_path[len++] = '/';
				resolved_path[len] = '\0';
			}
			// Append component name.
			if (len + comp_len >= max_len) return -1;
			strcpy(resolved_path + len, component);
		}
	}

	if (resolved_path[0] == '\0') {
		strcpy(resolved_path, "/");
	}
	return 0;
}