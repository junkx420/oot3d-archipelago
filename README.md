# OoT3D Archipelago

[Archipelago](https://archipelago.gg) multiworld support for
[TriAevum](https://github.com/coccofresco/TriAevum), the native
recompilation of *The Legend of Zelda: Ocarina of Time 3D*.

**Alpha.** The current state is on the
[status page](https://junkx420.github.io/oot3d-archipelago/); releases carry
`oot3d.apworld` for the Archipelago launcher. Playing needs TriAevum built
from source with your own ROM: a game build cannot be distributed, because it
contains code compiled from the ROM.

What works in the alpha: the game's own item locations send checks and give
multiworld items (your own with their real model), received items arrive in
the game, the goal (Ganon), DeathLink, optional cutscene skips, auto-connect
and reconnect, in-game messages, and Universal Tracker.

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

## Use

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
