//> Chunks of Bytecode main-c
//> Scanning on Demand main-includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//< Scanning on Demand main-includes
#include "common.h"
//> main-include-chunk
#include "chunk.h"
//< main-include-chunk
//> main-include-debug
#include "debug.h"
//< main-include-debug
//> A Virtual Machine main-include-vm
#include "vm.h"
//< A Virtual Machine main-include-vm
//> JIT Integration main-include-jit
#include "jit.h"
//< JIT Integration main-include-jit

//> JIT Integration command line parsing
static void printUsage() {
  fprintf(stderr, "Usage: gem [options] [path]\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --jit-off           Disable JIT compilation\n");
  fprintf(stderr, "  --jit-stats         Print JIT statistics at exit\n");
  fprintf(stderr, "  --jit-threshold N   Set function compilation threshold (default: 100)\n");
  fprintf(stderr, "  --jit-loop-threshold N Set loop compilation threshold (default: 50)\n");
  fprintf(stderr, "  --help              Show this help message\n");
}

// Global flags for JIT control
static bool showJitStats = false;
//< JIT Integration command line parsing

//> Scanning on Demand repl

static void repl() {
  char line[1024];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}
//< Scanning on Demand repl
//> Scanning on Demand read-file
static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
//> no-file
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }
//< no-file

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
//> no-buffer
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  
//< no-buffer
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
//> no-read
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  
//< no-read
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}
//< Scanning on Demand read-file
//> Scanning on Demand run-file
static void runFile(const char* path) {
  char* source = readFile(path);
  InterpretResult result = interpret(source);
  free(source); // [owner]

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}
//< Scanning on Demand run-file

int main(int argc, const char* argv[]) {
//> JIT Integration argument parsing
  // Parse command line arguments
  const char* scriptPath = NULL;
  
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--jit-off") == 0) {
      // Disable JIT - we'll do this after initVM()
    } else if (strcmp(argv[i], "--jit-stats") == 0) {
      showJitStats = true;
    } else if (strcmp(argv[i], "--jit-threshold") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --jit-threshold requires a number\n");
        exit(64);
      }
      // In a full implementation, we'd set the threshold here
      i++; // Skip the number argument
    } else if (strcmp(argv[i], "--jit-loop-threshold") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --jit-loop-threshold requires a number\n");
        exit(64);
      }
      // In a full implementation, we'd set the loop threshold here
      i++; // Skip the number argument
    } else if (strcmp(argv[i], "--help") == 0) {
      printUsage();
      exit(0);
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
      printUsage();
      exit(64);
    } else {
      if (scriptPath != NULL) {
        fprintf(stderr, "Error: Multiple script files specified\n");
        printUsage();
        exit(64);
      }
      scriptPath = argv[i];
    }
  }
//< JIT Integration argument parsing

//> A Virtual Machine main-init-vm
  initVM();

//< A Virtual Machine main-init-vm

//> JIT Integration post-init setup
  // Disable JIT if requested
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--jit-off") == 0) {
      jitContext.enabled = false;
      printf("JIT compilation disabled\n");
      break;
    }
  }
//< JIT Integration post-init setup

//> Scanning on Demand args
  if (scriptPath == NULL) {
    repl();
  } else {
    runFile(scriptPath);
  }
  
//> JIT Integration print stats
  if (showJitStats) {
    printJitStats();
  }
//< JIT Integration print stats
  
  freeVM();
//< Scanning on Demand args
  return 0;
}
