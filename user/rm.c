#include <lib.h>
#include <fs.h>

static int rm_path(const char *path, int r_flag, int f_flag);

int main(int argc, char **argv) {
    int r_flag = 0;
    int f_flag = 0;
    int i;
    int exit_code = 0;
    int path_start_index = 1;

    if (argc < 2) {
        printf("rm: missing operand\n");
        return 1;
    }

    // Argument parsing
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            path_start_index = i;
            break;
        }
        if (argv[i][1] == '\0') {
            path_start_index = i;
            break;
        }
        for (int j = 1; argv[i][j] != '\0'; j++) {
            if (argv[i][j] == 'r') {
                r_flag = 1;
            } else if (argv[i][j] == 'f') {
                f_flag = 1;
            } else {
                printf("rm: invalid option -- '%c'\n", argv[i][j]);
                return 1;
            }
        }
        path_start_index = i + 1;
    }

    if (path_start_index >= argc) {
        if (f_flag) return 0;
        printf("rm: missing operand\n");
        return 1;
    }

    for (i = path_start_index; i < argc; i++) {
        if (rm_path(argv[i], r_flag, f_flag) < 0) {
            exit_code = 1;
        }
    }
    return exit_code;
}

static int rm_path(const char *path, int r_flag, int f_flag) {
    int r;
    struct Stat st;

    // First, check if the path exists.
    if ((r = stat(path, &st)) < 0) {
        if (f_flag && r == -E_NOT_FOUND) {
            return 0; // With -f, "not found" is not an error.
        }
        printf("rm: cannot remove '%s': No such file or directory\n", path);
        return -1;
    }

    // If it's a directory, we must handle it specially.
    if (st.st_isdir) {
        if (!r_flag) {
            printf("rm: cannot remove '%s': Is a directory\n", path);
            return -1;
        }
        
        // It's a directory and we have -r, so recurse.
        int fd;
        if ((fd = open(path, O_RDONLY)) < 0) {
            if (f_flag) return 0;
            printf("rm: cannot open directory '%s'\n", path);
            return -1;
        }

        struct File f;
        while (read(fd, &f, sizeof(f)) == sizeof(f)) {
            if (f.f_name[0] != '\0' && strcmp(f.f_name, ".") != 0 && strcmp(f.f_name, "..") != 0) {
                char child_path[MAXPATHLEN];
                strcpy(child_path, path);
                if (path[strlen(path) - 1] != '/') {
                    strcpy(child_path + strlen(child_path), "/");
                }
                strcpy(child_path + strlen(child_path), f.f_name);
                
                // Recurse on the child path
                if (rm_path(child_path, r_flag, f_flag) < 0) {
                    close(fd);
                    return -1;
                }
            }
        }
        close(fd);
    }
    
    // After recursion (if any), remove the file or the now-empty directory.
    if ((r = remove(path)) < 0) {
        if (f_flag) return 0;
        printf("rm: cannot remove '%s': Operation failed\n", path);
        return -1;
    }

    return 0;
}