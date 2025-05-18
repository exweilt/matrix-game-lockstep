# Cross-compiling MatrixGame on Linux

You can cross-compile the game on Linux for Windows and then launch it using wine!

You can also debug it! 

The problems are:
- There is no text in the game (if you run standalone game on Linux).
- There is no pretty prints inside debugger (you would need to compile the gdb with python enabled).

### Install dependencies

- Arch Linux: `sudo pacman -S cmake ninja`

### Install cross-compiler

There is a linux compiler which can target Windows, install it:

- Arch Linux: `sudo pacman -S mingw-w64-gcc mingw-w64-headers mingw-w64-winpthreads mingw-w64-binutils mingw-w64-crt`

### Configure CMake project

Specify the toolchain file when configuring:

`cmake -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE="cmake/linux-toolchain.cmake"`

If you are using GUI cmake click "custom toolchain file" and choose `cmake/linux-toolchain.cmake`

### Building

`cmake --build build`

Or if you are using CLion IDE you can open the `./build` directory as project and then you'll be able to build/run by pressing "Build/Run/Debug". The IDE will take care of running the game using `wine` under the hood.

### Running

If you don't have the CLion IDE you can run the game with

`wine MatrixGame.exe`

You may be required to bring some libraries to to directory with the game:
- `libgcc_s_dw2-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`

If it is the case you can copy them from `/usr/i686-w64-mingw32/bin/`.

## Debugging on linux

It's not easy but possible!

Since the game can only be built for Windows it can only be run under `wine` on Linux.

Therefore, there are 2 ways to debug:

- Run PE (.exe) windows debugger(gdb) under `wine` alongside the game (which is also run under `wine`).
- Using an ELF linux cross-debugger(runs on Linux, understands only Windows) and then run the game using `gdb-server.exe` under `wine` and try to connect. (**untested**) 

The only way I tested (and it worked) was to use `/usr/i686-w64-mingw32/bin/gdb.exe` as the debugger of choice in CLion IDE.

### Download Windows debugger

- Arch linux: `yay -S mingw-w64-gdb`

Select the `/usr/i686-w64-mingw32/bin/gdb.exe` as the debugger of choice inside CLion.

Set up a breakpoint inside IDE and press "Debug".

The only problem is that the `gdb.exe` which comes from `mingw-w64-gdb` package on AUR was built without python support so it lacks good display of std templates, such as `std::wstring`.

If you want good printing you would need to compile the debugger yourself with python enabled option!