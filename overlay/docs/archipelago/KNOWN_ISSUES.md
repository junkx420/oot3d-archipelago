# Archipelago: known issues

## Check detection not yet seen in the game

Play test 1 confirmed chests and offer matching. Not yet seen in the game:

- Skulltula tokens after the fix in `a5560858` (host-tested only).
- Freestanding items (heart pieces, keys): collectable keys, host-tested.
- Songs: delayed keys from a direct `Item_Give`, host-tested.
- Shops: the randomizer keys shop slots (`0x30+`), the observer keys item
  ids. Matters only with shopsanity.

## Developer whole-AOT build faults when Link becomes controllable

Seen 2026-10-25 in the first play test, with Archipelago on:

```
native A32 process faulted: boot reached an unimplemented A32 fallback at pc=0x2d89d8, raw=0x0, reason=4
```

Not caused by Archipelago. `0x002D89D8` is
`kOot3dCameraInterfaceDelayContinue` of TriAevum's typed gameplay bridge
(`oot3d_typed_gameplay_bridge.h:144`). In developer builds (not product
builds) that bridge leaves whole-AOT at `0x002D89D0`, runs a C++
replacement and resumes mid-function at `0x002D89D8`. Resuming there needs
the generated A32 registry (`build-codex/oot3d_a32_generated`), which this
developer tree does not have, so the dispatcher faults. The camera path runs
only under player control, so title and scenario runs without input do not
reach it.

Workaround: launch with `--disable-typed-gameplay`. Typed exits are then not
added to the whole-AOT exit list (`oot3d_native_a32_window_setup.cpp:452`)
and the original camera code runs inside whole-AOT. Product builds exclude
the bridge anyway (`#if !defined(OOT3D_WHOLE_AOT_PRODUCT_MODE)`).

Status: confirmed by the play test with `--ui-profile oot3d`: with the
workaround Link was controllable and the game kept running.

Root fix: generate the A32 registry and configure with it, which this tree
now does:

```powershell
python tools/oot3d/native_a32_runtime/generate_aot.py --output build/a32-generated
cmake ... -DOOT3D_A32_AOT_GENERATED_DIR=build/a32-generated
```

(794,929 of 815,162 reachable slots, 29 shards, about 1 minute.) The play
launcher keeps `--disable-typed-gameplay` until a controlled play test with
typed gameplay on has passed.

## TopScreen (single-screen UI) faulted without the A32 registry

`--ui-profile topscreen` faulted with "unimplemented A32 fallback at
pc=0x30dbe4": a block inside a whole-AOT function that is not a dispatcher
entry, reached by a mid-function resume. With the A32 registry linked, a
Kokiri Forest scenario ran 2,400 frames in TopScreen without a fault and
rendered the scene. HUD and inventory need a loaded save and are not yet
verified. `--ui-profile oot3d` shows only the upper 3DS screen, so it has no
inventory; the play launcher now uses `topscreen`, like the released game.

## TopScreen without Forge's UI pack fails on the HUD

The next play test failed with "integrated N64 UI presentation failed: OoT3D
UI semantic is not a native localized texture"
(`oot3d_native_ui_texture_provider.cpp:237`). Forge normally installs the
SingleScreen 3.0 UI pack (`atlas_overrides.o3tu`, `zelda_link_ext.zar`,
`font_coverage.zip`) and passes it with `--topscreen-texture-overrides`,
`--topscreen-link-animations` and `--topscreen-config`; a checkout launch
lacks them. The pack can be built with Forge's own
`topscreen_assets.prepare_topscreen_assets`, with a package-shaped root whose
`resources` points at `runtime/triaevum_resources`. The play launcher now
passes all three. Whether this cures the HUD failure needs the play test.

## Whole-AOT product audit rejects the catalogue EUR ExHeader

`audit_whole_aot_product` pins the legacy EUR ExHeader `9f065aec…`. The
catalogue EUR ROMs (No-Intro 0004 and 1168) have `a584b067…`, which differs in
the signature area and one flag byte. The generated program is otherwise
identical to the contract. This blocks `Configure-Oot3dWholeAotProduct.ps1`
on this machine, so the product build and its equivalence suite have not run.

## Tests failing before and without Archipelago

- `oot3d_native_pica_vulkan_plan_tests`: SegFault, no output.
- `oot3d_native_a32_process_tests` in whole-AOT trees: "overlapping memcpy
  must retain the ARM fallback".

## apworld logic

- The apworld follows OoT3D_Randomizer where it was more permissive
  (PROGRESS.md, Phase 3). Some of those 3D rules may be stricter than the
  game needs, for example its enemy-aware rooms (the Spirit Temple Anubis),
  and the Kakariko backyard, which the 3D logic gives no walking path. Being
  stricter is safe; loosening one needs a play test on the 3DS game.
- The comparison only covers checks of the default options and cannot catch
  a rule both logics get wrong.
- `test_can_kill.TestCanKillEnemy.test_shabom` fails (bow, magic and Ice
  Arrow should defeat a Shabom; the rule says no). Not understood; skipped.
- Link's Pocket now needs time travel, as in the unmodified game. The client
  must report the Light Medallion from Rauru as that location (Phase 4).
- Ship of Harkinian adjusts some options during generation (for example
  `start_with_links_pocket`), after the client's option check has run.

## Item override and receiving (Phase 4)

- Untested in the game. The readiness check is the randomizer's standing
  case only; items wait while Link swims.
- Play while connected: until a slot is connected in the session, locations
  give their own items, and an item found that way also arrives from the
  server later.
- Another player's items and this world's custom items (dungeon keys, songs,
  magic) show a recovery heart at their location until Phase 5.
- The client page at `0x06200000` is assumed unused by the title; mapping it
  fails loudly ("Cannot give items in the game") if it is not.

## File selection invisible in the developer TopScreen build (fixed)

Seen 2026-10-25: with `--ui-profile topscreen` the file selection showed only
the sky, so files were chosen blind. Not caused by this branch: the same with
Archipelago off, typed gameplay on, `--disable-whole-aot`, default graphics
settings and without the TopScreen asset pack. `--ui-profile oot3d` showed it.

Cause, from a log of the Reframed sites and a PICA trace: in the developer
build the pause AOT barrier (`TopScreenPauseAotBarrier`) turned whole-AOT off
on the file selection, so the upper pass ran as mass-AOT regions. Only the
sites after a call return reached the dispatcher (scene, caption, text). The
sites the pass falls through to (logo 0x00300520, messages 0x0030052C and
0x00300550, the replayed file interface 0x0030055C) are whole-AOT block starts
but not mass-AOT ones, so they never ran: no file interface on the upper
screen. The product has no barrier and keeps every TopScreen site an exit.

Fix (upstream code, two small edits): the barrier stays off while Reframed
owns the file selection (`oot3d_native_a32_window_frame.cpp`; Reframed keeps
both native targets, no pause route needs it), and the developer build makes
Reframed's sites whole-AOT observable exits as the product does
(`oot3d_native_a32_window_setup.cpp`). The barrier fix alone shows the boards
without Reframed's layout (full-width heading, unscaled names); both together
show Reframed's layout. Verified headless at the file selection with
Archipelago off and on. Still missing there: Link and the title logo on the
right half; not investigated.

## Client (alpha)

- Auto-connect does not save the password: a server that needs one refuses
  the slot until it is entered and Connect is selected.
- A server address without `ws://` or `wss://` first tries a secure
  connection; against a plain local server the log shows one "TLS handshake
  failed" before it connects.
- Loading a save reads its progress file on the game thread (about 6 ms).
- Messages name another game's item only when the server's data package has
  arrived before the scouts; otherwise they say "an item".- The game's save is noticed by the save file's write time, up to half a
  second late; an item given in that moment would count as saved.
- A new file is recognized by an empty inventory in Link's house; not yet
  seen in a run.- Holding B with quick text on also presses B in choice boxes and while the
  ocarina is out (B leaves the ocarina), as on hardware when B is pressed.
- Pots, crates, grass, rocks, bushes and trees are not locations: neither
  OoT3D_Randomizer nor this client has their data; their options are hidden.