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