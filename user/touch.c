#include <lib.h>

int main(int argc, char **argv) {
    int i;
    int r;
    int exit_code = 0; // Will be set to 1 if any operation fails.

    // Check for the correct number of arguments.
    if (argc < 2) {
        printf("touch: missing file operand\n");
        return 1;
    }

    // Loop through each file operand provided on the command line.
    for (i = 1; i < argc; i++) {
        const char *path = argv[i];

        // The core of 'touch' is opening a file with the O_CREAT flag.
        // - If the file does not exist, it will be created.
        // - If the file already exists, the open succeeds and nothing changes,
        //   which fulfills the requirement. We do not use O_TRUNC.
        r = open(path, O_CREAT | O_RDONLY);

        if (r < 0) {
            // According to the requirements, if creation fails (e.g., because a parent
            // directory doesn't exist), we print a specific error message.
            printf("touch: cannot touch '%s': No such file or directory\n", path);
            exit_code = 1; // Mark that an error occurred.
        } else {
            // If open succeeded, the file now exists. We must close the file descriptor
            // to avoid resource leaks.
            close(r);
        }
    }

    // Return 0 on complete success, or a non-zero value if any touch operation failed.
    return exit_code;
}