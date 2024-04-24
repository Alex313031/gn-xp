# MinGW

At the time of writing: 2024-04-13
Test environment: Windows 11, intel x86_64 CPU

## How to build `gn`?

1.  install [msys2](https://www.msys2.org/)
1.  In the msys2 installation folder, `clang32.exe`, `clang64.exe`, `clangarm64.exe`,`mingw32.exe`, `mingw64.exe`, `ucrt64.exe` are terminal for MinGW environment. I take `clang64.exe` for example, please open `clang64.exe` terminal.
1.  run command: `pacman -S mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-ninja git`
1.  run command: `git clone https://gn.googlesource.com/gn`
1.  run command: `cd gn`
1.  run command: `build/gen.py --platform mingw --no-static-libstdc++`
    if you want to build with warnings, add `--allow-warning` flag.
    if you want to run `gn` for cross environment (like clang64, ucrt), remove `--no-static-libstdc++` flag.
1.  run command: `ninja -C out`
1.  run command: `out/gn --version`
1.  run command: `out/gn_unittests`

> Notes
> For "mingw-w64-clang-x86_64-toolchain" in the clang64 environment, g++ is copy of clang++. toolchain that build `gn` do not use vendor lock-in compiler command flags, so you can build `gn` with these clang++, g++.
But [examples/simple_build](../examples/simple_build/) in the `gn` repo, toolchain use vendor (GCC) lock-in compiler command flags, so you must use `mingw-w64-ucrt-x86_64-toolchain` in the ucrt64 environment, this g++ is not copy of clang++.
