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
- multi-line comments
- semantic analysis;
- diagnostics;
- LLVM-backed code generation;
- object-file output;
- optional LLVM IR output;
- unit tests for compiler subsystems;
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
- constants with `const`;
- corresponding semantic-analysis support;
- code-generation support for the new primitive types;
- tests covering the added language features.

The exact set of integer types is:
 - `i8`;
 - `i16`;
 - `i64`;
 - `u8`;
 - `u16`;
 - `u32`;
 - `u64`;

---