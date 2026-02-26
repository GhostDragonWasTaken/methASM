# Standard Library

The standard library lives under `stdlib/`. Modules are imported by path. The `std/` prefix is resolved under the stdlib root (default `stdlib`).

## std/io

Console and file I/O. `puts` writes a null-terminated string and appends a newline; `putchar` writes a single character; `getchar` reads one. `print` and `println` write a cstring (println adds a newline). `print_int` and `println_int` write an integer in decimal. `cstr(s: string) -> cstring` converts a MethASM string to a C string for passing to C functions. File operations: `fopen`, `fclose`, `fread`, `fwrite`, `fputs`, `fgets`, `fflush`. File handles are `cstring` (opaque `FILE*`). Stream accessors: `get_stdin`, `get_stdout`, `get_stderr`.

## std/mem

Memory management. C runtime functions: `malloc`, `calloc`, `realloc`, `free`, `memset`, `memcpy`, `memmove`, `memcmp`. Helpers: `alloc_zeroed` (allocate and zero-initialize), `buf_dup` (allocate and copy a buffer). Use `malloc` for buffers, C interop, or when the GC is not linked. Use `new` for MethASM struct instances that should be garbage-collected.

## std/math

Math utilities. `abs` (C runtime). `min`, `max`, `clamp` (integer operations on int64).

## std/conv

Conversions and character classification. C runtime: `atoi`, `atol`. Digit helpers: `digit_to_char`, `char_to_digit`. Classification: `is_digit`, `is_upper`, `is_lower`, `is_alpha`, `is_alnum`, `is_space`. Case conversion: `to_lower`, `to_upper`. String utilities: `strlen`, `streq`.

## std/process

Process control. `exit` terminates the program with an exit code. `rand`, `srand` for pseudo-random numbers.

## std/net

Winsock2 bindings for Windows. Requires linking with `-lws2_32`. Socket constants: `AF_INET`, `SOCK_STREAM`, `SOCK_DGRAM`, etc. Core functions: `socket`, `bind`, `listen`, `accept`, `send`, `recv`, `closesocket`. Lifecycle: `net_init`, `net_cleanup`. `sockaddr_in` builds a sockaddr_in structure for IPv4. See the `web/` directory for a complete server example.

## std/prelude

The prelude re-exports `std/io`, `std/math`, `std/conv`, `std/mem`, `std/process`, `std/net`. Use with `--prelude` to automatically import these modules without explicit `import` statements. The prelude is opt-in; it is not loaded by default.

```bash
methasm --prelude main.masm -o main.s
```
