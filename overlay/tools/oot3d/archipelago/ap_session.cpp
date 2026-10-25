#include "ap_session.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <random>
#include <variant>
#include <sstream>

namespace Oot3dArchipelago {
namespace {

constexpr std::size_t kConsoleLines = 200;
constexpr int kSaveSlots = 3;

std::string NewUuid() {
  std::random_device device;
  std::mt19937_64 generator(
      (static_cast<std::uint64_t>(device()) << 32U) ^ static_cast<std::uint64_t>(device()));
  char text[40];
  std::snprintf(text, sizeof(text), "%016llx%016llx",
                static_cast<unsigned long long>(generator()),
                static_cast<unsigned long long>(generator()));
  return text;
}

// The save slot of the file the game has loaded ("ZELDAZ" present), if any.
std::optional<int> LoadedSaveSlot(oot3d::recomp::a32::MemoryBus &memory) {
  char newf[6] = {};
  for (std::uint32_t i = 0; i < 6U; ++i) {
    std::uint8_t byte = 0;
    if (!memory.Read8(kSaveNewf + i, &byte))
      return std::nullopt;
    newf[i] = static_cast<char>(byte);
  }
  std::uint32_t fileNum = 0;
  if (std::string_view(newf, 6) != "ZELDAZ" || !memory.Read32(kSaveFileNum, &fileNum) ||
      fileNum >= static_cast<std::uint32_t>(kSaveSlots))
    return std::nullopt;
  return static_cast<int>(fileNum);
}

} // namespace

Session::Session(Paths paths) : mPaths(std::move(paths)) {
  LoadSettings();
  std::string error;
  mLocations = LocationTable::Load(mPaths.LocationTable, &error);
  if (mLocations.has_value())
    Log("Location table: " + std::to_string(mLocations->Size()) + " locations");
  else
    Log("No location table yet (" + error + "); checks are logged, not sent.");
  mItems = ItemTable::Load(mPaths.ItemTable, &error);
  if (mItems.has_value())
    Log("Item table: " + std::to_string(mItems->Size()) + " items");
  else
    Log("No item table (" + error + "); locations keep their own items and nothing is received.");
}

void Session::SetGuestRegionMapper(GuestRegionMapper mapper) {
  std::lock_guard lock(mMutex);
  mReceiver = ItemReceiver(std::move(mapper));
}

Session::~Session() {
  std::lock_guard lock(mMutex);
  Persist();
}

void Session::Log(std::string line) {
  std::fprintf(stderr, "[archipelago] %s\n", line.c_str());
  mConsole.push_back(std::move(line));
  while (mConsole.size() > kConsoleLines)
    mConsole.pop_front();
}

void Session::LoadSettings() {
  std::ifstream input(mPaths.SettingsFile, std::ios::binary);
  if (input) {
    std::ostringstream text;
    text << input.rdbuf();
    const auto document = nlohmann::json::parse(text.str(), nullptr, false);
    if (document.is_object()) {
      mSettings.Server = document.value("server", mSettings.Server);
      mSettings.Slot = document.value("slot", "");
      mUuid = document.value("uuid", "");
      mSkips.Cutscenes = document.value("skip_cutscenes", false);
      mSkips.Intro = document.value("skip_intro", false);
    }
  }
  if (mUuid.empty()) {
    mUuid = NewUuid();
    SaveSettings();
  }
}

void Session::SaveSettings() const {
  std::error_code created;
  std::filesystem::create_directories(mPaths.SettingsFile.parent_path(), created);
  std::ofstream output(mPaths.SettingsFile, std::ios::binary | std::ios::trunc);
  output << nlohmann::json{{"server", mSettings.Server},
                           {"slot", mSettings.Slot},
                           {"uuid", mUuid},
                           {"skip_cutscenes", mSkips.Cutscenes},
                           {"skip_intro", mSkips.Intro}}
                .dump(1)
         << '\n';
}

CutsceneSkipOptions Session::Skips() const {
  std::lock_guard lock(mMutex);
  return mSkips;
}

void Session::SetSkips(const CutsceneSkipOptions &skips) {
  std::lock_guard lock(mMutex);
  mSkips = skips;
  mSkipsLoggedForSlot = -1;
  SaveSettings();
}

ConnectionSettings Session::Settings() const {
  std::lock_guard lock(mMutex);
  return mSettings;
}

void Session::Connect(const ConnectionSettings &settings) {
  std::lock_guard lock(mMutex);
  mSettings = settings;
  SaveSettings();
  if (!mClient)
    mClient = std::make_unique<NetworkClient>(mUuid);
  Log("Connecting to " + mSettings.Server + " as " + mSettings.Slot);
  mClient->Connect(mSettings);
}

void Session::Disconnect() {
  std::lock_guard lock(mMutex);
  if (mClient)
    mClient->Disconnect();
}

void Session::Say(std::string text) {
  std::lock_guard lock(mMutex);
  if (mClient && !text.empty())
    mClient->Say(std::move(text));
}

ConnectionState Session::State() const {
  std::lock_guard lock(mMutex);
  return mClient ? mClient->State() : ConnectionState::Disconnected;
}

std::vector<std::string> Session::Console() const {
  std::lock_guard lock(mMutex);
  return {mConsole.begin(), mConsole.end()};
}

Session::Counters Session::Stats() const {
  std::lock_guard lock(mMutex);
  Counters counters;
  counters.LocationTableSize = mLocations ? mLocations->Size() : 0U;
  counters.ChecksReported = mSidecar ? mSidecar->ReportedLocations.size() : 0U;
  counters.ChecksUnmapped = mUnmapped;
  counters.ItemsFromServer = mServerItems.size();
  counters.ItemsApplied = mSidecar ? mSidecar->AppliedItems : 0;
  counters.LocationsScouted = mPlacements.size();
  counters.SaveSlot = mSidecar ? mSidecar->Identity.SaveSlot : -1;
  return counters;
}

void Session::OnCheck(const CheckEvent &event) {
  std::lock_guard lock(mMutex);
  if (!event.Key.has_value())
    return;
  std::optional<std::int64_t> id;
  if (mLocations.has_value()) {
    // Master Quest choice comes from slot_data once the apworld defines it.
    id = mLocations->Find(*event.Key, nullptr);
  }
  if (!id.has_value()) {
    ++mUnmapped;
    Log("Check without a location id: " + Describe(*event.Key));
    return;
  }
  ReportLocation(*id);
  // The game gave this world's item at its location itself; the server's
  // copy of it is skipped while the save records the location.
  if (mSidecar.has_value() && mShownAtLocation.count(*id) != 0U &&
      mSidecar->GivenAtLocation.insert(*id).second)
    Persist();
}

std::optional<std::uint16_t> Session::OverrideGetItem(const LocationKey &key,
                                                      oot3d::recomp::a32::MemoryBus &memory) {
  std::lock_guard lock(mMutex);
  if (!mSidecar.has_value() || !mSlot.has_value() || !mLocations.has_value())
    return std::nullopt;
  const auto id = mLocations->Find(key, nullptr);
  if (!id.has_value())
    return std::nullopt;
  const auto placement = mSidecar->Placements.find(*id);
  if (placement == mSidecar->Placements.end())
    return std::nullopt; // not scouted yet: the game's own item
  mShownAtLocation.erase(*id);
  // Another player's item, or ours that the server already gave: a
  // placeholder; the real item travels through the server.
  if (placement->second.Player != mSlot->Slot || mSidecar->GivenByServer.count(*id) != 0U ||
      !mItems.has_value())
    return kPlaceholderGetItem;
  const ItemPlan plan = mItems->Plan(placement->second.Item, memory);
  if (const auto *give = std::get_if<GiveGetItem>(&plan)) {
    mShownAtLocation.insert(*id);
    return give->GetItemId;
  }
  return kPlaceholderGetItem; // no get-item in the game: given by the server
}

void Session::ReportLocation(std::int64_t id) {
  if (!mSidecar.has_value()) {
    mUnsentChecks.push_back(id);
    return;
  }
  if (!mSidecar->ReportedLocations.insert(id).second)
    return; // checks are idempotent; already sent for this save
  const auto *entry = mLocations ? mLocations->ById(id) : nullptr;
  Log("Check: " + (entry != nullptr ? entry->Name : std::to_string(id)));
  if (mClient)
    mClient->SendLocationChecks({id});
  Persist();
}

void Session::Update(oot3d::recomp::a32::MemoryBus &memory) {
  std::lock_guard lock(mMutex);
  if (mClient) {
    for (const auto &event : mClient->TakeEvents())
      HandleEvent(event);
  }
  OpenSidecarIfReady(memory);
  if (mSkips.Cutscenes || mSkips.Intro) {
    if (const auto slot = LoadedSaveSlot(memory)) {
      const int set = ApplyCutsceneSkips(mSkips, memory);
      if (set > 0 && mSkipsLoggedForSlot != *slot) {
        Log("Cutscene skips: " + std::to_string(set) + " flags set in save slot " +
            std::to_string(*slot + 1));
        mSkipsLoggedForSlot = *slot;
      }
    }
  }
  GiveReceivedItems(memory);
}

// Received items are always items of this game, whoever found them.
std::string Session::ItemName(std::int64_t item) const {
  if (mItems.has_value())
    if (auto name = mItems->Name(item))
      return *name;
  return "item " + std::to_string(item);
}

void Session::GiveReceivedItems(oot3d::recomp::a32::MemoryBus &memory) {
  if (!mSidecar.has_value() || !mSlot.has_value() || !mItems.has_value() ||
      PlayerActor(memory) == 0U)
    return;
  while (mSidecar->AppliedItems < static_cast<std::int64_t>(mServerItems.size())) {
    const ReceivedItem &item = mServerItems[static_cast<std::size_t>(mSidecar->AppliedItems)];
    // Items of this world come from this slot's own locations.
    const bool ours = item.Player == mSlot->Slot;
    const std::string name = ItemName(item.Item);
    if (ours && mSidecar->GivenAtLocation.count(item.Location) != 0U) {
      const auto *location = mLocations ? mLocations->ById(item.Location) : nullptr;
      const auto taken = location != nullptr ? LocationTakenInSave(location->Key, memory) : std::nullopt;
      if (!taken.has_value() || *taken) {
        ++mSidecar->AppliedItems;
        Persist();
        continue;
      }
      // The save lost the location (another or an older save): give it again.
      mSidecar->GivenAtLocation.erase(item.Location);
    }
    const ItemPlan plan = mItems->Plan(item.Item, memory);
    if (const auto *give = std::get_if<GiveGetItem>(&plan)) {
      std::string error;
      const auto result = mReceiver.Give(give->GetItemId, memory, &error);
      if (result == ItemReceiver::Result::Unavailable) {
        if (!mReceiverReported)
          Log("Cannot give items in the game: " + error);
        mReceiverReported = true;
        return;
      }
      if (result == ItemReceiver::Result::Waiting)
        return; // one item at a time, when Link can take it
      Log("Received " + name);
    } else if (const auto *direct = std::get_if<GiveDirect>(&plan)) {
      if (!ApplyDirect(*direct, memory))
        return;
      Log("Received " + name);
    } else {
      Log("Not given: " + std::get<GiveUnsupported>(plan).Reason);
    }
    if (ours)
      mSidecar->GivenByServer.insert(item.Location);
    ++mSidecar->AppliedItems;
    Persist();
  }
}

void Session::OpenSidecarIfReady(oot3d::recomp::a32::MemoryBus &memory) {
  if (!mSlot.has_value())
    return;
  const auto loaded = LoadedSaveSlot(memory);
  if (!loaded.has_value())
    return;
  SidecarIdentity identity = *mSlot;
  identity.SaveSlot = *loaded;
  if (mSidecar.has_value() && mSidecar->Identity == identity)
    return;
  Persist();
  std::string error;
  auto state = LoadSidecar(mPaths.SidecarDirectory, identity, &error);
  if (!state.has_value()) {
    Log(error + " Nothing is sent for this save.");
    mSidecar.reset();
    mSlot.reset();
    if (mClient)
      mClient->Disconnect();
    return;
  }
  mSidecar = std::move(state);
  for (const auto &[location, placement] : mPlacements)
    mSidecar->Placements[location] = placement;
  Log("Save slot " + std::to_string(identity.SaveSlot) + ": " +
      std::to_string(mSidecar->ReportedLocations.size()) + " checks, " +
      std::to_string(mSidecar->AppliedItems) + " items applied");
  // Server checks count as reported; resend what the server lacks.
  std::vector<std::int64_t> resend;
  for (const std::int64_t id : mSidecar->ReportedLocations)
    if (mServerChecked.count(id) == 0U)
      resend.push_back(id);
  if (!resend.empty() && mClient)
    mClient->SendLocationChecks(resend);
  auto unsent = std::move(mUnsentChecks);
  mUnsentChecks.clear();
  for (const std::int64_t id : unsent)
    ReportLocation(id);
  Persist();
}

void Session::HandleEvent(const NetworkEventVariant &variant) {
  if (const auto *changed = std::get_if<NetworkEvent::StateChanged>(&variant)) {
    Log(std::string("Connection: ") + ToString(changed->State) +
        (changed->Detail.empty() ? "" : " (" + changed->Detail + ")"));
  } else if (const auto *slot = std::get_if<NetworkEvent::SlotConnected>(&variant)) {
    mSlot = SidecarIdentity{slot->Seed, slot->Team, slot->Slot, -1};
    mServerChecked = {slot->CheckedLocations.begin(), slot->CheckedLocations.end()};
    mServerItems.clear();
    mPlacements.clear();
    mSidecar.reset();
    Log("Slot connected: seed " + slot->Seed + ", team " + std::to_string(slot->Team) +
        ", slot " + std::to_string(slot->Slot) + ", " +
        std::to_string(slot->CheckedLocations.size()) + " checked, " +
        std::to_string(slot->MissingLocations.size()) + " missing");
  } else if (const auto *refused = std::get_if<NetworkEvent::SlotRefused>(&variant)) {
    std::string errors;
    for (const auto &error : refused->Errors)
      errors += (errors.empty() ? "" : ", ") + error;
    Log("Slot refused: " + errors);
  } else if (const auto *items = std::get_if<NetworkEvent::ItemsReceived>(&variant)) {
    // Update gives them in the game and advances AppliedItems. The server
    // resends from index 0 on every connection.
    for (const auto &item : items->Items) {
      if (item.Index < static_cast<std::int64_t>(mServerItems.size()))
        continue;
      mServerItems.push_back(item);
      Log("Item #" + std::to_string(item.Index) + ": " + ItemName(item.Item) +
          " from player " + std::to_string(item.Player));
    }
  } else if (const auto *scouted = std::get_if<NetworkEvent::LocationsScouted>(&variant)) {
    for (const auto &item : scouted->Items)
      mPlacements[item.Location] = Placement{item.Item, item.Player, item.Flags};
    if (mSidecar.has_value()) {
      for (const auto &[location, placement] : mPlacements)
        mSidecar->Placements[location] = placement;
      Persist();
    }
    Log("Scouted " + std::to_string(scouted->Items.size()) + " locations");
  } else if (const auto *checked = std::get_if<NetworkEvent::LocationsChecked>(&variant)) {
    mServerChecked.insert(checked->Locations.begin(), checked->Locations.end());
  } else if (const auto *message = std::get_if<NetworkEvent::Message>(&variant)) {
    Log(message->Text);
  }
}

void Session::Persist() {
  if (!mSidecar.has_value())
    return;
  std::string error;
  if (!SaveSidecar(mPaths.SidecarDirectory, *mSidecar, &error))
    Log("Cannot save Archipelago progress: " + error);
}

} // namespace Oot3dArchipelago
