#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <stdbool.h>

// Initialize line editing
bool initLineEdit(void);

// Cleanup line editing
void cleanupLineEdit(void);

// Read a line with editing capabilities
// Returns NULL on EOF, otherwise returns a malloc'd string that must be freed
char* readLine(const char* prompt);

// Add a line to history
void addHistory(const char* line);

// Save history to file
void saveHistory(const char* filename);

// Load history from file
void loadHistory(const char* filename);

#endif // LINEEDIT_H 