# Favorites Toggle

Select an equipped favorite again to unequip it. Weapons, armor, and the
selected Starborn Power can be toggled independently. Consumables and Quantum
Essence retain their normal behavior.

Favorites Toggle supports all 12 vanilla slots and Favorites Menu Extended
5.0, including keyboard, mouse, and gamepad input.

## Requirements

- Steam Starfield 1.16.244
- SFSE 0.2.21
- Address Library for SFSE Plugins v22
- Optional: Favorites Menu Extended 5.0.0

## Installation

Install the release archive with a mod manager and launch Starfield through
SFSE.

## Configuration

`Data/SFSE/Plugins/FavoritesToggle.ini` contains three default-on settings:

```ini
[Toggle]
Weapons=true
Armor=true
Powers=true
```

`FavoritesToggleCustom.ini` in the same folder can override the shipped file.
Changes require a game restart.

## Building

Clone recursively and generate the Visual Studio solution once:

```powershell
xmake project -k vsxmake -m "debug,release,releasedbg"
```

Open the generated `FavoritesToggle.sln` and build normally in Visual Studio.

## License

GPL-3.0-or-later with the exceptions in [EXCEPTIONS](EXCEPTIONS).
