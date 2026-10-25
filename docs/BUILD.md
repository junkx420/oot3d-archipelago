# Building TriAevum with Archipelago

1. Apply this project to a TriAevum checkout:

   ```powershell
   tools/setup.ps1 -TriAevum C:\path\to\TriAevum
   ```

2. Build TriAevum as its own documentation describes, with clang-cl and
   Ninja, and add `-DTRIAEVUM_ENABLE_ARCHIPELAGO=ON`. The option fetches
   apclientpp, wswrap, asio and WebSocket++ by pinned SHA-256 and takes
   OpenSSL and zlib from vcpkg. The configure steps that worked on the
   development machine, including the playable whole-AOT developer build,
   are in `overlay/docs/archipelago/RECON.md`, section 1.

3. Start the game with `TRIAEVUM_ARCHIPELAGO=1` in the environment. The
   client is under F1 > Game > Archipelago.

Tests without a ROM: the CMake target `triaevum_archipelago_tests`. The
apworld's tests run in an Archipelago checkout with the world linked or
copied to `worlds/oot3d`:

```powershell
python -m pytest worlds/oot3d
```
