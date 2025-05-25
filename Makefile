# Gem Programming Language Interpreter Makefile

# Compiler settings
CC = gcc
CFLAGS = -O3 -flto -DNDEBUG
LDFLAGS = -lm

# Directories
SRC_DIR = src
STL_DIR = stl
BIN_DIR = bin

# Source files
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
STL_FILES = $(wildcard $(STL_DIR)/*.gem)

# Generated files
EMBEDDED_STL_HEADER = $(SRC_DIR)/embedded_stl.h

# Default target - compile with standard library
all: $(BIN_DIR)/gemc

# Generate embedded STL header from STL files
$(EMBEDDED_STL_HEADER): $(STL_FILES)
	@echo "Generating embedded STL header..."
	@echo "// Auto-generated file - do not edit manually" > $(EMBEDDED_STL_HEADER)
	@echo "// Generated from STL files in $(STL_DIR)/ directory" >> $(EMBEDDED_STL_HEADER)
	@echo "" >> $(EMBEDDED_STL_HEADER)
	@echo "#ifndef EMBEDDED_STL_H" >> $(EMBEDDED_STL_HEADER)
	@echo "#define EMBEDDED_STL_H" >> $(EMBEDDED_STL_HEADER)
	@echo "" >> $(EMBEDDED_STL_HEADER)
	@echo "#ifdef WITH_STL" >> $(EMBEDDED_STL_HEADER)
	@echo "" >> $(EMBEDDED_STL_HEADER)
	@for stl_file in $(STL_FILES); do \
		module_name=$$(basename "$$stl_file" .gem); \
		module_name_upper=$$(echo "$$module_name" | tr '[:lower:]' '[:upper:]'); \
		echo "Processing $$stl_file -> $$module_name"; \
		echo "// Embedded STL module: $$module_name" >> $(EMBEDDED_STL_HEADER); \
		printf "static const char STL_%s_MODULE[] = \n" "$$module_name_upper" >> $(EMBEDDED_STL_HEADER); \
		first_line=true; \
		while IFS= read -r line; do \
			escaped_line=$$(printf '%s' "$$line" | sed 's/\\/\\\\/g' | sed 's/"/\\"/g'); \
			if [ "$$first_line" = true ]; then \
				printf '"%s\\n"\n' "$$escaped_line" >> $(EMBEDDED_STL_HEADER); \
				first_line=false; \
			else \
				printf '"%s\\n"\n' "$$escaped_line" >> $(EMBEDDED_STL_HEADER); \
			fi; \
		done < "$$stl_file"; \
		echo ";" >> $(EMBEDDED_STL_HEADER); \
		echo "" >> $(EMBEDDED_STL_HEADER); \
	done
	@echo "// STL module lookup table" >> $(EMBEDDED_STL_HEADER)
	@echo "typedef struct {" >> $(EMBEDDED_STL_HEADER)
	@echo "    const char* name;" >> $(EMBEDDED_STL_HEADER)
	@echo "    const char* source;" >> $(EMBEDDED_STL_HEADER)
	@echo "} EmbeddedSTLModule;" >> $(EMBEDDED_STL_HEADER)
	@echo "" >> $(EMBEDDED_STL_HEADER)
	@echo "static const EmbeddedSTLModule EMBEDDED_STL_MODULES[] = {" >> $(EMBEDDED_STL_HEADER)
	@for stl_file in $(STL_FILES); do \
		module_name=$$(basename "$$stl_file" .gem); \
		module_name_upper=$$(echo "$$module_name" | tr '[:lower:]' '[:upper:]'); \
		echo "    {\"$$module_name\", STL_$${module_name_upper}_MODULE}," >> $(EMBEDDED_STL_HEADER); \
	done
	@echo "    {NULL, NULL} // Sentinel" >> $(EMBEDDED_STL_HEADER)
	@echo "};" >> $(EMBEDDED_STL_HEADER)
	@echo "" >> $(EMBEDDED_STL_HEADER)
	@echo "// Function to get embedded STL module source" >> $(EMBEDDED_STL_HEADER)
	@echo "static const char* getEmbeddedSTLModule(const char* moduleName) {" >> $(EMBEDDED_STL_HEADER)
	@echo "    for (int i = 0; EMBEDDED_STL_MODULES[i].name != NULL; i++) {" >> $(EMBEDDED_STL_HEADER)
	@echo "        if (strcmp(EMBEDDED_STL_MODULES[i].name, moduleName) == 0) {" >> $(EMBEDDED_STL_HEADER)
	@echo "            return EMBEDDED_STL_MODULES[i].source;" >> $(EMBEDDED_STL_HEADER)
	@echo "        }" >> $(EMBEDDED_STL_HEADER)
	@echo "    }" >> $(EMBEDDED_STL_HEADER)
	@echo "    return NULL;" >> $(EMBEDDED_STL_HEADER)
	@echo "}" >> $(EMBEDDED_STL_HEADER)
	@echo "" >> $(EMBEDDED_STL_HEADER)
	@echo "#endif // WITH_STL" >> $(EMBEDDED_STL_HEADER)
	@echo "" >> $(EMBEDDED_STL_HEADER)
	@echo "#endif // EMBEDDED_STL_H" >> $(EMBEDDED_STL_HEADER)
	@echo "Generated $(EMBEDDED_STL_HEADER) with embedded STL modules"

# Compile with standard library (default)
$(BIN_DIR)/gemc: $(SRC_FILES) $(EMBEDDED_STL_HEADER)
	@mkdir -p $(BIN_DIR)
	@echo "Building Gem interpreter with standard library..."
	$(CC) $(SRC_FILES) -o $(BIN_DIR)/gemc $(CFLAGS) $(LDFLAGS) -DWITH_STL -DSTL_PATH='"$(STL_DIR)"'
	@echo "Built: $(BIN_DIR)/gemc (with standard library)"

# Compile without standard library
$(BIN_DIR)/gemch: $(SRC_FILES)
	@mkdir -p $(BIN_DIR)
	@echo "Building Gem interpreter without standard library..."
	$(CC) $(SRC_FILES) -o $(BIN_DIR)/gemch $(CFLAGS) $(LDFLAGS)
	@echo "Built: $(BIN_DIR)/gemch (without standard library)"

# Convenience targets
gemc: $(BIN_DIR)/gemc

gemch: $(BIN_DIR)/gemch

# Alternative target for --no-stl flag
no-stl: $(BIN_DIR)/gemch

# Clean build artifacts
clean:
	rm -rf $(BIN_DIR)
	rm -f $(EMBEDDED_STL_HEADER)

# Install to system (optional)
install: $(BIN_DIR)/gemc
	cp $(BIN_DIR)/gemc /usr/local/bin/

# Uninstall from system (optional)
uninstall:
	rm -f /usr/local/bin/gemc

# Run tests (if any)
test: $(BIN_DIR)/gemc
	@echo "Running tests..."
	@if [ -d "tests" ]; then \
		for test_file in tests/*.gem; do \
			if [ -f "$$test_file" ]; then \
				echo "Running $$test_file..."; \
				./$(BIN_DIR)/gemc "$$test_file"; \
			fi; \
		done; \
	else \
		echo "No test directory found"; \
	fi

# Help target
help:
	@echo "Gem Programming Language Interpreter Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build gemc with standard library (default)"
	@echo "  gemc       - Build gemc with standard library"
	@echo "  gemch      - Build gemch without standard library"
	@echo "  no-stl     - Alias for gemch (without standard library)"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install gemc to /usr/local/bin"
	@echo "  uninstall  - Remove gemc from /usr/local/bin"
	@echo "  test       - Run test files (if any)"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Build output:"
	@echo "  Binaries are built in the $(BIN_DIR)/ directory"
	@echo ""
	@echo "Examples:"
	@echo "  make               # Build with standard library ($(BIN_DIR)/gemc)"
	@echo "  make gemc          # Build with standard library"
	@echo "  make gemch         # Build without standard library"
	@echo "  make no-stl        # Build without standard library"
	@echo "  make clean         # Clean build artifacts"

.PHONY: gemc gemch clean install uninstall test help no-stl 