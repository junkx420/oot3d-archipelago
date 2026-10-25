# Archipelago for TriAevum: Phase 0 reconnaissance

Date: 2026-10-25. Branch `feature/archipelago`, based on TriAevum
`8f69a26f390bf3a500bf3149b6596c19724a5b5d`.

Status words used below:

- **Verified**: checked against code or data on this machine; the evidence is named.
- **From source**: read from source code, not exercised at runtime.
- **Open**: not verified. Work that depends on it must not assume it.

## Inputs on this machine

| Input | Identity |
| --- | --- |
| ROM | Europe Rev 1, No-Intro 1168, complete SHA-1 `43dc87494690361fab8846b5aa2946fb009470f4`, product code `CTR-P-AQEP` |
| Extracted `code.bin` | 4,567,040 bytes, SHA-256 `16a6b0aa4c4784680220a6f780f7f8a73cfb205557aa9f9f0e705179e0613220` |
| Extracted ExHeader | SHA-256 `a584b067427f81edcca739497ec7b4ce1217eb9612090e6ae5b846e5674d12c1` (recipe `oot3d-eur-catalogue-a584b067`) |
| Extracted RomFS | SHA-256 `a3ad05dcf12893de5f1617a60f4e80f54f5e1c80069922a96a4f9c8c8cbac8ca` |

The files were extracted with TriAevum's own reader
(`tools/triaevum_release/ctr_rom.py`, `extract_decrypted_rom`) into
`../private/extract`, outside the repository. Nothing extracted is tracked.

Reference checkouts, outside the repository in `../refs`:

| Repository | Commit |
| --- | --- |
| gamestabled/OoT3D_Randomizer | `ae0fcf56cbc7166c809f675668b43e24a9640fe1` |
| ArchipelagoMW/Archipelago | `fcff008e942dda58e7acbf9b4e97be712faa435e` |
| HarbourMasters/Archipelago-SoH `oot-soh` | `84c059f24c5148250ce8fcb84cd8c91ce9d814ce` (world 1.4.2) |
| aMannus/Shipwright `archipelago` (sparse) | `618cac24354471c547a6c6d9d761bbbc696c5e36` |
| black-sliver/apclientpp | `6b28cc1781006deae0e28c61752d11a83819585a` |
| black-sliver/wswrap | `aeba7ac428028723fb26ce92488f260660f786b1` |

## 1. Local build

Toolchain installed on 2026-10-25 with winget, with the user's approval:
Python 3.12.10, CMake (Kitware), Ninja 1.13.2, LLVM 22.1.6 (the version the
scripts pin), Visual Studio 2022 Community 17.14 with the C++ workload,
its bundled CMake and Windows SDK 10.0.26100.

Git Bash cannot start processes on this machine
(`dofork: child -1 - forked process ... died unexpectedly, exit code 0xC0000142`).
This also breaks `git submodule`. All commands below use PowerShell and the
native `git.exe`. `runtime/three_ds_recomp` is vendored as regular files
(1,220 tracked files, no gitlink), so no submodule update is needed.

Build variants found in the source:

| Variant | Defines | Needs | Use for AP |
| --- | --- | --- | --- |
| Developer runtime, no whole-AOT | none | vcpkg packages | Interpreter / compiled-function paths. Slow ("substantially slower fallback paths", `oot3d_native_a32_window_setup.cpp:392`). Good for hook bring-up. |
| Developer runtime + generated whole-AOT | `OOT3D_WHOLE_AOT_CODE_BIN`, generated shards | `build-codex/oot3d_whole_aot/aot_program.json`, `generate_whole_aot_cpp.py` | Playable speed with developer switches. |
| Whole-AOT product (`OOT3D_WHOLE_AOT_PRODUCT_MODE`) | product manifest, `prepare_whole_aot_product.py` | `scripts/oot3d/Build-Oot3dWholeAotProduct.ps1` then `Configure-Oot3dWholeAotProduct.ps1` | Release behaviour. |
| Public runtime + private title plugin (`OOT3D_DIRECT_AOT_PLUGIN`) | target `triaevum_public_runtime` | Forge builds `triaevum_title_aot.dll` locally | What players run. |

Configure command started for the developer runtime:

```powershell
cmake -S . -B build/msvc -G "Visual Studio 17 2022" -A x64
```

What it took to configure:

1. vcpkg bootstrapped itself into `build/msvc/vcpkg` and built all 20
   packages (`x64-windows-static`).
2. FetchContent failed on ImGui: its git step runs `git submodule`, a shell
   script, and hits the fork crash above. Workaround: clone the six
   FetchContent dependencies with native git into `build/deps-src`, apply
   `imgui-fixes-and-config.patch` by hand, and pass
   `-DFETCHCONTENT_SOURCE_DIR_<NAME>=<dir>` for IMGUI, LIBGFXD, THREADPOOL,
   PRISM, MONOCYPHER and OOT3D_CACAO.
3. Configure then stopped at `runtime/three_ds_recomp/CMakeLists.txt:134`:
   `Could NOT find Vulkan`. Vulkan SDK 1.4.363.0 installed with winget
   (approved), at `C:\VulkanSDK\1.4.363.0`.
4. NRI, zstd and googletest are also git FetchContent dependencies; they are
   cloned the same way. NRI uses TriAevum's own switch
   `THREE_DS_RECOMP_NRI_SOURCE_DIR`, which also applies TriAevum's NRI patches.
5. With MSVC, `#embed` is missing, so configure takes the `.inc` fallback in
   `spirv_pack.cmake`, which fails on this path with a space
   (`CMake Error: Not a file: C:/Users/Apex/Documents/OOT3D Archi/TriAevum`).
   The project's intended compiler is clang-cl (`cmake/Oot3dLlvmClToolchain.cmake`),
   which has `#embed`. **Configure with clang-cl and Ninja succeeds.**

Working developer configure (run inside `Launch-VsDevShell.ps1 -Arch amd64`,
with `VULKAN_SDK` set and `OOT3D_LLVM_ROOT=C:\Program Files\LLVM`):

```powershell
cmake -S . -B build/llvm -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_TOOLCHAIN_FILE=build/msvc/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=cmake/Oot3dLlvmClToolchain.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static -DVCPKG_HOST_TRIPLET=x64-windows-static `
  -DOOT3D_LLVM_ROOT="C:/Program Files/LLVM" `
  -DTHREE_DS_RECOMP_ENABLE_DX11=OFF -DOOT3D_FAST_DEV_LINK=ON `
  -DTHREE_DS_RECOMP_FAST_DEV_LINK=ON -DAUTOMATE_VCPKG_UPDATE=OFF `
  -DTHREE_DS_RECOMP_NRI_SOURCE_DIR=build/deps-src/nri `
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=build/deps-src/imgui `
  ... (same for LIBGFXD, THREADPOOL, PRISM, MONOCYPHER, OOT3D_CACAO, ZSTD, GOOGLETEST)
cmake --build build/llvm --target oot3d_native_a32_process_tests
```

This tree first had no whole-AOT backend (no generated shards), so it was the
"developer runtime, no whole-AOT" variant.

### Developer runtime with whole-AOT (2026-10-25, works)

1. `tools/oot3d/operational_inputs.json` points `oot3dExtractRoot` at the
   original author's `E:\` drive. A local, uncommitted edit (marked
   `git update-index --skip-worktree`) points it at a private copy laid out as
   `exefs\code.bin` and `ncch0\exheader.bin`.
2. Python dependencies in a venv outside the repository
   (`tools/triaevum_release/requirements-tests.txt`: capstone 5.0.7,
   numpy 2.3.4, Pillow 10.4.0).
3. Program and C++ generation:

```powershell
python tools/oot3d/native_a32_runtime/generate_whole_aot.py `
  --output build/whole-aot/aot_program.json `
  --inventory-output build/whole-aot/inventory_with_process_entry.csv
python tools/oot3d/native_a32_runtime/generate_whole_aot_cpp.py `
  --program build/whole-aot/aot_program.json `
  --selection tools/oot3d/native_a32_runtime/whole_aot_functions.json `
  --code <private code.bin> --output build/llvm/oot3d_whole_aot_cpp `
  --shards 256 --shard-strategy affinity
```

   The program matches the product contract exactly: 12,442 functions,
   162,766 blocks, 924,535 instructions. `audit_whole_aot_product` reports a
   single error, "ExHeader SHA-256 does not match the product contract": the
   contract pins the legacy EUR ExHeader `9f065aec…`, this ROM has the
   catalogue ExHeader `a584b067…` (TriAevum documents the 512-byte difference:
   signature area and one flag byte). The generator reads only the entry
   point and text size from the ExHeader, which are equal.
4. Configure as above plus `-DOOT3D_WHOLE_AOT_GENERATED_DIR=<shards>`,
   `-DOOT3D_WHOLE_AOT_CODE_BIN=<code.bin>`, `-DOOT3D_REBUILD_WHOLE_AOT=ON`,
   `-DOOT3D_REQUIRE_WHOLE_AOT=ON`, `-DOOT3D_WHOLE_AOT_PRODUCT_MODE=OFF`,
   `-DOOT3D_WHOLE_AOT_MAX_PARALLEL_COMPILES=8`, then build `oot3d_native_game`
   (about 3 minutes on an i7-13700KF).
5. Title data: `python tools/triaevum_release/forge.py prepare --recipe
   oot3d-eur-catalogue-a584b067 --code … --exheader … --romfs … --output-root
   <private>` writes `process-manifest.json`. Launch with
   `--a32-process-manifest <it> --resource-root runtime/triaevum_resources`.
   The game boots and runs the title sequence.

`--disable-whole-aot` faults at boot in this tree ("boot reached an
unimplemented A32 fallback at pc=0x100000"): the other execution paths need
the generated A32 registry (`build-codex/oot3d_a32_generated`), which is not
generated here. Interpreter behaviour is covered by host tests instead.

## 2. Executed code image

**Verified: the EUR image is executed, for EUR and USA ROMs alike.**

- `tools/triaevum_release/input_coverage/recipes/adapters/oot3d-usa-ef210566-copies.json`
  declares `canonical.sha256 = 16a6b0aa…`: the USA adapter rebuilds the EUR
  `code.bin` from byte ranges of the USA input.
- `whole_aot_observed_sites.json`, `whole_aot_product_manifest.json` and
  `supported_revisions.json` bind the title module to `16a6b0aa…`.
- `docs/OOT3D_TOPSCREEN_MOD_SEMANTIC_PORT.md` names `16a6b0aa…` the
  "EUR original `code.bin`".
- This machine's EUR Rev 1 ROM extracts to exactly that hash. EUR uses a
  direct recipe; no adapter runs.

Consequence: every address must be an EUR address. OoT3D_Randomizer's USA
symbols are not usable as they are.

## 3. Hook mechanism

### How a block-entry callback runs

**From source.** `NativeA32Process::SetBlockEntryCallback`
(`oot3d_native_a32_process.cpp:857`) stores one callback and a sorted PC list.
In product mode `DispatchWholeAotProduct` (`oot3d_native_a32_process.cpp:27`)
passes both to `ExecuteOot3dCompiledFunction` for every dispatch.

Inside generated whole-AOT code the check runs at block starts
(`oot3d_native_whole_aot_runtime.h:208-257`, `Oot3dAotEnterBlock`):

1. A 4096-bit hash filter, then an exact binary search of the PC list.
2. On a hit: `state.Flush(0x7FFF, true)`, then the callback, then
   `state.Reload(0x7FFF, true)`.

So inside whole-AOT:

| Question | Answer |
| --- | --- |
| Can the callback read registers and guest memory? | Yes: full `GuestState` and `MemoryBus`. |
| Do r0–r14 and flags written by the callback survive? | **Yes**: the promoted registers are reloaded after the call. |
| Does writing `state.r[15]` redirect execution? | **No**, not inline: the generated block continues after `Reload`. |
| How is execution redirected then? | The callback throws an observable exit, `Oot3dWholeAotExitAt(pc, state)` (`oot3d_native_whole_aot_runtime.h:125`). The dispatcher restores the snapshot, sets `r15 = pc` and offers the PC to the native block callback (`productMod`) before compiled code. TopScreen and the object-bank extension already use this: hook, then `ResumePcs` / `NativeCallPcs`. |

### When a new PC must be added to the title module

**From source.** `oot3d_native_a32_window_setup.cpp:550-579` and `603-627`:
"The whole-AOT graph reports only its block starts: a hook or resume pc that
is not an entry of the title module would silently never run." Product builds
throw at startup when a required PC is missing from
`Oot3dWholeAotEntryPoints()`. `tools/oot3d/native_a32_runtime/whole_aot_observed_sites.json`
tells the generator to start a block at each listed PC.

| Hook PC | Needs a new title module? |
| --- | --- |
| Function entry (for example `Item_Give`, `Actor_OfferGetItem`) | **Expected no**: function entries are block starts. 363 of the mapped randomizer functions are entries of `whole_aot_functions.json` (section 4). Open until the experiment below runs. |
| Mid-function site (randomizer `.patch_*` sites) | **Yes**: add it to `whole_aot_observed_sites.json` and regenerate the module. Players' Forge regenerates it locally, so this is a release-process cost, not a blocker. |
| Redirect target / resume PC | **Yes**, same rule. |

Ordering detail: `blockEntryHooks` is sorted and deduplicated
(`oot3d_native_a32_window_setup.cpp:358-363`); `ProfileA32Blocks` or block
tracing replaces the list with an empty one, which means "observe every block".

### Execution modes

| Mode | Callback path | Status |
| --- | --- | --- |
| Interpreter (`Dispatch`, `a32_runtime.h:258`) | `block_entry` per block | From source |
| Compiled functions / mass AOT / true AOT | same PC list, `BuildOot3dMassAotPcFilter` | From source |
| Whole-AOT developer | `Oot3dAotEnterBlock` | From source |
| Whole-AOT product and direct plugin | same, plus `productMod` | From source |

### Minimal experiment: not yet run

The interpreter-path experiment is written as host test
`TestBlockEntryCallbackRegisterWritesAndIgnoredRedirect`
(`oot3d_native_a32_process_tests.cpp`): the callback at the entry block
writes `r2 = 0xBEEF` and `r15 = entry + 0x10`. Expected from source: the
first SVC is still the entry block's (redirect ignored) and `r2` keeps
`0xBEEF`. **Passed** on 2026-10-25 in `build/llvm`
(`oot3d_native_a32_process_tests: ok`; 2 generated true-AOT cases skipped
because the generated A32 AOT inputs are absent).

The interpreter source agrees: `Dispatch` (`a32_runtime.cpp:856-881`) calls
the callback, then looks up the block with its local `pc`, not `state.r[15]`.

### Whole-AOT experiment: done (2026-10-25)

`Math_SinF` (`0x003727F0`, named so by TriAevum's own generator and in no
TriAevum hook list) was added as a probe PC through
`TRIAEVUM_ARCHIPELAGO_PROBE`, with the title module unchanged. Over 2,685
guest frames of the title sequence under whole-AOT (579,265 whole-AOT
calls), the callback fired **3,584 times**. The generated code explains it:
every copy of a function (affinity sharding duplicates hot functions into
about 100 shards) starts with `Oot3dAotEnterBlock(context, frame, state,
<entry pc>, …)`, which consults the runtime PC list.

Conclusion: a function-entry hook needs no title-module rebuild in the
developer whole-AOT build. The product's `ProductTopScreenBlocks` startup
check applies only to TopScreen and object-bank sites; mid-function sites
still need `whole_aot_observed_sites.json`. The direct-plugin product path
was not exercised.

## 4. Address mapping

**Verified offline, with the script
`tools/oot3d/archipelago/recon/map_rando_addresses.py`.** Output:
`tools/oot3d/archipelago/addresses.json` (920 entries; addresses, offsets and
SHA-256 hashes only, no code bytes).

Method: OoT3D_Randomizer's `code/oot.ld` gives each vanilla symbol and patch
site a USA address and piecewise EUR offsets (`_LD_OFF = _LD_EUR ? X : _LD_OFF`).
The randomizer ships EUR builds with these values. For each entry the script:

1. computes `exec_addr = usa_ref + eur_offset`;
2. checks it against TriAevum's own, independently discovered
   `whole_aot_functions.json` (12,419 entries over about 900,000 possible
   word addresses in the text section, so a random hit is about 1.4 %);
3. checks for an ARM prologue (PUSH/STMFD with LR, STR LR pre-index, SUB SP);
4. stores the SHA-256 of the 32 bytes at the address, for a startup self-check.

| Kind | Count | Status |
| --- | ---: | --- |
| Text symbols, entry of TriAevum's function list | 363 | `signature_pinned` |
| Text symbols, not a function entry | 17 | `needs_runtime_check` (mostly `ret_*` return addresses, which are mid-function by design) |
| Patch sites (mid-function) | 470 | `image_pinned_only`: hash pins them to this image, but no second source confirms the mapping |
| Data symbols and data patch sites | 70 | `needs_runtime_check` |

363 of 380 function symbols matching an independently discovered entry is
strong evidence that the EUR offsets are right for the text section.

AP-relevant entries:

| Name | USA | EUR / exec | Evidence |
| --- | --- | --- | --- |
| `Item_Give` | `0x00376A78` | `0x00376A78` | AOT entry, prologue |
| `Actor_OfferGetItem` | `0x003724DC` | `0x003724DC` | AOT entry, prologue |
| `Flags_GetSwitch` / `Flags_SetSwitch` | `0x0036E864` / `0x00375C10` | same | AOT entry |
| `Flags_GetCollectible` / `Flags_SetCollectible` | `0x0036405C` / `0x003329D8` | same | AOT entry |
| `Flags_GetClear` | `0x0036CF6C` | same | AOT entry |
| `Health_ChangeBy` | `0x00352DBC` | same | AOT entry, prologue |
| `Player_SetupGetItemOrHoldBehavior` | `0x00354894` | same | AOT entry, prologue |
| `Player_InCsMode` / `Player_InBlockingCsMode` | `0x0036A7A0` / `0x0035DB20` | same | AOT entry |
| `Message_GetState` | `0x003769D8` | same | AOT entry |
| `Actor_Spawn` | `0x003738D0` | same | AOT entry, prologue |
| `.patch_OverrideItemAndTextID` | `0x002BC1C8` | same | image pinned only |
| `.patch_ActorGetItemOffer` | `0x003725AC` | same | image pinned only |
| `.patch_SaveFile_Init` | `0x00449A04` | `0x00449A24` | image pinned only |
| `gSaveContext` | `0x00587958` | `0x00587958` | randomizer EUR offset; also TriAevum `kSaveContext` in `oot3d_dev_warp.cpp` (independent) |
| `gDrawItemTable` | `0x004D88C8` | `0x004D88C8` | needs runtime check |

The EUR offset is 0 in the ranges that hold most AP functions; it is nonzero
in `0x41A144..0x4A5AFF` (`+0x24`, then `+0x20`), for example `SaveFile_Init`.

Not in `oot.ld`, so still **open**:

- `gGlobalContext`: the randomizer captures the pointer at runtime in its own
  hooks. Candidate: TriAevum reads the play state through the pointer at
  `0x005043E0` (`kPlayStatePointer`, `oot3d_native_a32_actor_visibility.cpp:56`;
  the same address as `0x005043D4 + 0x0C` in `oot3d_top_screen_mod_profile_hud.cpp:375`).
  Its equality with the randomizer's `gGlobalContext` needs a runtime check.
- The GetItem table (`ItemTable` in the randomizer is the randomizer's own
  copy, not a vanilla symbol). The vanilla table address is open.

## 5. How OoT3D_Randomizer detects a check

**From source**, `code/src/item_override.c`.

- Hook `.patch_ActorGetItemOffer` inside `Actor_OfferGetItem` calls
  `ItemOverride_OfferGetItem(fromActor, player, incomingItemId)`.
- `ItemOverride_GetSearchKey` (line 116) builds `(scene, type, flag)`:

| Actor | `type` | `scene` | `flag` |
| --- | --- | --- | --- |
| Chest (`ACTOR_CHEST`) | `OVR_CHEST` | current scene | `params & 0x1F` |
| `EnItem00` collectible | `OVR_COLLECTABLE` | current scene | collectible flag; `0x40..0x46` for rupee circles. Respawning drops are skipped. |
| Gold Skulltula token | `OVR_SKULL` | `(params >> 8) & 0x1F` | `params & 0xFF` |
| Grotto business scrub | `OVR_GROTTO_SCRUB` | `respawn[RETURN].data` | item id |
| Deku Baba drops | none | | |
| Everything else | `OVR_BASE_ITEM` | current scene | get-item id |

- Songs, dungeon rewards and many NPC gifts do not pass through
  `Actor_OfferGetItem`. The randomizer reaches them through about 75 specific
  patch sites (`.patch_SariasSongItemGive`, `.patch_MedigoronItemOverrideOne`,
  `ItemOverride_PushDelayedOverride`, ...).
- Pending items are given from `ItemOverride_Update` only when
  `ItemOverride_CheckPlayerReadiness` passes, by pointing
  `PLAYER->interactRangeActor` at a dummy actor.

Can the key serve as the AP location key? **Yes for identity, with a stable
mapping table.** The `(scene, type, flag)` triple is stable per location in
the randomizer's own tables (`source/item_location.cpp`, 500 entries matched
by a quick count of collectable, token, base, hint-stone, grotto-scrub and
delayed kinds). Recommendation: AP location ID = fixed base + index in a
generated table, with the triple stored next to it. Do not encode the triple
into the ID: delayed and hint-stone locations have no natural triple.

## 6. Persistence

Recommendation: a sidecar JSON per save slot in TriAevum's data folder,
`archipelago/<seed>-<slot>-<save-slot>.json`, holding `seed_name`, `slot`,
`save_slot`, `received_index`, the set of reported location IDs and the seed
hash. Free bytes in the guest SaveContext are not used: their safety is
unknown, and writing them changes the native save file.

## 7. Savestates

**From source**, `docs/TRIAEVUM_SAVESTATES.md`: a state stores the whole guest
memory, so SaveContext and inventory roll back on load. A sidecar outside the
state does not roll back.

Failure if nothing is done: received items applied after the state was saved
are lost, because `received_index` says they were already given.

Proposal:

- Store an `ap_applied_index` in the savestate as an optional host section
  (the container already carries an optional `host_settings` section).
  On load, items from `ap_applied_index` onward are applied again.
- Checks are idempotent: the server ignores duplicates, so re-reporting after
  a load is harmless. Location checks are reported once per session from the
  sidecar set.
- The server's `ReceivedItems` index remains the source of truth.

## 8. Risks and effort

See the Gate 0 report in `PROGRESS.md`.
