# OoT3D Archipelago

[Archipelago](https://archipelago.gg) multiworld support for
[TriAevum](https://github.com/coccofresco/TriAevum), the native
recompilation of *The Legend of Zelda: Ocarina of Time 3D*.

The current state is on the
[status page](https://junkx420.github.io/oot3d-archipelago/).

## Download

Each [release](https://github.com/junkx420/oot3d-archipelago/releases) has:

- `oot3d.apworld`, the world "Ocarina of Time 3D" for the Archipelago
  launcher (0.6.7 or newer);
- `TriAevum-Archipelago-<version>-Windows-x64.zip`, a replacement
  `TriAevum.exe` with the Archipelago client for an installed TriAevum
  v0.8.0-beta.1.2 that Forge has prepared from your own ROM (EUR). It holds
  TriAevum's runtime only: the game's code still comes from TriAevum's own
  release and your ROM. Its `README-ARCHIPELAGO.txt` says how to install it.

The setup guide is the apworld's `docs/setup_en.md`
([here](overlay/tools/oot3d/archipelago/apworld/oot3d/docs/setup_en.md)).
Building from source works too ([docs/BUILD.md](docs/BUILD.md)).

## What works

- The game's own item locations send checks and give multiworld items: your
  own with their real model, another player's as a recovery heart whose text
  says what it was and for whom. Received items arrive in the game.
- Shuffles: songs, dungeon rewards, Gold Skulltula tokens, Deku scrubs, cows, shop slots, ocarinas,
  the Weird Egg, the Gerudo card; dungeon keys, maps and compasses in their
  dungeon.
- Logic checked against OoT3D Randomizer, with its tricks and glitched logic;
  world options (open forest, Kakariko gate, Door of Time, Zora's Fountain,
  Jabu-Jabu, rainbow bridge conditions, Ganon's trials, Triforce hunt,
  starting items and hearts).
- The goal (Ganon), DeathLink, auto-connect and reconnect, in-game messages,
  the server's chat and commands, Universal Tracker.
- Client options: cutscene and blue warp skips, quick text, fast chests,
  song replay skip, a damage multiplier, and cheats (infinite health, magic,
  ammo, rupees).

What is not there yet: Master Quest,
entrance shuffle, models for other games' items. Known issues are in
[KNOWN_ISSUES.md](overlay/docs/archipelago/KNOWN_ISSUES.md).

## What is here

| Path | Content |
| --- | --- |
| `overlay/` | the files this project adds to TriAevum, at their TriAevum paths |
| `overlay/tools/oot3d/archipelago/` | the in-game client (check detection, network, F1 panel, save sidecar) and its tests |
| `overlay/tools/oot3d/archipelago/apworld/oot3d/` | the Archipelago world "Ocarina of Time 3D" |
| `overlay/tools/oot3d/archipelago/recon/` | table generators and the logic comparison against OoT3D_Randomizer |
| `overlay/docs/archipelago/` | reconnaissance, architecture, progress, known issues |
| `patches/triaevum.patch` | the changes to files TriAevum already has (CMake option, window hooks) |
| `UPSTREAM` | the TriAevum commit the overlay and patch apply to |
| `docs/` | the status page (GitHub Pages) |

This is a separate project, not a fork: it carries only its own work. No
ROMs, game code, extracted assets or TriAevum sources are in it. You need
your own copy of the game.

## Building from source

```powershell
tools/setup.ps1 -TriAevum C:\path\to\TriAevum
```

clones TriAevum if needed, checks out the commit in `UPSTREAM` on a new
branch, copies `overlay/` in and applies the patch. Then build TriAevum with
`-DTRIAEVUM_ENABLE_ARCHIPELAGO=ON` ([docs/BUILD.md](docs/BUILD.md)). With the
option OFF the build is TriAevum unchanged; with it ON and the client not
connected the game is unchanged.

Development happens on a TriAevum branch; `tools/export.ps1 -TriAevum <checkout>`
refreshes `overlay/` and the patch from it.

## License

GPL-3.0-or-later, like TriAevum (`LICENSE`). Parts derived from other
projects keep their notices: the apworld comes from Ship of Harkinian's
Archipelago world (MIT, `overlay/tools/oot3d/archipelago/apworld/oot3d/LICENSE`),
and the location tables are generated from
[OoT3D_Randomizer](https://github.com/gamestabled/OoT3D_Randomizer), whose
license text carries OoT-Randomizer's MIT License (both in the apworld, with
`NOTICE.md`). The
full list is in TriAevum's `THIRD_PARTY_NOTICES.md` as patched by
`patches/triaevum.patch`.

Not affiliated with Nintendo, Archipelago, TriAevum, Ship of Harkinian or
OoT3D_Randomizer.
