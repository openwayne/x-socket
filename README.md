# Proxy Application - Installation 1

## Overview
This is the first installment of the Proxy Application project. It implements the client-side functionality using C and the BSD socket API. The client connects to a remote server, sends a test message (simulating intercepted network data), and prints the server's response.

## Requirements
- GCC or another C compiler
- Unix-like operating system (Linux, macOS, or Windows with a suitable POSIX environment)
- Make (optional, if you wish to use the provided Makefile)

## Building the Client
1. Clone the repository or download the source files.
2. Open a terminal in the project directory.
3. Run the command:
   ```bash
   make
