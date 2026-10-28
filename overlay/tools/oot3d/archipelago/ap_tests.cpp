// Host tests of the Archipelago check observer. They need no ROM: guest
// memory is a small fake bus.

#include "ap_cheats.h"
#include "ap_check_observer.h"
#include "ap_cutscene_skip.h"
#include "ap_fairy_rewards.h"
#include "ap_item_receiver.h"
#include "ap_item_table.h"
#include "ap_location_key.h"
#include "ap_location_table.h"
#include "ap_save_sync.h"
#include "ap_session.h"
#include "ap_sha256.h"
#include "ap_shops.h"
#include "ap_sidecar.h"
#include "ap_world_state.h"

#include <filesystem>
#include <sstream>

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace Oot3dArchipelago;

namespace {

void Expect(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

class FakeMemory final : public oot3d::recomp::a32::MemoryBus {
public:
  bool Read32(std::uint32_t address, std::uint32_t *value) override {
    std::uint32_t result = 0;
    for (std::uint32_t i = 0; i < 4U; ++i) {
      std::uint8_t byte = 0;
      if (!Read8(address + i, &byte))
        return false;
      result |= static_cast<std::uint32_t>(byte) << (i * 8U);
    }
    *value = result;
    return true;
  }
  bool Write32(std::uint32_t address, std::uint32_t value) override {
    for (std::uint32_t i = 0; i < 4U; ++i)
      mBytes[address + i] = static_cast<std::uint8_t>(value >> (i * 8U));
    return true;
  }
  bool Read8(std::uint32_t address, std::uint8_t *value) override {
    const auto found = mBytes.find(address);
    if (found == mBytes.end())
      return false;
    *value = found->second;
    return true;
  }
  bool Read16(std::uint32_t address, std::uint16_t *value) override {
    std::uint8_t low = 0, high = 0;
    if (!Read8(address, &low) || !Read8(address + 1U, &high))
      return false;
    *value = static_cast<std::uint16_t>(low | (high << 8U));
    return true;
  }
  bool Write8(std::uint32_t address, std::uint8_t value) override {
    mBytes[address] = value;
    return true;
  }
  bool Write16(std::uint32_t address, std::uint16_t value) override {
    mBytes[address] = static_cast<std::uint8_t>(value);
    mBytes[address + 1U] = static_cast<std::uint8_t>(value >> 8U);
    return true;
  }

private:
  std::map<std::uint32_t, std::uint8_t> mBytes;
};

// The five Phase 1 locations, with the randomizer's own location data
// (source/item_location.cpp at ae0fcf5): scene, flag and override type.
void TestRepresentativeKeys() {
  const auto chest = KeyForOffer({kActorChest, 0x0000U, 0x55U, 0x3BU, 0U, 0U});
  Expect(chest.has_value() && *chest == LocationKey{0x55U, OverrideType::Chest, 0x00U},
         "KF Kokiri Sword Chest is (0x55, chest, 0x00)");

  const auto malon = KeyForOffer({0x00E7U, 0x0000U, 0x5FU, 0x47U, 0U, 0U});
  Expect(malon.has_value() && *malon == LocationKey{0x5FU, OverrideType::BaseItem, 0x47U},
         "HC Malon Egg is (0x5F, base, 0x47)");

  const auto heartPiece = KeyForOffer({kActorEnItem00, kItem00HeartPiece, 0x54U, 0x3EU, 0x04U, 0U});
  Expect(heartPiece.has_value() &&
             *heartPiece == LocationKey{0x54U, OverrideType::Collectable, 0x04U},
         "ZR Near Open Grotto Freestanding PoH is (0x54, collectable, 0x04)");

  const auto token = KeyForOffer({kActorSkullToken, 0x0C02U, 0x2DU, 0x5BU, 0U, 0U});
  Expect(token.has_value() && *token == LocationKey{0x0CU, OverrideType::Skull, 0x02U},
         "KF GS Know It All House is (0x0C, skull, 0x02)");

  const auto saria = KeyForDirectGive(0x62U);
  Expect(saria.has_value() && *saria == LocationKey{0xFFU, OverrideType::Delayed, 18U},
         "Song from Saria is (0xFF, delayed, DLYOVR_SONG_SARIA = 18)");
}

void TestKeyPackingMatchesRandomizerUnion() {
  // ItemOverride_Key {char pad_; u8 scene; u8 type; u8 flag} read as u32 LE.
  const LocationKey key{0x55U, OverrideType::Chest, 0x1FU};
  Expect(key.Packed() == 0x1F015500U, "key packing follows ItemOverride_Key.all");
}

void TestKeysTheRandomizerSkips() {
  Expect(!KeyForOffer({kActorChest, static_cast<std::uint16_t>(kGetItemRupeePurpleLose << 5U),
                       kSceneTreasureBoxShop, kGetItemRupeePurpleLose, 0U, 0U}),
         "the chest game's losing rupee has no key");
  Expect(!KeyForOffer({kActorEnItem00, 0x0000U, 0x54U, 0x01U, 0x00U, 0U}),
         "a drop without a collectible flag has no key");
  Expect(!KeyForOffer({kActorEnItem00, 0x0000U, 0x54U, 0x01U, 0x20U, 0U}),
         "a respawning drop has no key");
  Expect(!KeyForOffer({kActorEnItem00, 0x0008U, 0x54U, 0x01U, 0x04U, 0U}),
         "an arrow drop has no key");
  Expect(!KeyForOffer({kActorDekuBaba, 0x0000U, 0x55U, 0x01U, 0U, 0U}),
         "Deku Baba drops have no key");
  const auto scrub = KeyForOffer({kActorBusinessScrubSalesman, 0x0000U, kSceneGrottos, 0x2EU, 0U, 0x51U});
  Expect(scrub.has_value() && *scrub == LocationKey{0x51U, OverrideType::GrottoScrub, 0x2EU},
         "grotto scrubs use the grotto's return scene");
  Expect(!KeyForDirectGive(0x59U) && !KeyForDirectGive(0x6FU),
         "only songs and dungeon rewards are direct-give locations");
}

void TestSha256() {
  const std::string_view abc = "abc";
  const std::vector<std::uint8_t> bytes(abc.begin(), abc.end());
  Expect(Sha256Hex(bytes) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
         "SHA-256 of \"abc\"");
  Expect(Sha256Hex({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
         "SHA-256 of the empty string");
}

void TestAddressTableMatchesJson(const char *path) {
  std::ifstream input(path);
  Expect(input.good(), "addresses.json is readable");
  const auto document = nlohmann::json::parse(input);
  Expect(document.at("code_sha256") ==
             "16a6b0aa4c4784680220a6f780f7f8a73cfb205557aa9f9f0e705179e0613220",
         "addresses.json belongs to the EUR execution image");
  for (const GuestFunction &function : kGuestFunctions) {
    bool found = false;
    for (const auto &entry : document.at("entries")) {
      if (entry.at("name") != function.Name ||
          (entry.at("kind") != "symbol" && entry.at("kind") != "hook_block"))
        continue;
      found = true;
      char address[16];
      std::snprintf(address, sizeof(address), "0x%08X", function.Address);
      Expect(entry.at("exec_addr") == address, "hooked address matches addresses.json");
      Expect(entry.at("signature") == std::string("sha256:") + function.WindowSha256,
             "hooked signature matches addresses.json");
      Expect(entry.at("status") == "signature_pinned", "hooked function is signature pinned");
    }
    Expect(found, "hooked function is listed in addresses.json");
  }
  for (const QuickTextSite &site : kQuickTextSites) {
    bool found = false;
    for (const auto &entry : document.at("entries")) {
      if (entry.at("name") != site.Name)
        continue;
      found = true;
      char address[16];
      std::snprintf(address, sizeof(address), "0x%08X", site.Address);
      Expect(entry.at("exec_addr") == address, "quick text address matches addresses.json");
      Expect(entry.at("signature") == std::string("sha256:") + site.WindowSha256,
             "quick text signature matches addresses.json");
      Expect(entry.at("status") == "signature_pinned", "quick text site is signature pinned");
    }
    Expect(found, "quick text site is listed in addresses.json");
  }
  bool capacities = false;
  for (const auto &entry : document.at("entries")) {
    if (entry.at("name") != "gUpgradeCapacities")
      continue;
    capacities = true;
    char address[16];
    std::snprintf(address, sizeof(address), "0x%08X", kUpgradeCapacities);
    Expect(entry.at("exec_addr") == address, "the capacity table matches addresses.json");
    // Its first 32 bytes are the quiver, bomb bag, strength and scale rows.
    std::array<std::uint8_t, 32> window{};
    for (std::size_t i = 0; i < 16U; ++i) {
      window[i * 2U] = static_cast<std::uint8_t>(kUpgradeCapacitiesExpected[i]);
      window[i * 2U + 1U] = static_cast<std::uint8_t>(kUpgradeCapacitiesExpected[i] >> 8U);
    }
    Expect(entry.at("signature") == "sha256:" + Sha256Hex(window), "the capacity values match addresses.json");
    Expect(entry.at("status") == "signature_pinned", "the capacity table is signature pinned");
  }
  Expect(capacities, "the capacity table is listed in addresses.json");
}

// Quick text on a fake text object: its command list (0xD4), count (0xD8),
// pool (0xDC) and pool use (0xE0).
void TestQuickText() {
  constexpr std::uint32_t kText = 0x08300000U, kList = 0x08310000U, kPool = 0x08320000U;
  constexpr std::uint32_t kParse = 0x08330000U, kChars = 0x08340000U;
  FakeMemory memory;
  memory.Write32(kText + 0xD4U, kList);
  memory.Write32(kText + 0xD8U, 2U);
  memory.Write32(kText + 0xDCU, kPool);
  memory.Write32(kText + 0xE0U, 0x10U);
  QuickText::SetInstantText(kText, memory);
  std::uint32_t count = 0, used = 0, listed = 0, tag = 0;
  std::uint8_t type = 0;
  memory.Read32(kText + 0xD8U, &count);
  memory.Read32(kText + 0xE0U, &used);
  memory.Read32(kList + 2U * 4U, &listed);
  memory.Read32(kPool + 0x10U, &tag);
  memory.Read8(kPool + 0x10U + 4U, &type);
  Expect(count == 3U && used == 0x18U && listed == kPool + 0x10U && tag == 0x004ED174U && type == 6U,
         "SetInstantText appends the instant command as the game does");
  memory.Write32(kText + 0xD8U, 0x200U);
  QuickText::SetInstantText(kText, memory);
  memory.Read32(kText + 0xE0U, &used);
  Expect(used == 0x18U, "a full command list is left alone");
  memory.Write32(kText + 0xD8U, 3U);

  QuickText text{false};
  oot3d::recomp::a32::GuestState state{};
  state.r[5] = kText;
  state.r[6] = kParse;
  state.r[8] = kChars;
  state.r[9] = 0U;
  memory.Write8(kParse + 4U, kTextInstantOff);
  text.ObserveBlockEntry(kQuickTextCheckControlCodeBlock, state, memory);
  text.ObserveBlockEntry(kQuickTextHandleControlCodeBlock, state, memory);
  memory.Read32(kText + 0xD8U, &count);
  std::uint8_t code = 0;
  memory.Read8(kParse + 4U, &code);
  Expect(count == 3U && code == kTextInstantOff, "off changes nothing");

  text.SetMode(QuickTextMode::Instant);
  text.ObserveBlockEntry(kQuickTextCheckControlCodeBlock, state, memory);
  memory.Read32(kText + 0xD8U, &count);
  Expect(count == 4U, "a box's first character makes it instant");
  state.r[9] = 5U;
  memory.Write8(kChars - 2U, kTextControlCode);
  memory.Write8(kChars - 1U, 0x1CU); // a new line, not a box break
  text.ObserveBlockEntry(kQuickTextCheckControlCodeBlock, state, memory);
  memory.Read32(kText + 0xD8U, &count);
  Expect(count == 4U, "inside a box nothing is added");
  memory.Write8(kChars - 1U, kTextWaitForInput);
  text.ObserveBlockEntry(kQuickTextCheckControlCodeBlock, state, memory);
  memory.Read32(kText + 0xD8U, &count);
  Expect(count == 5U, "after a box break the next box is instant too");
  text.ObserveBlockEntry(kQuickTextHandleControlCodeBlock, state, memory);
  memory.Read8(kParse + 4U, &code);
  Expect(code == kTextInstantOn, "instant-off becomes instant-on");
  memory.Write8(kParse + 4U, kTextUnskippable);
  text.ObserveBlockEntry(kQuickTextHandleControlCodeBlock, state, memory);
  memory.Read8(kParse + 4U, &code);
  Expect(code == kTextInstantOn, "unskippable text becomes skippable");

  // Turbo: B held in an open text box reaches the game as 2 refreshes
  // pressed, 2 released.
  constexpr std::uint32_t kB = 0x2U, kA = 0x1U;
  const auto filtered = [&](std::uint32_t buttons) {
    text.FilterInput(buttons, kB, memory);
    return buttons;
  };
  memory.Write8(kMessageObject + kMessageObjectState, 0U);
  Expect(filtered(kB | kA) == (kB | kA), "instant alone leaves B alone");
  text.SetMode(QuickTextMode::Turbo);
  Expect(filtered(kB) == kB && filtered(kB) == kB, "without a text box B is the player's");
  memory.Write8(kMessageObject + kMessageObjectState, 3U);
  std::array<std::uint32_t, 6> seen{};
  for (auto &buttons : seen)
    buttons = filtered(kB | kA);
  Expect(seen == std::array<std::uint32_t, 6>{kB | kA, kB | kA, kA, kA, kB | kA, kB | kA},
         "held B in a text box is pressed every other game frame, A untouched");
  Expect(text.Counts().TurboPresses == 1U, "each new press is counted");
  Expect(filtered(kA) == kA && filtered(kB) == kB, "releasing B starts over with a press");

  Expect(ParseQuickTextMode("turbo") == QuickTextMode::Turbo && !ParseQuickTextMode("fast").has_value(),
         "modes parse by name");
}

// A chest is offered for a few frames, accepted, then given.
void TestObserverReportsAcceptedOffer() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kChest = 0x08100000U;
  constexpr std::uint32_t kPlayer = 0x08200000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x55U);
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  memory.Write16(kChest + kActorId, kActorChest);
  memory.Write16(kChest + kActorParams, 0x0760U); // Kokiri Sword (0x3B << 5), flag 0
  memory.Write8(kSaveRespawnReturnData, 0U);
  memory.Write8(kPlayer + kPlayerGetItemId, 0x3BU);
  memory.Write32(kPlayer + kPlayerInteractRangeActor, kChest);

  CheckObserver observer{false};
  std::vector<CheckEvent> events;
  observer.SetSink([&](const CheckEvent &event) { events.push_back(event); });
  oot3d::recomp::a32::GuestState state{};
  for (int frame = 0; frame < 3; ++frame) {
    state.r[0] = kChest;
    state.r[1] = kPlay;
    state.r[2] = 0x3BU;
    observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  }
  state.r[0] = kPlay;
  state.r[1] = 0x3BU; // ITEM_SWORD_KOKIRI
  observer.ObserveBlockEntry(kItemGive, state, memory);

  Expect(events.size() == 1U, "one Item_Give reports one event");
  Expect(std::string_view(events[0].Source) == "offer", "the accepted offer is the source");
  Expect(events[0].Key.has_value() &&
             *events[0].Key == LocationKey{0x55U, OverrideType::Chest, 0x00U},
         "the event carries the chest key");
  Expect(observer.Hits().ActorOfferGetItem == 3U && observer.Hits().ItemGive == 1U,
         "hook hits are counted");
  Expect(state.r[0] == kPlay && state.r[1] == 0x3BU, "the observer leaves registers unchanged");
}

// Phase 4: the override rewrites the get-item argument, keeps its sign, and
// the give still matches the offer through the replaced id.
void TestObserverOverridesOffer() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kChest = 0x08100000U;
  constexpr std::uint32_t kPlayer = 0x08200000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x55U);
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  memory.Write16(kChest + kActorId, kActorChest);
  memory.Write16(kChest + kActorParams, 0x0760U);
  memory.Write8(kSaveRespawnReturnData, 0U);
  memory.Write8(kPlayer + kPlayerGetItemId, 0x09U); // Player holds the replaced id
  memory.Write32(kPlayer + kPlayerInteractRangeActor, 0U);

  CheckObserver observer{false};
  std::vector<CheckEvent> events;
  std::vector<LocationKey> asked;
  observer.SetSink([&](const CheckEvent &event) { events.push_back(event); });
  observer.SetOverride([&](const LocationKey &key, oot3d::recomp::a32::MemoryBus &) {
    asked.push_back(key);
    return std::optional<std::uint16_t>(0x09U); // GI_LONGSHOT
  });
  oot3d::recomp::a32::GuestState state{};
  state.r[0] = kChest;
  state.r[1] = kPlay;
  state.r[2] = static_cast<std::uint32_t>(-0x3B);
  observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  Expect(static_cast<std::int32_t>(state.r[2]) == -0x09, "the override replaces r2 and keeps its sign");
  Expect(asked.size() == 1U && asked[0] == LocationKey{0x55U, OverrideType::Chest, 0x00U},
         "the override is asked with the original key");
  state.r[0] = kPlay;
  state.r[1] = 0x0AU; // ITEM_LONGSHOT
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 1U && events[0].Key.has_value() && events[0].OverrideGetItemId == 0x09U,
         "the give of the replaced item is the location's check");
}

// Play test 2026-10-25: Item_Give(ITEM_SKULL_TOKEN) arrives with
// player->getItemId 0 and no interactRangeActor.
void TestObserverMatchesTokenWithoutPlayerState() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kToken = 0x08100000U;
  constexpr std::uint32_t kPlayer = 0x08200000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x00U);
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  memory.Write16(kToken + kActorId, kActorSkullToken);
  memory.Write16(kToken + kActorParams, 0x0004U); // Deku Tree GS Basement Vines
  memory.Write8(kSaveRespawnReturnData, 0U);
  memory.Write8(kPlayer + kPlayerGetItemId, 0U);
  memory.Write32(kPlayer + kPlayerInteractRangeActor, 0U);

  CheckObserver observer{false};
  std::vector<CheckEvent> events;
  observer.SetSink([&](const CheckEvent &event) { events.push_back(event); });
  oot3d::recomp::a32::GuestState state{};
  state.r[0] = kToken;
  state.r[1] = kPlay;
  state.r[2] = 0x5BU; // GI_SKULL_TOKEN
  observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  state.r[0] = kPlay;
  state.r[1] = kItemSkullToken;
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 1U && events[0].Key.has_value() &&
             *events[0].Key == LocationKey{0x00U, OverrideType::Skull, 0x04U},
         "a token give takes the token offer's key");

  // The offer is consumed: a second give without a new offer has no key.
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 2U && !events[1].Key.has_value(),
         "an offer forms one check only");
}

// A rupee picked up after walking past a chest keeps no chest key.
void TestStaleOfferIsNotInherited() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kChest = 0x08100000U;
  constexpr std::uint32_t kPlayer = 0x08200000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x55U);
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  memory.Write16(kChest + kActorId, kActorChest);
  memory.Write16(kChest + kActorParams, 0x0760U);
  memory.Write8(kSaveRespawnReturnData, 0U);
  memory.Write8(kPlayer + kPlayerGetItemId, 0U);
  memory.Write32(kPlayer + kPlayerInteractRangeActor, 0U);
  CheckObserver observer{false};
  std::vector<CheckEvent> events;
  observer.SetSink([&](const CheckEvent &event) { events.push_back(event); });
  oot3d::recomp::a32::GuestState state{};
  state.r[0] = kChest;
  state.r[1] = kPlay;
  state.r[2] = 0x3BU;
  observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  state.r[0] = kPlay;
  state.r[1] = 0x84U; // ITEM_RUPEE_GREEN from the grass
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 1U && !events[0].Key.has_value() &&
             std::string_view(events[0].Source) == "none",
         "a give that matches no offer forms no key");
}

void TestObserverReportsDirectSong() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x56U);
  memory.Write32(kPlay + kPlayPlayerActor, 0U);
  CheckObserver observer{false};
  std::vector<CheckEvent> events;
  observer.SetSink([&](const CheckEvent &event) { events.push_back(event); });
  oot3d::recomp::a32::GuestState state{};
  state.r[0] = kPlay;
  state.r[1] = 0x62U; // ITEM_SONG_SARIA
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 1U && std::string_view(events[0].Source) == "direct" &&
             events[0].Key.has_value() && events[0].Key->Type == OverrideType::Delayed,
         "a song given without an offer is a delayed location");
  Expect(FormatEvent(events[0]).find("type=delayed flag=0x12") != std::string::npos,
         "the log line names the key");
}

void TestVerifyAddressesRejectsOtherImage() {
  FakeMemory memory;
  for (const GuestFunction &function : kGuestFunctions)
    for (std::uint32_t i = 0; i < 32U; ++i)
      memory.Write8(function.Address + i, 0U);
  std::string error;
  Expect(!CheckObserver::VerifyAddresses(memory, &error), "zeroed code fails the self-check");
  Expect(error.find("does not match its signature") != std::string::npos,
         "the self-check names the mismatch");
  FakeMemory unmapped;
  Expect(!CheckObserver::VerifyAddresses(unmapped, &error) &&
             error.find("is not mapped") != std::string::npos,
         "unmapped code fails the self-check");

  CheckObserver observer;
  oot3d::recomp::a32::GuestState state{};
  bool threw = false;
  try {
    observer.ObserveBlockEntry(kItemGive, state, memory);
  } catch (const std::runtime_error &) {
    threw = true;
  }
  Expect(threw && !observer.Verified(), "the first hooked entry fails loudly on another image");
}

void TestHookPcsAreSorted() {
  for (const std::optional<std::uint32_t> probe :
       {std::optional<std::uint32_t>{}, std::optional<std::uint32_t>{0x003727F0U},
        std::optional<std::uint32_t>{kItemGive}}) {
    const auto pcs = CheckObserver::HookPcs(probe);
    for (std::size_t i = 1; i < pcs.size(); ++i)
      Expect(pcs[i - 1] < pcs[i], "hook pcs are sorted and unique");
  }
  Expect(CheckObserver::HookPcs().size() == 28U,
         "checks, deaths, blue warps, the forest guard, cows, shops, fast chests, song blocks, damage, item "
         "text and quick text need 28 hooks");
  Expect(CheckObserver::HookPcs(0x003727F0U).size() == 29U, "a probe adds one hook");
  Expect(CheckObserver::HookPcs(kItemGive).size() == 28U, "a probe on a check hook is merged");
}

void TestObserverReportsDeath() {
  FakeMemory memory;
  CheckObserver observer{false};
  int deaths = 0;
  observer.SetDeathSink([&] { ++deaths; });
  oot3d::recomp::a32::GuestState state{};
  observer.ObserveBlockEntry(kGameOverStartBlock, state, memory);
  Expect(deaths == 1 && observer.Hits().GameOver == 1U, "the game-over block reports a death");
}

// Play test 2026-10-26, Deku Tree: a token collected beside a chest that
// offered a recovery heart, with getItemId still holding the heart of the
// chest opened before, took that chest's key.
void TestTokenIgnoresNearbyChestOffer() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kChest = 0x08100000U;
  constexpr std::uint32_t kPlayer = 0x08200000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x00U);
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  memory.Write16(kChest + kActorId, kActorChest);
  memory.Write16(kChest + kActorParams, 0x5906U);
  memory.Write8(kSaveRespawnReturnData, 0U);
  memory.Write8(kPlayer + kPlayerGetItemId, 0x48U); // left over from the last chest
  memory.Write32(kPlayer + kPlayerInteractRangeActor, 0U);
  CheckObserver observer{false};
  std::vector<CheckEvent> events;
  observer.SetSink([&](const CheckEvent &event) { events.push_back(event); });
  oot3d::recomp::a32::GuestState state{};
  state.r[0] = kChest;
  state.r[1] = kPlay;
  state.r[2] = 0x48U;
  observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  state.r[0] = kPlay;
  state.r[1] = kItemSkullToken;
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 1U && !events[0].Key.has_value(), "a token never takes a chest's key");
  // The chest's own give still matches its offer.
  state.r[1] = 0x83U; // ITEM_RECOVERY_HEART
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 2U && events[1].Key.has_value() &&
             *events[1].Key == LocationKey{0x00U, OverrideType::Chest, 0x06U},
         "the chest keeps its offer for its own give");
}

void TestDungeonRewardKeys() {
  Expect(KeyForDirectGive(kItemKokiriEmerald) == LocationKey{kTempleScene, OverrideType::Temple, 0U},
         "Kokiri's Emerald is Queen Gohma's location");
  Expect(KeyForDirectGive(kItemZoraSapphire) == LocationKey{kTempleScene, OverrideType::Temple, 2U},
         "Zora's Sapphire is Barinade's location");
  Expect(KeyForDirectGive(kItemMedallionForest) == LocationKey{kTempleScene, OverrideType::Temple, 3U},
         "the Forest Medallion is Phantom Ganon's location");
  Expect(KeyForDirectGive(0x6AU) == LocationKey{kTempleScene, OverrideType::Temple, 7U},
         "the Shadow Medallion is Bongo Bongo's location");
  Expect(KeyForDirectGive(kItemMedallionLight) ==
             LocationKey{kTempleScene, OverrideType::Temple, kLinksPocketFlag},
         "the Light Medallion is Link's Pocket");
  Expect(!KeyForDirectGive(kItemSkullToken).has_value(), "a token is no direct give");
  // Play test 2026-10-26: both were never reported.
  Expect(KeyForDirectGive(kItemFairyOcarina) ==
             LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedSariasGift},
         "Saria's ocarina is LW Gift From Saria");
  Expect(KeyForOffer({kActorChildZelda, 0x0000U, 0x4AU, kGetItemZeldasLetter, 0U, 0U}) ==
             LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedZeldasLetter},
         "Zelda's offer of her letter is HC Zeldas Letter");
}

void TestFairyRewards() {
  // Spells and the Light Arrows are direct gives of their fairy or cutscene.
  Expect(KeyForDirectGive(kItemDinsFire) ==
             LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedFairyHyruleCastle},
         "Din's Fire is HC Great Fairy Reward");
  Expect(KeyForDirectGive(kItemLightArrows) ==
             LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedLightArrowCutscene},
         "the Light Arrows are ToT Light Arrow Cutscene");

  // The upgrade fountain: which change is which fairy.
  const FairyUpgrades none{};
  Expect(FairyRewardLocation(none, {true, false, false}) == kDelayedFairyMountainTrail, "magic is DMT");
  Expect(FairyRewardLocation({true, false, false}, {true, true, false}) == kDelayedFairyCrater,
         "double magic is DMC");
  Expect(FairyRewardLocation(none, {true, true, false}) == kDelayedFairyCrater, "both at once is DMC");
  Expect(FairyRewardLocation(none, {false, false, true}) == kDelayedFairyOutsideGanonsCastle,
         "double defense is OGC");
  Expect(!FairyRewardLocation({true, false, false}, {true, false, false}).has_value(), "no change, no fairy");

  FakeMemory memory;
  memory.Write8(kSaveContext + kSaveMagicAcquired, 1U);
  memory.Write8(kSaveContext + kSaveDoubleMagic, 1U);
  memory.Write8(kSaveContext + kSaveDoubleDefense, 0U);
  memory.Write8(kSaveContext + kSaveMagicLevel, 2U);
  memory.Write8(kSaveContext + kSaveMagic, 0x60U);
  memory.Write16(kSaveContext + kSaveMagicMeterSize, 0x60U);
  Expect(RevertFairyReward(kDelayedFairyCrater, {true, false, false}, memory), "double magic taken back");
  const auto after = ReadFairyUpgrades(memory);
  std::uint8_t level = 0, magic = 0;
  std::uint16_t size = 0;
  memory.Read8(kSaveContext + kSaveMagicLevel, &level);
  memory.Read8(kSaveContext + kSaveMagic, &magic);
  memory.Read16(kSaveContext + kSaveMagicMeterSize, &size);
  Expect(after.has_value() && after->Magic && !after->DoubleMagic && level == 1U && magic == 0x30U &&
             size == 0x30U,
         "back to a single meter");
}

// A received item's Item_Give has no offer: it is no direct location, and a
// direct give is replaced when the session says so.
void TestDirectGiveReceivedAndReplaced() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x3DU);
  memory.Write32(kPlay + kPlayPlayerActor, 0U);
  CheckObserver observer{false};
  std::vector<CheckEvent> events;
  bool receiving = true;
  observer.SetSink([&](const CheckEvent &event) { events.push_back(event); });
  observer.SetReceivingProbe([&] { return receiving; });
  observer.SetDirectGiveOverride([](const LocationKey &key, oot3d::recomp::a32::MemoryBus &) {
    return key.Flag == kDelayedFairyHyruleCastle ? std::optional<std::uint8_t>(kItemRecoveryHeart)
                                                 : std::nullopt;
  });
  oot3d::recomp::a32::GuestState state{};
  state.r[0] = kPlay;
  state.r[1] = kItemDinsFire;
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 1U && !events[0].Key.has_value() && std::string_view(events[0].Source) == "received" &&
             state.r[1] == kItemDinsFire,
         "a received Din's Fire is no location and stays Din's Fire");
  receiving = false;
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(events.size() == 2U && events[1].Key.has_value() && state.r[1] == kItemRecoveryHeart &&
             events[1].OverrideGetItemId == kItemRecoveryHeart,
         "the fairy's Din's Fire becomes a heart and reports her location");
}
// closed_forest deku_only/off: the exit Kokiri sees the Kokiri Emerald for
// his test only.
void TestForestExitGuard() {
  constexpr std::uint32_t kTable = 0x08300000U;
  constexpr std::uint32_t kEmerald = 0x00040000U;
  FakeMemory memory;
  memory.Write32(kSaveContext + kSaveQuestItems, 0x00000001U);
  memory.Write32(kTable + 0x48U, kEmerald);
  CheckObserver observer{false};
  bool open = false;
  observer.SetForestExitProbe([&] { return open; });
  oot3d::recomp::a32::GuestState state{};
  state.r[8] = kSaveContext;
  state.r[6] = kTable;
  std::uint32_t quest = 0;
  observer.ObserveBlockEntry(kKokiriGuardCheckBlock, state, memory);
  memory.Read32(kSaveContext + kSaveQuestItems, &quest);
  Expect(quest == 0x1U && observer.Hits().ForestExit == 0U, "a closed forest changes nothing");
  open = true;
  observer.ObserveBlockEntry(kKokiriGuardCheckBlock, state, memory);
  memory.Read32(kSaveContext + kSaveQuestItems, &quest);
  Expect(quest == (0x1U | kEmerald) && observer.Hits().ForestExit == 1U, "the test sees the emerald");
  observer.ObserveBlockEntry(kKokiriGuardMovesBlock, state, memory);
  memory.Read32(kSaveContext + kSaveQuestItems, &quest);
  Expect(quest == 0x1U, "the save is restored right after");
  state.r[8] = 0x08400000U; // not the save context: left alone
  observer.ObserveBlockEntry(kKokiriGuardCheckBlock, state, memory);
  observer.ObserveBlockEntry(kKokiriGuardStaysBlock, state, memory);
  memory.Read32(kSaveContext + kSaveQuestItems, &quest);
  Expect(quest == 0x1U && observer.Hits().ForestExit == 1U, "an unexpected register changes nothing");
}
// Tokens: the save's gsFlags name every collected token, and a token's own
// Item_Give takes its key from the token's offer even with a chest offering
// in between.
void TestTokenShuffle() {
  std::array<std::uint8_t, kSaveGsFlagCount> flags{};
  flags[0] = 0x05U;  // Deku Tree: bits 0 and 2
  flags[21] = 0x80U;
  const auto keys = TokenKeys(flags);
  Expect(keys.size() == 3U && keys[0] == LocationKey{0x00U, OverrideType::Skull, 0x01U} &&
             keys[1] == LocationKey{0x00U, OverrideType::Skull, 0x04U} &&
             keys[2] == LocationKey{0x15U, OverrideType::Skull, 0x80U},
         "gsFlags bits are skull keys");

  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kToken = 0x08100000U;
  constexpr std::uint32_t kChest = 0x08180000U;
  constexpr std::uint32_t kPlayer = 0x08200000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x00U);
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  memory.Write16(kToken + kActorId, kActorSkullToken);
  memory.Write16(kToken + kActorParams, 0x0004U);
  memory.Write16(kChest + kActorId, kActorChest);
  memory.Write16(kChest + kActorParams, 0x5906U);
  memory.Write8(kSaveRespawnReturnData, 0U);
  memory.Write8(kPlayer + kPlayerGetItemId, 0U);
  memory.Write32(kPlayer + kPlayerInteractRangeActor, 0U);
  CheckObserver observer{false};
  std::vector<CheckEvent> events;
  std::vector<LocationKey> asked;
  observer.SetSink([&](const CheckEvent &event) { events.push_back(event); });
  observer.SetDirectGiveOverride([&](const LocationKey &key, oot3d::recomp::a32::MemoryBus &) {
    asked.push_back(key);
    return std::optional<std::uint8_t>(kItemRecoveryHeart);
  });
  oot3d::recomp::a32::GuestState state{};
  state.r[0] = kToken;
  state.r[1] = kPlay;
  state.r[2] = 0x5BU;
  observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  state.r[0] = kChest; // the chest beside it offers every frame
  state.r[2] = 0x48U;
  observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  state.r[0] = kPlay;
  state.r[1] = kItemSkullToken;
  observer.ObserveBlockEntry(kItemGive, state, memory);
  Expect(asked.size() == 1U && asked[0] == LocationKey{0x00U, OverrideType::Skull, 0x04U} &&
             state.r[1] == kItemRecoveryHeart,
         "the token asks with its own key and becomes a heart");
  Expect(events.size() == 1U && events[0].Key.has_value() &&
             *events[0].Key == LocationKey{0x00U, OverrideType::Skull, 0x04U},
         "the token reports its own location, not the chest");
}
// Cows: numbered as EnCow_rInit numbers them, keyed 0x14 + number, and given
// a bottle while their location holds an item.
void TestCows() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kCowOne = 0x08100000U;
  constexpr std::uint32_t kTail = 0x08110000U;
  constexpr std::uint32_t kCowTwo = 0x08120000U;
  constexpr std::uint32_t kPlayer = 0x08200000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, 0x36U); // Lon Lon Ranch stables
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  memory.Write8(kSaveRespawnReturnData, 0U);
  for (const std::uint32_t cow : {kCowOne, kCowTwo})
    memory.Write16(cow + kActorId, kActorCow);
  CheckObserver observer{false};
  std::vector<LocationKey> asked;
  bool gives = true;
  observer.SetCowProbe([&](const LocationKey &key) {
    asked.push_back(key);
    return gives;
  });
  std::vector<LocationKey> offered;
  observer.SetOverride([&](const LocationKey &key, oot3d::recomp::a32::MemoryBus &) {
    offered.push_back(key);
    return std::optional<std::uint16_t>{};
  });
  oot3d::recomp::a32::GuestState state{};
  state.r[1] = kPlay;
  for (const std::uint32_t actor : {kCowOne, kTail, kCowTwo}) {
    state.r[0] = actor;
    observer.ObserveBlockEntry(kEnCowInit, state, memory);
  }
  state.r[0] = kCowTwo;
  state.r[1] = kPlay;
  state.r[2] = 0x50U; // GI_MILK
  observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  Expect(offered.size() == 1U && offered[0] == LocationKey{0x36U, OverrideType::BaseItem, 0x16U},
         "the second cow offers as base item 0x16");
  Expect(state.r[2] == 0x50U, "the offered milk stays without an override");

  state.r[0] = 0U; // no empty bottle
  state.r[4] = kCowOne;
  state.r[5] = kPlay;
  observer.ObserveBlockEntry(kCowBottleCheckBlock, state, memory);
  Expect(state.r[0] == 1U && asked.size() == 1U && asked[0] == LocationKey{0x36U, OverrideType::BaseItem, 0x15U},
         "the first cow's open location stands in for a bottle");
  gives = false;
  state.r[0] = 0U;
  observer.ObserveBlockEntry(kCowBottleCheckBlock, state, memory);
  Expect(state.r[0] == 0U, "a done cow needs a bottle again");

  state.r[0] = kCowOne;
  observer.ObserveBlockEntry(kEnCowDestroy, state, memory);
  memory.Write16(kPlay + kPlaySceneNum, kSceneGrottos);
  memory.Write8(kSaveRespawnReturnData, kSceneHyruleField);
  state.r[0] = kCowOne;
  state.r[1] = kPlay;
  observer.ObserveBlockEntry(kEnCowInit, state, memory);
  gives = true;
  state.r[0] = 0U;
  state.r[4] = kCowOne;
  observer.ObserveBlockEntry(kCowBottleCheckBlock, state, memory);
  Expect(asked.back() == LocationKey{kSceneGrottos, OverrideType::BaseItem, 0x16U},
         "the Hyrule Field grotto's cow is number 2");
}
// Shops: slot keys, shop ware get-items, and a shelf item sold, bought and
// sold out.
void TestShops() {
  Expect(ShopSlotKey(0x2DU, 0U, 0) == LocationKey{0x2DU, OverrideType::BaseItem, 0x30U}, "KF Shop Item 1");
  Expect(ShopSlotKey(kSceneBazaar, 7U, 0x052C) == LocationKey{kSceneBazaar, OverrideType::BaseItem, 0x37U},
         "MK Bazaar Item 8");
  Expect(ShopSlotKey(kSceneBazaar, 0U, kEntranceKakarikoBazaar) ==
             LocationKey{kSceneBazaar, OverrideType::BaseItem, 0x38U},
         "Kak Bazaar Item 1");

  FakeMemory memory;
  memory.Write32(kShopItemEntries + 13U * kShopItemEntrySize + kShopItemEntryGetItem, 0x29U);
  Expect(ShopItemGetItem(13U, memory) == std::uint16_t{0x29U}, "Buy Deku Shield is GI_SHIELD_DEKU");
  Expect(!ShopItemGetItem(60U, memory).has_value(), "no shop item past the table");

  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kShelf = 0x08100000U;
  memory.Write16(kPlay + kPlaySceneNum, 0x2DU);
  memory.Write32(kSaveContext + kSaveEntranceIndex, 0x00C1U);
  memory.Write16(kSaveContext + kSaveRupees, 50U);
  for (std::uint32_t i = 0; i < 8U; ++i)
    memory.Write16(kShelf + i * 0x200U + kActorParams, static_cast<std::uint16_t>(13U));
  CheckObserver observer{false};
  std::vector<LocationKey> bought;
  bool sold = false;
  observer.SetShopProbe([&](const LocationKey &key, oot3d::recomp::a32::MemoryBus &) {
    CheckObserver::ShopSlotState state;
    if (key.Flag == 0x32U) {
      state.Type = sold ? CheckObserver::ShopSlotState::Kind::SoldOut : CheckObserver::ShopSlotState::Kind::Sell;
      state.Item = ShopSlotItem{0x0BU, 40};
    }
    return state;
  });
  observer.SetShopBoughtSink([&](const LocationKey &key) { bought.push_back(key); });
  oot3d::recomp::a32::GuestState state{};
  state.r[1] = kPlay;
  for (std::uint32_t i = 0; i < 8U; ++i) {
    state.r[0] = kShelf + i * 0x200U;
    observer.ObserveBlockEntry(kEnGirlAInit, state, memory);
  }
  const std::uint32_t third = kShelf + 2U * 0x200U;
  state.r[0] = third;
  observer.ObserveBlockEntry(kEnGirlADraw, state, memory);
  std::uint32_t getItem = 0, buyEvent = 0;
  std::uint16_t price = 0;
  memory.Read32(third + kGirlGetItemId, &getItem);
  memory.Read16(third + kGirlBasePrice, &price);
  memory.Read32(third + kGirlBuyEventFunc, &buyEvent);
  Expect(getItem == 0x0BU && price == 40U && buyEvent == kShopBuyEventDefault, "the third slot sells the seed's item");

  state.r[0] = 2U; // CANBUY_RESULT_CANT_GET_NOW from the game's own test
  state.r[6] = third;
  observer.ObserveBlockEntry(kShopCanBuyResultBlocks[3], state, memory);
  Expect(state.r[0] == kCanBuySuccessFanfare, "50 rupees buy a 40 rupee slot");
  memory.Write16(kSaveContext + kSaveRupees, 20U);
  state.r[0] = 0U;
  observer.ObserveBlockEntry(kShopCanBuyResultBlocks[0], state, memory);
  Expect(state.r[0] == kCanBuyNeedRupees, "20 rupees do not");
  state.r[0] = 2U;
  state.r[6] = kShelf; // a slot of the game's own
  observer.ObserveBlockEntry(kShopCanBuyResultBlocks[0], state, memory);
  Expect(state.r[0] == 2U, "the game's own slots keep their test");

  state.r[0] = kPlay;
  state.r[1] = third;
  observer.ObserveBlockEntry(kShopBuyEventDefault, state, memory);
  std::uint16_t params = 0;
  memory.Read16(third + kActorParams, &params);
  Expect(bought.size() == 1U && bought[0] == LocationKey{0x2DU, OverrideType::BaseItem, 0x32U} &&
             params == kShopItemSoldOut && observer.Hits().ShopBuys == 1U,
         "buying reports KF Shop Item 3 and sells it out");
  observer.ObserveBlockEntry(kShopBuyEventDefault, state, memory);
  Expect(bought.size() == 1U, "a sold-out slot reports nothing more");

  // The shop again: the bought slot starts sold out.
  sold = true;
  for (std::uint32_t i = 0; i < 8U; ++i)
    memory.Write16(kShelf + i * 0x200U + kActorParams, 13U);
  state.r[1] = kPlay;
  for (std::uint32_t i = 0; i < 8U; ++i) {
    state.r[0] = kShelf + i * 0x200U;
    observer.ObserveBlockEntry(kEnGirlAInit, state, memory);
  }
  memory.Read16(third + kActorParams, &params);
  Expect(params == kShopItemSoldOut, "a bought slot inits sold out");
}
// Bridge conditions and starting hearts from slot data.
void TestBridgeAndHearts() {
  const auto medallions = ParseWorldState(R"({"rainbow_bridge":3,"rainbow_bridge_medallions_required":2,"starting_hearts":5})");
  Expect(medallions.BridgeCondition == WorldState::Bridge::Medallions && medallions.BridgeCount == 2 &&
             medallions.StartingHearts == 5 && !medallions.Empty(),
         "a medallion bridge and five hearts are read");
  Expect(ParseWorldState(R"({"rainbow_bridge":7})").BridgeCondition == WorldState::Bridge::Game,
         "Greg's bridge stays the game's");
  Expect(ParseWorldState(R"({"starting_hearts":3})").StartingHearts == 0, "three hearts are the game's");

  FakeMemory memory;
  for (std::uint32_t i = 0; i < 14U; ++i)
    memory.Write16(kSaveContext + kSaveEventChkInf + i * 2U, 0U);
  memory.Write32(kSaveContext + kSaveQuestItems, 0x00040001U); // Kokiri's Emerald, Forest Medallion
  memory.Write16(kSaveContext + kSaveGsTokens, 12U);
  Expect(BridgeProgress(WorldState::Bridge::Stones, memory) == 1 &&
             BridgeProgress(WorldState::Bridge::Medallions, memory) == 1 &&
             BridgeProgress(WorldState::Bridge::Rewards, memory) == 2 &&
             BridgeProgress(WorldState::Bridge::Tokens, memory) == 12,
         "stones, medallions, rewards and tokens are counted");
  memory.Write16(kSaveContext + kSaveEventChkInf, 0x0080U); // event 0x07, the Deku Tree
  Expect(BridgeProgress(WorldState::Bridge::Dungeons, memory) == 1, "the Deku Tree counts as a dungeon");

  Expect(ApplyWorldState(medallions, memory) == 0, "one medallion builds no bridge");
  memory.Write32(kSaveContext + kSaveQuestItems, 0x00040003U);
  Expect(ApplyWorldState(medallions, memory) == 1, "two medallions build it");
  std::uint16_t row4 = 0;
  memory.Read16(kSaveContext + kSaveEventChkInf + 4U * 2U, &row4);
  Expect((row4 & 0x2000U) != 0U, "the bridge flag is set");

  Expect(ApplyStartingHearts(medallions, memory), "five hearts are written");
  std::uint16_t capacity = 0, health = 0;
  memory.Read16(kSaveContext + kSaveHealthCapacity, &capacity);
  memory.Read16(kSaveContext + kSaveHealth, &health);
  Expect(capacity == 80U && health == 80U, "five full hearts");
}

void TestFastChests() {
  FakeMemory memory;
  CheckObserver observer{false};
  oot3d::recomp::a32::GuestState state{};
  state.r[0] = kItemNone; // a new item: the long animation
  observer.ObserveBlockEntry(kFastChestsObtainabilityBlock, state, memory);
  Expect(state.r[0] == kItemNone && observer.Hits().FastChests == 0U, "off by default");
  observer.SetFastChests(true);
  observer.ObserveBlockEntry(kFastChestsObtainabilityBlock, state, memory);
  Expect(state.r[0] != kItemNone && observer.Hits().FastChests == 1U, "on, a new item opens fast");
  state.r[0] = 0x05U; // already obtainable: short anyway
  observer.ObserveBlockEntry(kFastChestsObtainabilityBlock, state, memory);
  Expect(state.r[0] == 0x05U && observer.Hits().FastChests == 1U, "a short opening is left alone");
}

void TestSongReplays() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  FakeMemory memory;
  memory.Write32(kGlobalContextPointer, kPlay);
  memory.Write16(kPlay + kPlaySceneNum, 0x55U);
  memory.Write16(kPlay + kPlayLastPlayedSong, 9U);
  const auto mode = [&] {
    std::uint8_t value = 0;
    (void)memory.Read8(kPlay + kPlayMsgMode, &value);
    return value;
  };
  memory.Write8(kPlay + kPlayMsgMode, kMsgModeReplay);
  Expect(!SkipSongReplay(SongReplayMode::Off, memory) && mode() == kMsgModeReplay, "off leaves the replay");
  Expect(SkipSongReplay(SongReplayMode::Skip, memory) && mode() == kMsgModeReplayEnd, "the replay is skipped");
  Expect(!SkipSongReplay(SongReplayMode::Skip, memory) && mode() == kMsgModeReplayEnd, "its end is left alone");
  memory.Write8(kPlay + kPlayMsgMode, kMsgModeReplayMusic);
  Expect(!SkipSongReplay(SongReplayMode::Skip, memory) && mode() == kMsgModeReplayMusic, "skip keeps the music");
  Expect(SkipSongReplay(SongReplayMode::SkipNoSound, memory) && mode() == kMsgModeReplayEnd, "no sound skips it");
  memory.Write8(kPlay + kPlayMsgMode, kMsgModeReplayMusic);
  memory.Write16(kPlay + kPlayLastPlayedSong, kSongScarecrow);
  Expect(!SkipSongReplay(SongReplayMode::SkipNoSound, memory), "the scarecrow's music spawns Pierre");
  memory.Write16(kPlay + kPlaySceneNum, kSceneWaterTemple);
  memory.Write16(kPlay + kPlayLastPlayedSong, kSongZeldasLullaby);
  memory.Write8(kPlay + kPlayMsgMode, kMsgModeReplay);
  int frames = 0;
  while (SkipSongReplay(SongReplayMode::Skip, memory))
    ++frames;
  Expect(frames == 4 && mode() == kMsgModeReplayEnd, "the Water Temple's lullaby goes through modes 20 to 22");
  memory.Write8(kPlay + kPlayMsgMode, 0U);
  Expect(!SkipSongReplay(SongReplayMode::SkipNoSound, memory) && mode() == 0U, "no song, nothing changes");
  Expect(ParseSongReplayMode("skip_no_sound") == SongReplayMode::SkipNoSound &&
             !ParseSongReplayMode("fast").has_value(),
         "song replay modes parse by name");

  CheckObserver observer{false};
  oot3d::recomp::a32::GuestState state{};
  constexpr std::uint32_t kBlock = 0x08100000U;
  state.r[0] = kBlock + 0x100U;
  const auto timer = [&] {
    std::uint16_t value = 0;
    (void)memory.Read16(state.r[0] + kSongBlockTimer, &value);
    return value;
  };
  memory.Write16(state.r[0] + kSongBlockTimer, kSongBlockTimerReplay);
  observer.ObserveBlockEntry(kTimeBlockTimerBlock, state, memory);
  Expect(timer() == kSongBlockTimerReplay, "the blocks wait for the replay by default");
  observer.SetSongReplays(true);
  observer.ObserveBlockEntry(kTimeBlockTimerBlock, state, memory);
  Expect(timer() == kSongBlockTimerSkipped, "a Song of Time block waits less");
  memory.Write16(state.r[0] + kSongBlockTimer, kSongBlockTimerReplay);
  observer.ObserveBlockEntry(kWarpBlockTimerBlock, state, memory);
  Expect(timer() == kSongBlockTimerSkipped && observer.Hits().SongBlocks == 2U, "a warp block too");
  memory.Write16(state.r[0] + kSongBlockTimer, 0x50U);
  observer.ObserveBlockEntry(kWarpBlockTimerBlock, state, memory);
  Expect(timer() == 0x50U && observer.Hits().SongBlocks == 2U, "a running timer is left alone");
}

void TestDamageMultiplier() {
  {
    DamageScaler scaler;
    Expect(scaler.Apply(DamageMultiplier::Normal, -4, false) == -4, "x1 leaves damage alone");
    Expect(scaler.Apply(DamageMultiplier::Double, -4, false) == -8, "x2 doubles a heart");
    Expect(scaler.Apply(DamageMultiplier::Sexdecuple, -16, false) == -256, "x16");
    Expect(scaler.Apply(DamageMultiplier::Half, -4, false) == -2, "x1/2 halves");
    Expect(scaler.Apply(DamageMultiplier::OneHitKo, -1, false) == kOneHitKoChange, "one hit KO");
    Expect(scaler.Apply(DamageMultiplier::OneHitKo, 16, false) == 16, "healing is left alone");
    Expect(scaler.Apply(DamageMultiplier::Double, 0, false) == 0, "no change stays none");
    const std::int32_t twice = scaler.Apply(DamageMultiplier::Half, -1, false) + scaler.Apply(DamageMultiplier::Half, -1, false);
    Expect(twice == -1, "half a quarter heart alternates");
  }
  {
    DamageScaler scaler;
    const std::int32_t first = scaler.Apply(DamageMultiplier::Half, -2, true);
    const std::int32_t second = scaler.Apply(DamageMultiplier::Half, -2, true);
    Expect(first + second == -2 && (first == -2 || second == -2), "double defense alternates -2 and 0");
  }
  Expect(ParseDamageMultiplier("ohko") == DamageMultiplier::OneHitKo && !ParseDamageMultiplier("x3").has_value(),
         "damage multipliers parse by name");

  FakeMemory memory;
  CheckObserver observer{false};
  oot3d::recomp::a32::GuestState state{};
  state.r[1] = static_cast<std::uint32_t>(-4);
  observer.ObserveBlockEntry(kHealthChangeBy, state, memory);
  Expect(state.r[1] == static_cast<std::uint32_t>(-4) && observer.Hits().Damage == 0U, "x1 by default");
  observer.SetDamageMultiplier(DamageMultiplier::Quadruple);
  observer.ObserveBlockEntry(kHealthChangeBy, state, memory);
  Expect(state.r[1] == static_cast<std::uint32_t>(-16) && observer.Hits().Damage == 1U, "x4 at Health_ChangeBy");
  state.r[1] = 0x10U;
  observer.ObserveBlockEntry(kHealthChangeBy, state, memory);
  Expect(state.r[1] == 0x10U && observer.Hits().Damage == 1U, "a heart refill is left alone");
}

void TestItemText() {
  const std::string white("\x7F\x1D\x00", 3);
  const std::string end("\x7F\x00", 2);
  Expect(FormatItemFoundText("Hookshot", "Alice") ==
             "You found \x7F\x1D\x41Hookshot " + white + "for \x7F\x1D\x43" "Alice" + white + "!" + end,
         "another player's item in red, the player in blue");
  Expect(FormatItemFoundText("Small Key", "") == "You found \x7F\x1D\x41Small Key" + white + "!" + end,
         "this world's item");
  Expect(FormatItemFoundText("", "Bob").find("an item") != std::string::npos, "an unnamed item");
  Expect(FontSafe("Pok\xC3\xA9mon \xE2\x98\x83", 64U) == "Pok\xC3\xA9mon ?", "the font shows é, not a snowman");
  Expect(FontSafe("abcdef", 3U) == "abc", "cut to the byte limit");
  Expect(TextPixelWidth("You") == 10U + 9U + 7U, "pixel widths");

  // A long name wraps, every line fits the box, and there are at most four.
  const std::string text = FormatItemFoundText(
      "Progressive Super Long Item Name From Another Game That Goes On And On Forever",
      "A Player With A Long Name");
  std::size_t lines = 1;
  std::size_t start = 0;
  for (std::size_t newline = text.find("\x7F\x1C", start);; newline = text.find("\x7F\x1C", start)) {
    std::string line = text.substr(start, newline == std::string::npos ? std::string::npos : newline - start);
    // Drop control codes before measuring.
    for (std::size_t code = line.find('\x7F'); code != std::string::npos; code = line.find('\x7F'))
      line.erase(code, line[code + 1U] == '\x1D' ? 3U : 2U);
    Expect(TextPixelWidth(line) <= kTextLineWidth, "each line fits the box");
    if (newline == std::string::npos)
      break;
    ++lines;
    start = newline + 2U;
  }
  Expect(lines >= 2U && lines <= kTextMaximumLines, "a long text wraps into at most four lines");
  Expect(text.ends_with(end), "the text ends with the message end");

  // The entry keeps the original header and points every language at the
  // text, relative to the file's base.
  FakeMemory memory;
  constexpr std::uint32_t kOriginal = 0x08000000U;
  constexpr std::uint32_t kBase = 0x08100000U;
  for (std::uint32_t i = 0; i < 4U; ++i)
    memory.Write32(kOriginal + i * 4U, 0x55U + i);
  const std::string body = FormatItemFoundText("Bow", "Carol");
  Expect(WriteMessageEntry(memory, kOriginal, kBase, body, kClientMessageEntry, kClientMessageText,
                           kClientMessageTextCapacity),
         "the entry is written");
  std::uint32_t id = 0, type = 0, offset = 0, length = 0;
  (void)memory.Read32(kClientMessageEntry, &id);
  (void)memory.Read32(kClientMessageEntry + 8U, &type);
  (void)memory.Read32(kClientMessageEntry + kMessageEntryHeader + 9U * 8U, &offset);
  (void)memory.Read32(kClientMessageEntry + kMessageEntryHeader + 9U * 8U + 4U, &length);
  Expect(id == 0x55U && type == 0x57U, "the header is the original's");
  Expect(kBase + offset == kClientMessageText && length % 4U == 0U && length >= body.size(),
         "Message_GetText finds the text");
  std::uint8_t first = 0;
  (void)memory.Read8(kClientMessageText, &first);
  Expect(first == 'Y', "the text is in the page");
  Expect(!WriteMessageEntry(memory, kOriginal, kBase, std::string(2000U, 'a'), kClientMessageEntry,
                            kClientMessageText, kClientMessageTextCapacity),
         "a text too long for the page is refused");

  // The observer asks at both lookups with the entry's id and the base.
  CheckObserver observer{false};
  oot3d::recomp::a32::GuestState state{};
  constexpr std::uint32_t kOwner = 0x08200000U;
  memory.Write32(kOwner + kMessageObjectOffset + kMessageObjectTextBase, kBase);
  std::uint32_t askedId = 0, askedBase = 0;
  std::optional<LocationKey> askedOffer;
  observer.SetMessageOverride([&](std::uint32_t textId, std::uint32_t, std::uint32_t textBase,
                                  const std::optional<LocationKey> &offer,
                                  oot3d::recomp::a32::MemoryBus &) -> std::optional<std::uint32_t> {
    askedId = textId;
    askedBase = textBase;
    askedOffer = offer;
    return textId == kTextRecoveryHeart ? std::optional<std::uint32_t>(kClientMessageEntry) : std::nullopt;
  });
  state.r[0] = kOriginal;
  state.r[5] = kOwner;
  observer.ObserveBlockEntry(kMessageEntryOneBlock, state, memory);
  Expect(state.r[0] == kClientMessageEntry && askedId == kTextRecoveryHeart && askedBase == kBase,
         "the heart's text is replaced");
  state.r[0] = kOriginal + 4U; // id 0x56
  state.r[4] = kOwner;
  observer.ObserveBlockEntry(kMessageEntryTwoBlock, state, memory);
  Expect(state.r[0] == kOriginal + 4U && askedId == 0x56U && observer.Hits().Messages == 1U,
         "other texts are left alone");
  state.r[0] = 0U;
  askedId = 0U;
  observer.ObserveBlockEntry(kMessageEntryOneBlock, state, memory);
  Expect(state.r[0] == 0U && askedId == 0U, "a text not found is not asked about");
  Expect(!askedOffer.has_value(), "no offer, no location");

  // Two chests offer; the text is asked with the one Link interacts with.
  constexpr std::uint32_t kPlay = 0x08300000U;
  constexpr std::uint32_t kPlayer = 0x08400000U;
  constexpr std::uint32_t kChestA = 0x08500000U;
  constexpr std::uint32_t kChestB = 0x08600000U;
  memory.Write32(kGlobalContextPointer, kPlay);
  memory.Write16(kPlay + kPlaySceneNum, 0x55U);
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  memory.Write8(kSaveRespawnReturnData, 0U);
  memory.Write16(kChestA + kActorId, kActorChest);
  memory.Write16(kChestA + kActorParams, 0x0760U); // flag 0
  memory.Write16(kChestB + kActorId, kActorChest);
  memory.Write16(kChestB + kActorParams, 0x0761U); // flag 1
  observer.SetOverride([](const LocationKey &, oot3d::recomp::a32::MemoryBus &) -> std::optional<std::uint16_t> {
    return kPlaceholderGetItem;
  });
  for (const std::uint32_t chest : {kChestA, kChestB}) {
    state.r[0] = chest;
    state.r[1] = kPlay;
    state.r[2] = 0x3BU;
    observer.ObserveBlockEntry(kActorOfferGetItem, state, memory);
  }
  memory.Write32(kPlayer + kPlayerInteractRangeActor, kChestA);
  state.r[0] = kOriginal;
  state.r[5] = kOwner;
  observer.ObserveBlockEntry(kMessageEntryOneBlock, state, memory);
  Expect(askedOffer.has_value() && *askedOffer == LocationKey{0x55U, OverrideType::Chest, 0x00U},
         "the text is the opened chest's, not the last offer's");
}

void TestCheats() {
  FakeMemory memory;
  for (std::size_t i = 0; i < kUpgradeCapacitiesExpected.size(); ++i)
    memory.Write16(kUpgradeCapacities + static_cast<std::uint32_t>(i) * 2U, kUpgradeCapacitiesExpected[i]);
  memory.Write16(kSaveContext + kSaveHealthCapacity, 0x60U);
  memory.Write16(kSaveContext + kSaveHealth, 0x10U);
  memory.Write8(kSaveContext + kSaveMagicAcquired, 0U);
  memory.Write8(kSaveContext + kSaveMagicLevel, 0U);
  memory.Write8(kSaveContext + kSaveMagic, 0U);
  memory.Write16(kSaveContext + kSaveRupees, 5U);
  for (std::uint32_t slot = 0; slot < 24U; ++slot)
    memory.Write8(kSaveContext + kSaveItems + slot, kItemNone);
  for (std::uint32_t slot = 0; slot < 15U; ++slot)
    memory.Write8(kSaveContext + kSaveAmmo + slot, 0U);
  memory.Write8(kSaveContext + kSaveItems + 0x00U, 0x00U); // Deku Sticks, no upgrade yet
  memory.Write8(kSaveContext + kSaveItems + 0x02U, 0x02U); // Bombs
  memory.Write8(kSaveContext + kSaveItems + kSlotBombchu, 0x09U);
  // Bomb bag 2 (30), wallet 1 (200).
  memory.Write32(kSaveContext + kSaveUpgrades, (2U << 3U) | (1U << 12U));

  const auto read8 = [&](std::uint32_t offset) {
    std::uint8_t value = 0;
    memory.Read8(kSaveContext + offset, &value);
    return value;
  };
  const auto read16 = [&](std::uint32_t offset) {
    std::uint16_t value = 0;
    memory.Read16(kSaveContext + offset, &value);
    return value;
  };

  CheatApplier cheats;
  Expect(cheats.Apply({}, memory) == 0 && read16(kSaveHealth) == 0x10U, "no cheat writes nothing");
  CheatOptions all{true, true, true, true};
  cheats.Apply(all, memory);
  Expect(read16(kSaveHealth) == 0x60U, "health is filled to the capacity");
  Expect(read8(kSaveMagic) == 0U, "no magic without the meter");
  Expect(read8(kSaveAmmo + 0x00U) == 10U, "sticks without an upgrade hold 10");
  Expect(read8(kSaveAmmo + 0x02U) == 30U, "the second bomb bag holds 30");
  Expect(read8(kSaveAmmo + 0x03U) == 0U, "no arrows without the bow");
  Expect(read8(kSaveAmmo + kSlotBombchu) == 50U, "bombchus hold 50");
  Expect(read16(kSaveRupees) == 200U, "the adult wallet holds 200");
  Expect(cheats.Apply(all, memory) == 0, "full values are not written again");

  memory.Write8(kSaveContext + kSaveMagicAcquired, 1U);
  memory.Write8(kSaveContext + kSaveMagicLevel, 2U);
  memory.Write16(kSaveContext + kSaveHealth, 0U);
  cheats.Apply(all, memory);
  Expect(read8(kSaveMagic) == 0x60U, "double magic is filled");
  Expect(read16(kSaveHealth) == 0U, "a dead Link stays dead");

  // A different image: ammo and rupees stay off, health still works.
  FakeMemory other;
  for (std::size_t i = 0; i < kUpgradeCapacitiesExpected.size(); ++i)
    other.Write16(kUpgradeCapacities + static_cast<std::uint32_t>(i) * 2U, 1U);
  other.Write16(kSaveContext + kSaveHealthCapacity, 0x30U);
  other.Write16(kSaveContext + kSaveHealth, 0x08U);
  other.Write16(kSaveContext + kSaveRupees, 5U);
  other.Write32(kSaveContext + kSaveUpgrades, 0U);
  CheatApplier wrong;
  wrong.Apply(all, other);
  std::uint16_t rupees = 0, health = 0;
  other.Read16(kSaveContext + kSaveRupees, &rupees);
  other.Read16(kSaveContext + kSaveHealth, &health);
  Expect(!wrong.Disabled().empty() && rupees == 5U && health == 0x30U,
         "another capacity table turns ammo and rupees off");
}
void TestBlueWarpSkip() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  FakeMemory memory;
  memory.Write16(kPlay + kPlaySceneNum, kSceneDekuTreeBoss);
  memory.Write16(kSaveContext + kSaveEventChkInf, 0U);
  memory.Write16(kSaveContext + kSaveEventChkInf + 2U, 0U);
  memory.Write16(kSaveContext + kSaveEventChkInf + 4U, 0U);
  CheckObserver observer{false};
  std::vector<std::uint8_t> scenes;
  observer.SetBlueWarpSink([&](std::uint32_t pc, std::uint8_t scene, oot3d::recomp::a32::MemoryBus &bus) {
    Expect(pc == kChildBlueWarpBlock, "the sink learns the block");
    scenes.push_back(scene);
    const auto dungeon = SkipBlueWarpCutscene(scene, bus);
    Expect(dungeon == kDungeonDekuTree, "the Deku Tree warp is skipped once");
    Expect(!SkipBlueWarpCutscene(scene, bus).has_value(), "a second warp plays as usual");
  });
  oot3d::recomp::a32::GuestState state{};
  state.r[7] = kPlay;
  observer.ObserveBlockEntry(kChildBlueWarpBlock, state, memory);
  Expect(scenes.size() == 1U && scenes[0] == kSceneDekuTreeBoss && observer.Hits().BlueWarp == 1U,
         "the warp block passes the scene it leaves");
  std::uint16_t events0 = 0;
  std::uint16_t events1 = 0;
  memory.Read16(kSaveContext + kSaveEventChkInf, &events0);
  memory.Read16(kSaveContext + kSaveEventChkInf + 2U, &events1);
  Expect(events0 == 0x0280U && events1 == 0x1000U,
         "the Deku Tree's death, its cutscene and Mido's talk are set");
  Expect(SkipBlueWarpCutscene(kSceneDodongosCavernBoss, memory) == kDungeonDodongosCavern,
         "King Dodongo's warp is skipped");
  std::uint16_t events2 = 0;
  memory.Read16(kSaveContext + kSaveEventChkInf + 4U, &events2);
  Expect(events2 == 0x0020U, "event 0x25 is set");
  Expect(!SkipBlueWarpCutscene(kSceneJabuJabuBoss, memory).has_value(), "Jabu-Jabu's warp is not handled here");

  // Adult dungeons: the randomizer's event decides; Volvagia also clears the
  // cloud, Bongo Bongo also gives the medallion the game's code tests.
  for (std::uint32_t i = 0; i < 14U; ++i)
    memory.Write16(kSaveContext + kSaveEventChkInf + i * 2U, 0U);
  memory.Write32(kSaveContext + kSaveQuestItems, 0U);
  Expect(SkipBlueWarpCutscene(0x15U, memory) == 4U, "Volvagia's warp is the Fire Temple's");
  Expect(SkipBlueWarpCutscene(kSceneShadowTempleBoss, memory) == 7U, "Bongo Bongo's warp is the Shadow Temple's");
  Expect(!SkipBlueWarpCutscene(kSceneShadowTempleBoss, memory).has_value(), "a done dungeon warps as usual");
  std::uint16_t row2 = 0, row4 = 0;
  std::uint32_t quest = 0;
  memory.Read16(kSaveContext + kSaveEventChkInf + 2U * 2U, &row2);
  memory.Read16(kSaveContext + kSaveEventChkInf + 4U * 2U, &row4);
  memory.Read32(kSaveContext + kSaveQuestItems, &quest);
  Expect(row2 == 0x8000U && row4 == (0x0200U | 0x0040U) && quest == 0x10U,
         "events 0x49 and 0x46, the cloud and the Shadow Medallion are set");

  // Jabu-Jabu: the cutscene the game chose is cleared, nothing else.
  memory.Write16(kSaveContext + kSaveNextCutsceneIndex, 0xFFF0U);
  Expect(!ClearJabuJabuWarpCutscene(kSceneDekuTreeBoss, memory), "only Barinade's room");
  Expect(ClearJabuJabuWarpCutscene(kSceneJabuJabuBoss, memory), "Barinade's warp loses its cutscene");
  std::uint16_t next = 1;
  memory.Read16(kSaveContext + kSaveNextCutsceneIndex, &next);
  Expect(next == 0U && !ClearJabuJabuWarpCutscene(kSceneJabuJabuBoss, memory), "cleared once");

  // The adult block passes r8's play state.
  CheckObserver adult{false};
  std::vector<std::pair<std::uint32_t, std::uint8_t>> seen;
  adult.SetBlueWarpSink([&](std::uint32_t pc, std::uint8_t scene, oot3d::recomp::a32::MemoryBus &) {
    seen.emplace_back(pc, scene);
  });
  memory.Write16(0x08100000U + kPlaySceneNum, 0x16U);
  oot3d::recomp::a32::GuestState adultState{};
  adultState.r[8] = 0x08100000U;
  adult.ObserveBlockEntry(kAdultBlueWarpBlock, adultState, memory);
  Expect(seen.size() == 1U && seen[0] == std::make_pair(kAdultBlueWarpBlock, std::uint8_t{0x16U}),
         "the adult block reads the scene through r8");
}

void TestWorldState() {
  const auto vanilla = ParseWorldState(
      R"({"jabu_jabu":0,"zoras_fountain":0,"rainbow_bridge":0,"ganons_trials":1,"ganons_trials_count":6,)"
      R"("required_trials":["Forest Trial","Fire Trial","Water Trial","Shadow Trial","Spirit Trial","Light Trial"]})");
  Expect(vanilla.Empty(), "the game's own settings set nothing");
  Expect(ParseWorldState("not json").Empty() && ParseWorldState("{}").Empty(),
         "old or broken slot data sets nothing");

  const auto open = ParseWorldState(
      R"({"jabu_jabu":1,"zoras_fountain":2,"rainbow_bridge":1,"ganons_trials":1,"ganons_trials_count":2,)"
      R"("required_trials":["Light Trial","Fire Trial"]})");
  Expect(open.OpenJabuJabu && open.OpenZorasFountain && open.OpenRainbowBridge,
         "open settings are read");
  Expect(open.SkippedTrials.size() == 4U, "trials the seed does not require are skipped");
  Expect(!ParseWorldState(R"({"zoras_fountain":1})").OpenZorasFountain,
         "closed as child is not open");

  FakeMemory memory;
  for (std::uint32_t i = 0; i < 14U; ++i)
    memory.Write16(kSaveContext + kSaveEventChkInf + i * 2U, 0U);
  Expect(ApplyWorldState(open, memory) == 7, "three settings and four trials set flags");
  Expect(ApplyWorldState(open, memory) == 0, "a second pass sets nothing");
  std::uint16_t row3 = 0, row4 = 0, row10 = 0, row11 = 0, row12 = 0;
  memory.Read16(kSaveContext + kSaveEventChkInf + 3U * 2U, &row3);
  memory.Read16(kSaveContext + kSaveEventChkInf + 4U * 2U, &row4);
  memory.Read16(kSaveContext + kSaveEventChkInf + 10U * 2U, &row10);
  memory.Read16(kSaveContext + kSaveEventChkInf + 11U * 2U, &row11);
  memory.Read16(kSaveContext + kSaveEventChkInf + 12U * 2U, &row12);
  Expect(row3 == 0x0408U && row4 == 0x2000U, "Jabu-Jabu, King Zora and the bridge");
  Expect(row10 == 0x2000U && row11 == (0x0800U | 0x1000U | 0x2000U) && row12 == 0U,
         "Forest, Water, Shadow and Spirit done; the barrier stays");

  const auto gates = ParseWorldState(R"({"door_of_time":2,"kakariko_gate":1,"closed_forest":2})");
  Expect(gates.OpenDoorOfTime && gates.OpenKakarikoGate && gates.MidoMoved && gates.OpenForestExit,
         "door, gate and forest are read");
  const auto dekuOnly = ParseWorldState(R"({"door_of_time":1,"closed_forest":1})");
  Expect(!dekuOnly.OpenDoorOfTime && !dekuOnly.MidoMoved && dekuOnly.OpenForestExit && dekuOnly.Empty(),
         "song only is not open; deku only opens the exit, not Mido's path");

  const auto none = ParseWorldState(R"({"ganons_trials":0})");
  Expect(none.SkippedTrials.size() == 6U, "skip means every trial is done");
  Expect(ApplyWorldState(none, memory) == 3, "the last two trials and the barrier");
}

void TestProbeCountsOnlyItsPc() {
  FakeMemory memory;
  CheckObserver observer{false};
  observer.SetProbePc(0x003727F0U);
  oot3d::recomp::a32::GuestState state{};
  observer.ObserveBlockEntry(0x003727F0U, state, memory);
  observer.ObserveBlockEntry(0x003727F0U, state, memory);
  observer.ObserveBlockEntry(0x00100000U, state, memory);
  Expect(observer.Hits().Probe == 2U, "the probe counts its own pc only");
}

constexpr const char *kTableJson = R"({
  "format": "triaevum_archipelago_locations_v1",
  "locations": [
    {"id": 1000, "name": "KF Kokiri Sword Chest", "scene": 85, "type": 1, "flag": 0},
    {"id": 1001, "name": "Deku Tree Map Chest", "scene": 0, "type": 1, "flag": 3},
    {"id": 1002, "name": "Deku Tree MQ Map Chest", "scene": 0, "type": 1, "flag": 3,
     "master_quest": true}
  ]
})";

void TestLocationTable() {
  std::string error;
  const auto table = LocationTable::Parse(kTableJson, &error);
  Expect(table.has_value() && table->Size() == 3U, error.c_str());
  const LocationKey sword{0x55U, OverrideType::Chest, 0x00U};
  Expect(table->Find(sword, nullptr) == 1000, "a key finds its id");
  const LocationKey map{0x00U, OverrideType::Chest, 0x03U};
  Expect(table->Find(map, nullptr) == 1001, "unknown dungeon mode is vanilla");
  Expect(table->Find(map, [](std::uint8_t) { return true; }) == 1002,
         "a Master Quest dungeon finds its own id");
  Expect(!table->Find({0x55U, OverrideType::Chest, 0x01U}, nullptr).has_value(),
         "an unknown key has no id");
  Expect(table->ById(1002) != nullptr && table->ById(1002)->MasterQuest, "ids are indexed");

  Expect(!LocationTable::Parse(R"({"format":"other","locations":[]})", &error),
         "another format is rejected");
  Expect(!LocationTable::Parse(R"({"format":"triaevum_archipelago_locations_v1","locations":[
      {"id":1,"scene":0,"type":1,"flag":0},{"id":1,"scene":0,"type":1,"flag":1}]})", &error) &&
             error.find("duplicate id") != std::string::npos,
         "duplicate ids are rejected");
  Expect(!LocationTable::Parse(R"({"format":"triaevum_archipelago_locations_v1","locations":[
      {"id":1,"scene":0,"type":1,"flag":0},{"id":2,"scene":0,"type":1,"flag":0}]})", &error) &&
             error.find("duplicate key") != std::string::npos,
         "duplicate keys of one variant are rejected");
}

// The generated table resolves the keys seen in play test 1 and the five
// representative locations.
void TestGeneratedLocationTable(const char *path) {
  std::string error;
  const auto table = LocationTable::Load(path, &error);
  Expect(table.has_value(), error.c_str());
  const auto name = [&](LocationKey key) {
    const auto id = table->Find(key, nullptr);
    return id.has_value() ? table->ById(*id)->Name : std::string();
  };
  Expect(name({0x55U, OverrideType::Chest, 0x00U}) == "KF Kokiri Sword Chest", "Kokiri Sword");
  Expect(name({0x28U, OverrideType::Chest, 0x02U}) == "KF Mido Bottom Left Chest", "Mido chest");
  Expect(name({0x00U, OverrideType::Chest, 0x01U}) == "Deku Tree Slingshot Chest", "slingshot");
  Expect(name({0x00U, OverrideType::Chest, 0x06U}) == "Deku Tree Compass Room Side Chest",
         "compass room side chest");
  Expect(name({0x5FU, OverrideType::BaseItem, 0x47U}) == "HC Malon Egg", "Malon");
  Expect(name({0x54U, OverrideType::Collectable, 0x04U}) == "ZR Near Open Grotto Freestanding PoH",
         "heart piece");
  Expect(name({0x0CU, OverrideType::Skull, 0x02U}) == "KF GS Know It All House", "token");
  Expect(name(*KeyForDirectGive(0x62U)) == "Song from Saria", "song");
  // Master Quest variant of a shared key.
  const auto mq = table->Find({0x00U, OverrideType::Chest, 0x03U}, [](std::uint8_t) { return true; });
  Expect(mq.has_value() && table->ById(*mq)->Name == "Deku Tree MQ Map Chest", "MQ variant");
}

std::filesystem::path FreshDirectory(const char *name) {
  const auto directory = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  return directory;
}

void TestSidecarRoundTripAndMismatch() {
  const auto directory = FreshDirectory("triaevum_archipelago_sidecar_test");
  const SidecarIdentity identity{"AP 1234/x", 0, 2, 1};
  std::string error;
  auto fresh = LoadSidecar(directory, identity, &error);
  Expect(fresh.has_value() && fresh->AppliedItems == 0 && fresh->ReportedLocations.empty(),
         "a missing sidecar is a fresh state");
  fresh->AppliedItems = 7;
  fresh->AppliedAtLastSave = 5;
  fresh->ReportedLocations = {1000, 1001};
  fresh->Placements[1000] = Placement{3997700, 2, 1U};
  fresh->GivenAtLocation = {1000};
  fresh->GivenByServer = {1001};
  fresh->GoalReached = true;
  Expect(SaveSidecar(directory, *fresh, &error), error.c_str());
  Expect(SidecarPath(directory, identity).filename() == "AP_1234_x-0-2-save1.json",
         "the seed is reduced to a safe file name");
  const auto loaded = LoadSidecar(directory, identity, &error);
  Expect(loaded.has_value() && loaded->AppliedItems == 7 && loaded->AppliedAtLastSave == 5 &&
             loaded->ReportedLocations == std::set<std::int64_t>{1000, 1001} &&
             loaded->Placements.at(1000) == Placement{3997700, 2, 1U} &&
             loaded->GivenAtLocation == std::set<std::int64_t>{1000} &&
             loaded->GivenByServer == std::set<std::int64_t>{1001} && loaded->GoalReached,
         "the sidecar round-trips");

  // Another seed whose name reduces to the same file must not be adopted.
  const SidecarIdentity other{"AP 1234:x", 0, 2, 1};
  Expect(SidecarPath(directory, other) == SidecarPath(directory, identity),
         "the test needs a colliding file name");
  Expect(!LoadSidecar(directory, other, &error) &&
             error.find("another seed") != std::string::npos,
         "a sidecar of another seed is refused");
  const auto untouched = LoadSidecar(directory, identity, &error);
  Expect(untouched.has_value() && untouched->AppliedItems == 7, "a refused sidecar is untouched");
  std::filesystem::remove_all(directory);
}

void TestSidecarWriter() {
  const auto directory = FreshDirectory("triaevum_archipelago_writer_test");
  const SidecarIdentity first{"AP 1", 0, 1, 0}, second{"AP 1", 0, 1, 2};
  std::string error;
  {
    SidecarWriter writer(directory);
    SidecarState state;
    state.Identity = first;
    for (std::int64_t applied = 1; applied <= 50; ++applied) {
      state.AppliedItems = applied;
      writer.Write(state);
    }
    state.Identity = second;
    state.AppliedItems = 3;
    writer.Write(state);
    writer.WaitIdle();
    const auto loaded = LoadSidecar(directory, first, &error);
    Expect(loaded.has_value() && loaded->AppliedItems == 50, "the newest queued state is written");
    Expect(!writer.TakeError().has_value(), "no write error");
    state.AppliedItems = 4;
    writer.Write(state); // written by the destructor at the latest
  }
  const auto loaded = LoadSidecar(directory, second, &error);
  Expect(loaded.has_value() && loaded->AppliedItems == 4,
         "each save's file is written, the last one before the writer stops");
  std::filesystem::remove_all(directory);
}

// An empty save as the game starts one: item slots hold ITEM_NONE and the
// dungeon key counters -1.
void FillFreshSave(FakeMemory &memory) {
  for (std::uint32_t offset = 0; offset < 0x15C4U; ++offset)
    memory.Write8(kSaveContext + offset, 0U);
  for (std::uint32_t slot = 0; slot < 24U; ++slot)
    memory.Write8(kSaveContext + kSaveItems + slot, kItemNone);
  for (std::uint32_t dungeon = 0; dungeon < 19U; ++dungeon)
    memory.Write8(kSaveContext + kSaveDungeonKeys + dungeon, 0xFFU);
}

std::map<std::string, std::int64_t> ItemIds(const char *path) {
  std::ifstream input(path);
  const auto document = nlohmann::json::parse(input);
  std::map<std::string, std::int64_t> ids;
  for (const auto &item : document.at("items"))
    ids[item.at("name").get<std::string>()] = item.at("id").get<std::int64_t>();
  return ids;
}

// The generated item table against the randomizer's upgrades and effects.
void TestGeneratedItemTable(const char *path) {
  std::string error;
  const auto table = ItemTable::Load(path, &error);
  Expect(table.has_value(), error.c_str());
  const auto ids = ItemIds(path);
  FakeMemory memory;
  FillFreshSave(memory);

  const auto getItem = [&](const char *name) -> std::optional<std::uint16_t> {
    const auto plan = table->Plan(ids.at(name), memory);
    if (const auto *give = std::get_if<GiveGetItem>(&plan))
      return give->GetItemId;
    return std::nullopt;
  };
  Expect(getItem("Kokiri Sword") == 0x27U, "the Kokiri Sword is its own get-item");
  Expect(getItem("Progressive Hookshot") == 0x08U, "the first hookshot is the Hookshot");
  memory.Write8(kSaveContext + kSaveItems + kSlotHookshot, 0x0AU);
  Expect(getItem("Progressive Hookshot") == 0x09U, "with the Hookshot it is the Longshot");
  Expect(getItem("Progressive Strength Upgrade") == 0x54U, "the first strength is the Goron Bracelet");
  memory.Write32(kSaveContext + kSaveUpgrades, 1U << 6U);
  Expect(getItem("Progressive Strength Upgrade") == 0x35U, "then the Silver Gauntlets");

  const auto direct = [&](const char *name) -> std::optional<GiveDirect> {
    const auto plan = table->Plan(ids.at(name), memory);
    if (const auto *give = std::get_if<GiveDirect>(&plan))
      return *give;
    return std::nullopt;
  };
  const auto key = direct("Forest Temple Small Key");
  Expect(key.has_value() && key->Effect == ItemEffect::GiveSmallKey && key->Arg1 == 3,
         "a dungeon key counts for its own dungeon (DUNGEON_FOREST_TEMPLE)");
  Expect(ApplyDirect(*key, memory) && ApplyDirect(*key, memory), "keys are written");
  std::uint8_t keys = 0;
  memory.Read8(kSaveContext + kSaveDungeonKeys + 3U, &keys);
  Expect(keys == 2U, "the counter starts at -1 and counts from 0");

  const auto magic = direct("Progressive Magic Meter");
  Expect(magic.has_value() && magic->Effect == ItemEffect::GiveMagic, "the first magic is the meter");
  Expect(ApplyDirect(*magic, memory), "magic is written");
  const auto doubleMagic = direct("Progressive Magic Meter");
  Expect(doubleMagic.has_value() && doubleMagic->Effect == ItemEffect::GiveDoubleMagic,
         "with magic, the next is double magic");

  const auto lullaby = direct("Zelda's Lullaby");
  Expect(lullaby.has_value() && lullaby->Effect == ItemEffect::GiveSong, "songs are quest bits");
  Expect(ApplyDirect(*lullaby, memory) && ApplyDirect(*lullaby, memory), "songs are written");
  std::uint32_t quest = 0;
  memory.Read32(kSaveContext + kSaveQuestItems, &quest);
  Expect(quest == (1U << static_cast<std::uint32_t>(lullaby->Arg1)), "a song sets its bit once");

  const auto bugs = direct("Bottle with Bugs");
  Expect(bugs.has_value() && bugs->Effect == ItemEffect::GiveBottle && ApplyDirect(*bugs, memory),
         "a filled bottle goes into the first free bottle slot");
  std::uint8_t bottle = 0;
  memory.Read8(kSaveContext + kSaveItems + kSlotBottle1, &bottle);
  Expect(bottle == static_cast<std::uint8_t>(bugs->Arg1), "the bottle holds the bugs");

  const auto soul = table->Plan(ids.at("Gohma Soul"), memory);
  Expect(std::holds_alternative<GiveUnsupported>(soul), "enemy souls are outside the client");
}

// Link stands ready; the receiver offers its actor, Player takes it.
void TestReceiverGivesThroughPlayer() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  constexpr std::uint32_t kPlayer = 0x08200000U;
  FakeMemory memory;
  memory.Write32(kGlobalContextPointer, kPlay);
  memory.Write32(kPlay + kPlayPlayerActor, kPlayer);
  for (const std::uint32_t offset : {kPlayTitleDelay, kPlayTitleDuration, kPlayTitleAlpha, kPlaySceneLoadFlag})
    memory.Write8(kPlay + offset, 0U);
  memory.Write32(kPlayer + kPlayerStateFlags1, 0U);
  memory.Write32(kPlayer + kPlayerStateFlags2, 0U);
  memory.Write32(kPlayer + kPlayerInteractRangeActor, 0U);
  memory.Write8(kPlayer + kPlayerGetItemId, 0U);
  memory.Write32(kPlayer + kActorDraw, 0x00123456U);
  memory.Write16(kPlayer + kActorBgCheckFlags, kBgCheckGround);

  int maps = 0;
  ItemReceiver receiver([&](std::uint32_t base, std::uint32_t, std::string *) {
    ++maps;
    memory.Write8(base, 0U);
    return true;
  });
  std::string error;
  memory.Write32(kPlayer + kPlayerStateFlags1, 0x20000000U); // a busy state
  Expect(receiver.Give(0x08U, memory, &error) == ItemReceiver::Result::Waiting, "a busy Link waits");
  std::uint32_t interacting = 0;
  memory.Read32(kPlayer + kPlayerInteractRangeActor, &interacting);
  Expect(interacting == 0U && maps == 1, "nothing is offered while busy; the page is mapped once");

  memory.Write32(kPlayer + kPlayerStateFlags1, 0U);
  Expect(receiver.Give(0x08U, memory, &error) == ItemReceiver::Result::Waiting, "offered");
  std::uint8_t getItem = 0;
  memory.Read32(kPlayer + kPlayerInteractRangeActor, &interacting);
  memory.Read8(kPlayer + kPlayerGetItemId, &getItem);
  Expect(interacting == kClientRegionBase && getItem == 0x08U, "the item actor is offered to Player");
  Expect(receiver.Give(0x08U, memory, &error) == ItemReceiver::Result::Waiting,
         "a standing offer is renewed");

  memory.Write32(kClientRegionBase + kActorParent, kPlayer); // Player took it
  Expect(receiver.Give(0x08U, memory, &error) == ItemReceiver::Result::Given, "the take is seen");
  std::uint32_t parent = 1;
  memory.Read32(kClientRegionBase + kActorParent, &parent);
  Expect(parent == 0U, "the actor is free for the next item");

  // A savestate of a run without the page: it is mapped again.
  FakeMemory restored;
  Expect(receiver.EnsurePage(restored, &error) && maps == 2, "a page lost with a savestate is mapped again");

  ItemReceiver unavailable([](std::uint32_t, std::uint32_t, std::string *error) {
    *error = "no page";
    return false;
  });
  FakeMemory empty;
  Expect(unavailable.Give(0x08U, empty, &error) == ItemReceiver::Result::Unavailable &&
             error == "no page",
         "an unmappable page is reported");
}

void TestLocationTakenInSave() {
  constexpr std::uint32_t kPlay = 0x08000000U;
  FakeMemory memory;
  FillFreshSave(memory);
  memory.Write32(kGlobalContextPointer, kPlay);
  memory.Write16(kPlay + kPlaySceneNum, 0x55U);
  memory.Write32(kPlay + kPlayChestFlags, 1U << 0U);
  memory.Write32(kPlay + kPlayCollectFlags, 0U);
  Expect(LocationTakenInSave({0x55U, OverrideType::Chest, 0x00U}, memory) == true,
         "the current scene's chest flags come from the play state");
  Expect(LocationTakenInSave({0x55U, OverrideType::Collectable, 0x04U}, memory) == false,
         "an untaken collectible");
  memory.Write32(kSaveContext + kSaveSceneFlags + 0x28U * kSceneFlagsSize + kSceneFlagsChest, 1U << 2U);
  Expect(LocationTakenInSave({0x28U, OverrideType::Chest, 0x02U}, memory) == true,
         "other scenes' flags come from the save");
  Expect(!LocationTakenInSave({0x5FU, OverrideType::BaseItem, 0x47U}, memory).has_value(),
         "an NPC gift has no single flag");
}

void TestCutsceneSkips() {
  FakeMemory memory;
  FillFreshSave(memory);
  memory.Write32(kSaveContext + kSaveCutsceneIndex, 0xFFF1U);
  memory.Write16(kSaveContext + kSaveSceneIndex, 0x34U);
  Expect(ApplyCutsceneSkips({}, memory) == 0, "skips are off by default");
  Expect(ApplyCutsceneSkips({true, false}, memory) == 10, "every skip flag is set once");
  Expect(ApplyCutsceneSkips({true, false}, memory) == 0, "set flags are left alone");
  std::uint16_t bosses = 0;
  memory.Read16(kSaveContext + kSaveEventChkInf + 7U * 2U, &bosses);
  Expect(bosses == 0x01FFU, "boss intros are marked as seen");
  std::uint32_t cutscene = 0;
  memory.Read32(kSaveContext + kSaveCutsceneIndex, &cutscene);
  Expect(cutscene == 0xFFF1U, "the intro stays unless its option is on");
  Expect(ApplyCutsceneSkips({false, true}, memory) == 1, "the intro is skipped");
  memory.Read32(kSaveContext + kSaveCutsceneIndex, &cutscene);
  Expect(cutscene == 0U, "a new file starts without the intro");
  memory.Write32(kSaveContext + kSaveCutsceneIndex, 0xFFF1U);
  memory.Write16(kSaveContext + kSaveSceneIndex, 0x55U);
  Expect(ApplyCutsceneSkips({false, true}, memory) == 0, "only a file in Link's house is a new file");
  // Nothing the logic depends on: no item, no Master Sword or Sheik flag.
  std::uint16_t masterSword = 0;
  memory.Read16(kSaveContext + kSaveEventChkInf + 4U * 2U, &masterSword);
  Expect(masterSword == 0U, "the time travel flags stay untouched");
}

void TestSessionWithoutConnection() {
  const auto directory = FreshDirectory("triaevum_archipelago_session_test");
  {
    std::ofstream(directory / "locations.json") << kTableJson;
    Session session({directory / "archipelago.json", directory / "sidecars",
                     directory / "locations.json", TRIAEVUM_ARCHIPELAGO_ITEMS_JSON});
    Expect(session.State() == ConnectionState::Disconnected, "no connection is opened by itself");
    CheckEvent event;
    event.Key = LocationKey{0x55U, OverrideType::Chest, 0x00U};
    session.OnCheck(event);
    event.Key = LocationKey{0x55U, OverrideType::Chest, 0x09U};
    session.OnCheck(event);
    FakeMemory memory;
    Expect(!session.OverrideGetItem({0x55U, OverrideType::Chest, 0x00U}, memory).has_value(),
           "without a connected slot every location keeps its own item");
    const auto stats = session.Stats();
    Expect(stats.LocationTableSize == 3U, "the session loads the location table");
    Expect(stats.ChecksUnmapped == 1U, "an unmapped key is counted");
    Expect(stats.ChecksReported == 0U, "without a slot and save nothing is reported yet");
  }
  std::stringstream text;
  {
    std::ifstream settings(directory / "archipelago.json");
    text << settings.rdbuf();
  }
  const auto document = nlohmann::json::parse(text.str());
  Expect(document.value("uuid", "").size() == 32U, "the session stores an installation uuid");
  Expect(!document.contains("password"), "the password is never written");
  Expect(document.value("auto_connect", false) && document.value("notifications", false),
         "auto-connect and messages are on by default");
  std::filesystem::remove_all(directory);
}

void TestNewFileAndProgressRecord() {
  FakeMemory memory;
  FillFreshSave(memory);
  memory.Write16(kSaveContext + kSaveSceneIndex, kSceneLinksHouse);
  Expect(IsNewFile(memory), "a fresh save in Link's house is a new file");
  memory.Write8(kSaveContext + kSaveItems, 0x00U); // Deku Stick
  Expect(!IsNewFile(memory), "any item makes it an old file");
  memory.Write8(kSaveContext + kSaveItems, kItemNone);
  memory.Write16(kSaveContext + kSaveSceneIndex, 0x55U);
  Expect(!IsNewFile(memory), "only Link's house starts a new file");

  Expect(!ReadProgressRecord(memory).has_value(), "no record on an unmapped page");
  for (std::uint32_t offset = 0; offset < 12U; ++offset)
    memory.Write8(kClientProgressRecord + offset, 0U);
  Expect(!ReadProgressRecord(memory).has_value(), "no record without the magic");
  const SidecarIdentity first{"AP 1", 0, 1, 0}, third{"AP 1", 0, 1, 2};
  Expect(IdentityHash(first) != IdentityHash(third), "each save slot has its own identity");
  Expect(WriteProgressRecord(memory, {IdentityHash(first), 9U}), "the record is written");
  const auto record = ReadProgressRecord(memory);
  Expect(record.has_value() && record->Identity == IdentityHash(first) && record->AppliedItems == 9U,
         "the record reads back");
}

void TestDecideProgress() {
  SidecarState state;
  state.Identity = {"AP 1", 0, 1, 0};
  state.AppliedItems = 7;
  state.AppliedAtLastSave = 4;
  const auto ours = [&](std::uint32_t applied) {
    return std::optional<ProgressRecord>(ProgressRecord{IdentityHash(state.Identity), applied});
  };
  auto decision = DecideProgress(state, true, false, ours(7));
  Expect(decision.Event == ProgressEvent::LoadedFromDisk && decision.AppliedItems == 4,
         "a file loaded from disk holds the items given until the game saved");
  decision = DecideProgress(state, true, true, std::nullopt);
  Expect(decision.Event == ProgressEvent::NewFile && decision.AppliedItems == 0,
         "a new file receives everything again");
  decision = DecideProgress(state, false, false, ours(5));
  Expect(decision.Event == ProgressEvent::Savestate && decision.AppliedItems == 5,
         "a savestate brings its own count");
  decision = DecideProgress(state, false, false, ProgressRecord{IdentityHash({"AP 2", 0, 1, 0}), 1U});
  Expect(decision.Event == ProgressEvent::None, "another seed's record is ignored");
  Expect(DecideProgress(state, false, false, ours(7)).Event == ProgressEvent::None &&
             DecideProgress(state, false, false, std::nullopt).Event == ProgressEvent::None,
         "nothing changes while playing on");
}

void TestRetryDelay() {
  Expect(RetryDelay(1) == std::chrono::seconds(5) && RetryDelay(2) == std::chrono::seconds(10) &&
             RetryDelay(4) == std::chrono::seconds(40),
         "failed connections are retried after 5, 10, 20, 40 seconds");
  Expect(RetryDelay(5) == std::chrono::seconds(60) && RetryDelay(50) == std::chrono::seconds(60),
         "retries wait at most a minute");
}

void TestEnabledSettingAndAutoConnect() {
  if (std::getenv("TRIAEVUM_ARCHIPELAGO") != nullptr)
    return; // the variable decides instead of the file
  const auto directory = FreshDirectory("triaevum_archipelago_enable_test");
  const auto settings = directory / "archipelago.json";
  Expect(!EnabledInSettings(settings), "Archipelago is off without a settings file");
  Expect(SetEnabledInSettings(settings, true) && EnabledInSettings(settings),
         "the switch turns it on for the next start");
  std::ofstream(directory / "locations.json") << kTableJson;
  {
    // A slot from an earlier session: the session connects by itself (to a
    // port nothing listens on) and keeps the switch while saving.
    auto document = nlohmann::json::parse(std::ifstream(settings));
    document["server"] = "127.0.0.1:1";
    document["slot"] = "Tester";
    std::ofstream(settings) << document.dump();
    Session session({settings, directory / "sidecars", directory / "locations.json",
                     TRIAEVUM_ARCHIPELAGO_ITEMS_JSON});
    Expect(session.State() != ConnectionState::Disconnected, "a saved slot is connected at start");
    Expect(session.Notifications().empty(), "no messages before anything happened");
    ClientOptions options{false, false};
    options.FastChests = true;
    options.SongReplays = SongReplayMode::SkipNoSound;
    options.Damage = DamageMultiplier::OneHitKo;
    session.SetOptions(options);
  }
  Expect(EnabledInSettings(settings), "saving the session keeps the switch");
  {
    Session session({settings, directory / "sidecars", directory / "locations.json",
                     TRIAEVUM_ARCHIPELAGO_ITEMS_JSON});
    Expect(session.State() == ConnectionState::Disconnected, "auto-connect can be turned off");
    Expect(!session.Options().Notifications, "the message option is kept");
    Expect(session.Options().FastChests, "fast chests are kept");
    Expect(session.Options().SongReplays == SongReplayMode::SkipNoSound, "the song replay mode is kept");
    Expect(session.Options().Damage == DamageMultiplier::OneHitKo, "the damage multiplier is kept");
  }
  const auto withPassword = [&](bool remember) {
    auto document = nlohmann::json::parse(std::ifstream(settings));
    document["remember_password"] = remember;
    document["password"] = "secret";
    std::ofstream(settings) << document.dump();
  };
  withPassword(false);
  {
    Session session({settings, directory / "sidecars", directory / "locations.json",
                     TRIAEVUM_ARCHIPELAGO_ITEMS_JSON});
    Expect(session.Settings().Password.empty(), "a password is not read unless it is remembered");
    session.SetOptions(session.Options());
  }
  Expect(!nlohmann::json::parse(std::ifstream(settings)).contains("password"),
         "a password not remembered is removed from the file");
  withPassword(true);
  {
    Session session({settings, directory / "sidecars", directory / "locations.json",
                     TRIAEVUM_ARCHIPELAGO_ITEMS_JSON});
    Expect(session.Settings().Password == "secret" && session.Options().RememberPassword,
           "a remembered password is read for connecting automatically");
  }
  std::filesystem::remove_all(directory);
}

} // namespace

int main(int argc, char **argv) {
  try {
    TestRepresentativeKeys();
    TestKeyPackingMatchesRandomizerUnion();
    TestKeysTheRandomizerSkips();
    TestSha256();
    TestAddressTableMatchesJson(argc > 1 ? argv[1] : TRIAEVUM_ARCHIPELAGO_ADDRESSES_JSON);
    TestObserverReportsAcceptedOffer();
    TestObserverOverridesOffer();
    TestObserverMatchesTokenWithoutPlayerState();
    TestStaleOfferIsNotInherited();
    TestObserverReportsDirectSong();
    TestVerifyAddressesRejectsOtherImage();
    TestHookPcsAreSorted();
    TestProbeCountsOnlyItsPc();
    TestObserverReportsDeath();
    TestTokenIgnoresNearbyChestOffer();
    TestDungeonRewardKeys();
    TestBlueWarpSkip();
    TestBridgeAndHearts();
    TestCheats();
    TestFastChests();
    TestSongReplays();
    TestDamageMultiplier();
    TestItemText();
    TestShops();
    TestCows();
    TestTokenShuffle();
    TestForestExitGuard();
    TestFairyRewards();
    TestDirectGiveReceivedAndReplaced();
    TestWorldState();
    TestRetryDelay();
    TestSidecarWriter();
    TestNewFileAndProgressRecord();
    TestDecideProgress();
    TestQuickText();
    TestEnabledSettingAndAutoConnect();
    TestLocationTable();
    TestGeneratedLocationTable(TRIAEVUM_ARCHIPELAGO_LOCATIONS_JSON);
    TestSidecarRoundTripAndMismatch();
    TestGeneratedItemTable(TRIAEVUM_ARCHIPELAGO_ITEMS_JSON);
    TestReceiverGivesThroughPlayer();
    TestLocationTakenInSave();
    TestCutsceneSkips();
    TestSessionWithoutConnection();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "triaevum_archipelago_tests: %s\n", error.what());
    return 1;
  }
  std::puts("triaevum_archipelago_tests: ok");
  return 0;
}
