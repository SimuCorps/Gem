# Gem Programming Language Interpreter Makefile

# Compiler settings
CC = gcc
CFLAGS = -O3 -flto -DNDEBUG
LDFLAGS = -lm

# Directories
SRC_DIR = src
STL_DIR = stl
BIN_DIR = bin
DOCS_DIR = docs

# Source files
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
STL_FILES = $(wildcard $(STL_DIR)/*.gem)

# Generated files
EMBEDDED_STL_HEADER = $(SRC_DIR)/embedded_stl.h
VERSION_HEADER = $(SRC_DIR)/version.h
VERSION_CONF = version.conf

# Load version from configuration file
include $(VERSION_CONF)

# Calculate derived version strings
ifeq ($(strip $(VERSION_SUFFIX)),)
    VERSION_STRING := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)
    VERSION_DISPLAY := v$(VERSION_STRING)
else
    VERSION_STRING := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)-$(VERSION_SUFFIX)
    VERSION_DISPLAY := v$(VERSION_STRING)
endif

# Default target - compile with standard library
all: $(BIN_DIR)/gemc

# Generate version header from version configuration
$(VERSION_HEADER): $(VERSION_CONF)
	@echo "Generating version header..."
	@echo "// Auto-generated file - do not edit manually" > $(VERSION_HEADER)
	@echo "// Generated from $(VERSION_CONF)" >> $(VERSION_HEADER)
	@echo "" >> $(VERSION_HEADER)
	@echo "#ifndef VERSION_H" >> $(VERSION_HEADER)
	@echo "#define VERSION_H" >> $(VERSION_HEADER)
	@echo "" >> $(VERSION_HEADER)
	@echo "#define VERSION_MAJOR $(VERSION_MAJOR)" >> $(VERSION_HEADER)
	@echo "#define VERSION_MINOR $(VERSION_MINOR)" >> $(VERSION_HEADER)
	@echo "#define VERSION_PATCH $(VERSION_PATCH)" >> $(VERSION_HEADER)
	@echo "#define VERSION_SUFFIX \"$(VERSION_SUFFIX)\"" >> $(VERSION_HEADER)
	@echo "" >> $(VERSION_HEADER)
	@echo "#define VERSION_STRING \"$(VERSION_STRING)\"" >> $(VERSION_HEADER)
	@echo "#define VERSION_DISPLAY \"$(VERSION_DISPLAY)\"" >> $(VERSION_HEADER)
	@echo "" >> $(VERSION_HEADER)
	@echo "// Helper macros for version comparisons" >> $(VERSION_HEADER)
	@echo "#define VERSION_NUMBER ((VERSION_MAJOR * 10000) + (VERSION_MINOR * 100) + VERSION_PATCH)" >> $(VERSION_HEADER)
	@echo "" >> $(VERSION_HEADER)
	@echo "#endif // VERSION_H" >> $(VERSION_HEADER)
	@echo "Generated $(VERSION_HEADER) with version $(VERSION_STRING)"

# Update documentation files with current version
update-docs: $(VERSION_HEADER)
	@echo "Updating documentation with version $(VERSION_STRING)..."
	@if [ -f "$(DOCS_DIR)/index.html" ]; then \
		sed -i.bak 's/<span class="version">v[^<]*<\/span>/<span class="version">$(VERSION_DISPLAY)<\/span>/g' $(DOCS_DIR)/index.html; \
		sed -i.bak 's/<span class="version-badge">v[^<]*<\/span>/<span class="version-badge">$(VERSION_DISPLAY)<\/span>/g' $(DOCS_DIR)/index.html; \
		sed -i.bak 's/Gem Programming Language v[^<]*/Gem Programming Language $(VERSION_DISPLAY)/g' $(DOCS_DIR)/index.html; \
		rm -f $(DOCS_DIR)/index.html.bak; \
		echo "Updated $(DOCS_DIR)/index.html"; \
	fi
	@if [ -f "$(DOCS_DIR)/changeblog.html" ]; then \
		sed -i.bak 's/Gem Programming Language v[^<]*/Gem Programming Language $(VERSION_DISPLAY)/g' $(DOCS_DIR)/changeblog.html; \
		rm -f $(DOCS_DIR)/changeblog.html.bak; \
		echo "Updated $(DOCS_DIR)/changeblog.html"; \
	fi

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
$(BIN_DIR)/gemc: $(SRC_FILES) $(EMBEDDED_STL_HEADER) $(VERSION_HEADER)
	@mkdir -p $(BIN_DIR)
	@echo "Building Gem interpreter with standard library..."
	$(CC) $(SRC_FILES) -o $(BIN_DIR)/gemc $(CFLAGS) $(LDFLAGS) -DWITH_STL -DSTL_PATH='"$(STL_DIR)"'
	@echo "Built: $(BIN_DIR)/gemc (with standard library) - version $(VERSION_STRING)"

# Compile without standard library
$(BIN_DIR)/gemch: $(SRC_FILES) $(VERSION_HEADER)
	@mkdir -p $(BIN_DIR)
	@echo "Building Gem interpreter without standard library..."
	$(CC) $(SRC_FILES) -o $(BIN_DIR)/gemch $(CFLAGS) $(LDFLAGS)
	@echo "Built: $(BIN_DIR)/gemch (without standard library) - version $(VERSION_STRING)"

# Convenience targets
gemc: $(BIN_DIR)/gemc

gemch: $(BIN_DIR)/gemch

# Alternative target for --no-stl flag
no-stl: $(BIN_DIR)/gemch

# Update version and rebuild everything
version-update: clean $(VERSION_HEADER) update-docs all
	@echo "Version update complete: $(VERSION_STRING)"

# Show current version
version:
	@echo "Current version: $(VERSION_STRING)"
	@echo "Display version: $(VERSION_DISPLAY)"

# Clean build artifacts
clean:
	rm -rf $(BIN_DIR)
	rm -f $(EMBEDDED_STL_HEADER)
	rm -f $(VERSION_HEADER)

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
	@echo "Current version: $(VERSION_STRING)"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build gemc with standard library (default)"
	@echo "  gemc          - Build gemc with standard library"
	@echo "  gemch         - Build gemch without standard library"
	@echo "  no-stl        - Alias for gemch (without standard library)"
	@echo "  version       - Show current version information"
	@echo "  version-update- Update version and rebuild everything"
	@echo "  update-docs   - Update documentation files with current version"
	@echo "  clean         - Remove build artifacts"
	@echo "  install       - Install gemc to /usr/local/bin"
	@echo "  uninstall     - Remove gemc from /usr/local/bin"
	@echo "  test          - Run test files (if any)"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Build output:"
	@echo "  Binaries are built in the $(BIN_DIR)/ directory"
	@echo ""
	@echo "Version management:"
	@echo "  Edit $(VERSION_CONF) to change version"
	@echo "  Run 'make version-update' to apply changes everywhere"
	@echo ""
	@echo "Examples:"
	@echo "  make               # Build with standard library ($(BIN_DIR)/gemc)"
	@echo "  make version       # Show current version"
	@echo "  make version-update# Update version and rebuild"
	@echo "  make clean         # Clean build artifacts"

.PHONY: gemc gemch clean install uninstall test help no-stl version version-update update-docs 