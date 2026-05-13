# FixedPointCalc

`fpc` is a command-line fixed-point calculator. It can evaluate expressions directly, run `.fpc` script files, or start an interactive readline-based REPL.

## Features

- Fixed-point arithmetic with configurable total bits, integer bits, and fractional bits
- Decimal, hexadecimal, binary, and raw fixed-point literals
- Rounding and overflow modes
- Interactive REPL with readline history
- Script and one-shot expression execution

## Dependencies

- CMake 3.16 or newer
- A C++20 compiler with `__int128` support, such as GCC or Clang
- GNU readline
- Terminal support library (`tinfo` or `ncurses`)
- Bash, `grep`, and `mktemp` for the CTest-based CLI test script

The current source uses `unistd.h`, GNU readline, and `__int128` intermediates while limiting user-visible fixed-point formats to at most 64 bits. It is intended to build on Linux, BSD, and Windows through MSYS2 MinGW. A native MSVC build is not expected to work without code and dependency changes.

No GMP, Boost, or pkg-config dependency is required.

Typical package names:

- Debian/Ubuntu: `cmake`, `g++`, `libreadline-dev`, `libncurses-dev`
- Fedora: `cmake`, `gcc-c++`, `readline-devel`, `ncurses-devel`
- Arch Linux: `cmake`, `gcc`, `readline`, `ncurses`
- FreeBSD: `cmake`, `readline`, `ncurses`
- OpenBSD: `cmake`, `readline`

## Build With CMake

Configure and build out of tree:

```sh
cmake -S . -B build
cmake --build build
```

The executable is created as `build/fpc` on Unix-like systems and `build/fpc.exe` on Windows/MSYS2.

Run a quick expression:

```sh
./build/fpc -e '1.5 + 2.25'
```

Install with CMake:

```sh
cmake --install build
```

You can also use the helper script on Unix-like systems:

```sh
./install.sh
```

## Run Tests With CTest

Build first, then run the test suite:

```sh
ctest --test-dir build --output-on-failure
```

Equivalent form from inside the build directory:

```sh
cd build
ctest --output-on-failure
```

The test suite currently contains CLI regression tests driven by `tests/run_cli_tests.sh`.

## Windows Build With MSYS2

Use an MSYS2 MinGW environment, such as `UCRT64` or `MINGW64`. Do not use the plain `MSYS` shell for the final build unless you specifically want an MSYS-linked binary.

Install dependencies in an `UCRT64` shell:

```sh
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-readline \
  mingw-w64-ucrt-x86_64-ncurses
```

For a `MINGW64` shell, use the same package names with `mingw-w64-x86_64-` instead of `mingw-w64-ucrt-x86_64-`.

Configure, build, and test:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the executable:

```sh
./build/fpc.exe -e '1.5 + 2.25'
```

Dependency check for Windows/MSYS2:

- `readline` and `history` are provided by the MSYS2 MinGW readline package.
- `ncurses` provides the terminal support library used when `tinfo` is not found.
- `unistd.h`, `isatty`, and `STDIN_FILENO`/`STDOUT_FILENO` are available in the MSYS2 MinGW toolchain.
- `__int128` is supported by the MSYS2 GCC toolchains used above.
- CTest invokes a Bash script, so run tests from an MSYS2 shell with Bash and standard Unix tools available.

## Linux And BSD Notes

The default CMake configuration should work when the compiler and readline development files are installed in standard system paths:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

On BSD systems where readline is installed under `/usr/local`, pass the include and library paths explicitly if CMake does not find it automatically. For example:

```sh
cmake -S . -B build \
  -DREADLINE_INCLUDE_DIR=/usr/local/include \
  -DREADLINE_LIBRARY=/usr/local/lib/libreadline.so \
  -DHISTORY_LIBRARY=/usr/local/lib/libhistory.so \
  -DNCURSES_LIBRARY=/usr/local/lib/libncurses.so
```

Adjust library suffixes if your BSD installation uses a versioned shared library name.

## Usage

```sh
fpc                         # start interactive REPL
fpc -e '1.5 + 2.25'         # evaluate one expression or command
fpc '1.5 + 2.25'            # evaluate one expression or command
fpc tests/examples/basic.fpc # run a script file
fpc --help                  # print help
fpc --version               # print version
```

Example:

```sh
$ fpc -e '1.5 + 2.25'
3.75
```
