# Changelog

## 0.1.0 - Unreleased

Initial release.

- Enables Bethesda's existing exact-inventory-instance unequip branch for the
  12 vanilla favorite slots.
- Adds repeat-selection unequip for FME 5.0 inventory entries that Starfield
  actually marks equipped. Targeted vanilla categories are guns, melee
  weapons, grenades and mines, spacesuits, helmets, packs, and apparel.
- Lets an already selected Power reach Bethesda's downstream native unequip
  path in vanilla slots and provides the same result in FME 5.0 slots.
- Routes Quantum Essence in FME slots through Bethesda's native action handler
  instead of treating it as a selectable Power.
- Leaves Aid and other non-equipped consumables on their original repeated-use
  or refusal paths and honors the equip-state-locked inventory flag.
- Supports vanilla keyboard keys, FME configured hotkeys, mouse menu selection,
  and gamepad menu confirmation.
- Adds default-on INI toggles for weapons, armor, and Powers.
- Adds no plugin records, Papyrus scripts, menu replacement, or save data.
- Supports Steam Starfield 1.16.244.0 only; FME integration is signature-pinned
  to 5.0.0. Arbitrary mod-added form behavior and unrelated Favorites-menu
  overhauls are not claimed compatible.
