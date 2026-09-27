# Angry Rocket C Roadmap

This document describes the current development direction of **Angry Rocket C (Arc)**.

The roadmap is organized around language and compiler milestones rather than dates. It is directional, not a compatibility promise, and may change as the language and compiler architecture evolve.

---

# 0.0.1 — Initial Language

## Goal

Create the first minimal version of the Arc language and compiler that is capable of compiling real, single-file Arc programs.

## Planned

- single-file compilation;
- module declarations;
- import syntax recognition;
- mutable variables;
- primitive types:
  - `i32`;
  - `bool`;
  - `float`;
  - `void` for function return types;
- functions;
- function parameters;
- return values;
- `if` / `else`;
- `while`;
- arithmetic operators;
- comparison operators;
- single-line comments;
- semantic analysis;
- diagnostics;
- LLVM-backed code generation;
- object-file output;
- optional LLVM IR output;
- unit tests for compiler subsystems; and
- integration tests.

## Scope

Version 0.0.1 is intentionally small.

It is intended to establish the language, compiler pipeline, diagnostics, testing infrastructure, and LLVM backend without attempting to provide the complete Arc feature set.

---

# 0.0.2 — Expanded Primitive Types and Constants

## Goal

Expand the basic type system and introduce constant values.

## Planned

- additional integer types;
- constants;
- corresponding semantic-analysis support;
- code-generation support for the new primitive types; and
- tests covering the added language features.

The exact set of integer types and final syntax will be defined as the language design develops.

---

# 0.0.3 — Strings, Runes, and Raw Memory

## Goal

Introduce the basic types needed for text representation and raw memory access.

## Planned

- strings;
- runes;
- a `byte` type;
- byte pointers for raw memory access; and
- the supporting semantic and code-generation behavior.

## Type Model Direction

Arc will not use `void*` as its raw-memory pointer type.

Raw memory shall instead be represented through pointers to `byte`.

`void` shall not be usable as a normal value or storage type. Its use shall be limited to contexts such as function return types where the absence of a value must be represented.

---

# 0.0.4 — To Be Defined

Version 0.0.4 is currently reserved for a future milestone.

Its scope will be defined once the requirements of the preceding versions are better understood.

No features are committed to this version yet.

---

# 0.1 — Multi-File Compilation and Linking

## Goal

Turn Arc from a single-file compiler into a more practical compiler capable of compiling multiple source files and producing complete executables.

This milestone represents the first major usability expansion of the compiler.

## Planned

- multi-file compilation;
- functional module and import behavior;
- compilation of multiple Arc source files in a single invocation;
- executable generation;
- linking;
- object-file output;
- improved command-line interface;
- output-kind selection;
- the ability to stop after compilation without linking;
- the ability to continue through linking and produce an executable; and
- the beginning of Arc Language Server development.

## CLI Direction

The compiler should evolve from a minimal interface such as:

```text
arc <source-file>
```

toward an interface capable of accepting multiple input files and controlling the compilation pipeline.

The user should be able to choose whether compilation produces:

- object files; or
- a linked executable.

Additional compiler controls may be introduced as the command-line interface matures.

---

# 0.2 — Compound and Extended Types

## Goal

Expand Arc beyond primitive values by introducing compound data types and a more complete numeric type system.

## Planned

- structs;
- arrays;
- enumerations;
- additional user-facing types;
- `f32`;
- `f64`; and
- the semantic-analysis and code-generation support required by these features.

This milestone is intended to make Arc substantially more capable for real systems programming workloads.

---

# Language Server

The **Arc Language Server (ARC-LSP)** is part of the main Angry Rocket C repository.

Active development of the language server is expected to begin once multi-file compilation is available.

The language server is intended to reuse compiler infrastructure from `ARC-CORE`.

Its detailed feature roadmap will be defined separately as the compiler and language become mature enough to support editor tooling reliably.

---

# Optimization

Arc uses LLVM as its backend and will make use of LLVM's optimization infrastructure.

Optimization is not a primary early-language milestone.

As the compiler command-line interface becomes more capable, Arc is expected to expose optimization controls corresponding to LLVM optimization levels.

The exact command-line interface and optimization policy will be defined later.

---

# Platform Direction

Angry Rocket C is intended to be cross-platform.

The compiler codebase should be written portably so that Arc can be built on supported systems when the required toolchain is available.

The intended host platforms are:

- Linux;
- Windows; and
- macOS.

Platform-specific code should be avoided where possible and isolated when unavoidable.

---

# Future Compiler Tooling

A debugger may eventually become part of the main Angry Rocket C project.

Debugger development is not currently part of the near-term roadmap and will begin only after the core language and compiler are sufficiently mature.

---

# Separate Angry Rocket Projects

The following components are related to the Angry Rocket ecosystem but are not part of the main Angry Rocket C repository roadmap.

## Standard Library

The Arc standard library will be developed as a separate project.

It will depend on the Arc language and compiler but will not be maintained as part of the compiler repository itself.

## Build System

The Arc build system will be developed as a separate project.

It is not part of the Angry Rocket C compiler repository.

## Self-Hosting

A self-hosted Arc compiler may eventually be developed as a separate Arc project.

Self-hosting depends on the native Arc compiler becoming sufficiently complete and stable first.

---

# Long-Term Direction

The long-term direction of Angry Rocket C is to become:

- a general-purpose systems programming language;
- a practical language for the Angry Rocket ecosystem;
- portable across Linux, Windows, and macOS;
- capable of compiling multi-file programs;
- capable of producing object files and executables;
- supported by language-server tooling;
- capable of using LLVM optimization facilities; and
- mature enough to support later projects such as a standard library, build system, debugger, and self-hosted compiler.

Arc is not intended to replace C, C++, Rust, or other established systems programming languages.

It exists to provide its own approach to low-level and systems programming.

---

# Roadmap Policy

This roadmap does not define release dates.

Milestones are feature-oriented and will be completed when their requirements are satisfied.

Features may move between versions, change in scope, or be redesigned as implementation experience reveals better solutions.

Development versions of Arc do not provide a source-compatibility guarantee unless explicitly stated otherwise.