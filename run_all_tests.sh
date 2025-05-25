#!/bin/bash

# Gem Language Test Runner
# Runs all test files and provides comprehensive reporting

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test configuration
TEST_DIR="tests"
COMPILER="./bin/gemc"
LOG_FILE="test_results.log"
PASSED_TESTS=0
FAILED_TESTS=0
TOTAL_TESTS=0

# Create log file
echo "Gem Language Test Suite - $(date)" > "$LOG_FILE"
echo "========================================" >> "$LOG_FILE"

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
    echo "$message" >> "$LOG_FILE"
}

# Function to run a single test
run_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .gem)
    
    print_status "$BLUE" "Running test: $test_name"
    
    # Check if test file exists
    if [[ ! -f "$test_file" ]]; then
        print_status "$RED" "  ❌ Test file not found: $test_file"
        ((FAILED_TESTS++))
        return 1
    fi
    
    # Run the test (removed timeout for macOS compatibility)
    if "$COMPILER" "$test_file" >> "$LOG_FILE" 2>&1; then
        print_status "$GREEN" "  ✅ PASSED: $test_name"
        ((PASSED_TESTS++))
        return 0
    else
        local exit_code=$?
        print_status "$RED" "  ❌ FAILED: $test_name (exit code: $exit_code)"
        echo "    Error details logged to $LOG_FILE"
        ((FAILED_TESTS++))
        return 1
    fi
}

# Function to run tests by category
run_test_category() {
    local category=$1
    shift
    local tests=("$@")
    
    print_status "$PURPLE" "\n=== $category ==="
    
    for test in "${tests[@]}"; do
        run_test "$TEST_DIR/$test"
        ((TOTAL_TESTS++))
    done
}

# Main execution
main() {
    print_status "$CYAN" "🚀 Starting Gem Language Test Suite"
    print_status "$CYAN" "Compiler: $COMPILER"
    print_status "$CYAN" "Test Directory: $TEST_DIR"
    print_status "$CYAN" "Log File: $LOG_FILE"
    
    # Check if compiler exists
    if [[ ! -f "$COMPILER" ]]; then
        print_status "$RED" "❌ Compiler not found: $COMPILER"
        print_status "$YELLOW" "Please ensure the Gem compiler is built and available."
        exit 1
    fi
    
    # Check if test directory exists
    if [[ ! -d "$TEST_DIR" ]]; then
        print_status "$RED" "❌ Test directory not found: $TEST_DIR"
        exit 1
    fi
    
    # Make compiler executable
    chmod +x "$COMPILER"
    
    echo "" >> "$LOG_FILE"
    
    # Core Language Features
    run_test_category "Core Language Features" \
        "test_basic_types.gem" \
        "test_variables.gem" \
        "test_nullable_types.gem" \
        "test_optional_semicolons.gem"
    
    # Operators and Expressions
    run_test_category "Operators and Expressions" \
        "test_arithmetic.gem" \
        "test_modulo_operator.gem" \
        "test_comparisons.gem" \
        "test_logical_operators.gem" \
        "test_ternary_operator.gem"
    
    # String Operations
    run_test_category "String Operations" \
        "test_strings.gem" \
        "test_string_interpolation.gem"
    
    # Control Flow
    run_test_category "Control Flow" \
        "test_conditionals.gem" \
        "test_loops.gem" \
        "test_scopes.gem"
    
    # Functions and Closures
    run_test_category "Functions and Closures" \
        "test_functions.gem" \
        "test_closures.gem"
    
    # Object-Oriented Programming
    run_test_category "Object-Oriented Programming" \
        "test_classes.gem" \
        "test_inheritance.gem"
    
    # Module System
    run_test_category "Module System" \
        "test_modules.gem"
    
    # Mutability
    run_test_category "Mutability" \
        "test_mut_comprehensive.gem" \
        "test_mut_errors.gem" \
        "test_mut_keyword.gem"
    
    # Advanced Features
    run_test_category "Advanced Features" \
        "test_memory_safety.gem" \
        "test_type_safety.gem" \
        "test_jit_compilation.gem"
    
    # Final Summary
    print_status "$CYAN" "\n🏁 Test Suite Complete!"
    print_status "$CYAN" "================================"
    print_status "$GREEN" "✅ Passed: $PASSED_TESTS"
    print_status "$RED" "❌ Failed: $FAILED_TESTS"
    print_status "$BLUE" "📊 Total:  $TOTAL_TESTS"
    
    if [[ $FAILED_TESTS -eq 0 ]]; then
        print_status "$GREEN" "🎉 All tests passed!"
        echo ""
        print_status "$CYAN" "Full test log available in: $LOG_FILE"
        exit 0
    else
        print_status "$YELLOW" "⚠️  Some tests failed. Check $LOG_FILE for details."
        exit 1
    fi
}

# Trap to handle interruption
trap 'print_status "$YELLOW" "\n⚠️ Test suite interrupted by user"; exit 130' INT

# Run main function
main "$@" 