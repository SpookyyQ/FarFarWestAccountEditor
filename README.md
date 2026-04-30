# FarFarWest Unlock all tool

Native Windows rewrite of the FarFarWest save editor as a portable desktop `.exe`.



## Current Features

- Native Win32 desktop UI with a dark purple liquid-glass inspired look
- AES-256-CBC decrypt/encrypt for FarFarWest save files
- GVAS parsing and serialization for the property types used by current FarFarWest saves
- Auto-import from `%LOCALAPPDATA%\FarFarWest\Saved\SaveGames`
- Save and Save As with timestamped backups
- Editable tabs for `Overview`, `Inventory`, `Levels & Stats`, `Item Upgrades`, `Jokers`, `Rewards`, and `Other Fields`
- Bulk actions for weapon levels, spell levels, weapon prestige, missing buildable weapons, and `Unlock All`

## Distribution

No Python installation is required for end users. The current release format is a portable Windows app folder:

- `FarFarWest Unlock all tool.exe`
- `libc++.dll`
- `libunwind.dll`
- `libwinpthread-1.dll`
- `libc++.whl`
- `libunwind.whl`
- `README.txt`
