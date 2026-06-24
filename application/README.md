# EZ PNGTuber C++ App

wxWidgets desktop prototype for monitoring Discord StreamKit voice overlay state.

## What It Does

The app monitors a Discord StreamKit voice overlay without requiring a Discord Developer Portal application, client secret, access token, Node sidecar, or bundled browser.

Current flow:

1. Configure/copy a Discord StreamKit voice overlay URL.
2. Paste it into the `StreamKit Overlay` tab.
3. Pick or create a group for that setup.
4. Choose a detected Chromium-family browser, or paste a browser executable path.
5. Save the group or start the monitor.

EZ PNGTuber launches the selected browser with a dedicated app profile, attaches to its local Chrome DevTools endpoint, and polls the rendered overlay DOM for users and speaking state.

## Features

- Detects common Chromium-family browsers on Windows.
- Supports headless monitoring or a visible browser window for debugging.
- Uses a persistent browser profile at `%LOCALAPPDATA%\EZ PNGTuber\StreamKitBrowserProfile`.
- Stores StreamKit groups in `%LOCALAPPDATA%\EZ PNGTuber\groups.db`.
- Includes an optional prompt-bypass flag bundle for Chromium private/local-network access prompts.
- Displays users in call and speaking status in the main table.
- Provides a pop-out logs window for browser launch and DevTools diagnostics.
- Poll interval is configurable from the UI.

## Requirements

- CMake 3.24 or newer
- C++20 compiler:
  - Windows: MSVC 2022
  - Other platforms may require more work; the browser launcher currently targets Windows APIs.
- A Chromium-family browser such as Microsoft Edge, Google Chrome, or Brave.
- vcpkg or equivalent dependencies:
  - `curl`
  - `nlohmann-json`
  - `wxwidgets`

## Build

With Visual Studio 2022 on Windows:

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug --config Debug
```

The helper script from the repository root also configures/builds the MSVC preset:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-msvc.ps1
```

## vcpkg

This directory includes a `vcpkg.json` manifest. When configuring with a vcpkg toolchain file, vcpkg can install the dependency set for the active triplet.

Use a full vcpkg checkout from `https://github.com/microsoft/vcpkg`. The Visual Studio-bundled vcpkg under `VC/vcpkg` may not include the built-in ports registry needed for manifest installs.

## Notes

- The app intentionally does not use Discord RPC authentication in the UI. StreamKit is the active path.
- The optional `Bypass local app prompt` setting uses experimental Chromium flags. If Chromium ignores those flags in a future version, run once with `Start With Browser Window` and allow the StreamKit local-app prompt; the dedicated profile should remember that decision.
- The table can only show what the StreamKit overlay renders. Hidden Discord state is not available through this approach.
