# FarFarWest Unlock all tool

Portable native Windows save editor for **FarFarWest**.

This project rewrites the earlier tool as a standalone desktop app built with Win32 and WebView2, so end users can open, inspect, edit, and save `.save` files without needing a Python installation.

![FarFarWest Unlock all tool screenshot](docs/images/app-screenshot.svg)

> UI preview based on the current desktop layout.

## Highlights

- Native Windows `.exe` with a custom WebView2 UI
- AES-256-CBC decrypt/encrypt pipeline for FarFarWest save files
- GVAS parse and serialize support for the property types used by current saves
- Auto import from `%LOCALAPPDATA%\FarFarWest\Saved\SaveGames`
- Timestamped backup creation before overwriting an existing save
- Save and Save As support
- Quick actions for common progression edits
- Dedicated tabs for `Overview`, `Inventory`, `Levels`, `Upgrades`, `Jokers`, `Rewards`, and `Other`

## What You Can Do

The editor is focused on the fields that matter most during normal save editing:

- Browse the loaded save by category instead of digging through raw binary data
- Change scalar values directly from the right-hand editor panel
- Auto-load the latest save from the default FarFarWest save folder
- Apply the built-in one-click actions for `Weapons 100`, `Spells 100`, `Prestige 10`, `Add Weapons`, and `Unlock Everything`

## Quick Start

### For players

1. Download the packaged release.
2. Keep all shipped files in the same folder.
3. Start `FarFarWest Unlock all tool.exe`.
4. Click `Auto Import` to load the newest save automatically, or use `Open Save`.
5. Make your edits.
6. Click `Save` to overwrite the current file, or `Save As` to write a copy.

Default save folder:

`%LOCALAPPDATA%\FarFarWest\Saved\SaveGames`

### Save safety

When you overwrite an existing file, the app first creates a timestamped backup with this pattern:

`<save-name>.backup_cpp_YYYYMMDD_HHMMSS.save`

The write itself goes through a temporary file and then replaces the original save, which reduces the chance of leaving a half-written file behind.

## Build From Source

### Requirements

- Windows x64
- `clang++` from the Windhawk compiler toolchain
- Microsoft Edge WebView2 Runtime installed on the target machine

The current `build.bat` is configured for this local compiler layout:

- `C:\Program Files\Windhawk\Compiler\bin\clang++.exe`
- `C:\Program Files\Windhawk\Compiler\x86_64-w64-mingw32\bin`

If your toolchain is installed elsewhere, update the paths at the top of [build.bat](build.bat).

### Build

```bat
build.bat
```

Build output is written to:

- `build/FarFarWest Unlock all tool.exe`
- `build/WebView2Loader.dll`
- `build/libc++.dll`
- `build/libunwind.dll`
- `build/libwinpthread-1.dll`
- `build/ui/...`

### Package a release

```bat
package_release.bat
```

This creates:

- `release/FarFarWest Unlock all tool/`
- `release/FarFarWest Unlock all tool.zip`

## Smoke Test

The executable supports a simple round-trip smoke test for a specific save file:

```bat
build\FarFarWest Unlock all tool.exe --smoke-test "C:\path\to\your.save"
```

The smoke test verifies that the app can:

1. derive the save key,
2. decrypt the file,
3. parse the GVAS payload,
4. serialize it again,
5. re-encrypt the result.

It is useful for validating parser compatibility after code changes.

## Project Layout

```text
.
|-- assets/
|   `-- ui/                 # HTML/CSS/JS frontend hosted inside WebView2
|-- docs/
|   `-- images/             # README assets
|-- src/
|   `-- main.cpp            # Win32 app, save parsing, crypto, UI bridge
|-- third_party_webview2/   # Bundled WebView2 SDK files
|-- build.bat
`-- package_release.bat
```

## Notes

- This is a Windows-only desktop application.
- The editor targets the property types currently used by FarFarWest saves. Future game updates may require parser changes.
- The packaged release is portable, but the runtime DLLs and `ui` folder must stay next to the executable.
- If auto import does not find a save, use `Save Folder` or `Open Save`.

## Disclaimer

Use the tool on copies or let the built-in backup system keep a recovery point. Editing save data always carries some risk, especially after game updates that change the file format.
