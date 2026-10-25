# Ocarina of Time 3D (TriAevum) setup

Development state: the world has no logic yet. Generated seeds test the
client only and may be unbeatable.

1. Build TriAevum with `-DTRIAEVUM_ENABLE_ARCHIPELAGO=ON`.
2. Start the game with the environment variable `TRIAEVUM_ARCHIPELAGO=1`.
3. Press F1, open **Game > Archipelago**, enter server, slot name and
   password, and select **Connect**.
4. Load a save file. Checks are sent from then on; progress is kept per save
   slot in `savedata/archipelago/`.
