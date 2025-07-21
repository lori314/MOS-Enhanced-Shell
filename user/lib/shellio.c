#include <lib.h>
#include <fs.h>
#include <shell.h> // Our new header
#include <string.h> // FIX: Include for memmove, strcpy, etc.

// History buffer: MAX_HISTORY for stored commands, +1 for the current line
static char history_buf[MAX_HISTORY + 1][MAX_CMD_LEN];
static int history_count = 0; // Number of commands loaded from file
static int history_head = 0; // The index of the oldest command in the circular buffer
static int history_tail = 0; // The index where the next command will be added

// Current position in history we are navigating (0 is newest, count-1 is oldest)
// -1 means we are on the new, currently typed line.
static int history_nav_pos = -1;

// Helper to redraw the command line
static void redraw_line(const char *prompt, char *buf, int len, int cursor_pos) {
    // \x1b[2K: clear entire line
    // \r: carriage return
    printf("\x1b[2K\r");
    printf("%s%s", prompt, buf);
    // \x1b[<N>D: move cursor left N columns
    if (len > cursor_pos) {
        printf("\x1b[%dD", len - cursor_pos);
    }
}

// Load history from /.mos_history
void history_load(void) {
    int fd;
    char buf[MAX_CMD_LEN];
    int n, i = 0, line_start = 0;

    history_count = 0;
    history_head = 0;
    history_tail = 0;

    // Use remove/create to avoid issues with O_TRUNC in this environment
    if ((fd = open("/.mos_history", O_RDONLY)) < 0) {
        return; // File doesn't exist, no history to load
    }

    // This loading logic seems fine and is only run once.
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        for (i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                buf[i] = '\0';
                if (i - line_start > 0) {
                    strcpy(history_buf[history_tail], buf + line_start);
                    history_tail = (history_tail + 1) % MAX_HISTORY;
                    if (history_count < MAX_HISTORY) {
                        history_count++;
                    } else {
                        history_head = (history_head + 1) % MAX_HISTORY; // Overwrite oldest
                    }
                }
                line_start = i + 1;
            }
        }
    }
    close(fd);
}

// Add a command to history and save
void history_add(const char *cmd) {
    if (cmd == NULL || cmd[0] == '\0' || (history_count > 0 && strcmp(cmd, history_buf[(history_tail - 1 + MAX_HISTORY) % MAX_HISTORY]) == 0)) {
        return; // Ignore empty or duplicate commands
    }
    strcpy(history_buf[history_tail], cmd);
    history_tail = (history_tail + 1) % MAX_HISTORY;
    if (history_count < MAX_HISTORY) {
        history_count++;
    } else {
        history_head = (history_head + 1) % MAX_HISTORY; // Oldest is overwritten
    }
    history_save();
}

// Save history to /.mos_history
void history_save(void) {
    int fd;
    // Use remove/create which is the reliable way to truncate in this OS.
    remove("/.mos_history");
    if ((fd = open("/.mos_history", O_WRONLY | O_CREAT)) < 0) {
        return; // Cannot open file
    }
    int current = history_head;
    for (int i = 0; i < history_count; i++) {
        fprintf(fd, "%s\n", history_buf[current]);
        current = (current + 1) % MAX_HISTORY;
    }
    close(fd);
}

// The advanced readline function
int readline_advanced(char *buf, int size) {
    static char local_buf[MAX_CMD_LEN];
    int len = 0;
    int cursor_pos = 0;
    int r;
    char c;

    history_nav_pos = -1;
    local_buf[0] = '\0';

    const char *prompt = "$ ";
    printf("%s", prompt);

    while (1) {
        if ((r = read(0, &c, 1)) != 1) {
            if (r < 0) return -1; // Error
            return 0;  // EOF
        }

        switch (c) {
        case '\b': // backspace
        case 0x7f: // delete
            if (cursor_pos > 0) {
                memmove(&local_buf[cursor_pos - 1], &local_buf[cursor_pos], len - cursor_pos + 1);
                cursor_pos--;
                len--;
                redraw_line(prompt, local_buf, len, cursor_pos);
            }
            break;
        
        case '\r':
        case '\n':
            printf("\n");
            strncpy(buf, local_buf, size - 1);
            buf[size - 1] = '\0';
            return len;

        case 0x1b: // Escape sequence
            read(0, &c, 1);
            if (c == '[') {
                read(0, &c, 1);
                if (c == 'A') { // Up arrow
                    if (history_nav_pos < history_count - 1) {
                        if (history_nav_pos == -1) {
                            // --- MODIFICATION START ---
                            // Save current line to the DEDICATED temporary slot.
                            strcpy(history_buf[MAX_HISTORY], local_buf);
                            // --- MODIFICATION END ---
                        }
                        history_nav_pos++;
                        int idx = (history_tail - 1 - history_nav_pos + MAX_HISTORY) % MAX_HISTORY;
                        strcpy(local_buf, history_buf[idx]);
                        len = cursor_pos = strlen(local_buf);
                        redraw_line(prompt, local_buf, len, cursor_pos);
                    }
                } else if (c == 'B') { // Down arrow
                    if (history_nav_pos > -1) {
                        history_nav_pos--;
                        // --- MODIFICATION START ---
                        // If navigating back to the new line, retrieve it from the DEDICATED slot.
                        char *src = (history_nav_pos == -1) ? history_buf[MAX_HISTORY] : history_buf[(history_tail - 1 - history_nav_pos + MAX_HISTORY) % MAX_HISTORY];
                        // --- MODIFICATION END ---
                        strcpy(local_buf, src);
                        len = cursor_pos = strlen(local_buf);
                        redraw_line(prompt, local_buf, len, cursor_pos);
                    }
                } else if (c == 'C') { // Right arrow
                    if (cursor_pos < len) {
                        cursor_pos++;
                        printf("\x1b[C");
                    }
                } else if (c == 'D') { // Left arrow
                    if (cursor_pos > 0) {
                        cursor_pos--;
                        printf("\x1b[D");
                    }
                }
            }
            break;

        case 0x01: // Ctrl-A: move to beginning of line
            cursor_pos = 0;
            redraw_line(prompt, local_buf, len, cursor_pos);
            break;

        case 0x05: // Ctrl-E: move to end of line
            cursor_pos = len;
            redraw_line(prompt, local_buf, len, cursor_pos);
            break;
        
        case 0x0b: // Ctrl-K: delete from cursor to end of line
            local_buf[cursor_pos] = '\0';
            len = cursor_pos;
            redraw_line(prompt, local_buf, len, cursor_pos);
            break;
        
        case 0x15: // Ctrl-U: delete from beginning to cursor
            if (cursor_pos > 0) {
                memmove(local_buf, &local_buf[cursor_pos], len - cursor_pos + 1);
                len -= cursor_pos;
                cursor_pos = 0;
                redraw_line(prompt, local_buf, len, cursor_pos);
            }
            break;

        case 0x17: // Ctrl-W: delete word to the left
            if (cursor_pos > 0) {
                int end_pos = cursor_pos;
                int start_pos = cursor_pos;
                // Skip trailing whitespace
                while(start_pos > 0 && strchr(" \t", local_buf[start_pos-1])) start_pos--;
                // Find start of word
                while(start_pos > 0 && !strchr(" \t", local_buf[start_pos-1])) start_pos--;
                
                if (end_pos > start_pos) {
                    memmove(&local_buf[start_pos], &local_buf[end_pos], len - end_pos + 1);
                    len -= (end_pos - start_pos);
                    cursor_pos = start_pos;
                    redraw_line(prompt, local_buf, len, cursor_pos);
                }
            }
            break;
        
        default: // Regular character
            if (len < sizeof(local_buf) - 1 && c >= 32 && c < 127) {
                memmove(&local_buf[cursor_pos + 1], &local_buf[cursor_pos], len - cursor_pos + 1);
                local_buf[cursor_pos] = c;
                len++;
                cursor_pos++;
                redraw_line(prompt, local_buf, len, cursor_pos);
            }
            break;
        }
    }
}