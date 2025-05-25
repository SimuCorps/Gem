#include "lineedit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 4096
#define MAX_HISTORY 1000

// Terminal control
static struct termios orig_termios;
static bool raw_mode_enabled = false;

// History management
static char* history[MAX_HISTORY];
static int history_count = 0;
static int history_index = 0;

// Current line state
typedef struct {
    char buffer[MAX_LINE_LENGTH];
    int length;
    int cursor_pos;
    int display_start;
} LineState;

// Terminal escape sequences
#define ESCAPE_SEQ "\x1b["
#define CLEAR_LINE "\x1b[2K"
#define CURSOR_TO_START "\r"
#define CURSOR_LEFT "\x1b[D"
#define CURSOR_RIGHT "\x1b[C"

// Key codes
#define CTRL_A 1
#define CTRL_B 2
#define CTRL_C 3
#define CTRL_D 4
#define CTRL_E 5
#define CTRL_F 6
#define CTRL_H 8
#define CTRL_K 11
#define CTRL_L 12
#define CTRL_N 14
#define CTRL_P 16
#define CTRL_U 21
#define CTRL_V 22
#define CTRL_W 23
#define ESCAPE 27
#define BACKSPACE 127
#define DELETE 126

static void disableRawMode(void) {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = false;
    }
}

static bool enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return false;
    }
    
    atexit(disableRawMode);
    
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return false;
    }
    
    raw_mode_enabled = true;
    return true;
}

static int getTerminalWidth(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return 80; // Default width
    }
    return ws.ws_col;
}

static void refreshLine(const char* prompt, LineState* state) {
    // Move cursor to beginning of line and clear it
    printf("\r\x1b[K");
    
    // Print prompt
    printf("%s", prompt);
    
    // Print the entire line (for simplicity, we'll handle long lines later if needed)
    printf("%s", state->buffer);
    
    // Position cursor correctly by moving back from the end
    int chars_to_move_back = state->length - state->cursor_pos;
    for (int i = 0; i < chars_to_move_back; i++) {
        printf("\b");
    }
    
    fflush(stdout);
}

static void insertChar(LineState* state, char c) {
    if (state->length >= MAX_LINE_LENGTH - 1) {
        return; // Buffer full
    }
    
    // Shift characters to the right
    memmove(state->buffer + state->cursor_pos + 1,
            state->buffer + state->cursor_pos,
            state->length - state->cursor_pos);
    
    state->buffer[state->cursor_pos] = c;
    state->cursor_pos++;
    state->length++;
    state->buffer[state->length] = '\0';
}

static void deleteChar(LineState* state) {
    if (state->cursor_pos == 0) {
        return; // Nothing to delete
    }
    
    // Shift characters to the left
    memmove(state->buffer + state->cursor_pos - 1,
            state->buffer + state->cursor_pos,
            state->length - state->cursor_pos);
    
    state->cursor_pos--;
    state->length--;
    state->buffer[state->length] = '\0';
}

static void deleteCharForward(LineState* state) {
    if (state->cursor_pos >= state->length) {
        return; // Nothing to delete
    }
    
    // Shift characters to the left
    memmove(state->buffer + state->cursor_pos,
            state->buffer + state->cursor_pos + 1,
            state->length - state->cursor_pos - 1);
    
    state->length--;
    state->buffer[state->length] = '\0';
}

static void moveCursorLeft(LineState* state) {
    if (state->cursor_pos > 0) {
        state->cursor_pos--;
    }
}

static void moveCursorRight(LineState* state) {
    if (state->cursor_pos < state->length) {
        state->cursor_pos++;
    }
}

static void moveCursorToStart(LineState* state) {
    state->cursor_pos = 0;
}

static void moveCursorToEnd(LineState* state) {
    state->cursor_pos = state->length;
}

static void killToEnd(LineState* state) {
    state->length = state->cursor_pos;
    state->buffer[state->length] = '\0';
}

static void killToStart(LineState* state) {
    memmove(state->buffer, state->buffer + state->cursor_pos,
            state->length - state->cursor_pos);
    state->length -= state->cursor_pos;
    state->cursor_pos = 0;
    state->buffer[state->length] = '\0';
}

static void clearScreen(void) {
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
}

static void loadHistoryEntry(LineState* state, int index) {
    if (index < 0 || index >= history_count || !history[index]) {
        return;
    }
    
    strcpy(state->buffer, history[index]);
    state->length = strlen(state->buffer);
    state->cursor_pos = state->length;
    state->display_start = 0;
}

static void historyPrev(LineState* state) {
    if (history_index > 0) {
        history_index--;
        loadHistoryEntry(state, history_index);
    }
}

static void historyNext(LineState* state) {
    if (history_index < history_count - 1) {
        history_index++;
        loadHistoryEntry(state, history_index);
    } else if (history_index == history_count - 1) {
        // Go to empty line
        history_index = history_count;
        state->buffer[0] = '\0';
        state->length = 0;
        state->cursor_pos = 0;
        state->display_start = 0;
    }
}

bool initLineEdit(void) {
    // Initialize history
    for (int i = 0; i < MAX_HISTORY; i++) {
        history[i] = NULL;
    }
    history_count = 0;
    history_index = 0;
    
    return true;
}

void cleanupLineEdit(void) {
    disableRawMode();
    
    // Free history
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
        history[i] = NULL;
    }
    history_count = 0;
}

static void ensureCleanTerminal(void) {
    // Make sure we're at the start of a new line with normal terminal settings
    printf("\r");
    fflush(stdout);
}

char* readLine(const char* prompt) {
    if (!enableRawMode()) {
        // Fallback to simple fgets if raw mode fails
        printf("%s", prompt);
        fflush(stdout);
        
        char* line = malloc(MAX_LINE_LENGTH);
        if (!line) return NULL;
        
        if (fgets(line, MAX_LINE_LENGTH, stdin)) {
            // Remove trailing newline
            int len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }
            return line;
        } else {
            free(line);
            return NULL;
        }
    }
    
    LineState state = {0};
    history_index = history_count;
    
    // Print prompt initially
    printf("%s", prompt);
    fflush(stdout);
    
    while (1) {
        int c = getchar();
        
        if (c == '\r' || c == '\n') {
            // Enter pressed - print newline, disable raw mode, and return
            printf("\n");
            disableRawMode();
            ensureCleanTerminal();
            
            char* result = malloc(state.length + 1);
            if (result) {
                strcpy(result, state.buffer);
            }
            return result;
        }
        
        if (c == CTRL_C) {
            // Ctrl+C
            printf("^C\n");
            disableRawMode();
            ensureCleanTerminal();
            return NULL;
        }
        
        if (c == CTRL_D) {
            // Ctrl+D (EOF)
            if (state.length == 0) {
                printf("\n");
                disableRawMode();
                ensureCleanTerminal();
                return NULL;
            }
            // If there's text, delete character forward
            deleteCharForward(&state);
        } else if (c == BACKSPACE || c == CTRL_H) {
            // Backspace
            deleteChar(&state);
        } else if (c == CTRL_A) {
            // Ctrl+A - move to start
            moveCursorToStart(&state);
        } else if (c == CTRL_E) {
            // Ctrl+E - move to end
            moveCursorToEnd(&state);
        } else if (c == CTRL_B) {
            // Ctrl+B - move left
            moveCursorLeft(&state);
        } else if (c == CTRL_F) {
            // Ctrl+F - move right
            moveCursorRight(&state);
        } else if (c == CTRL_K) {
            // Ctrl+K - kill to end
            killToEnd(&state);
        } else if (c == CTRL_U) {
            // Ctrl+U - kill to start
            killToStart(&state);
        } else if (c == CTRL_L) {
            // Ctrl+L - clear screen
            clearScreen();
            printf("%s%s", prompt, state.buffer);
            // Position cursor
            int chars_to_move_back = state.length - state.cursor_pos;
            for (int i = 0; i < chars_to_move_back; i++) {
                printf("\b");
            }
            fflush(stdout);
            continue;
        } else if (c == CTRL_P) {
            // Ctrl+P - previous history
            historyPrev(&state);
        } else if (c == CTRL_N) {
            // Ctrl+N - next history
            historyNext(&state);
        } else if (c == ESCAPE) {
            // Handle escape sequences (arrow keys, etc.)
            int c1 = getchar();
            if (c1 == '[') {
                int c2 = getchar();
                switch (c2) {
                    case 'A': // Up arrow
                        historyPrev(&state);
                        break;
                    case 'B': // Down arrow
                        historyNext(&state);
                        break;
                    case 'C': // Right arrow
                        moveCursorRight(&state);
                        break;
                    case 'D': // Left arrow
                        moveCursorLeft(&state);
                        break;
                    case '3': // Delete key
                        if (getchar() == '~') {
                            deleteCharForward(&state);
                        }
                        break;
                    case 'H': // Home
                        moveCursorToStart(&state);
                        break;
                    case 'F': // End
                        moveCursorToEnd(&state);
                        break;
                }
            }
        } else if (isprint(c)) {
            // Regular printable character
            insertChar(&state, c);
        }
        
        // Refresh the line after any change
        refreshLine(prompt, &state);
    }
}

void addHistory(const char* line) {
    if (!line || strlen(line) == 0) {
        return;
    }
    
    // Don't add duplicate consecutive entries
    if (history_count > 0 && history[history_count - 1] && 
        strcmp(history[history_count - 1], line) == 0) {
        return;
    }
    
    // If history is full, remove oldest entry
    if (history_count >= MAX_HISTORY) {
        free(history[0]);
        memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(char*));
        history_count--;
    }
    
    // Add new entry
    history[history_count] = malloc(strlen(line) + 1);
    if (history[history_count]) {
        strcpy(history[history_count], line);
        history_count++;
    }
}

void saveHistory(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    for (int i = 0; i < history_count; i++) {
        if (history[i]) {
            fprintf(file, "%s\n", history[i]);
        }
    }
    
    fclose(file);
}

void loadHistory(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return;
    
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) && history_count < MAX_HISTORY) {
        // Remove trailing newline
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        if (strlen(line) > 0) {
            addHistory(line);
        }
    }
    
    fclose(file);
} 