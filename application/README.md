# EZ PNGTuber C++ App

wxWidgets boilerplate for the EZ PNGTuber desktop app.

## Requirements

- CMake 3.24 or newer
- A C++20 compiler:
  - Windows: MSVC 2022 or MinGW-w64 GCC
  - Linux: GCC or Clang
- wxWidgets 3.2 or newer

You can provide wxWidgets with a system install, MSYS2, Linux distro packages, or vcpkg.

## Build

From this directory:

```sh
cmake --preset ninja-debug
cmake --build --preset ninja-debug
```

With Visual Studio 2022 on Windows:

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug
```

If CMake cannot find wxWidgets, pass its install prefix:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/wxwidgets
```

For MSYS2 UCRT64, the prefix is usually:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
```

## vcpkg

This directory includes a `vcpkg.json` manifest with `wxwidgets`.
When configuring with a vcpkg toolchain file, vcpkg can install the dependency for the active triplet:

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

Use a full vcpkg checkout from `https://github.com/microsoft/vcpkg`.
The Visual Studio-bundled vcpkg under `VC/vcpkg` may not include the built-in ports registry needed for manifest installs.
