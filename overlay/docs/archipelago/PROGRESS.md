# Archipelago progress

## Phase 0: reconnaissance (2026-10-25)

Report: [RECON.md](RECON.md).

### Done

- Toolchain installed (with approval), reference repositories checked out
  outside the repository, ROM identified as EUR Rev 1 and extracted privately.
- Executed image established: EUR `code.bin` `16a6b0aa…` for every region.
- USA→EUR address table generated from OoT3D_Randomizer `oot.ld`:
  `tools/oot3d/archipelago/addresses.json`, script
  `tools/oot3d/archipelago/recon/map_rando_addresses.py`.
- Hook semantics read from source for the interpreter and whole-AOT paths;
  host test `TestBlockEntryCallbackRegisterWritesAndIgnoredRedirect` added to
  `oot3d_native_a32_process_tests.cpp`.

### Build and test baseline (2026-10-25, `build/llvm`, clang-cl 22.1.6, no whole-AOT)

- Configure and `cmake --build build/llvm` (target `all`) succeed.
- All 83 ctest executables build. `ctest -j 8`: 81 passed, 1 skipped,
  1 failed.
  - Skipped: `oot3d_native_audio_tests` ("needs the extracted title
    (OOT3D_TEST_EXTRACT_ROOT)").
  - Failed: `oot3d_native_pica_vulkan_plan_tests`, SegFault after 0.2 s with no
    output. Not caused by this branch: the only source change is in
    `oot3d_native_a32_process_tests.cpp`, which that executable does not
    compile. Root cause not investigated.
- New test `TestBlockEntryCallbackRegisterWritesAndIgnoredRedirect` passes.

### Not done in Phase 0

- Whole-AOT equivalence suite (`Test-Oot3dWholeAotProductSuite.ps1`): needs
  the whole-AOT product build (product mode or direct plugin), not built.

Hook strategy chosen by the user on 2026-10-25: option A (observable exits
plus native block callbacks; mid-function sites in
`whole_aot_observed_sites.json`).

## Phase 1: hook proof (2026-10-25, in progress)

Design: [ARCHITECTURE.md](ARCHITECTURE.md).

### Done

- Developer whole-AOT build works on this machine (RECON.md section 1).
- Whole-AOT hook experiment: a new function-entry probe fired 3,584 times
  with an unchanged title module (RECON.md section 3).
- Module `tools/oot3d/archipelago/`, CMake option
  `TRIAEVUM_ENABLE_ARCHIPELAGO` (default OFF), runtime switch
  `TRIAEVUM_ARCHIPELAGO=1`.
- Check observer at `Actor_OfferGetItem` and `Item_Give` (function entries).
  Key formation follows `ItemOverride_GetSearchKey`; songs given without an
  offer map to delayed keys.
- Address self-check on the real image: `verified=1` in every AP run.
- Host tests `triaevum_archipelago_tests` (no ROM): the five representative
  keys against the randomizer's own location data, packing, skipped cases,
  SHA-256 vectors, address table vs `addresses.json`, observer on fake
  memory, self-check failure.

### Representative locations

| Kind | Location | Expected key |
| --- | --- | --- |
| Chest | KF Kokiri Sword Chest | scene `0x55`, chest, flag `0x00` |
| NPC | HC Malon Egg | scene `0x5F`, base, GI `0x47` |
| Freestanding | ZR Near Open Grotto PoH | scene `0x54`, collectable, flag `0x04` |
| Skulltula | KF GS Know It All House | scene `0x0C`, skull, flag `0x02` |
| Song | Song from Saria | scene `0xFF`, delayed, `18` (DLYOVR_SONG_SARIA) |

### Flag OFF unchanged

- All 8 window translation units preprocess (`clang-cl /EP`) to identical
  output with and without the Archipelago edits.
- The executable is not byte-identical across the two builds. The rebuild
  log shows why: the reconfigure regenerated the FidelityFX SSSR permutation
  headers and rebuilt `oot3d_ffx_sssr_backend_vk`, which is not Archipelago
  code. Not investigated further.

### Tests (both trees: whole-AOT developer, clang-cl 22.1.6)

| Tree | Result |
| --- | --- |
| `build/llvm` (flag OFF) | 83 tests: 80 passed, 1 skipped, 2 failed |
| `build/llvm-ap` (flag ON) | 84 tests: 81 passed, 1 skipped, 2 failed |

The same two fail in both trees:
`oot3d_native_pica_vulkan_plan_tests` (SegFault, also failed in Phase 0
before any Archipelago code) and `oot3d_native_a32_process_tests`
("overlapping memcpy must retain the ARM fallback"). The second failure
appears only once whole-AOT is linked; it passed in the Phase 0 tree without
whole-AOT. The new Archipelago test and the Phase 0 block-entry test pass.

### Gate 1: in progress

Play test 1 (2026-10-25, by the user, TopScreen, AP on): 19 `Item_Give`
events, 1,027 offers. Excerpt (`private/evidence/ap-2026-10-25-run1.log`):

```
check #5 source=offer scene=0x28 item=0x84 player_gi=0x4C actor=0x000A params=0x5982 offer_gi=0x4C scene=0x28 type=chest flag=0x02 key=0x02012800
check #6 source=offer scene=0x28 item=0x85 player_gi=0x4D actor=0x000A params=0x59A0 offer_gi=0x4D scene=0x28 type=chest flag=0x00 key=0x00012800
check #13 source=offer scene=0x2D item=0x3E player_gi=0x29 actor=0x003D params=0x0000 offer_gi=0x29 scene=0x2D type=base flag=0x29 key=0x29002D00
check #15 source=offer scene=0x00 item=0x06 player_gi=0x05 actor=0x000A params=0x00A1 offer_gi=0x05 scene=0x00 type=chest flag=0x01 key=0x01010000
check #16 source=none scene=0x00 item=0x71 player_gi=0x00 actor=0x0000 params=0x0000 offer_gi=0x00 key=none
check #17 source=offer scene=0x00 item=0x83 player_gi=0x48 actor=0x000A params=0x5906 offer_gi=0x48 scene=0x00 type=chest flag=0x06 key=0x06010000
```

| Category | Result | Randomizer location |
| --- | --- | --- |
| Chest | ✔ 6 of 6 | KF Mido's four chests (`0x28`, 0x00–0x03); Deku Tree Slingshot Chest (`0x00`, 0x01); Deku Tree Compass Room Side Chest (`0x00`, 0x06) |
| Offer from an NPC/shop | Offer path ✔; key ✗ for shops | Deku Shield bought: key (`0x2D`, base, 0x29); the randomizer keys KF Shop Item 1 as (`0x2D`, base, 0x30): shop slots, not item ids |
| Skulltula | ✗ → fixed, needs retest | Token `Item_Give` came with `player->getItemId` 0 and no `interactRangeActor` |
| Freestanding | not yet collected | |
| Song | not yet collected | |
| Non-locations | ✔ | Rupees and seeds from grass and drops: `key=none` |

Fix after play test 1: a token `Item_Give` (item 0x71) takes the latest
offer when that offer came from a token actor; an offer is consumed by the
check it forms. Host tests added for both. Shop keys (slot 0x30+) are left
for the apworld phase (shopsanity is outside the conservative option set).

Launcher: `private/play-archipelago-log.ps1` (outside the repository); the
log goes to `private/run/play/ap.log`.

Decision 2026-10-25: the user moved on to Phase 2 without further play
tests ("würde zu lange dauern"). Gate 1 therefore stands **partly
verified**: chests, offer matching and non-locations in the game; tokens,
freestanding items and songs in host tests only. These three stay open in
KNOWN_ISSUES.md until a play test covers them.

Not covered yet: the get-item path matching (`interactRangeActor` and
`getItemId` at `Item_Give`) is inferred from the randomizer's code, not yet
observed. Locations reached through the randomizer's ~75 special patch sites
(most NPC gifts that skip `Actor_OfferGetItem`) are out of Phase 1.

### Gate 0 result

The block-entry callback does **not** honour a write to `r15` (interpreter:
`a32_runtime.cpp:858-881` keeps a local `pc`; whole-AOT:
`Oot3dAotEnterBlock` continues the block after `Reload`). Register writes
r0–r14 are honoured.

That alone does not force Plan B: TriAevum already redirects execution
through observable exits plus native function / native block callbacks
(TopScreen source ports, object-bank extension). The AP module can use the
same route. Its cost: every redirect site must be a block start of the title
module, so mid-function sites need `whole_aot_observed_sites.json` and a
regenerated module.

Options for the item override and the receive path are in the Gate 0 message
to the user and in RECON.md section 3.

### Effort estimate

| Phase | Estimate | Main risk |
| --- | --- | --- |
| 1 Hook proof (5 checks, logging) | 2–4 days | Whole-AOT developer build on this machine; finding `gGlobalContext` |
| 2 AP client + ImGui | 4–6 days | apclientpp/wswrap + OpenSSL/asio build under clang-cl and vcpkg static |
| 3 apworld | 2–3 weeks | Logic port from OoT3D_Randomizer, location ID schema, test coverage |
| 4 Gameplay integration | 3–5 weeks | About 75 NPC/song special cases, safe-state detection, get-item animation |
| 5 Polish | 1–2 weeks | Foreign-item text and models |

## Phase 2: AP client in the game (2026-10-25)

Decisions by the user on 2026-10-25: location ids are base + table index
(append-only); progress is a sidecar JSON per seed, slot and save slot, plus
the applied-item count in savestates; the tables may be generated from
OoT3D_Randomizer with attribution.

### Done

- `NetworkClient` (`ap_network_client.*`): apclientpp owned by one network
  thread; commands and events cross through mutex-guarded queues. Items
  handling 0b111 (the server is the only item source once Phase 4 overrides
  locations). Checks queued while not slot-connected are sent on connect.
- `Session` (`ap_session.*`): runs on the guest thread once per guest frame
  and from the check observer; every public call locks, because the F1 panel
  may draw on another thread. Maps keys to ids through the generated table,
  opens the sidecar when a save file is loaded (`newf` is "ZELDAZ",
  `fileNum` 0..2), refuses a sidecar of another seed without touching it,
  and resends reported checks the server lacks.
- Sidecar (`ap_sidecar.*`):
  `savedata/archipelago/<seed>-<team>-<slot>-save<n>.json`, written to a
  temporary file and renamed.
- F1 > Game > Archipelago (`ap_panel.*`): server, slot, password, connect,
  status, counters, console, chat. Server, slot and an installation uuid are
  kept in `config/archipelago.json`; the password is never written.
- Dependencies by pinned SHA-256 (apclientpp a5b7b96, wswrap 4743819,
  asio 1.12.2, websocketpp 0.8.1: the combination Ship of Harkinian and
  ap-soeclient use), OpenSSL 3.6.5 and zlib from vcpkg, only with the
  option ON. Notices in THIRD_PARTY_NOTICES.md.

### Verified

- Host tests (no ROM): location table parsing and rejection cases, sidecar
  round trip and foreign-seed refusal, session without connection, the
  generated table resolving every key of play test 1.
- Network end to end against a local Archipelago server (commit fcff008,
  `MultiServer.py`, a seed of this apworld):
  `triaevum_archipelago_net_smoke ws://127.0.0.1:38281 Link3D 3997697`
  connected, received slot_data, sent "KF Kokiri Sword Chest", received
  item #0 and, on a later connection, found the check in the server's
  checked_locations. Server log: "(Team #1) Link3D sent Huge Rupee to
  Link3D (KF Kokiri Sword Chest)".
- The game starts with the session, loads the table and keeps running
  (scenario smoke run).

### Not done

- Connecting from inside the running game (F1) has not been exercised.
- The applied-item count in savestates: deferred to Phase 4, where items
  are first applied (until then AppliedItems stays 0).
- wss:// against archipelago.gg: built with OpenSSL, untested; certificates
  come from the system store, as in Ship of Harkinian.
- apclientpp marks own checks as checked locally; LocationsChecked events
  only report checks made elsewhere (for example !collect).

## Phase 3: apworld (2026-10-25, logic ported)

Tables: `recon/generate_tables.py`, 867 locations (676 vanilla, 191 Master
Quest) and 245 items from OoT3D_Randomizer ae0fcf5, ids from 0x3D0000.

### Logic: Ship of Harkinian apworld as the base

Decision by the user on 2026-10-25: port the logic with the Ship of
Harkinian apworld (Archipelago-SoH `worlds/oot_soh` 84c059f, MIT) as the
template, adapted to the 3DS game.

- `apworld/oot3d` is that world (import commit `3b474661`) with its rules
  and regions. Its location and item enums keep their identifiers; their
  names are the 3D names (`recon/map_soh_names.py`, map in
  `data/soh_name_map.json`, 455 pairs checked against Ship of Harkinian's
  scene and flag table by `recon/verify_soh_map.py`). Locations and items get
  the generated 3D ids; ones the 3DS game lacks are dropped.
- `oot3d_options.py` restricts every option to what the TriAevum client
  supports now: the unmodified game, dungeon items in their own dungeon or
  vanilla, optional Kokiri Sword shuffle. An unsupported value raises
  OptionError; nothing is overridden silently.
- Seeds with default options generate (8 of 8, seeds 1 to 8), and their
  playthroughs reach Ganon.

### Logic: checked against OoT3D_Randomizer

`recon/logic_oracle/` builds the 3D randomizer's own logic for the host
(`build_oracle.ps1`; only a stand-in `<3ds.h>` and a driver are ours). It is
set to the game's vanilla behaviour and answers which locations a set of
items reaches. `recon/compare_logic.py` asks the apworld the same about the
same item sets and reports each location one side reaches and the other
does not, with a smallest item set that shows it (`--minimize`).

Comparison: the 272 checks of a default seed, about 1,200 item sets per run
(none, all, all with only k copies of each item, all without each pair of
items, 300 random sets); checks the world fills with their vanilla item act
as events on both sides.

Rule: where the apworld reaches more than the 3D logic, the 3D rule is
taken, so the apworld never expects more of the player than OoT3D_Randomizer
does. Found and changed:

| Area | Ship of Harkinian | Now (as OoT3D_Randomizer) |
| --- | --- | --- |
| Leaving the forest | child Link could leave through the Lost Woods shortcuts to Goron City and Zora's River, and by warp songs, before the Deku Tree | none of these before the Deku Tree is cleared (`can_leave_forest`; 251 locations depended on it) |
| Kakariko backyard | child Link walks in at day | no walking path; through the potion shop, the rooftop or a grotto |
| Lake Hylia sun | refilled lake, or the longshot and scarecrow | refilled lake only |
| Spirit Temple, child side | the early torches chest without fighting the Anubis | the Anubis must be defeated (fire) |
| Spirit Temple, room with three Anubis and a Beamos | explosives | explosives and fire (Silver Gauntlets chest, Mirror Shield chest, Four Armos chest, both invisible chests) |
| Link's Pocket | at the start | after the first time travel (Rauru, in the unmodified game) |

Result after the changes, world seeds 1 to 7: no location where the apworld
is more permissive than the 3D logic. It is stricter at 4 to 8 locations per
seed (Spirit Temple key counts, Link's Pocket), which costs nothing but
freedom. The comparison also showed two oracle settings that must not be the
randomizer defaults: Door of Time "Intended" (the stones are needed; "Closed"
is a randomizer patch).

### Tests

- `worlds/oot3d` (Archipelago fcff008, pytest): 477 passed, 33 skipped,
  0 failed. New `test/test_oot3d.py` generates with the supported defaults
  and pins each rule in the table above. The imported Ship of Harkinian
  tests run their own option sets past the client's option check; the 33
  skipped ones test features the 3DS game lacks (Greg, bombchu bag,
  medallion-locked trials, grass, a minimal pool's hearts, the C-button
  ocarina shuffle) and one that is not understood yet (KNOWN_ISSUES.md);
  the list with reasons is `NOT_IN_OOT3D` in `test/bases.py`.
- With the rule changes stashed, the imported tests failed in exactly the
  same 71 subtests (the tests now skipped); the changes added no failure.
- Archipelago's generic tests (`test/general`, every installed world): 378
  passed, 1 failed. The failure is `test_no_failed_world_loads`: three other
  worlds (soe, tww, zillion) lack their Python packages in this venv. They
  found item groups naming items without a 3D id; groups now leave those out.
- pytest was installed into the local `.venv-ap` only.

### Not done

- The comparison covers what both sides call checks under the default
  options; tokens, scrubs, shops and cows are not checks yet. Where both
  logics are wrong it cannot tell.
- Tricks: the Ship of Harkinian tricks describe N64 techniques; the option
  rejects them until 3D tricks are mapped.

## Phase 4: gameplay integration (2026-10-25, started)

Design and the user's decision: ARCHITECTURE.md, "Items in the game".

### Done

- Item table generated with the location table
  (`runtime/triaevum_resources/archipelago/items.json`, format
  `triaevum_archipelago_items_v1`): AP item id -> the randomizer's get-item
  id, plus the patch's 274 item rows (action id, upgrade, effect, arguments).
- `ap_item_table`: progressive items resolve against the save as the
  randomizer's `ItemTable_ResolveUpgrades` does (hookshot, strength, bomb bag,
  quiver, bullet bag, wallet, scale, nuts, sticks, magic, bombchus, ocarina,
  Goron sword, the "to rupee" fallbacks, Ruto's letter). A get-item below 0x7C
  is the game's own; custom ones are written into the save as the
  randomizer's effects do (dungeon keys and items, songs, medallions, stones,
  magic, filled bottles, double defense). Other effects (enemy souls,
  ocarina buttons, key rings, ice traps, Triforce pieces) are reported, not
  given; the supported options do not place them.
- Scouting: on every slot connection the client asks for all of its
  locations; the placement is cached in the sidecar.
- Override: at `Actor_OfferGetItem` the observer rewrites `r2`. This world's
  item with a game get-item shows itself (model, text, Item_Give); another
  player's item, or one of ours without a get-item, shows a recovery heart.
- Receiving: one item per frame through the item actor at `0x06200000`
  (mapped through `NativeA32Memory::MapRegion`), when Link is ready; custom
  items directly. The server's copy of an item the game gave at its location
  is skipped while the save records the location (chest and collectible
  flags; other locations by the sidecar); otherwise it is given, and the
  location then gives a placeholder.
- Host tests: override rewrite and matching, the generated item table
  (resolution and effects), the receiver against Player state, save flags,
  sidecar round trip of the new fields. Both trees build (flag ON and OFF).
- End to end against a local server: 272 locations scouted, placement
  received ("KF Kokiri Sword Chest holds item 3997696").

### Not done

- **Not tried in the game yet.** Needs a play test: open chests with own and
  foreign items, receive an item, reload a save.
- The applied-item count in savestates; goal; DeathLink.
- Locations that do not pass through `Actor_OfferGetItem` keep their own
  item (songs, most NPC gifts); with the supported options those hold their
  vanilla items.
- Without a connection in the session, locations keep their own items.

### Cutscene skips (2026-10-25, the user's request)

- Client setting in F1 > Game > Archipelago, off by default, kept in
  `config/archipelago.json`: "Skip cutscenes" sets ten save flags
  OoT3D_Randomizer sets for every new file (`ap_cutscene_skip.cpp` lists them:
  Saria's greeting, Darunia in the Fire Temple, the Lake Hylia and Desert
  Colossus owls, the Nabooru battle, boss and Dodongo's Cavern intros, Ruto in
  the Water Temple, the Club Moblin and the Forest Temple Poe sisters
  cutscenes). "Skip the intro of a new file" (experimental) clears the
  pending intro cutscene of a file in Link's house.
- Only cutscenes and conversations: no flag gives an item, opens a path or
  changes the logic. Left out: the Master Sword and Sheik flags (Link's
  Pocket), Ruto in Jabu-Jabu, Malon, Nabooru as a child, temple doors, the
  Dampe race and minigame phases.
- Host test of both settings. Not tried in the game; whether the intro skip
  lands before the scene reads the index is open.

### Play test 2 (2026-10-25, by the user)

Seed 18246537981234658761, local server, TopScreen launcher:

- Override: Mido's four chests gave what the spoiler places there; "KF Mido
  Bottom Left Chest" showed the Light Arrow with its own model and text.
- No doubles: 4 checks sent, 4 items echoed by the server, 4 skipped as
  given at their location (sidecar `given_at_location` 4).
- Receiving: a Progressive Hookshot sent with `!getitem` reached Link through
  Player's own get-item (`Item_Give` 0x0A with `player_gi` 0x08 and no
  location key); applied count 5.
- Cutscene skips: 10 flags and the intro skip set on a new file.
- Fix after the test: items without a location (starting inventory,
  `!getitem`) no longer count as this world's location items.
- Still open: reloading a save after receiving, a new file receiving
  everything again (were blocked by the invisible file selection, fixed
  2026-10-25, KNOWN_ISSUES).

### Goal and DeathLink (2026-10-25)

- Death: a block-entry hook at 0x00252420, the block holding
  OoT3D_Randomizer's GameOverStart patch site (a `nop` at 0x00252424). It
  follows a call, so every execution mode starts a block there; the game
  reaches it only after setting the game-over state to "death" (with a fairy
  it sets "revive" and branches past). `addresses.json` lists it as kind
  `hook_block` (`map_rando_addresses.py`, CLIENT_HOOK_BLOCKS), signature
  pinned and verified by the startup self-check like the other hooks.
- DeathLink: apworld option `death_link` (Archipelago's DeathLink), passed in
  slot_data. The client then joins the DeathLink tag (also after reconnects),
  sends a Bounce on every death and, for a received one, sets Link's health
  to 0 once Link can be hit by it; the game's own death follows (a bottled
  fairy still saves him). The game over that follows a received death is not
  sent back (15-second window).
- Goal: Link leaves Ganon's battle (scene 0x4F) alive, without a game over
  there, still in gameplay. Kept in the sidecar and reported on every
  connection.
- Tests: host tests for the death hook and the sidecar goal flag. Network end
  to end against a local server with a DeathLink seed:
  `triaevum_archipelago_net_smoke` joined DeathLink, sent a death and
  received one from a second client (`private/deathlink-peer.py`).
- Not tried in the game: neither a death nor the goal. The goal condition
  is inferred from the game's flow, not observed; loading a savestate out of
  Ganon's battle would count as winning.

### Play test 3: DeathLink (2026-10-25, by the user)

Seed 89757102641179993453 (death_link on), local server, a second DeathLink
client (`private/deathlink-peer.py`):

- Link died in the game: the second client received "Link3D fell in Hyrule."
- The second client sent a death: Link's health went to 0 and he died; the
  game over that followed was not sent back.

### Archipelago badge (2026-10-25)

- With the module on (`TRIAEVUM_ARCHIPELAGO=1`), the title and the file
  selection show an "Archipelago" badge in the bottom-right corner: a ring of
  colored dots, the name and the connection state ("Not connected - F1 >
  Archipelago", "Connected as <slot>"). It is an ImGui overlay drawn with the
  frame (`DrawBadge` in `ap_panel.cpp`), shown while `SaveContext.game_mode`
  is not 0, so never in gameplay; the preparation screen covers it.
- Verified headless at the file selection: badge with the module on, none
  with it off. Not seen in a window yet.
### Universal Tracker (2026-10-25)

- The world came with Ship of Harkinian's Universal Tracker support
  (`ut_can_gen_without_yaml`, `UniversalTracker.py` reading every
  logic-relevant option back from slot_data). Added `interpret_slot_data`,
  which hands the slot data to the tracker's regeneration, as other
  tracker-ready worlds do, and fixed the imported passthrough of
  `shuffle_gerudo_membership_card` (it replaced the option instead of
  setting its value).
- `test/test_universal_tracker.py` does what the tracker does: a generated
  seed's slot_data after a JSON round trip, a world rebuilt from it with
  default options and the generation steps up to the fill. The rebuilt world
  has the same locations and the same reachable locations (events only, with
  no items and with seven key items). apworld suite: 479 passed, 33 skipped.
- Live test (2026-10-26) with Universal Tracker 0.3.6 (`tracker.apworld`
  from FarisTheAncient/Archipelago, release Tracker_v0.3.6, SHA-256
  39e9bc0f...cb750) in the local Archipelago 0.6.9 checkout, headless
  (`Launcher.py "Universal Tracker" -- --nogui --list|--count`), against the
  DeathLink seed 89757102641179993453 on a local server:
  - The tracker generated the world without a yaml from slot_data and
    connected to slot Link3D with the `Tracker` tag.
  - No items: 5 locations in logic (Kokiri Sword chest, Mido's four chests),
    the same as `private/ut-expect.py` (the world rebuilt offline from the
    seed's slot_data).
  - After `!getitem Kokiri Sword`: 11 in logic (`--count`), the same as
    offline (the six Deku Tree chests added).
  - `--list` also prints the tracker's "glitched" locations (29 here),
    reached only with Ship of Harkinian's `Glitched` item, so with N64 glitch
    rules that are not checked for 3D. The tracker can hide them in its
    settings.
- The world no longer names a glitches item (`glitches_item_name`), so the
  tracker shows no glitched list (decided with the user, 2026-10-26). The
  `Glitched Item` stays for the imported trick tests. Live again: `--list`
  gives the 11 in-logic locations only. apworld suite: 480 passed, 33
  skipped.
### Alpha preparation (2026-10-26)

Features and quality of life, decided with the user:

- Enable without an environment variable: F1 > Game > Archipelago has an
  "Archipelago enabled" switch, kept as `enabled` in `archipelago.json`
  beside the game's config and read at the next start (the check observer's
  hooks are installed with the game). While off, the tab shows only the
  switch and the game runs unchanged. `TRIAEVUM_ARCHIPELAGO=1` or `0` still
  decides when set.
- Auto-connect: with a saved server and slot the session connects at
  startup, before a file is loaded (option, on by default; the password is
  not saved). Reconnect: apclientpp reopens a socket the server closed; a
  session that fails with an error is now started again after 5, 10, 20, 40,
  then 60 seconds until Disconnect. A repeated socket error is logged once.
- In-game messages (option, on by default), top center, six seconds each,
  at most five: "Sent X to Y" when this world's location holds another
  player's item (names from the server's data package, resolved with the
  scouts), "Received X from Y" or "Got X" when an item is given, DeathLink
  deaths sent and received, the goal, connected, lost and refused.

Optimization, measured headless (title, file selection, gameplay in the Deku
Tree, 5,400 frames): `Session::Update` costs 7 to 20 microseconds per guest
frame on average. Writing the progress file (temporary file and rename) took
several milliseconds on the game thread after every check and item; it now
happens at most once per frame and on a writer thread (`SidecarWriter`). The
one remaining spike is reading the file when a save is loaded (about 6 ms,
during the load).

Verified: host tests (retry delays, the switch and options in the settings
file, auto-connect, the writer); a headless run enabled only by the settings
file auto-connected, showed the badge and "Connected to Archipelago as
Link3D"; with the server stopped and started again the game reconnected and
scouted again by itself; in gameplay "Received Kokiri Sword" appeared and the
item was given; `triaevum_archipelago_net_smoke` passed with a second
DeathLink client.
### Saves, reloads and savestates (2026-10-26)

The native save file stays untouched (its spare bytes are not documented and
OoT3D_Randomizer keeps its own data elsewhere), so the applied-item count
follows the game in three ways (`ap_save_sync.{h,cpp}`):

- The game saved: the session checks the save file's write time twice a
  second and then notes the count in the sidecar (`applied_at_last_save`).
- Loaded from disk: when gameplay starts from the file selection, or the
  save's sidecar opens for the first time in a run while playing, the count
  goes back to the noted one; items given after the game last saved come
  again.
- New file: in Link's house without any item or quest item, the count, the
  given-at-location and given-by-server sets and the goal start over; every
  received item comes again.
- Savestates: the client page in guest memory (which savestates carry)
  holds a progress record (magic, identity hash, count), written every frame
  in gameplay; after a load the count follows the record of that moment. A
  savestate of a run without the page has none and changes nothing; the page
  is mapped again when it is missing.

Verified headless against a local server (seed 89757102641179993453, two
items for the slot): a savestate taken before the items and loaded after
them gave both again; a second run on the same saves, the game not saved,
gave both again on loading the file; touching the save file in a run noted
the count ("The game saved: 2 received items are in the save"), and the next
run gave nothing twice. Host tests cover the new-file check, the record and
the decisions. Not seen in a run: a new file (name entry is not scripted).
### Quick text and the Options Creator (2026-10-26)

- Quick text after OoT3D_Randomizer's setting, an option in F1 >
  Archipelago ("Text boxes": normal, instant, instant and hold B to skip;
  `quick_text` in archipelago.json, off by default). Instant: block-entry
  hooks at the blocks before the randomizer's CheckForTextControlCode and
  HandleTextControlCode sites (`addresses.json`, kind hook_block, evidence
  block_after_branch) perform Message_SetInstantText from the host at a
  box's first character and after a box break, and turn "instant off" and
  "unskippable" codes into "instant on". Turbo: the randomizer's B checks
  are not block starts the hooks reach in every execution mode (the advance
  and close blocks were never entered in a run), so held B is filtered
  instead: while the message object 0x005C1878 (Message_GetState's literal)
  has a text box open (+0xF38 not 0), B reaches the game 2 refreshes
  pressed, 2 released. It follows the game's B, so a remapped controller
  works with whatever button is mapped to B.
- Verified headless: 17 to 18 boxes made instant per run; with B held while
  the Heart Container box waited, a turbo press closed it and the game went
  on. Host tests cover the instant command, the code replacement and the
  B pattern.
- Options Creator: options the client cannot honour are hidden
  (`Visibility.none`): Ship of Harkinian's hints, its shuffles, every option
  with a single supported value. Shown: Archipelago's common options,
  DeathLink, the item pool, starting inventory from the pool, the Kokiri
  Sword and the dungeon item options, whose help names the supported values.
  Archipelago's own option classes are never changed. The apworld manifest
  gained `version` and `compatible_version` (required from Archipelago 0.7),
  world version 0.4.0. The packed apworld loads and generates on Archipelago
  0.6.8 (the installed launcher's version) and 0.6.9; apworld suite 486
  passed, 33 skipped.
### Crash on leaving Kokiri Forest (2026-10-26)

- Reported by the user: the game stopped with "oot3d_native_game: guest
  memory regions overlap" when leaving Kokiri Forest (Lost Woods, the Deku
  Tree, the path to the sword chest). The client's item page sat at
  0x06200000, where TriAevum maps its object bank extension when a scene
  needs more object slots than the title has; Kokiri Forest never did, the
  next scenes do.
- The page moved to 0x06F00000. The window setup asserts at compile time
  that it overlaps none of TriAevum's own regions (Link animations, actor
  lights, object bank extension, release queue, dash blend).
- Verified headless: a received item still reached Link through the moved
  page; host tests pass. Savestates made with the old build carry the page
  at 0x06200000 and can hit the same overlap; they should not be loaded.

### World settings, blue warp skips, missed checks (2026-10-26)

- Play test by the user (Deku Tree, Lost Woods, Hyrule Castle) showed three
  checks that never reached the server: Queen Gohma's reward, Saria's gift
  (Fairy Ocarina, given in a cutscene) and Zelda's Letter (offered by child
  Zelda, actor 0x01D3, keyed as a base item). Dungeon rewards given by
  Item_Give now form their temple keys (stones by boss, medallions by
  dungeon, the Light Medallion as Link's Pocket); the Fairy Ocarina and
  Zelda's offer form the delayed keys 2 and 3 (OoT3D_Randomizer
  item_override.c). The same log showed a Gold Skulltula token taking a
  nearby chest's key, because player->getItemId still held an earlier
  chest's item: a token now matches only a token's offer.
- Blue warp cutscenes: the Deku Tree's speech and death and the Goron Ruby
  cutscene are skipped with "Skip cutscenes" while a save is connected. A
  block-entry hook at OoT3D_Randomizer's ChildBlueWarpOverride site
  (0x003F5774, addresses.json ChildBlueWarpBlock, block_after_branch) sets
  the dungeon's events before the game reads them; the game's own code then
  takes its "already done" path to entrance 0x0457 (0x047A) without a
  cutscene (disassembly 0x003F5774 to 0x003F58A8). The reward location is
  reported and its item comes from the server. Jabu-Jabu and the adult
  dungeons are not covered yet.
- World settings from the seed (ap_world_state.cpp): Jabu-Jabu open, Zora's
  Fountain open, rainbow bridge always open and Ganon's trials (skip, or any
  count; slot_data required_trials) are set as save flags while a connected
  save plays, as SaveFile_Init sets them. Settings that need code changes in
  the game (open forest, Kakariko gate, Door of Time, fortress carpenters)
  stay unsupported.
- Starting items (Kokiri Sword, Deku Shield, ocarinas, magic beans): Ship
  of Harkinian pushes them as events, which the server never sends; they
  are real items now, sent like Archipelago's start inventory.
- apworld 0.5.0: those options are shown in the Options Creator. A seed with
  all of them generates (slot_data and starting items checked); host tests
  cover the new keys, the token fix, the warp flags and the world flags.
  Not yet seen in the game: the warp skip and the world flags.

### All blue warps, Great Fairies, open forest, gate and door (2026-10-27)

- Blue warps: "Skip cutscenes" with a connected save now covers every
  dungeon. The adult warp (AdultBlueWarpBlock 0x001E3F50, r8 = play) gets the
  randomizer's event set first (0x48, 0x49 with Death Mountain's cloud, 0x4A
  with Lake Hylia's water, 0x47 and 0x46 with the Spirit and Shadow
  Medallions, which the game's code tests for those two) and takes the
  game's own path to the warp pad. Jabu-Jabu's warp has no such path; at
  RutoBlueWarpDoneBlock (0x001A1944, the target of the branch that ends it)
  the cutscene index 0xFFF0 is cleared, and the game gives the sapphire.
- Great Fairies and the Light Arrow cutscene are ordinary locations in Ship
  of Harkinian's world (the user's seed held bombs, nuts and heart pieces
  there), and none was reported. Spells and Light Arrows come through a
  direct Item_Give: they form delayed keys 4 to 6 and 1, and once the seed's
  placements are known the item becomes a recovery heart (r1 at Item_Give),
  the location's item coming from the server. The upgrade fountain (scene
  0x3B) writes magic, double magic and double defense itself: the client
  sees the change, reports DMT, DMC or OGC (7 to 9) and takes the reward
  back unless the location holds this world's same upgrade.
- Received items also reach Item_Give without an offer; while the client's
  get-item is offered, a give is "received" and forms no key (a Fairy
  Ocarina from the starting inventory had formed Saria's gift).
- World options: Door of Time open (eventChkInf[4] 0x0800), Kakariko gate
  open (infTable[7] 0x40, as the N64 randomizer; the mask shop opens early
  too), closed_forest deku_only and off. The exit Kokiri tests the Kokiri
  Emerald (questItems at r8 + 0xBC against the mask at r6 + 0x48); with the
  exit open the client adds the emerald bit for that one block
  (KokiriGuardCheckBlock 0x003ACCF4) and restores the save in both blocks
  after it (0x003ACD04, 0x003ACD28). Setting the Deku Tree's event instead
  would have broken its warp. "off" also moves Mido (eventChkInf[0] 0x10).
- Tests: host tests cover the warp flags, the Jabu-Jabu cutscene, the fairy
  rewards and their reverts, received gives, the replacement and the
  forest guard; apworld 0.6.0 suite 492 passed, 33 skipped. Headless, dev
  build and the release exe in a copy of a Forge-prepared TriAevum
  beta.1.2: the hooks verified against the running image, a seed with the
  new options set its flags. Not seen in the game: the warp skips, the
  fairies and the forest guard.

### Token and scrub shuffles (2026-10-27)

- Gold Skulltula tokens (shuffle_skull_tokens dungeon, overworld, all) are
  reported from the save: the game sets the token's gsFlags bit (save
  +0xEB4, 22 bytes; array index and bit are the randomizer's skull key)
  however the token was taken, so a token taken while disconnected counts
  too. The token's offer is kept apart from other offers (a chest beside it
  offers every frame); when its location is part of the seed, the token's
  Item_Give becomes a recovery heart and the location's item comes from the
  server. Offers of tokens are not overridden.
- Business scrubs (shuffle_scrubs one_time_only, all): offers already form
  the randomizer's keys (grotto scrubs by return scene and get-item, others
  as base items). The game's scrubs sell again; once a scrub's location is
  reported it sells its own item. Prices stay the game's (all below 99
  rupees, so the logic's wallet needs hold); Ship of Harkinian's price
  options are hidden.
- skulls_sun_song (logic only) is allowed.
- Not done: cows need an empty bottle in the game, which the logic does not
  ask for, and a key per cow the game does not have; shops need a bought
  flag and a shown item the game does not have. Both need code changes in
  the game (OoT3D_Randomizer cow.c, shops.c).
- Tests: host tests for token keys and per-token replacement; apworld 0.7.0
  suite 492 passed, 33 skipped; a seed with all tokens and scrubs has 408
  locations, all in the client's table. Headless against it (dev build and
  release exe in the TriAevum beta.1.2 copy): the dev save's two Deku Tree
  tokens were reported on connecting and the server's item came back.

### Cow and shop shuffles (2026-10-27)

- Cows (shuffle_cows): EnCow_Init (function entry) numbers the cows of a
  scene as EnCow_rInit does (1, then 2, a cow's tail counts; the grottos by
  the scene they return to, DMT 1, Hyrule Field 2), and a cow's offer is
  keyed base item 0x14 + its number. While the location is open, the block
  after Inventory_HasEmptyBottle (CowBottleCheckBlock 0x003F0660) reads 1
  instead of the game's answer, so the cow gives without an empty bottle, as
  the logic expects; afterwards it gives milk into a bottle again.
- Shops (shuffle_shops, shuffle_shops_item_amount): OoT3D_Randomizer
  replaces the shelf item actor; the client changes the game's EnGirlA
  fields when it is drawn (get-item, the seed's price from slot_data
  shop_prices, count 1, no direct give, the default buy event 0x00114C04
  from EnGirlA_ShopItemEntries). The shopkeeper's five calls through
  canBuyFunc are found by their instruction pattern; the block after each
  reads "enough rupees" or "need rupees" for a changed slot. The buy event
  reports the slot (base item 0x30 + position, 0x38 on in the Kakariko
  Bazaar, entrance 0x00B7) and sells it out (SI_SOLD_OUT, reinitialized), and
  a bought slot starts sold out on the next visit. Shop ware placed by the
  seed ("Buy Deku Shield") resolves through the game's shop table. A
  shopkeeper's own offer forms no key. The shelf still shows the game's
  model and text.
- recon/map_rando_addresses.py pins the shop table by three vanilla prices
  and get-items and derives the buy event and the five result blocks from it
  (evidence: shop item table, canBuyFunc offset, block after call).
- Tests: host tests for the cow numbering, keys and bottle answer, and a
  shelf sold, refused, bought and sold out; apworld 0.8.0 suite 492 passed,
  33 skipped; a seed with shops and cows (313 locations, 32 shop slots all
  priced, 9 cows) is fully in the client's table. Headless: dev build and the
  release exe connect and scout it. Not seen in the game: a cow or a shop
  purchase.

### More options in the Options Creator (2026-10-27)

- Rainbow bridge by stones, medallions, dungeon rewards, dungeons or tokens
  (counts within what the game has: 3 stones, 6 medallions, 8 dungeons):
  the client counts the save (quest bits, the boss events the blue warps
  set, gsTokens) and sets the bridge flag once the count is reached.
- starting_hearts 1 to 20: written to a new file's health capacity.
- Triforce hunt: the goal is sent once the required Triforce Pieces were
  received (counted from the server's items; a notification counts them).
- shuffle_ocarinas: Saria's gift becomes a recovery heart once the seed's
  placements are known, as the Great Fairies' spells do. shuffle_weird_egg and
  shuffle_gerudo_membership_card: Malon's and the Gerudo's offers already
  give the location's item.
- The Options Creator notes partial ranges as "1 to 6".
- Not done: the 100 Gold Skulltula reward (no location in OoT3D_Randomizer's
  table), tricks and glitched logic (next: the 3D randomizer's tricks and
  glitches, checked one by one against its own logic with the oracle of
  recon/compare_logic.py).
- Tests: host tests for the bridge counts, the flag and the hearts; apworld
  0.9.0 suite 492 passed, 33 skipped; two seeds with the new options
  generate with every location in the client's table. Headless: the release
  exe connects and scouts. Not seen in the game.

### 3D tricks in logic (2026-10-27)

- Ship of Harkinian's 135 tricks describe the N64 game. recon/map_tricks.py
  pairs OoT3D_Randomizer's 88 tricks with them by effect: with the same item
  sets, the locations a trick adds on each side (the 3D side through the
  logic oracle, which now takes extra settings), matched by Jaccard. With
  tokens and scrubs shuffled on both sides, 32 pairs matched (31 exactly).
- recon/verify_tricks.py runs the full logic comparison for each pair with
  both tricks on: all 32 pairs are never more permissive than the 3D logic.
- The same comparison found one rule more permissive with tokens shuffled:
  the Deku Tree basement vines GS took a jump slash without a trick. It now
  follows gs_deku_tree.cpp (projectile, Din's Fire, or the new trick "Deku
  B1 Vines GS" = "DT B1 Vines GS w/ Jump Slash", verified the same way); the
  baseline is then 0 more permissive.
- oot3d_tricks.py: tricks_in_logic offers the 33 verified tricks by their
  3D randomizer names (valid_keys and the frozen _valid_keys);
  can_do_trick reads a SoH trick through its counterpart, and a SoH trick
  without one is never in logic (only Universal Tracker's glitched item).
  enable_all_tricks enables the verified ones.
- 55 3D tricks showed no effect in these item sets (Master Quest, and tricks
  whose alternative was always at hand); they stay out until a pairing is
  verified.
- Glitched logic: OoT3D_Randomizer keeps it as a second rule per entrance and
  location of its own area graph (732 glitch conditions over 20 glitch kinds
  with difficulties and 14 single glitches). The apworld uses Ship of
  Harkinian's graph, so it needs those areas mapped first; not started.
- Tests: apworld 0.10.0 suite 497 passed, 33 skipped, plus tests that only
  verified tricks are offered and checked; trick seeds generate with the
  packed apworld on Archipelago 0.6.8, and an unverified SoH trick is
  refused there.

### Glitched logic (2026-10-28)

- recon/extract_glitched.py reads OoT3D_Randomizer's area graph (447 areas,
  2188 events, locations and exits) and keeps the 274 entries with a
  /*Glitched*/ condition (573 glitch uses) in data/glitched_logic.json, as
  C++ expressions, plus the 13 glitchless conditions whose key count rises
  with glitched logic (SmallKeys(dungeon, glitchless, glitched)).
- oot3d_glitched.py parses those expressions and builds them into rule
  builder rules with the apworld's helpers, with CanDoGlitch translated from
  logic.cpp. With glitched logic an entry is open when its glitchless or its
  glitched condition holds (Entrance::GetConditionsMet), so the condition is
  ORed into the SoH entrance between the same regions (or a new entrance
  where SoH has none), into the location's rule (with the 3D area's
  reachability when SoH places it elsewhere), and into the SoH event of the
  same region. The raised key counts are ANDed into the matching SoH rules.
  3D area names match SoH region names for 313 of 446 areas; a few aliases
  cover more.
- Left out, so never expected: 38 of 274 conditions with every glitch on
  (areas SoH splits differently: Gerudo Fortress, parts of Jabu-Jabu, the
  Bottom of the Well and Dodongo's Cavern; the enemy randomizer's
  CanPassEnemy family; Master Quest; gossip stones and bean planting).
- oot3d_glitch_options.py: Logic Rules (glitchless, glitched), the 21 glitch
  categories with the difficulty levels the 3D randomizer offers for each,
  and the 14 miscellaneous glitches, in a "Glitches" option group. They go
  into slot data for Universal Tracker; the client plays the same.
- recon/verify_glitches.py compares with the logic oracle (Logic=Glitched,
  same settings): every glitch at its highest level, then each category and
  each single glitch alone. 36 of 36 runs are never more permissive than the
  3D logic. explain_difference.py found the two fixes it took: the Bottle
  with Big Poe no longer counts as a bottle (OoT3D_Randomizer's NumBottles
  leaves it out), and Sheik in the Ice Cavern needs adult Link.
- The apworld stays stricter in places (34 locations with every glitch on;
  Ledge Clip and the Forest Temple BK skip alone about 44 each), where a
  route crosses areas SoH splits differently or a key count is ANDed into a
  whole rule.
- Tests: apworld 0.11.0 suite 512 passed, 33 skipped (every condition
  parses; glitchless seeds take none; a ledge cancel gets child Link past
  King Zora only when chosen).

### Cheats (2026-10-28)

- ap_cheats.cpp: infinite health, magic, ammo and rupees, after Ship of
  Harkinian's cheats, as client settings in the F1 Archipelago tab (off by
  default; they apply with or without a connection, like the skips). Once
  per frame in gameplay with a save loaded, each tops up a save value and
  never goes past what the save allows: health to the heart capacity while
  Link is alive (so a DeathLink death still happens), magic to 0x30 per
  magic level once the meter is acquired, ammo of items Link has to the
  capacity of his upgrades, rupees to the wallet's.
- The capacities come from the game's gUpgradeCapacities (u16[8][4] as in
  Ocarina of Time) at 0x0053CBE4: OoT3D_Randomizer's TycoonWalletSize patch
  site (0x0053CC08) is the wallet row's third capacity. map_rando_addresses.py
  derives the address and pins it by the 32 capacities the EUR image holds;
  the client checks the same values at runtime and keeps ammo and rupees
  off when they differ. SaveContext.rupees (0x48) comes from the clang probe.
- The 3D randomizer has no cheats; its other gameplay options (fast chests,
  song replays, the damage multiplier) follow below.
- Tests: host tests for every cheat, the capacity limits, a dead Link and a
  different image.

### Fast chests (2026-10-28)

- A client option in the F1 Archipelago tab (off by default), as OoT3D
  Randomizer's "always fast" chest animations. The randomizer replaces two
  instructions (FastChests 0x00354CD4, FastChestsTwo 0x003F1048) with calls
  to Chest_OverrideAnimation. The client patches no code: in Link's chest
  opening the block after the Item_CheckObtainability call (0x00354D24,
  FastChests + 0x50, block_after_call) reads ITEM_NONE for a new item, which
  picks the long animation; the client turns it into another value there, so
  the game takes its own short path. That path stores -1 in the chest
  (+0x38C), which the chest reads at FastChestsTwo (r7 = chest + 0x300,
  ldrsh [r7, #0x8C]) and then skips the light and the fanfare: one hook is
  enough.
- Tests: host tests (off by default, a new item opens fast, a short opening
  is left alone, the option is kept in the settings file); not yet tried on
  a chest in the game.

### Skip song replays (2026-10-28)

- A client option in the F1 Archipelago tab (off, skip, skip without the
  replay's music), as OoT3D Randomizer's Settings_SkipSongReplays. Once per
  frame in gameplay ap_song_replays.cpp moves GlobalContext.msgMode (0x2A90,
  u8) on as the randomizer does: 19 (and 18 without sound, except the
  Scarecrow's Song) to 23; Zelda's Lullaby in the Water Temple (scene 5)
  through 20 to 22. msgMode and lastPlayedSong (0x2B7C) come from the clang
  probe; 0x2B7C is also what the Song of Time block code reads (0x207FD8).
- SkipSongReplays_TimeBlocksFix and _WarpBlocksFix (0x00208004, 0x00208068)
  set the blocks' timer (+0x1C6) to 0x6E, the replay's length; the
  randomizer sets 0x10. The client patches no code: the block after each
  site (+0xC, block_after_branch) counts the timer down, and the check
  observer turns a fresh 0x6E into 0x10 there.
- Not ported: the randomizer also kills the song effect actors at once
  (songs_visual_effects.c); here they play out.
- Tests: host tests (the modes, the scarecrow, the Water Temple lullaby,
  both block timers, the setting is kept); a probe to gameplay verifies the
  new hook signatures. Not yet tried with a song in the game.

### Damage multiplier (2026-10-28)

- A client option in the F1 Archipelago tab (x1/2, x1, x2, x4, x8, x16, one
  hit KO; x1 by default), as OoT3D Randomizer's Settings_ApplyDamageMultiplier.
  The randomizer replaces Health_ChangeBy's first instruction (0x00352DC0,
  mov r4, r1); the client hooks the function's entry (0x00352DBC, already
  signature-pinned as an AOT function entry) and changes r1, the s16 health
  change. Master Quest's doubling and double defense (save +0x0E, +0x51,
  read right after in the function) still follow in the game's code.
- ap_damage.cpp ports the rounding: half of a quarter heart alternates -1
  and 0, and -1 with double defense alternates -2 and 0 (the game's
  arithmetic shift leaves -1 whole). Unlike the randomizer, Master Quest
  keeps its double damage, so x1 leaves the game unchanged. One hit KO is
  -1000; healing is left alone.
- The seed's logic assumes x1 (the apworld has no damage option); the
  option changes nothing the seed or the server sees.
- Tests: host tests (each multiplier, the rounding, healing, the hook, the
  setting is kept); a probe to gameplay verifies the signatures. Not yet
  tried with a hit in the game.
