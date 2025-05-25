#!/bin/bash

# Simple Gem Test Runner
# Usage: ./run_test.sh [test_file.gem] or ./run_test.sh [category]

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

TEST_DIR="tests"
COMPILER="./bin/gemc"

print_colored() {
    echo -e "${1}${2}${NC}"
}

show_usage() {
    print_colored "$CYAN" "Gem Test Runner Usage:"
    echo ""
    print_colored "$YELLOW" "Run a specific test:"
    echo "  ./run_test.sh test_basic_types.gem"
    echo "  ./run_test.sh tests/test_functions.gem"
    echo ""
    print_colored "$YELLOW" "Run tests by category:"
    echo "  ./run_test.sh core        # Core language features"
    echo "  ./run_test.sh operators   # Operators and expressions"
    echo "  ./run_test.sh strings     # String operations"
    echo "  ./run_test.sh control     # Control flow"
    echo "  ./run_test.sh functions   # Functions and closures"
    echo "  ./run_test.sh oop         # Object-oriented programming"
    echo "  ./run_test.sh modules     # Module system"
    echo "  ./run_test.sh advanced    # Advanced features"
    echo ""
    print_colored "$YELLOW" "Run all tests:"
    echo "  ./run_all_tests.sh"
}

run_single_test() {
    local test_file=$1
    
    # Add .gem extension if not present
    if [[ ! "$test_file" =~ \.gem$ ]]; then
        test_file="${test_file}.gem"
    fi
    
    # Add tests/ prefix if not present
    if [[ ! "$test_file" =~ ^tests/ ]] && [[ ! -f "$test_file" ]]; then
        test_file="tests/$test_file"
    fi
    
    if [[ ! -f "$test_file" ]]; then
        print_colored "$RED" "❌ Test file not found: $test_file"
        return 1
    fi
    
    print_colored "$BLUE" "🧪 Running: $(basename "$test_file")"
    
    if "$COMPILER" "$test_file"; then
        print_colored "$GREEN" "✅ Test passed!"
        return 0
    else
        print_colored "$RED" "❌ Test failed!"
        return 1
    fi
}

run_category() {
    local category=$1
    local tests=()
    
    case "$category" in
        "core")
            tests=("test_basic_types.gem" "test_variables.gem" "test_nullable_types.gem" "test_optional_semicolons.gem")
            ;;
        "operators")
            tests=("test_arithmetic.gem" "test_modulo_operator.gem" "test_comparisons.gem" "test_logical_operators.gem" "test_ternary_operator.gem")
            ;;
        "strings")
            tests=("test_strings.gem" "test_string_interpolation.gem")
            ;;
        "control")
            tests=("test_conditionals.gem" "test_loops.gem" "test_scopes.gem")
            ;;
        "functions")
            tests=("test_functions.gem" "test_closures.gem")
            ;;
        "oop")
            tests=("test_classes.gem" "test_inheritance.gem")
            ;;
        "modules")
            tests=("test_modules.gem")
            ;;
        "mutability")
            tests=("test_mut_comprehensive.gem" "test_mut_errors.gem" "test_mut_keyword.gem")
            ;;
        "advanced")
            tests=("test_memory_safety.gem" "test_type_safety.gem" "test_jit_compilation.gem" "test_hashes.gem")
            ;;
        *)
            print_colored "$RED" "❌ Unknown category: $category"
            show_usage
            return 1
            ;;
    esac
    
    print_colored "$CYAN" "🚀 Running $category tests..."
    local passed=0
    local failed=0
    
    for test in "${tests[@]}"; do
        if run_single_test "$TEST_DIR/$test"; then
            ((passed++))
        else
            ((failed++))
        fi
        echo ""
    done
    
    print_colored "$CYAN" "📊 Category Results:"
    print_colored "$GREEN" "  ✅ Passed: $passed"
    print_colored "$RED" "  ❌ Failed: $failed"
}

main() {
    if [[ ! -f "$COMPILER" ]]; then
        print_colored "$RED" "❌ Compiler not found: $COMPILER"
        print_colored "$YELLOW" "Please ensure the Gem compiler is built."
        exit 1
    fi
    
    chmod +x "$COMPILER"
    
    if [[ $# -eq 0 ]]; then
        show_usage
        exit 0
    fi
    
    local arg=$1
    
    # Check if it's a category
    case "$arg" in
        "core"|"operators"|"strings"|"control"|"functions"|"oop"|"modules"|"advanced")
            run_category "$arg"
            ;;
        *)
            run_single_test "$arg"
            ;;
    esac
}

main "$@" 