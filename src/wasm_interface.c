//> WASM Interface for Gem Programming Language
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "version.h"

// Global buffer to capture output
static char* outputBuffer = NULL;
static size_t outputBufferSize = 0;
static size_t outputBufferUsed = 0;

// Custom output function for capturing prints
static void appendToOutput(const char* text) {
    if (!text) return;
    
    size_t textLen = strlen(text);
    size_t neededSize = outputBufferUsed + textLen + 1;
    
    // Ensure buffer is large enough
    if (neededSize > outputBufferSize) {
        size_t newSize = neededSize * 2;
        char* newBuffer = realloc(outputBuffer, newSize);
        if (!newBuffer) {
            return;
        }
        outputBuffer = newBuffer;
        outputBufferSize = newSize;
    }
    
    // Append text to buffer
    strcpy(outputBuffer + outputBufferUsed, text);
    outputBufferUsed += textLen;
}

// Initialize the Gem VM
EMSCRIPTEN_KEEPALIVE
void gem_init() {
    initVM();
    
    // Initialize output buffer
    if (!outputBuffer) {
        outputBufferSize = 4096;
        outputBuffer = malloc(outputBufferSize);
        if (outputBuffer) {
            outputBuffer[0] = '\0';
            outputBufferUsed = 0;
        }
    }
}

// Clean up the Gem VM
EMSCRIPTEN_KEEPALIVE
void gem_cleanup() {
    freeVM();
    
    if (outputBuffer) {
        free(outputBuffer);
        outputBuffer = NULL;
        outputBufferSize = 0;
        outputBufferUsed = 0;
    }
}

// Clear the output buffer
EMSCRIPTEN_KEEPALIVE
void gem_clear_output() {
    if (outputBuffer) {
        outputBuffer[0] = '\0';
        outputBufferUsed = 0;
    }
}

// Get the current output buffer contents
EMSCRIPTEN_KEEPALIVE
const char* gem_get_output() {
    return outputBuffer ? outputBuffer : "";
}

// Add text to output buffer (called from JavaScript)
EMSCRIPTEN_KEEPALIVE
void gem_add_to_output(const char* text) {
    appendToOutput(text);
}

// Interpret Gem source code
EMSCRIPTEN_KEEPALIVE
int gem_interpret(const char* source) {
    if (!source) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    // Clear previous output
    gem_clear_output();
    
    // Interpret the source
    InterpretResult result = interpret(source);
    
    return (int)result;
}

// Get version information
EMSCRIPTEN_KEEPALIVE
const char* gem_get_version() {
    return VERSION_DISPLAY;
}

// Check if the VM is initialized
EMSCRIPTEN_KEEPALIVE
int gem_is_initialized() {
    return outputBuffer != NULL;
} 