# Ocarina of Time 3D (TriAevum) setup

Development state: the logic is ported from the Ship of Harkinian apworld
and checked against OoT3D_Randomizer, but only the options the TriAevum
client supports can be generated.

1. Build TriAevum with `-DTRIAEVUM_ENABLE_ARCHIPELAGO=ON`.
2. Start the game with the environment variable `TRIAEVUM_ARCHIPELAGO=1`.
   The title and file selection then show an Archipelago badge.
3. Press F1, open **Game > Archipelago**, enter server, slot name and
   password, and select **Connect**.
4. Load a save file. Checks are sent from then on; progress is kept per save
   slot in `savedata/archipelago/`.

## Universal Tracker

The world supports [Universal Tracker](https://github.com/FarrakKilhn/UniversalTracker),
which shows the locations that are in logic. It needs no player yaml: it
rebuilds the world from the seed's slot data.

1. Put this world's `.apworld` and Universal Tracker's `.apworld` into the
   Archipelago launcher's `custom_worlds` folder.
2. Open **Universal Tracker** from the launcher and connect it to the same
   server and slot as the game (it connects beside the game, which stays
   connected too).
3. Locations in logic are listed as items arrive from the server.
