# Cross-compiling to Windows:
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 1)

# i686 for 32-bit
set(CMAKE_SYSTEM_PROCESSOR i686)

# MinGW-w64 cross-compilers:
set(CMAKE_C_COMPILER   i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)

set(CMAKE_AR          i686-w64-mingw32-ar         CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB      i686-w64-mingw32-ranlib     CACHE FILEPATH "" FORCE)

# DirectX 9 library should be present here:
set(CMAKE_FIND_ROOT_PATH  /usr/i686-w64-mingw32)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)  # host programs (like cmake-run) stay on Linux
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)   # libraries ONLY from the Windows sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)   # headers ONLY from the Windows sysroot
