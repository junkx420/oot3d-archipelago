# Ocarina of Time 3D (TriAevum) setup

The logic is ported from the Ship of Harkinian apworld and checked against
OoT3D_Randomizer. Only the options the TriAevum client supports can
be generated: the game's own item locations (chests, NPC gifts, heart pieces
and the like), with dungeon keys, maps and compasses in their own dungeon,
plus world settings the client sets in the save (Jabu-Jabu and Zora's
Fountain open, the rainbow bridge open, fewer of Ganon's trials, the forest,
the Kakariko gate and the Door of Time open) and items
to start with.

## What you need

- Your own Ocarina of Time 3D ROM (EUR). It is never distributed.
- TriAevum v0.8.0-beta.1.2 for Windows, prepared by its TriAevumForge from
  that ROM, and the `TriAevum-Archipelago-...-Windows-x64.zip` of this
  project's GitHub release: its `TriAevum.exe` (TriAevum's runtime with the
  Archipelago client) replaces the installed one, as its
  `README-ARCHIPELAGO.txt` describes. The game's code stays TriAevum's own,
  made from your ROM. Building TriAevum from source with
  `-DTRIAEVUM_ENABLE_ARCHIPELAGO=ON` works too (the project's
  `docs/BUILD.md`).
- This world's `oot3d.apworld` (from the project's GitHub releases) in the
  Archipelago launcher: **Install APWorld** in the launcher, or copy it into
  the `custom_worlds` folder (Archipelago 0.6.7 or newer). The launcher's
  Options Creator then lists "Ocarina of Time 3D" with the options the client
  supports: dungeon keys, maps, compasses and the Kokiri Sword; Zora's
  Fountain, Jabu-Jabu, the rainbow bridge, Ganon's trials, the forest, the
  Kakariko gate and the Door of Time; Gold Skulltula tokens, Deku scrubs, cows, shops, ocarinas, the Weird Egg and
  the Gerudo card; rainbow bridge conditions, Triforce hunt, starting hearts;
  logical tricks by their OoT3D Randomizer names, each checked against its logic;
  glitched logic (Logic Rules, in the Glitches group) with the OoT3D
  Randomizer's glitch categories, their difficulties and its single glitches,
  checked the same way; starting with
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
the saved server and slot by itself. The password is saved only with
**Remember password** (off by default; it is then kept as plain text in
`archipelago.json`). A lost connection is reopened automatically.
**In-game messages** show items sent and received, DeathLink deaths, the
goal and the connection. The panel's console shows the server's messages,
and its text field sends chat and commands such as `!hint`.

A location that holds another player's item shows a recovery heart; its text
box says what it was and for whom ("You found Hookshot for Alice!"). The
item itself reaches its owner through the server.

**Text boxes** (same tab): *Instant* fills every text box at once;
*Instant, hold B to skip* also advances and closes text boxes while B (the
sword button, whichever controller button it is mapped to) is held. Off by
default.

**Skip cutscenes** (same tab) skips story and boss cutscenes and first
conversations. Connected to a save, it also skips every dungeon's blue warp
cutscene: the warp takes Link straight to the dungeon's entrance or warp pad,
and the reward comes from the server (Jabu-Jabu's sapphire from the game).

**Fast chests** (same tab, off by default): every chest opens as a small one
does, without the slow opening, the light and the fanfare, as OoT3D
Randomizer's "always fast" chest animations.

**Song replays** (same tab, off by default): *Skip* does not play a song
back after Link plays it, as OoT3D Randomizer's "Skip Song Replays"; *Skip,
no sound* skips the replay's music too (not for the Scarecrow's Song, whose
music brings Pierre). The Song of Time and warp blocks do not wait for the
skipped replay. The song's light effects still play out.

**Damage taken** (same tab, x1 by default): x1/2 to x16 or one hit KO, as
OoT3D Randomizer's damage multiplier. Healing is not changed, and Master
Quest's double damage stays. The seed's logic assumes x1.

**Cheats** (same tab, off by default): infinite health, magic, ammo and
rupees, as in Ship of Harkinian. They keep what Link already has full, up to
his capacity: no magic before the meter, no arrows before the bow. A hit
bigger than all hearts and a DeathLink death still kill Link. They change
nothing the seed or the server sees.

`TRIAEVUM_ARCHIPELAGO=1` (or `0`) still overrides the switch.

## Universal Tracker

The world supports [Universal Tracker](https://github.com/FarisTheAncient/Archipelago/releases),
which shows the locations that are in logic. It needs no player yaml: it
rebuilds the world from the seed's slot data, including the seed's tricks
and glitches. It shows no "glitched" locations beyond those: Ship of
Harkinian's N64 tricks are not checked for 3D.

1. Put this world's `.apworld` and Universal Tracker's `tracker.apworld` into
   the Archipelago launcher's `custom_worlds` folder.
2. Open **Universal Tracker** from the launcher and connect it to the same
   server and slot as the game (it connects beside the game, which stays
   connected too).
