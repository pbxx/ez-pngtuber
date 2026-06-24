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

This directory includes a `vcpkg.json` manifest with `curl`, `nlohmann-json`, and `wxwidgets`.
When configuring with a vcpkg toolchain file, vcpkg can install the dependency for the active triplet:

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

Use a full vcpkg checkout from `https://github.com/microsoft/vcpkg`.
The Visual Studio-bundled vcpkg under `VC/vcpkg` may not include the built-in ports registry needed for manifest installs.

## Discord RPC

The app includes a Discord voice monitor prototype using Discord's local RPC IPC server.

1. Create a Discord application in the Developer Portal.
2. Add yourself as a tester while the app is unapproved for RPC access.
3. Start the Discord desktop client.
4. Run EZ PNGTuber, open `Discord > Connect / Authenticate`, enter the application client ID, and either:
   - enter a Discord OAuth access token with `rpc identify` scopes, or
   - enter the client secret so the app can exchange the RPC authorization code locally.
5. Pick a server, pick a voice channel, then use `Monitor Voice Channel`.

Discord's RPC API is local-client based. It can list guilds/channels visible to the authenticated Discord user and subscribe to voice-state and speaking events for a selected channel.

## StreamKit Overlay Monitoring

For a no-Developer-Portal flow, use the `StreamKit Overlay` tab:

1. Configure a Discord StreamKit voice overlay URL in a browser or OBS-style flow.
2. Paste the overlay URL into EZ PNGTuber.
3. Choose a detected Chromium-family browser, or paste a browser executable path.
4. Start the overlay monitor.

EZ PNGTuber launches the selected browser headlessly, attaches through the browser's local DevTools endpoint, and polls the rendered StreamKit overlay DOM for present users and speaking state. This avoids asking users for Discord client secrets or access tokens, but it can only observe state that the overlay renders.
