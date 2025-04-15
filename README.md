# x-socket


## Overview
This is a proxy project. It implements the client-side and server-side functionality using C and the BSD socket API. The client connects to a remote server, sends a test message (simulating intercepted network data), and prints the server's response. It can also get information about processes under different operating systems.

## Requirements
- GCC or another C compiler
- Unix-like operating system (Linux, macOS, or Windows with a suitable POSIX environment)
- Make

## Building the Client
1. Clone the repository or download the source files.
2. Open a terminal in the project directory.
3. Run the command:
   ```bash
   make
   ```
