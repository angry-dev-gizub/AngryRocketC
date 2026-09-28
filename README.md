<p align="center">
  <img src="docs/assets/arc-logo.png" alt="Angry Rocket C logo" width="320">
</p>

<h1 align="center">Angry Rocket C</h1>

<p align="center">
  <strong>A C-like, general-purpose systems programming language for low-level software development.</strong>
</p>

<p align="center">
  <em>Angry Rocket C is commonly abbreviated as Arc.</em>
</p>

> [!WARNING]
> Angry Rocket C is in **very early development**. The language, compiler architecture, syntax, APIs, tooling, and project structure may change significantly. There is currently no usable compiler implementation.

## Overview

**Angry Rocket C (Arc)** is a general-purpose systems programming language designed to provide low-level access to hardware and system resources while offering a cleaner and more convenient syntax than traditional C.

Arc is C-like in spirit, but it is not intended to replace C, C++, Rust, or other established systems programming languages.

The language has two primary purposes:

- to serve as a general-purpose systems programming language; and
- to support the wider **Angry Rocket ecosystem**, including the future Angry Rocket game engine.

The Angry Rocket game engine repository does not exist yet, so a link will be added once the project is publicly available.

## Goals

Arc aims to:

- provide predictable low-level programming capabilities;
- allow direct access to memory, hardware, operating-system facilities, and other system resources;
- provide a cleaner and more expressive syntax than C where doing so does not compromise low-level control;
- remain suitable for systems programming;
- provide reusable compiler infrastructure for tools such as the Arc language server;
- serve as a core language for the Angry Rocket ecosystem; and
- remain useful independently as a general-purpose programming language.

## Non-goals

Arc is **not** intended to:

- replace C;
- replace C++;
- replace Rust;
- replace every other systems programming language; or
- hide low-level programming behind abstractions that remove the programmer's ability to control the underlying system.

Arc exists to provide another approach to systems programming, not to declare the previous fifty years of language design a clerical error.

## Example

The syntax is still evolving, but the intended style currently looks like this:

```arc
module main
{
    import arc.std.io;

    fn main() -> i32
    {
        std::io::println("Hello World!");

        return 0;
    }
}
```

This example may change as the language specification develops.

## Current Status

Arc currently has **no functional compiler implementation**.

The repository is presently focused on project architecture, compiler infrastructure design, build configuration, testing infrastructure, documentation, and language design.

As implementation progresses, this section will be updated to describe features that are actually available rather than features that merely exist enthusiastically in a roadmap.

## Project Structure

```text
.
├── .vscode/       VS Code project configuration
├── ARC/           Arc compiler executable, CLI, and compiler-specific runtime/orchestration
├── ARC-CORE/      Reusable compiler core library
├── ARC-LSP/       Arc Language Server Protocol implementation
├── docs/          Documentation and documentation build configuration
├── external/      Third-party dependencies
├── tests/         Unit and integration tests
├── CMakeLists.txt Root CMake build configuration
├── VERSION        Current Angry Rocket C version
├── LICENSE        Software license
├── NOTICE         Copyright and attribution information
├── SECURITY.md    Security vulnerability reporting policy
├── RESPONSIBLE_USE.md
├── TRADEMARKS.md
├── requirements.md
├── roadmap.md
└── design_decisions.md
```

### `ARC-CORE`

`ARC-CORE` is a static library containing reusable compiler infrastructure.

It is intended to contain functionality shared by multiple Arc tools, such as the compiler and the language server. As the compiler develops, this may include components such as source management, diagnostics, lexical analysis, parsing, semantic analysis, type information, intermediate representations, and supporting infrastructure.

### `ARC`

`ARC` contains the Arc compiler executable.

It is responsible for the command-line interface and compiler-specific orchestration built on top of `ARC-CORE`.

### `ARC-LSP`

`ARC-LSP` contains the Arc language server implementation.

It uses compiler infrastructure from `ARC-CORE` to provide editor-facing language features.

The LSP is currently optional and disabled by default.

### `tests`

`tests` contains the project's tests.

The test suite uses C++ and GoogleTest. Tests are enabled by default when Angry Rocket C is configured as the top-level CMake project.

### `external`

`external` contains third-party dependencies used by Angry Rocket C.

Required source dependencies are included with the project, so users do not need to manually install those libraries before building Arc. Individual third-party components remain subject to their respective licenses.

### `docs`

`docs` contains project documentation and the build configuration for generated documentation.

## Building from Source

### Requirements

To build Angry Rocket C, you need:

- **CMake 3.12 or newer**;
- a **C11-compatible C compiler**;
- a **C++ compiler**; and
- a build backend supported by CMake, such as Ninja or Make.

Third-party source dependencies required by the project are included in the repository.

### Configure

From the repository root:

```bash
cmake -S . -B build
```

### Build

```bash
cmake --build build
```

Runtime executables are written to:

```text
build/bin/
```

### Optional Components

The Arc Language Server is currently disabled by default.

Enable it during configuration with:

```bash
cmake -S . -B build -DARC_BUILD_LSP=ON
```

Tests can be controlled with:

```bash
cmake -S . -B build -DARC_BUILD_TESTS=ON
```

or:

```bash
cmake -S . -B build -DARC_BUILD_TESTS=OFF
```

After changing configuration options, build normally:

```bash
cmake --build build
```

## Testing

When tests are enabled, run them through CTest:

```bash
ctest --test-dir build
```

The test infrastructure is written in C++ and uses GoogleTest.

## Development Tooling

The repository includes project configuration for:

- CMake;
- CTest;
- VS Code;
- clang-format; and
- clang-tidy.

### Formatting

The project's formatting rules are defined in:

```text
.clang-format
```

The included VS Code configuration enables formatting on save for C and C++ source files.

### Static Analysis

Static-analysis rules are defined in:

```text
.clang-tidy
```

Clang-tidy can be enabled through the project's CMake configuration when required.

The build also exports `compile_commands.json` for tooling that uses a C/C++ compilation database.

## Documentation

Project information is split across several files so that this README does not eventually become a small book:

- [`roadmap.md`](roadmap.md) describes planned development;
- [`requirements.md`](requirements.md) describes project requirements;
- [`design_decisions.md`](design_decisions.md) records significant architectural and design decisions;
- [`SECURITY.md`](SECURITY.md) explains how to report security vulnerabilities;
- [`RESPONSIBLE_USE.md`](RESPONSIBLE_USE.md) describes the project's position on malicious, unlawful, or abusive use;
- [`TRADEMARKS.md`](TRADEMARKS.md) describes the use of project names and branding; and
- [`docs/`](docs/) contains additional project and generated documentation.

## Versioning

The current Angry Rocket C version is stored in the root [`VERSION`](VERSION) file.

Arc is in early development, so backward compatibility is not currently guaranteed between development versions.

## Security

Security vulnerabilities in Angry Rocket C should be reported according to [`SECURITY.md`](SECURITY.md).

Please avoid publicly disclosing a vulnerability before it has been reviewed through the project's designated security-reporting process.

## Responsible Use

Angry Rocket C is a general-purpose programming language and compiler.

The project does not endorse malicious, unlawful, or abusive use of Arc. Users are responsible for software they create with or through Arc and for complying with applicable law.

See [`RESPONSIBLE_USE.md`](RESPONSIBLE_USE.md) for the project's responsible-use statement.

## License

Angry Rocket C is licensed under the **Apache License, Version 2.0**.

See [`LICENSE`](LICENSE) for the full license terms and [`NOTICE`](NOTICE) for applicable copyright and attribution information.

Third-party software included in or used by the project may be distributed under separate licenses.

## Project Name and Branding

**Angry Rocket C** is the official project name.

**Arc** is used throughout the project as an abbreviation for Angry Rocket C.

Use of the source code under the Apache License does not by itself grant permission to present a modified or independently distributed version as an official Angry Rocket C release.

See [`TRADEMARKS.md`](TRADEMARKS.md) for the project's branding policy.