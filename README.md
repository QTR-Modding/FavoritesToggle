# Favorites Toggle

Favorites Toggle turns a repeated favorite selection into an off action when
the selected item has one. Equipped inventory items are unequipped by exact
instance, and the currently selected Starborn Power is cleared. Consumables
keep their normal use behavior, and Quantum Essence keeps its native action.

## Features

- Supports all 12 vanilla favorite slots.
- Supports Favorites Menu Extended 5.0 slots and configured hotkeys.
- Works with favorite hotkeys, keyboard confirmation, mouse selection, and
  gamepad confirmation.
- Toggles inventory entries that Starfield marks equipped, including weapons,
  throwables, spacesuits, helmets, packs, and apparel.
- Lets an already selected Power reach Bethesda's downstream native unequip
  path; selecting a different Power still replaces it normally.
- Preserves Aid, food, chems, other consumables, Quantum Essence, and
  equip-state-locked inventory entries.
- Ships no plugin records, Papyrus scripts, menu files, or save data.

## Requirements

- Steam Starfield 1.16.244.0
- SFSE 0.2.21
- Address Library for SFSE Plugins v22
- Microsoft Visual C++ 2015-2022 Redistributable (x64)
- Optional: Favorites Menu Extended 5.0.0

## Installation

Install the release archive with a mod manager, enable it, and launch Starfield
through SFSE.

## Compatibility

Favorites Toggle is runtime- and signature-pinned to Starfield 1.16.244.0.
Favorites Menu Extended support is pinned to version 5.0.0. Unsupported hooks
fail closed and are reported in the SFSE log.

The plugin replaces no `favoritesmenu.swf`, so it creates no direct menu-file
conflict. Plugins that patch the same native Favorites routines or menu mods
that change Favorites event data may conflict.

## Building

Clone recursively, then build with Xmake 3.0.8 or newer, the Visual Studio C++
toolchain, and the Windows SDK:

```powershell
git clone --recurse-submodules https://github.com/QTR-Modding/FavoritesToggle.git
cd FavoritesToggle
xmake f -m release -a x64 -y
xmake -y
```

The DLL is written to:

```text
build/windows/x64/release/FavoritesToggle.dll
```

Set `COMMONLIBSF_PATH` to use another CommonLibSF checkout.

## License and source

Favorites Toggle is licensed under GPL-3.0-or-later with the exceptions in
`EXCEPTIONS`. Dependency revisions and corresponding-source details are in
`SOURCE.md`; third-party notices are in `THIRD_PARTY_NOTICES.md` and
`LICENSES/`.

Favorites Menu Extended is an optional compatibility target. No Favorites Menu
Extended code or assets are included.
