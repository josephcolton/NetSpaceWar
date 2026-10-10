# Cross-compile Windows (x86_64) executables from Linux with MinGW-w64.
#
#   cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
#   cmake --build build-windows
#
# Works with the distro's mingw-w64 package (apt install mingw-w64) or with
# llvm-mingw; either way, the x86_64-w64-mingw32-* tools must be on PATH.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

# Look for libraries and headers only in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Link the C runtime statically so the .exe files run without extra DLLs
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
