// Host tests of the Archipelago check observer. They need no ROM: guest
// memory is a small fake bus.

#include "ap_check_observer.h"
#include "ap_location_key.h"
#include "ap_location_table.h"
#include "ap_session.h"
#include "ap_sha256.h"
#include "ap_sidecar.h"

#include <filesystem>
#include <sstream>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
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
  Expect(!KeyForDirectGive(0x59U) && !KeyForDirectGive(0x66U),
         "only songs are direct-give locations");
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
      if (entry.at("name") != function.Name || entry.at("kind") != "symbol")
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
  Expect(CheckObserver::HookPcs().size() == 2U, "checks need two hooks");
  Expect(CheckObserver::HookPcs(0x003727F0U).size() == 3U, "a probe adds one hook");
  Expect(CheckObserver::HookPcs(kItemGive).size() == 2U, "a probe on a check hook is merged");
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
  fresh->ReportedLocations = {1000, 1001};
  Expect(SaveSidecar(directory, *fresh, &error), error.c_str());
  Expect(SidecarPath(directory, identity).filename() == "AP_1234_x-0-2-save1.json",
         "the seed is reduced to a safe file name");
  const auto loaded = LoadSidecar(directory, identity, &error);
  Expect(loaded.has_value() && loaded->AppliedItems == 7 &&
             loaded->ReportedLocations == std::set<std::int64_t>{1000, 1001},
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

void TestSessionWithoutConnection() {
  const auto directory = FreshDirectory("triaevum_archipelago_session_test");
  {
    std::ofstream(directory / "locations.json") << kTableJson;
    Session session({directory / "archipelago.json", directory / "sidecars",
                     directory / "locations.json"});
    Expect(session.State() == ConnectionState::Disconnected, "no connection is opened by itself");
    CheckEvent event;
    event.Key = LocationKey{0x55U, OverrideType::Chest, 0x00U};
    session.OnCheck(event);
    event.Key = LocationKey{0x55U, OverrideType::Chest, 0x09U};
    session.OnCheck(event);
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
    TestObserverMatchesTokenWithoutPlayerState();
    TestStaleOfferIsNotInherited();
    TestObserverReportsDirectSong();
    TestVerifyAddressesRejectsOtherImage();
    TestHookPcsAreSorted();
    TestProbeCountsOnlyItsPc();
    TestLocationTable();
    TestGeneratedLocationTable(TRIAEVUM_ARCHIPELAGO_LOCATIONS_JSON);
    TestSidecarRoundTripAndMismatch();
    TestSessionWithoutConnection();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "triaevum_archipelago_tests: %s\n", error.what());
    return 1;
  }
  std::puts("triaevum_archipelago_tests: ok");
  return 0;
}
