# Gem Programming Language

Gem is a modern, statically-typed programming language that prioritizes safety, clarity, and developer experience. Built as a bytecode virtual machine in C, Gem combines the performance of compiled languages with the safety of modern type systems.

## What Makes Gem Special

🔒 **Memory Safety** - Automatic scope-based memory management with compile-time borrow checking  
🛡️ **Type Safety** - Enhanced static typing with nullability and mutability control  
⚡ **Performance** - Bytecode compilation with JIT optimization  
🎯 **Developer Experience** - Clear error messages and interactive REPL  
🧩 **Modern Features** - First-class functions, classes, inheritance, and modules  

## Key Features

### Enhanced Type System
- **Immutable by default** - Variables are immutable unless explicitly marked mutable with `!`
- **Nullability safety** - Nullable types with `?` prevent null reference errors at compile time
- **Static type checking** - All type errors caught before execution
- **Combined modifiers** - Mix mutability and nullability (`string?!`)

### Memory Safety
- **Scope-based cleanup** - Automatic resource management without garbage collection
- **Borrow checking** - Prevents use-after-drop violations at compile time
- **Reference counting** - Efficient memory management
- **No memory leaks** - Compile-time guarantees for memory safety

### Language Features
- **Functions** - First-class functions with typed parameters and return values
- **Classes & Inheritance** - Object-oriented programming with single inheritance
- **Modules** - Modular code organization with `require` statements
- **Control Flow** - Loops, conditionals, and pattern matching
- **Standard Library** - Built-in modules for strings, math, and more
- **Interactive REPL** - Live coding environment with persistent state

### Developer Tools
- **Clear Error Messages** - Helpful compile-time and runtime error reporting
- **Debugging Support** - Built-in debugging and disassembly utilities
- **File & Interactive Modes** - Run scripts or use the interactive shell
- **Cross-platform** - Runs on Linux, macOS, and Windows

## Quick Start

### Building Gem

```bash
# Build with standard library (recommended)
make

# Build minimal version without STL
make no-stl

# Install system-wide
sudo make install
```

### Running Gem

```bash
# Interactive REPL
./gemc

# Run a file
./gemc program.gem
```

### Example Code

```gem
# Type-safe variables with mutability control
string name = "Alice";        # Immutable
int! counter = 0;             # Mutable
string? nickname = nil;       # Nullable

# Functions with type safety
def greet(string name, int age) string
    return "Hello " + name + ", age " + age;
end

# Classes with inheritance
class Person
    def init(string name) void
        this.name = name;
    end
    
    def introduce() string
        return "I'm " + this.name;
    end
end

# Memory-safe scoping
begin
    obj person = Person("Bob");
    puts person.introduce();
    # person automatically cleaned up here
end
```

## Documentation

Visit [gemlang.dev](https://gemlang.dev) for complete documentation, tutorials, and examples.

## Project Structure

- `src/` - Core interpreter implementation
- `stl/` - Standard library modules
- `tests/` - Comprehensive test suite
- `docs/` - Documentation website
- `benchmarks/` - Performance benchmarks

## Contributing

We're thrilled that you're interested in contributing to Gem! Please adopt the following branch and development guidelines to ensure a smooth and collaborative contribution environment.

### Branch Guidelines

**main**

This is the primary branch. It contains the most stable and tested version of the language. All development eventually merges back into main, but only after thorough testing and review. The main branch is what the average user should be using in production.

**dev**

This branch serves as the primary integration branch for new features. It's the "working" version of the language and contains new features that are under development but might not be fully tested. Features should be developed in separate branches and merged into dev when they reach a stable state.

**feature/***

For new features or significant changes, create individual branches off dev. Name these branches clearly based on the feature or change being implemented, e.g., feature/new-syntax, feature/variable-bindings. Once the feature is completed, tested, and approved, it gets merged back into the dev branch.

**release/***

When dev reaches a state where it's ready to be released (after all features planned for the release are merged and tests are passed), create a release branch, e.g., release/v0.1.0. This branch is used for final testing and documentation updates. After it's fully tested and ready, it merges into main and back into dev to ensure any version updates or last-minute fixes are not lost.

**hotfix/***

For urgent fixes that need to be applied directly to the stable version, create hotfix branches from main. For example, hotfix/critical-bug-fix. After the fix is implemented and tested, it merges back into both main and dev to ensure the fix is applied across all relevant versions.

### Development Guidelines

**Develop in Isolation**  
Work on new features and non-urgent fixes in their respective feature/* branches to keep changes isolated until they're ready.

**Regular Merges**  
Regularly merge changes from main into dev and then into feature branches to keep them up to date and minimize merge conflicts.

**Code Reviews**  
Use pull requests for merging from feature/*, release/*, and hotfix/* branches to facilitate code reviews and ensure quality.