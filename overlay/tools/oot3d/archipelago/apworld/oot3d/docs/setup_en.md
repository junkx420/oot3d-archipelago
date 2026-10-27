# Ocarina of Time 3D (TriAevum) setup

Alpha. The logic is ported from the Ship of Harkinian apworld and checked
against OoT3D_Randomizer. Only the options the TriAevum client supports can
be generated: the game's own item locations (chests, NPC gifts, heart pieces
and the like), with dungeon keys, maps and compasses in their own dungeon,
plus world settings the client sets in the save (Jabu-Jabu and Zora's
Fountain open, the rainbow bridge open, fewer of Ganon's trials, the forest,
the Kakariko gate and the Door of Time open) and items
to start with.

## What you need

- Your own Ocarina of Time 3D ROM (EUR). It is never distributed.
- TriAevum built from source with the Archipelago module
  (`-DTRIAEVUM_ENABLE_ARCHIPELAGO=ON`); the project's `docs/BUILD.md` says
  how. No game build can be distributed, because it contains code compiled
  from the ROM.
- This world's `oot3d.apworld` (from the project's GitHub releases) in the
  Archipelago launcher: **Install APWorld** in the launcher, or copy it into
  the `custom_worlds` folder (Archipelago 0.6.7 or newer). The launcher's
  Options Creator then lists "Ocarina of Time 3D" with the options the client
  supports: dungeon keys, maps, compasses and the Kokiri Sword; Zora's
  Fountain, Jabu-Jabu, the rainbow bridge, Ganon's trials, the forest, the
  Kakariko gate and the Door of Time; Gold Skulltula tokens, Deku scrubs, cows and shops
  shuffled; starting with
  the Kokiri Sword, a Deku Shield, an ocarina or magic beans; the item pool,
  DeathLink and Archipelago's own options.

## Playing

1. Start the game, press F1, open **Game > Archipelago** and turn on
   **Archipelago enabled**. Restart the game.
2. In the same tab enter server, slot name and password, and select
   **Connect**. The title and file selection show an Archipelago badge with
   the connection state.
3. Load a save file. Checks are sent from then on; progress is kept per save
   slot in `savedata/archipelago/`. The server stays the source of items:
   received items arrive again on a new file.

With **Connect automatically at start** (on by default) the game connects to
the saved server and slot by itself; the password is not saved. A lost
connection is reopened automatically. **In-game messages** show items sent
and received, DeathLink deaths, the goal and the connection.

**Text boxes** (same tab): *Instant* fills every text box at once;
*Instant, hold B to skip* also advances and closes text boxes while B (the
sword button, whichever controller button it is mapped to) is held. Off by
default.

**Skip cutscenes** (same tab) skips story and boss cutscenes and first
conversations. Connected to a save, it also skips every dungeon's blue warp
cutscene: the warp takes Link straight to the dungeon's entrance or warp pad,
and the reward comes from the server (Jabu-Jabu's sapphire from the game).

`TRIAEVUM_ARCHIPELAGO=1` (or `0`) still overrides the switch.

## Universal Tracker

The world supports [Universal Tracker](https://github.com/FarisTheAncient/Archipelago/releases),
which shows the locations that are in logic. It needs no player yaml: it
rebuilds the world from the seed's slot data. It shows no "glitched"
locations: Ship of Harkinian's N64 tricks are not checked for 3D.

1. Put this world's `.apworld` and Universal Tracker's `tracker.apworld` into
   the Archipelago launcher's `custom_worlds` folder.
2. Open **Universal Tracker** from the launcher and connect it to the same
   server and slot as the game (it connects beside the game, which stays
   connected too).
