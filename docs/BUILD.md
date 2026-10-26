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

3. Start the game, press F1, open Game > Archipelago, turn on
   "Archipelago enabled" and restart. Connect from the same tab; the game
   then connects to the saved server and slot at every start. Setting
   `TRIAEVUM_ARCHIPELAGO=1` (or `0`) in the environment overrides the switch.
   Player guide: `overlay/tools/oot3d/archipelago/apworld/oot3d/docs/setup_en.md`.

The apworld for the Archipelago launcher (`custom_worlds`) is built with

```powershell
python tools/build_apworld.py C:\path\to\TriAevum oot3d.apworld
```

Tests without a ROM: the CMake target `triaevum_archipelago_tests`. The
apworld's tests run in an Archipelago checkout with the world linked or
copied to `worlds/oot3d`:

```powershell
python -m pytest worlds/oot3d
```

## Release runtime (the replacement TriAevum.exe)

The released `TriAevum.exe` is TriAevum's public runtime with the module on:
the title code stays in TriAevum's `triaevum_title_aot.dll`, which Forge
activates from the user's ROM. The client's hooks are runtime data (the
generated title code has an observed variant of every function), so the
unchanged title module works with it. Configure as TriAevum's release does,
plus the option:

```
-G Ninja -DCMAKE_BUILD_TYPE=Release
-DOOT3D_WHOLE_AOT_PRODUCT_MODE=ON -DOOT3D_DIRECT_AOT_PLUGIN=ON
-DOOT3D_ENABLE_VULKAN_RENDERER=ON -DTHREE_DS_RECOMP_ENABLE_SSSR=ON
-DTRIAEVUM_DEVELOPER_TOOLS=OFF -DTHREE_DS_RECOMP_OFFER_EXPERIMENTAL_GI=OFF
-DTRIAEVUM_ENABLE_ARCHIPELAGO=ON
```

with the same toolchain and vcpkg settings as the developer build, inside a
Visual Studio x64 developer environment, and build the target
`triaevum_public_runtime`. The package holds `TriAevum.exe`,
`resources/archipelago/{locations,items}.json` (from
`runtime/triaevum_resources/archipelago/`) and the licence notices of
TriAevum, apclientpp, wswrap, WebSocket++, asio, OpenSSL and zlib. It must
not hold `triaevum_title_aot.dll`, the iMMERSE or DLSS modules, or anything
from the ROM; those come from the user's TriAevum installation.
