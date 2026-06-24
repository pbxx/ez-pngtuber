# Agent Guide

## Project Shape

This repository contains a C++20 wxWidgets desktop app in `application/`.

Current purpose: monitor Discord StreamKit voice overlay state without Discord Developer Portal setup, OAuth tokens, client secrets, Node, or bundled browser automation.

The app launches a user-selected Chromium-family browser, connects to its local DevTools endpoint, evaluates a DOM scraper against the StreamKit overlay page, and displays users/speaking state in the wx UI.

The UI now supports SQLite-backed `Groups` stored under app data. A group currently holds the StreamKit connection settings and is intended to later hold Discord username to PNGTuber ID pairings as well.

## Key Files

- `application/src/main.cpp`
  - wxWidgets application entry point.
- `application/src/MainWindow.h`
- `application/src/MainWindow.cpp`
  - Main wxFrame, group selector/actions, StreamKit controls, users table, pop-out logs window.
- `application/src/GroupStore.h`
- `application/src/GroupStore.cpp`
  - SQLite-backed persistence for StreamKit groups and app-level settings such as the active group.
- `application/src/AppPaths.h`
- `application/src/AppPaths.cpp`
  - Shared `%LOCALAPPDATA%\EZ PNGTuber` path helpers for the SQLite database and browser profile.
- `application/src/StreamKitMonitor.h`
- `application/src/StreamKitMonitor.cpp`
  - Browser detection, browser launch, DevTools WebSocket connection, DOM polling, log/status callbacks.
- `application/src/DiscordModels.h`
  - Shared lightweight structs for displayed users.
- `application/CMakeLists.txt`
  - App target and dependencies.
- `application/vcpkg.json`
  - Dependencies: `curl`, `nlohmann-json`, `sqlite3`, `wxwidgets`.
- `StreamkitOverlayTemplate.html`
  - Captured StreamKit markup sample useful for tuning DOM selectors.

There are legacy `DiscordRpcClient.*` files in the tree from an earlier prototype. They are not part of the current target and should not be reintroduced unless the product direction changes.

## Runtime Architecture

1. `MainWindow` loads groups from `%LOCALAPPDATA%\EZ PNGTuber\groups.db` through `GroupStore`.
2. The selected group currently supplies:
   - StreamKit overlay URL
   - browser executable path
   - visible/headless mode preference
   - local app prompt bypass preference
   - poll interval
3. `StreamKitMonitor::Start(...)` launches a worker thread.
4. The worker:
   - launches the browser using Windows `CreateProcessW`
   - uses a persistent profile at `%LOCALAPPDATA%\EZ PNGTuber\StreamKitBrowserProfile`
   - exposes DevTools on `127.0.0.1:<dynamic-port>`
   - queries `/json/list` with libcurl
   - connects to the page WebSocket with WinHTTP
   - periodically sends `Runtime.evaluate`
5. The evaluated JavaScript scrapes real StreamKit user rows:
   - user rows should have `data-userid`, `data-user-id`, or `data-id`
   - speaking is derived from row/avatar classes such as `wrapper_speaking`
6. Results flow back through callbacks to the UI thread via `CallAfter`.

## Important Constraints

- Do not add Node or a Playwright sidecar. The current direction explicitly avoids Node.
- Do not restore the old Discord RPC UI unless requested. StreamKit is the active path.
- The browser prompt bypass uses experimental Chromium flags. Keep it optional and logged.
- The app can only observe state that the StreamKit overlay renders.
- Browser automation is Windows-specific right now because it uses Windows process and WinHTTP APIs.
- Keep group persistence in SQLite under `%LOCALAPPDATA%\EZ PNGTuber`; do not introduce a second settings store for the same data without a strong reason.
- Group scope is the intended home for future Discord username to PNGTuber ID mappings.

## Build

From `application/`:

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug --config Debug
```

From repository root:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-msvc.ps1
```

If CMake reconfigures and vcpkg fails inside a restricted sandbox, rerun the build with network permission. vcpkg dependencies are already declared in `application/vcpkg.json`.

## Debugging Tips

- Use `Start With Browser Window` to see the actual StreamKit overlay.
- Use the `Groups` section to verify which saved configuration is active before debugging StreamKit connection issues.
- Use `Show Logs` for:
  - launch command
  - profile path
  - DevTools URL
  - process id
  - target list
  - selected DevTools target
  - WebSocket send/receive errors
  - scrape counts
- If user rows duplicate, inspect `StreamkitOverlayTemplate.html` and adjust `OverlayScrapeScript` in `StreamKitMonitor.cpp`.
- If speaking highlights the wrong row, avoid broad descendant selectors. Prefer row/avatar classes on actual user rows with concrete user ids.

## Code Style Notes

- Keep UI callbacks on the main thread with `CallAfter`.
- Keep persisted group and future mapping logic inside `GroupStore` rather than spreading SQLite calls through the UI.
- Keep browser/DevTools work inside `StreamKitMonitor`.
- Use the existing callback pattern for status, errors, logs, and user snapshots.
- Avoid broad DOM selectors that can match StreamKit containers.
- Prefer small, explicit UI controls over adding another settings surface.
