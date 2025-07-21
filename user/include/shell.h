#ifndef JOS_USER_INCLUDE_SHELL_H
#define JOS_USER_INCLUDE_SHELL_H

#define MAX_CMD_LEN 1024
#define MAX_HISTORY 20

// Function to read a line from console with advanced editing features
int readline_advanced(char *buf, int size);

// Functions for history management
void history_load(void);
void history_add(const char *cmd);
void history_save(void);

#endif // JOS_USER_INCLUDE_SHELL_H