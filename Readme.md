# proxy — tiny HTTP/HTTPS proxy for tcpsvd & ncat

Tiny C proxy that reads from stdin and writes to stdout (inetd/tcpsvd/ncat style). Handles HTTP CONNECT (HTTPS tunneling) and plain HTTP forwarding. Now fully cross-platform with Windows (MinGW/Clang) and Unix compatibility.

## Features
- **Cross-Platform:** Fully compatible with Linux/Unix and Windows.
- **Threaded Tunneling on Windows:** Utilizes a background thread on Windows to safely bypass the standard input `select()` limitation (where `select()` cannot monitor pipes/console descriptors).
- **Binary-Safe Stdio:** Automatically sets standard input/output streams to binary mode on Windows to prevent text-mode translation from corrupting raw TLS/HTTPS traffic.
- **Detailed Diagnostics:** Reports specific error codes (`errno` on Unix, `WSAGetLastError()` on Windows) when network operations fail.
- **Centralized Resource Management:** Uses `atexit()` for robust socket initialization cleanup on all exit paths.

## Build

### Unix / Linux
```sh
meson setup build
ninja -C build
```

### Windows (using MinGW)
Make sure you have Meson, Ninja, and a MinGW compiler (e.g. LLVM-MinGW) in your PATH:
```powershell
meson setup build
ninja -C build
```

## Usage

### Standalone Server Mode (Recommended - No external dependencies)
You can run the proxy directly as a standalone TCP server on both Windows and Unix:
```sh
# Run on port 8080
./build/proxy --listen 8080

# Run on port 8080 with verbose logging (displays method, host, and port for all connections)
./build/proxy --listen 8080 -v

# Run on port 8080 with authentication
./build/proxy --listen 8080 myuser mypass
```

### Classic Stdio / Wrapper Mode (Backward Compatible)

#### Unix / Linux
Using `tcpsvd` (from busybox/ucspi-tcp):
```sh
tcpsvd -vE 0.0.0.0 8080 ./build/proxy
```
Or using `socat`:
```sh
socat TCP-LISTEN:8080,reuseaddr,fork EXEC:./build/proxy
```

#### Windows
Using `ncat` (from Nmap):
```powershell
ncat -l -p 8080 -e ".\build\proxy.exe" --keep-open
```
Or using `socat` on Windows:
```powershell
socat TCP-LISTEN:8080,reuseaddr,fork EXEC:".\build\proxy.exe"
```

## Authentication
If you wish to require proxy authentication, pass the `username` and `password` as command-line arguments:

### Standalone Server Mode
```sh
# Example: require 'user' and 'pass'
./build/proxy --listen 8080 user pass
```

### Classic Stdio / Wrapper Mode
```sh
# Unix
socat TCP-LISTEN:8080,reuseaddr,fork EXEC:"./build/proxy user pass"

# Windows
ncat -l -p 8080 -e ".\build\proxy.exe user pass" --keep-open
```
