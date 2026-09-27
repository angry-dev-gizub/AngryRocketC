# Angry Rocket C Requirements

This document defines the requirements for milestone releases of **Angry Rocket C (Arc)**.

Requirements are grouped by version and describe the externally observable language, compiler, tooling, testing, and quality requirements for that release.

Internal implementation details are not requirements unless they directly affect required behavior.

---

# Version 0.0.1

## Purpose

Angry Rocket C 0.0.1 is intended to provide the first minimal version of the Arc compiler that is capable of compiling actual Arc programs.

The supported language is intentionally small. Version 0.0.1 is not intended to provide the full Arc language or ecosystem.

A valid Arc 0.0.1 program may consist of only a single source file.

---

## Language Requirements

### ARC-0.0.1-R001: Source Files

The compiler shall accept a single text file containing Arc source code.

The `.arc` file extension is the recommended convention but is not required in version 0.0.1.

The compiler shall interpret the input file as Arc source code regardless of its file extension.

---

### ARC-0.0.1-R002: Single-File Compilation

The compiler shall compile exactly one Arc source file at a time.

Multi-file compilation is not required.

Dependency resolution between multiple Arc files is not required.

---

### ARC-0.0.1-R003: Module Declaration

The lexer and parser shall recognize the `module` keyword.

An Arc source file shall begin with a module declaration.

The module declaration does not need to provide meaningful module isolation, symbol resolution, file discovery, or dependency behavior in version 0.0.1.

Its presence is primarily syntactic in this release.

---

### ARC-0.0.1-R004: Import Declaration

The lexer and parser shall recognize the `import` keyword.

Import syntax may be parsed, but actual module loading, dependency resolution, or cross-file compilation is not required in version 0.0.1.

---

### ARC-0.0.1-R005: Entry Point

A valid Arc program shall define a function named `main`.

The `main` function shall act as the program entry point.

A source file without a valid `main` function shall be rejected by the compiler.

---

### ARC-0.0.1-R006: Variables

Arc shall support mutable variable declarations.

A constant declaration system is not required in version 0.0.1.

Variables shall be able to store values of supported primitive types.

---

### ARC-0.0.1-R007: Primitive Types

Version 0.0.1 shall support the following primitive types:

- `i32`
- `bool`
- `float`
- `void`

No additional primitive or composite types are required.

---

### ARC-0.0.1-R008: Functions

Arc shall support function declarations and definitions.

Functions shall support:

- a function name;
- zero or more parameters;
- parameter types;
- a return type;
- a function body; and
- return statements where required.

Functions shall be callable from other functions within the same source file.

---

### ARC-0.0.1-R009: Function Parameters

Functions shall support zero or more typed parameters.

Arguments passed to a function shall correspond to its declared parameters.

---

### ARC-0.0.1-R010: Function Return Values

Functions may return supported Arc values.

Functions declared with the `void` return type are not required to return a value.

A function with a non-`void` return type shall contain a valid return statement on every reachable branch that reaches the end of the function.

Programs violating this requirement shall be rejected by semantic analysis.

---

### ARC-0.0.1-R011: Return Statements

The language shall support the `return` statement.

A return statement in a non-`void` function shall return a value compatible with the function's declared return type.

---

### ARC-0.0.1-R012: Conditional Statements

Arc shall support `if` statements.

Arc shall support optional `else` branches.

Conditional expressions shall use values accepted as boolean conditions by the language.

---

### ARC-0.0.1-R013: While Loops

Arc shall support `while` loops.

A `while` loop shall evaluate its condition before each iteration.

---

### ARC-0.0.1-R014: Arithmetic Operators

Arc shall support the following arithmetic operators:

- `+`
- `-`
- `*`
- `/`

The compiler shall perform the required semantic validation for operand types.

---

### ARC-0.0.1-R015: Comparison Operators

Arc shall support the following comparison operators:

- `==`
- `!=`
- `<`
- `<=`
- `>`
- `>=`

Comparison expressions shall produce a value of type `bool`.

The compiler shall perform the required semantic validation for operand types.

---

### ARC-0.0.1-R016: Comments

Arc shall support single-line comments.

Multi-line comments are not required in version 0.0.1.

---

## Unsupported Language Features

The following features are explicitly not required for version 0.0.1:

- constants;
- structs;
- pointers;
- references;
- arrays;
- strings;
- explicit casts;
- multi-file compilation;
- functional module resolution;
- functional import resolution; and
- a standard library.

Support for these features may be introduced in later versions.

---

# Compiler Requirements

## ARC-0.0.1-R017: Compilation Pipeline

The compiler shall provide the minimum complete compilation pipeline required to transform valid Arc source code into output generated through the LLVM backend.

The compiler architecture shall include the functionality necessary for:

- lexical analysis;
- parsing;
- semantic validation;
- LLVM code generation; and
- output generation.

---

## ARC-0.0.1-R018: LLVM Backend

The Arc compiler shall use LLVM as its backend.

The compiler shall be capable of generating LLVM IR internally as part of compilation.

---

## ARC-0.0.1-R019: Object File Output

By default, Arc shall produce an object file.

The output filename shall be derived from the input filename.

For example:

```text
program.arc
```

shall produce an object file corresponding to the platform convention, such as:

```text
program.o
```

or:

```text
program.obj
```

depending on the target platform.

The Arc compiler is not required to perform final executable linking in version 0.0.1.

Linking the generated object file into an executable is the responsibility of an external linker or toolchain.

---

## ARC-0.0.1-R020: LLVM IR Output

The compiler may provide a way to produce LLVM textual IR using the `.ll` format.

LLVM IR output is considered a supported compiler output form for development and inspection.

Object-file generation remains the primary compilation result.

---

## ARC-0.0.1-R021: Diagnostics

The compiler shall provide a structured diagnostic system.

Diagnostics shall be used for lexical, syntactic, semantic, and compilation errors.

Where applicable, diagnostics should contain sufficient source information to identify the location of the problem.

Invalid source code shall produce a diagnostic rather than causing undefined compiler behavior.

---

## ARC-0.0.1-R022: Invalid Input

The compiler shall not crash when processing invalid Arc source code under normal error conditions.

Invalid input shall be handled predictably.

Compilation shall fail cleanly and report appropriate diagnostics.

---

# Command-Line Interface Requirements

## ARC-0.0.1-R023: Compile Command

The compiler shall support the following invocation form:

```text
arc <source-file>
```

Example:

```text
arc hello.arc
```

This command shall compile the provided source file.

Only one source file is required to be accepted per invocation.

---

## ARC-0.0.1-R024: Version Command

The compiler shall support:

```text
arc --version
```

The command shall print the current Angry Rocket C compiler version.

The reported version shall correspond to the project version defined by the repository.

---

## ARC-0.0.1-R025: CLI Scope

No additional command-line commands or options are required for version 0.0.1 unless necessary for required compiler behavior.

A help command, project system, package manager, linker driver, build command, and dependency manager are not required.

---

# Platform Requirements

## ARC-0.0.1-R026: Portable Architecture

The compiler implementation shall be designed to remain portable across supported operating systems.

Core compiler code should avoid unnecessary platform-specific dependencies.

Platform-specific behavior shall be isolated where it becomes necessary.

---

## ARC-0.0.1-R027: Host Platforms

The architecture shall not intentionally depend on Windows-only or Linux-only behavior.

Support for additional platforms may depend on the availability of the required compiler toolchain and LLVM backend.

Version 0.0.1 does not guarantee identical behavior on every operating system.

---

# Testing Requirements

## ARC-0.0.1-R028: Subsystem Tests

Every significant compiler subsystem introduced in version 0.0.1 shall have corresponding automated tests.

Examples include:

- lexer;
- parser;
- diagnostic system;
- semantic analysis;
- code generation; and
- supporting compiler infrastructure.

---

## ARC-0.0.1-R029: Lexer Tests

The lexer shall have automated tests covering supported tokens, keywords, operators, literals, comments, and invalid lexical input.

---

## ARC-0.0.1-R030: Parser Tests

The parser shall have automated tests covering all syntax supported by version 0.0.1.

Invalid syntax shall also be tested.

---

## ARC-0.0.1-R031: Integration Tests

The project shall contain integration tests that exercise the compiler as a complete system.

Integration tests shall include valid Arc programs that successfully compile.

Integration tests shall also include invalid Arc programs that are rejected with diagnostics.

---

# Quality Requirements

## ARC-0.0.1-R032: Memory Safety of the Compiler

The Arc compiler shall not leak memory during normal compilation workflows.

Known memory leaks shall not be accepted as part of the completed version 0.0.1 milestone.

---

## ARC-0.0.1-R033: Compiler Stability

Valid compiler input shall not cause unexpected crashes.

Invalid user input shall not cause unexpected crashes under normal error conditions.

Compiler failures shall be handled predictably.

---

## ARC-0.0.1-R034: Compiler Warnings

The Angry Rocket C codebase shall compile without compiler warnings under the project's supported warning configuration.

Warnings shall not be knowingly ignored in completed version 0.0.1 code.

---

## ARC-0.0.1-R035: Formatting

Project-owned C and C++ source code shall conform to the repository's `.clang-format` configuration.

Third-party code inside `external/` is excluded from this requirement.

---

## ARC-0.0.1-R036: Static Analysis

Project-owned source code shall pass the project's configured clang-tidy checks.

Third-party dependencies are excluded unless explicitly maintained by the Angry Rocket C project.

---

# Compatibility and Stability

## ARC-0.0.1-R037: No Language Stability Guarantee

Angry Rocket C 0.0.1 does not provide a source compatibility guarantee.

Source code written for version 0.0.1 is not guaranteed to compile under later Arc versions.

Syntax, semantics, APIs, compiler behavior, and tooling may change between development releases.

---

## ARC-0.0.1-R038: Experimental Language Design

Version 0.0.1 shall be considered experimental.

Breaking changes are permitted while the language and compiler architecture are still being developed.

Backward compatibility shall not take priority over correcting or improving the language design during this stage.

---

# Acceptance Criteria

Angry Rocket C 0.0.1 shall be considered complete when all mandatory requirements in this section are satisfied.

At minimum, the compiler shall be able to compile a valid single-file Arc program containing:

- a module declaration;
- a `main` function;
- local variables;
- supported primitive types;
- function declarations and calls;
- function parameters;
- return values;
- arithmetic expressions;
- comparison expressions;
- `if` / `else` statements;
- `while` loops; and
- single-line comments.

The following example represents the general complexity expected to be supportable by the release:

```arc
module main
{
    fn add(i32 a, i32 b) -> i32
    {
        return a + b;
    }

    fn main() -> i32
    {
        i32 value = add(20, 22);

        if (value > 0)
        {
            while (value > 1)
            {
                value = value - 1;
            }
        }

        return value;
    }
}
```

The exact syntax may change before version 0.0.1 is finalized.

The compiler shall:

1. accept the source file;
2. tokenize and parse it successfully;
3. perform semantic validation;
4. generate valid LLVM-backed output;
5. emit an object file;
6. terminate successfully without compiler warnings, crashes, or memory leaks; and
7. pass the associated unit and integration test suite.

Invalid source programs shall fail compilation cleanly and produce appropriate diagnostics rather than causing compiler crashes or unpredictable behavior.

---

# Deferred Features

The following features are intentionally deferred beyond version 0.0.1:

- multiple source files;
- functional module loading;
- functional imports;
- constants;
- structs;
- pointers;
- references;
- arrays;
- strings;
- casts;
- linking;
- executable generation;
- package management;
- standard-library support;
- source compatibility guarantees; and
- stable language semantics.