#include <args.h>
#include <lib.h>
#include <fs.h>
#include <env.h>
#include "shell.h"
#include <string.h>

#define O_APPEND 0x1000

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

// Forward declaration
int runcmd(char *s);

// A simple getline function for reading from script files (fd 0)
static int getline(char *buf, int n) {
	int i, r;
	for (i = 0; i < n - 1; i++) {
		if ((r = read(0, &buf[i], 1)) != 1) {
			if (r < 0 || i == 0)
				return -1; // Error or EOF at start
			break;	       // EOF in the middle of a line
		}
		if (buf[i] == '\n' || buf[i] == '\r') {
			break; // End of line
		}
	}
	buf[i] = '\0';
	return i;
}

// Shell Challenge: simple isalnum, because we don't have ctype.h
static inline int isalnum(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// Shell Challenge: Helper to get the value of a variable.
// Returns an empty string for non-existent variables.
const char *get_var_value(const char *name) {
	static struct Var var; // Use a static struct to hold the result
	if (syscall_get_var(name, &var) < 0) {
		return ""; // Not found, return empty string
	}
	return var.value;
}

/*
 * Safely peeks at the first word of a command string without modifying it.
 */
void peek_first_word(char *buf, char *word_buf, int word_buf_len) {
	char *s = buf;
	while (*s != '\0' && strchr(WHITESPACE, *s)) {
		s++;
	}
	int i = 0;
	while (*s != '\0' && !strchr(WHITESPACE, *s) && !strchr(SYMBOLS, *s) && i < word_buf_len - 1) {
		word_buf[i++] = *s++;
	}
	word_buf[i] = '\0';
}

/*
 * Tokenizer function.
 */
int _gettoken(char *s, char **p1, char **p2) {
	*p1 = 0;
	*p2 = 0;
	if (s == 0)
		return 0;
	while (strchr(WHITESPACE, *s))
		*s++ = 0;
	if (*s == 0)
		return 0;

	// Check for '>>' before checking for single char symbols
	if (*s == '>' && *(s + 1) == '>') {
		*p1 = s;
		s[0] = 0; // Null terminate to create the token
		s[1] = 0;
		*p2 = s + 2;
		return 'a'; // Use 'a' as a special token for append
	}

	if (strchr(SYMBOLS, *s)) {
		int t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}
	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s))
		s++;
	*p2 = s;
	return 'w';
}

int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;
	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

#define MAXARGS 128

/*
 * Resolves a potentially relative path against a given current working directory (cwd).
 */
static int resolve_path(const char *cwd, const char *path, char *resolved_path, size_t max_len) {
	char component[MAXNAMELEN];
	const char *p = path;
	if (path == NULL || *path == '\0') {
		if (max_len < 2)
			return -1;
		strcpy(resolved_path, "/");
		return 0;
	}
	if (path[0] == '/') {
		if (max_len < 2)
			return -1;
		strcpy(resolved_path, "/");
		p = path + 1;
	} else {
		if (strlen(cwd) >= max_len)
			return -1;
		strcpy(resolved_path, cwd);
	}
	while (*p != '\0') {
		while (*p == '/')
			p++;
		if (*p == '\0')
			break;
		const char *end = strchr(p, '/'); // Uses strchr from <string.h>
		size_t comp_len = (end == NULL) ? strlen(p) : (size_t)(end - p);
		if (comp_len >= MAXNAMELEN)
			return -1;
		memcpy(component, p, comp_len);
		component[comp_len] = '\0';
		p += comp_len;
		if (strcmp(component, ".") == 0)
			continue;
		if (strcmp(component, "..") == 0) {
			size_t len = strlen(resolved_path);
			if (len > 1) {
				size_t i = len - 1;
				if (resolved_path[i] == '/')
					i--;
				while (i > 0 && resolved_path[i] != '/')
					i--;
				resolved_path[i > 0 ? i : 1] = '\0';
			}
		} else {
			size_t len = strlen(resolved_path);
			if (len > 1) {
				if (len + 1 >= max_len)
					return -1;
				resolved_path[len++] = '/';
				resolved_path[len] = '\0';
			}
			if (len + comp_len >= max_len)
				return -1;
			strcpy(resolved_path + len, component);
		}
	}
	if (resolved_path[0] == '\0') {
		if (max_len < 2)
			return -1;
		strcpy(resolved_path, "/");
	}
	return 0;
}

int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;
	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		switch (c) {
		case 0:
			return argc;
		case 'w':
			if (argc >= MAXARGS) {
				printf("too many arguments\n");
				return -1;
			}
			argv[argc++] = t;
			break;
		case '<':
		case '>':
		case 'a': // Special token for '>>'
			if (gettoken(0, &t) != 'w') {
				printf("syntax error: redirection not followed by word\n");
				return -1;
			}
			char cwd[MAXPATHLEN], resolved_path[MAXPATHLEN];
			if (syscall_getcwd(cwd, sizeof(cwd)) < 0) {
				return -1;
			}
			if (resolve_path(cwd, t, resolved_path, sizeof(resolved_path)) != 0) {
				return -1;
			}

			int dup_fd_num = (c == '<') ? 0 : 1;

			if (c == '<') { // Input redirection
				if ((fd = open(resolved_path, O_RDONLY)) < 0) {
					return -1;
				}
			} else if (c == '>') { // Output redirection (truncate)
				// The reliable way to truncate: remove the file first, then create it.
				// We ignore the error from remove() because the file might not exist.
				remove(resolved_path);
				if ((fd = open(resolved_path, O_RDWR | O_CREAT)) < 0) {
					return -1;
				}
			} else { // Append redirection '>>'
				fd = open(resolved_path, O_RDWR);
				if (fd < 0) {
					fd = open(resolved_path, O_RDWR | O_CREAT);
					if (fd < 0) {
						return -1;
					}
				} else {
					struct Stat st;
					if ((r = fstat(fd, &st)) < 0) {
						close(fd);
						return -1;
					}
					if ((r = seek(fd, st.st_size)) < 0) {
						close(fd);
						return -1;
					}
				}
			}

			dup(fd, dup_fd_num);
			close(fd);
			break;
		case '|':;
			int p[2];
			if ((r = pipe(p)) < 0) {
				return -1;
			}
			if ((*rightpipe = fork()) < 0) {
				return -1;
			}
			if (*rightpipe == 0) {
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe);
			} else {
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}
			break;
		}
	}
	return argc;
}

/*
 * Executes a command string. For built-ins, returns 0 on success,
 * -1 on failure. For commands with pipes, it returns the PID of the rightmost
 * process. For external commands, it spawns, waits, and exits with status.
 */
int runcmd(char *s) {
	char cmd_copy[1024];
	strcpy(cmd_copy, s);
	gettoken(cmd_copy, 0);

	char *argv[MAXARGS];
	int rightpipe = 0;
	int argc = parsecmd(argv, &rightpipe);

	if (argc < 0)
		return -1;
	if (argc == 0)
		return rightpipe;

	argv[argc] = 0;

	if (strcmp(argv[0], "cd") == 0) {
		if (argc > 2) {
			printf("Too many args for cd command\n");
			return -1;
		} else {
			const char *original_path = (argc == 1) ? "/" : argv[1];
			const char *target_path_for_resolve = (argc == 1) ? NULL : argv[1];

			char cwd[MAXPATHLEN], resolved_path[MAXPATHLEN];
			if (syscall_getcwd(cwd, sizeof(cwd)) < 0) {
				printf("cd: could not get current directory\n");
				return -1;
			} else if (resolve_path(cwd, target_path_for_resolve, resolved_path,
						sizeof(resolved_path)) != 0) {
				printf("cd: invalid path\n");
				return -1;
			} else {
				struct Stat st;
				if (stat(resolved_path, &st) < 0) {
					printf("cd: The directory '%s' does not exist\n", original_path);
					return -1;
				} else if (!st.st_isdir) {
					printf("cd: '%s' is not a directory\n", original_path);
					return -1;
				} else if (syscall_chdir(resolved_path) < 0) {
					printf("cd: failed to change to '%s'\n", original_path);
					return -1;
				}
			}
		}
		return rightpipe;
	}

	if (strcmp(argv[0], "pwd") == 0) {
		if (argc > 1) {
			printf("pwd: expected 0 arguments; got %d\n", argc - 1);
			return -1;
		} else {
			char cwd_buf[MAXPATHLEN];
			if (syscall_getcwd(cwd_buf, sizeof(cwd_buf)) < 0) {
				printf("pwd: failed to get current directory\n");
				return -1;
			} else {
				printf("%s\n", cwd_buf);
			}
		}
		return rightpipe;
	}

	if (strcmp(argv[0], "declare") == 0) {
		if (argc == 1) {
			struct Var var;
			for (int i = 0; i < MAX_VARS; i++) {
				if (syscall_get_var_by_index(i, &var) == 0) {
					printf("%s=%s\n", var.name, var.value);
				}
			}
		} else {
			u_int flags = 0;
			int arg_idx = 1;
			while (arg_idx < argc && argv[arg_idx][0] == '-') {
				for (int i = 1; argv[arg_idx][i]; i++) {
					if (argv[arg_idx][i] == 'x')
						flags |= VAR_EXPORT;
					else if (argv[arg_idx][i] == 'r')
						flags |= VAR_READONLY;
				}
				arg_idx++;
			}

			if (arg_idx < argc) {
				char *arg = argv[arg_idx];
				char *name = arg;
				char *value = "";
				char *eq = (char *)strchr(arg, '='); // Cast to silence warning
				if (eq) {
					*eq = '\0';
					value = eq + 1;
				}
				if (syscall_set_var(name, value, flags) < 0) {
					printf("declare: cannot set variable %s\n", name);
					return -1;
				}
			}
		}
		return rightpipe;
	}

	if (strcmp(argv[0], "unset") == 0) {
		if (argc != 2) {
			printf("unset: expected 1 argument\n");
			return -1;
		} else {
			if (syscall_unset_var(argv[1]) < 0) {
				printf("unset: cannot unset read-only variable %s\n", argv[1]);
				return -1;
			}
		}
		return rightpipe;
	}

	int child;
	if ((child = spawn(argv[0], argv)) < 0) {
		if (rightpipe)
			wait(rightpipe);
		exit(1);
	}
	close_all();

	int status = 0;
	if (child >= 0) {
		int child_exit_status = wait(child); // Get child's exit status
		status = child_exit_status;	     // Store the actual exit status
	}
	if (rightpipe) {
		status = wait(rightpipe);
	}
	exit(status);
}

char buf[1024];
char expanded_buf[2048];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit(1);
}

void run_history_cmd(void) {
	int fd, n;
	char read_buf[512];
	if ((fd = open("/.mos_history", O_RDONLY)) < 0) {
		return;
	}
	while ((n = read(fd, read_buf, sizeof(read_buf) - 1)) > 0) {
		read_buf[n] = '\0';
		printf("%s", read_buf);
	}
	close(fd);
}

// Helper function to handle backticks by running a command and capturing its output.
// Returns the child's exit status.
int run_and_capture_output(char *cmd, char *output_buf, int output_size) {
	int p[2];
	if (pipe(p) < 0) {
		return -1;
	}
	int child = fork();
	if (child < 0) {
		close(p[0]);
		close(p[1]);
		return -1;
	}
	if (child == 0) {
		close(p[0]);
		dup(p[1], 1);
		close(p[1]);

		char cmd_copy[1024];
		strcpy(cmd_copy, cmd);
		char first_word[128];
		peek_first_word(cmd_copy, first_word, sizeof(first_word));
		
		if (strcmp(first_word, "cd") == 0 || strcmp(first_word, "pwd") == 0 ||
		    strcmp(first_word, "declare") == 0 || strcmp(first_word, "unset") == 0) {
			int ret_code = runcmd(cmd);
			exit(ret_code < 0 ? 1 : 0);
		} else {
			runcmd(cmd);
			exit(1); 
		}
	} else {
		close(p[1]);
		int n, total_read = 0;
		while (total_read < output_size - 1 &&
		       (n = read(p[0], output_buf + total_read, output_size - total_read - 1)) > 0) {
			total_read += n;
		}
		output_buf[total_read] = '\0';
		close(p[0]);

		int child_exit_status = wait(child);

		for (int i = 0; i < total_read; ++i) {
			if (output_buf[i] == '\n' || output_buf[i] == '\r') {
				output_buf[i] = ' ';
			}
		}
		if (total_read > 0 && output_buf[total_read - 1] == ' ') {
			output_buf[total_read - 1] = '\0';
		}

		return child_exit_status;
	}
}

// Custom implementation of strstr since it's not guaranteed to be available
char *my_strstr(const char *haystack, const char *needle) {
	if (!*needle) {
		return (char *)haystack;
	}
	for (; *haystack; haystack++) {
		const char *h = haystack;
		const char *n = needle;
		while (*h && *n && *h == *n) {
			h++;
			n++;
		}
		if (!*n) { 
			return (char *)haystack;
		}
	}
	return NULL;
}

// Helper function: Finds and replaces backticked commands with their output.
// Returns the status of the command executed within the backticks, or -1 on error.
int handle_backticks(char *cmd_in, char *cmd_out, int out_size) {
	char *p_in = cmd_in;
	char *p_out = cmd_out;
	char *end_out = cmd_out + out_size - 1;

	int captured_cmd_status = 0;

	while (*p_in) {
		if (*p_in == '`') {
			p_in++;
			char *end_tick = (char *)strchr(p_in, '`');
			if (!end_tick) {
				printf("Syntax error: missing closing backtick\n");
				strcpy(cmd_out, cmd_in);
				return -1;
			}

			char sub_cmd[1024];
			strncpy(sub_cmd, p_in, end_tick - p_in);
			sub_cmd[end_tick - p_in] = '\0';

			char output_buf[1024];

			int cmd_status =
			    run_and_capture_output(sub_cmd, output_buf, sizeof(output_buf));

			if (cmd_status < 0) {
				return -1;
			}

			if (cmd_status != 0) {
				captured_cmd_status = cmd_status;
			}

			int len = strlen(output_buf);
			if (p_out + len >= end_out) {
				printf("Error: command substitution output is too large\n");
				return -1;
			}
			strcpy(p_out, output_buf);
			p_out += len;
			p_in = end_tick + 1;
		} else {
			if (p_out >= end_out) {
				printf("Error: command is too large after expansion\n");
				return -1;
			}
			*p_out++ = *p_in++;
		}
	}
	*p_out = '\0';

	return captured_cmd_status;
}

// main() function to handle complex command logic
int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2024                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");

	if (interactive) {
		history_load();
	}

	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0)
			user_panic("open %s: %d", argv[0], r);
		user_assert(r == 0);
		interactive = 0;
	}

	for (;;) {
		if (interactive) {
			if (readline_advanced(buf, sizeof buf) < 0)
				break;
		} else {
			if (getline(buf, sizeof buf) < 0)
				break;
		}

				// Normalize input before recording it in history

		// 1. Strip comments
		char *comment = (char *)strchr(buf, '#');
		if (comment) {
			*comment = '\0';
		}
		
		// 2. Trim trailing whitespace
		int len = strlen(buf);
		while (len > 0 && strchr(WHITESPACE, buf[len-1])) {
			len--;
		}
		buf[len] = '\0';

		// 3. Add the cleaned command to history (if not empty)
		if (interactive && buf[0] != '\0') {
			history_add(buf);
		}
		

		// Variable expansion
		char *p = buf;
		char *ep = expanded_buf;
		char var_name[MAX_VAR_NAME_LEN + 1];
		while (*p) {
			if (*p == '$') {
				p++;
				char *vn = var_name;
				while (*p && (isalnum(*p) || *p == '_')) {
					if (vn < var_name + MAX_VAR_NAME_LEN)
						*vn++ = *p++;
					else
						p++;
				}
				*vn = '\0';
				const char *val = get_var_value(var_name);
				strcpy(ep, val);
				ep += strlen(val);
			} else {
				*ep++ = *p++;
			}
		}
		*ep = '\0';

		if (echocmds)
			printf("# %s\n", expanded_buf);

		// Main command execution loop for handling ';', '&&', '||'
		char *line = expanded_buf;
		int last_status = 0;
		int should_run = 1;

		while (*line) {
			while (*line && strchr(WHITESPACE, *line))
				line++;
			if (*line == '\0')
				break;

			char *next_and = (char *)my_strstr(line, "&&");
			char *next_or = (char *)my_strstr(line, "||");
			char *next_semi = (char *)strchr(line, ';');
			char *separator = NULL;
			int sep_len = 0;

			if (next_and && (!separator || next_and < separator))
				separator = next_and;
			if (next_or && (!separator || next_or < separator))
				separator = next_or;
			if (next_semi && (!separator || next_semi < separator))
				separator = next_semi;

			char sub_cmd_buf[2048];
			if (separator) {
				sep_len = (separator == next_semi) ? 1 : 2;
				strncpy(sub_cmd_buf, line, separator - line);
				sub_cmd_buf[separator - line] = '\0';
			} else {
				strcpy(sub_cmd_buf, line);
			}

			if (should_run) {
				char final_cmd_buf[2048];
				int substitution_status = handle_backticks(sub_cmd_buf, final_cmd_buf, sizeof(final_cmd_buf));

				if (substitution_status < 0) {
					last_status = 1; 
				} else {
					if (substitution_status != 0) {
						last_status = substitution_status;
					}

					char first_word[128];
					peek_first_word(final_cmd_buf, first_word, sizeof(first_word));
					
					if (strcmp(first_word, "exit") == 0) {
						exit(0);
					} else if (strcmp(first_word, "history") == 0) {
						run_history_cmd();
						last_status = 0;
					} else if (strcmp(first_word, "cd") == 0 || strcmp(first_word, "pwd") == 0 ||
						   strcmp(first_word, "declare") == 0 || strcmp(first_word, "unset") == 0) {
						int ret_code = runcmd(final_cmd_buf);

						if (ret_code < 0) {
							last_status = 1;
						} else if (ret_code > 0) {
							last_status = wait(ret_code);
						} else {
							last_status = 0;
						}
					} else { 
						if ((r = fork()) < 0) user_panic("fork: %d", r);
						if (r == 0) {
							runcmd(final_cmd_buf);
							exit(1); 
						} else {
							last_status = wait(r);
						}
					}
				}
			}

			// Decide if the next command should run
			if (separator) {
				if (sep_len == 2 && separator[0] == '&') { // &&
					should_run = (last_status == 0);
				} else if (sep_len == 2 && separator[0] == '|') { // ||
					should_run = (last_status != 0);
				} else { // ;
					should_run = 1;
				}
				line = separator + sep_len;
			} else {
				break; 
			}
		}
	}
	return 0;
}