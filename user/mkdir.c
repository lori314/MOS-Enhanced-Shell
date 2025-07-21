#include <lib.h>
#include <string.h>

// Forward declaration of the recursive helper function for 'mkdir -p'
static int mkdir_p(const char *path);

// Custom implementation of strrchr, as it might not be in the standard library.
static char *strrchr(const char *s, int c) {
	const char *last = NULL;
	while (*s) {
		if (*s == (char)c) {
			last = s;
		}
		s++;
	}
	return (char *)last;
}

void main(int argc, char **argv) {
	int i;
	int p_flag = 0;
	int exit_code = 0;

	if (argc < 2) {
		printf("mkdir: missing operand\n");
		exit(1);
	}

	// Check for the -p flag.
	if (strcmp(argv[1], "-p") == 0) {
		p_flag = 1;
		i = 2; // Arguments start after -p.
		if (argc < 3) {
			printf("mkdir: missing operand after '-p'\n");
			exit(1);
		}
	} else {
		i = 1; // Arguments start at argv[1].
	}

	// Loop through all provided path arguments.
	for (; i < argc; i++) {
		const char *path = argv[i];
		int r;

		if (p_flag) {
			r = mkdir_p(path);
		} else {
			// Standard mkdir behavior.
			r = open(path, O_CREAT | O_MKDIR);
			if (r >= 0) {
				close(r);
				r = 0; // Set result to 0 for success.
			}
		}

		// Handle errors and set the final exit code.
		if (r < 0) {
			exit_code = 1; // Mark that at least one operation failed.
			if (r == -E_FILE_EXISTS) {
				printf("mkdir: cannot create directory '%s': File exists\n", path);
			} else if (r == -E_NOT_FOUND) {
				printf("mkdir: cannot create directory '%s': No such file or directory\n", path);
			} else {
				// A generic error for other cases.
				printf("mkdir: cannot create directory '%s': Unexpected error\n", path);
			}
		}
	}

	exit(exit_code);
}

// Recursive helper function for 'mkdir -p'.
static int mkdir_p(const char *path) {
	int r;
	struct Stat st;

	// First, try to create the directory.
	r = open(path, O_CREAT | O_MKDIR);
	if (r >= 0) {
		close(r);
		return 0; // Success.
	}

	// If it failed, check why.
	if (r == -E_FILE_EXISTS) {
		// Path already exists. Check if it's a directory.
		if (stat(path, &st) < 0) {
			return -E_UNSPECIFIED; // Should not happen, but for safety.
		}
		if (st.st_isdir) {
			return 0; // Already a directory, this is success for 'mkdir -p'.
		} else {
			return -E_FILE_EXISTS; // It's a file, this is an error.
		}
	}

	if (r != -E_NOT_FOUND) {
		// Any other error is a failure.
		return r;
	}

	// The error was -E_NOT_FOUND, so the parent directory might be missing.
	// Find the parent path.
	char parent_path[MAXPATHLEN];
	strcpy(parent_path, path);
	char *p = strrchr(parent_path, '/');

	if (p != NULL) {
		// Cut off the last component to get the parent path.
		if (p == parent_path) { // Path starts with '/', e.g., "/a" -> parent is "/".
			parent_path[1] = '\0';
		} else { // Path is like "a/b" -> parent is "a".
			*p = '\0';
		}
	} else {
		// No '/' in path (e.g., "a"). No parent to create, so the error is final.
		return r;
	}

	// Prevent infinite recursion if path calculation fails or we're at the root.
	if (strcmp(path, parent_path) == 0) {
		return r;
	}

	// Recursively create the parent.
	r = mkdir_p(parent_path);
	if (r < 0) {
		return r; // Propagate error from parent creation.
	}

	// Now that the parent should exist, retry creating the original path.
	r = open(path, O_CREAT | O_MKDIR);
	if (r >= 0) {
		close(r);
		return 0; // Success.
	}

	// If it still fails, return the new error.
	return r;
}