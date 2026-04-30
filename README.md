# FarFarWest Unlock Tool

Portable Windows desktop app for opening, editing, and saving **FarFarWest** save files (`.save`).  
You do **not** need Python or the command line: open a save, change values, save it again.

![FarFarWest Unlock Tool Screenshot](docs/images/app-screenshot.svg)

> Preview of the current desktop layout.

## What the program does

This tool opens your FarFarWest save, decrypts it locally on your PC, shows the important values in an editable interface, and then saves the file back in the correct format.

With it, you can for example:

- load saves directly from the default save folder
- edit values such as inventory, levels, and upgrades
- inspect and change joker and reward entries
- apply several progression changes with one click
- save the edited result back as a `.save` file

## Features

- Portable Windows `.exe`
- Automatic import of the newest save from `%LOCALAPPDATA%\FarFarWest\Saved\SaveGames`
- `Open Save`, `Save`, and `Save As`
- Automatic backup before overwriting an existing file
- Direct editing of individual values in the interface
- Clean tab-based layout instead of raw data or a hex view
- Quick actions for common progression changes

## Quick actions

The top area of the app includes built-in actions for common edits:

- `Weapons 100`: sets weapon levels to 100
- `Spells 100`: sets spell levels to 100
- `Prestige 10`: sets weapon prestige to 10
- `Add Weapons`: adds missing buildable weapons to the inventory
- `Unlock Everything`: combines several progression edits, including weapons, prestige, hero level, and currencies

## Interface

The app is split into three clear areas:

### Left side

- navigation tabs: `Overview`, `Inventory`, `Levels`, `Upgrades`, `Jokers`, `Rewards`, and `Other`
- file actions such as `Open Save`, `Auto Import`, `Save Folder`, `Save`, and `Save As`

### Center

- list of fields from the currently selected tab
- quick overview of the visible values
- depending on the tab, for example inventory entries, levels, upgrades, or rewards

### Right side

- detail view for the selected field
- current value and field type
- input field for a new value
- `Apply Value` to update the loaded save in memory first

Important: `Apply Value` changes the loaded save inside the app.  
Only `Save` or `Save As` writes the file to disk.

## What you can edit in each tab

- `Overview`: general important profile values
- `Inventory`: items and amounts
- `Levels`: progression and item levels
- `Upgrades`: upgrade values for specific weapons or entries
- `Jokers`: joker-related values and lists
- `Rewards`: saved reward entries
- `Other`: additional editable fields that do not fit the other categories

## Quick start

1. Download and extract the release.
2. Keep all included files together in the same folder.
3. Start `FarFarWest Unlock all tool.exe`.
4. Click `Auto Import` to load the newest save automatically.
5. Or use `Open Save` to choose a specific `.save` file.
6. Select a value, edit it on the right side, and click `Apply Value`.
7. Use `Save` to overwrite the current file or `Save As` to create a copy.

Default save folder:

`%LOCALAPPDATA%\FarFarWest\Saved\SaveGames`

## Save safety

When you overwrite an existing file, the tool first creates an automatic timestamped backup:

`<save-name>.backup_cpp_YYYYMMDD_HHMMSS.save`

It then writes through a temporary file and replaces the original only at the end. This reduces the risk of leaving behind a broken or half-written save.

## Notes

- Windows only
- The app works locally on your PC
- The included files next to the `.exe` must stay together
- If `Auto Import` does not find a save, use `Save Folder` or `Open Save`
- After game updates, the save format may change and some fields may need tool updates

## For developers

The source code is in this repository and the app is built as a native Win32 application with WebView2. If you want to build it yourself, see [build.bat](build.bat) and [package_release.bat](package_release.bat).

Build output:

- `build/FarFarWest Unlock all tool.exe`
- `release/FarFarWest Unlock all tool.zip`
