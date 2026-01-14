# Zenith Language

## What is it?

Zenith is a programming language created for developing self-contained applications. The project is being developed from scratch in pure C and combines systems programming capabilities with high-level abstractions. Zenith enables the creation of a wide range of applications—from console utilities to graphical programs and web servers—all within a unified ecosystem.

## Installation

1. Download the `zenith.zip` archive
2. Extract the archive to a directory of your choice
3. Navigate to the `installer` folder
4. Run the installer:
   ```bash
   ./zenith-installer
   ```
5. Follow the on-screen instructions

Done! Zenith is now installed and ready to use.

## Why is it needed?

The main goal of Zenith is to provide a tool that shortens the distance between an idea and its implementation. Instead of assembling complex dependency chains and learning multiple frameworks, you get a single environment with the essential tools needed to build working applications.

## Key Features

- C-like syntax — familiar and predictable
- Modular architecture — support for custom libraries via import
- Built-in capabilities:
  - Graphics (windows, 2D rendering via SDL2/OpenGL)
  - Cryptography (AES-256, SHA256 via OpenSSL)
  - Networking (multithreaded HTTP server)
  - File system
- Compilation to native binaries via TCC
- REPL mode for interactive development

## Advantages

- Minimal dependencies — applications contain everything necessary
- Quick start — no complex environment setup required
- Unified approach — consistent principles for different types of tasks
- Performance — compilation to native code
- Clarity — syntax is not overloaded with excessive abstractions

## Limitations and Current Status

The project is under active development. The current beta version has the following limitations:

- Only Linux is supported (work on a Windows version is underway)
- The standard library is being expanded
- Documentation is in the process of being written
- Testing in production scenarios is required

## Who is this language for?

- Developers who need rapid prototyping with minimal dependencies
- Students learning the fundamentals of programming and systems design
- Utility creators who value self-contained applications
- Anyone who appreciates simplicity and straightforwardness in tools

## Roadmap

- Core stabilization — debugging and optimizing basic syntax
- Expansion of the standard library — additional modules
- Public release — open source and documentation
