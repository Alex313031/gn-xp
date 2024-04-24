# Building and using `gn` with MinGW on Windows

At the time of writing: 2024-04-13
Test environment: Windows 11, intel x86_64 CPU

## Requirements

1.  Install [MSYS2](https://www.msys2.org/)
1.  In the MSYS2 installation folder, `clang32.exe`, `clang64.exe`, `clangarm64.exe`,`mingw32.exe`, `mingw64.exe`, `ucrt64.exe` are terminal for MinGW environment. I take `clang64.exe` for example, please open `clang64.exe` terminal.
1.  Run command: `pacman -S mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-ninja git`  
    Download clang toolchain, Ninja and git.

## Build `gn`

1.  Run command: `git clone https://gn.googlesource.com/gn`
    Clone `gn` source code.
1.  Run command: `cd gn`
1.  Run command: `build/gen.py --platform mingw`  
    Default configuration for MinGW build under MSYS2 generates a static executable that can be run anywhere.  
    It also mean that run `gn` for cross environment (like CLANG64, UCRT64).  
    Use `--help` flag to check more configuration options.  
    Use `--allow-warning` flag to build with warnings.  
    Use `--no-static-libstdc++` flag to not run anywhere.
1.  Run command: `ninja -C out`

## Testing

1.  Run command: `out/gn --version`
1.  Run command: `out/gn_unittests`

> Notes:
>
> For "mingw-w64-clang-x86_64-toolchain" in the clang64 environment,
> g++ is a copy of clang++.
> The toolchain that builds `gn` does not use vendor lock-in compiler command flags,
> so `gn` can be built with these clang++, g++.
> But [examples/simple_build](../examples/simple_build/) in the `gn` repo,
> toolchain use vendor (GCC) lock-in compiler command flags,
> so `mingw-w64-ucrt-x86_64-toolchain` in the UCRT64 environment must be used,
> this g++ is not a copy of clang++.
