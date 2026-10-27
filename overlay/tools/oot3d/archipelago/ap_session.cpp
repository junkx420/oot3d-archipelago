#include "ap_session.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <variant>
#include <sstream>

namespace Oot3dArchipelago {
namespace {

constexpr std::size_t kConsoleLines = 200;
constexpr int kSaveSlots = 3;
// How long a game over after a received death counts as that death.
constexpr int kLinkDeathFrames = 900;

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

nlohmann::json ReadSettings(const std::filesystem::path &file) {
  std::ifstream input(file, std::ios::binary);
  if (!input)
    return nlohmann::json::object();
  std::ostringstream text;
  text << input.rdbuf();
  auto document = nlohmann::json::parse(text.str(), nullptr, false);
  return document.is_object() ? document : nlohmann::json::object();
}

bool WriteSettings(const std::filesystem::path &file, const nlohmann::json &document) {
  std::error_code created;
  std::filesystem::create_directories(file.parent_path(), created);
  std::ofstream output(file, std::ios::binary | std::ios::trunc);
  output << document.dump(1) << '\n';
  return static_cast<bool>(output);
}

} // namespace

bool EnabledInSettings(const std::filesystem::path &settingsFile) {
  if (const char *environment = std::getenv("TRIAEVUM_ARCHIPELAGO"); environment != nullptr)
    return std::string_view(environment) == "1";
  return ReadSettings(settingsFile).value("enabled", false);
}

bool SetEnabledInSettings(const std::filesystem::path &settingsFile, bool enabled) {
  auto document = ReadSettings(settingsFile);
  document["enabled"] = enabled;
  return WriteSettings(settingsFile, document);
}

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
  // Before a file is loaded, so the save's first checks are not given as the
  // game's own items. The password is not saved: a server that needs one
  // refuses, and the panel connects with it.
  if (mOptions.AutoConnect && !mSettings.Slot.empty()) {
    mClient = std::make_unique<NetworkClient>(mUuid);
    Log("Connecting automatically to " + mSettings.Server + " as " + mSettings.Slot);
    mClient->Connect(mSettings);
  }
}

void Session::SetGuestRegionMapper(GuestRegionMapper mapper) {
  std::lock_guard lock(mMutex);
  mReceiver = ItemReceiver(std::move(mapper));
}

Session::~Session() {
  std::lock_guard lock(mMutex);
  Persist();
  FlushPersist();
}

void Session::Log(std::string line) {
  std::fprintf(stderr, "[archipelago] %s\n", line.c_str());
  mConsole.push_back(std::move(line));
  while (mConsole.size() > kConsoleLines)
    mConsole.pop_front();
}

void Session::Notify(Notification::Kind kind, std::string text) {
  mNotifications.push_back({kind, std::move(text), std::chrono::steady_clock::now()});
  while (mNotifications.size() > kMaximumNotifications)
    mNotifications.pop_front();
}

std::vector<Notification> Session::Notifications(std::chrono::steady_clock::time_point now) const {
  std::lock_guard lock(mMutex);
  while (!mNotifications.empty() && now - mNotifications.front().Time >= kNotificationLifetime)
    mNotifications.pop_front();
  if (!mOptions.Notifications)
    return {};
  return {mNotifications.begin(), mNotifications.end()};
}

void Session::LoadSettings() {
  const auto document = ReadSettings(mPaths.SettingsFile);
  mSettings.Server = document.value("server", mSettings.Server);
  mSettings.Slot = document.value("slot", "");
  mUuid = document.value("uuid", "");
  mSkips.Cutscenes = document.value("skip_cutscenes", false);
  mSkips.Intro = document.value("skip_intro", false);
  mOptions.AutoConnect = document.value("auto_connect", mOptions.AutoConnect);
  mOptions.Notifications = document.value("notifications", mOptions.Notifications);
  mOptions.QuickText =
      ParseQuickTextMode(document.value("quick_text", std::string(ToString(mOptions.QuickText))))
          .value_or(mOptions.QuickText);
  if (mUuid.empty()) {
    mUuid = NewUuid();
    SaveSettings();
  }
}

// Keeps keys this session does not own ("enabled").
void Session::SaveSettings() const {
  auto document = ReadSettings(mPaths.SettingsFile);
  document["server"] = mSettings.Server;
  document["slot"] = mSettings.Slot;
  document["uuid"] = mUuid;
  document["skip_cutscenes"] = mSkips.Cutscenes;
  document["skip_intro"] = mSkips.Intro;
  document["auto_connect"] = mOptions.AutoConnect;
  document["notifications"] = mOptions.Notifications;
  document["quick_text"] = ToString(mOptions.QuickText);
  WriteSettings(mPaths.SettingsFile, document);
}

void Session::SetOptionsSink(std::function<void(const ClientOptions &)> sink) {
  std::lock_guard lock(mMutex);
  mOptionsSink = std::move(sink);
  if (mOptionsSink)
    mOptionsSink(mOptions);
}

ClientOptions Session::Options() const {
  std::lock_guard lock(mMutex);
  return mOptions;
}

void Session::SetOptions(const ClientOptions &options) {
  std::lock_guard lock(mMutex);
  mOptions = options;
  SaveSettings();
  if (mOptionsSink)
    mOptionsSink(mOptions);
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
  counters.DeathLink = mDeathLink;
  counters.GoalReached = mSidecar && mSidecar->GoalReached;
  counters.SaveSlot = mSidecar ? mSidecar->Identity.SaveSlot : -1;
  counters.Frontend = mFrontend;
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
  // Another player's item leaves only as a placeholder in the game: say
  // what it was, once per location.
  const bool fresh = !mSidecar.has_value() || mSidecar->ReportedLocations.count(*id) == 0U;
  if (fresh && mSlot.has_value()) {
    const auto placement = mPlacements.find(*id);
    const auto names = mPlacementNames.find(*id);
    if (placement != mPlacements.end() && placement->second.Player != mSlot->Slot &&
        names != mPlacementNames.end())
      Notify(Notification::Kind::ItemSent,
             "Sent " + (names->second.first.empty() ? std::string("an item") : names->second.first) +
                 " to " + (names->second.second.empty() ? std::string("another player") : names->second.second));
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
  // A token's offer keeps the token; its Item_Give becomes a heart instead
  // (CheckObserver::SetTokenShuffleProbe).
  if (!mSidecar.has_value() || !mSlot.has_value() || !mLocations.has_value() || key.Type == OverrideType::Skull)
    return std::nullopt;
  const auto id = mLocations->Find(key, nullptr);
  if (!id.has_value())
    return std::nullopt;
  // A business scrub sells again after its location's item, and a cow gives
  // milk again: the game's own item from then on (OoT3D_Randomizer makes
  // them one-time instead).
  if (mSidecar->ReportedLocations.count(*id) != 0U) {
    const auto *entry = mLocations->ById(*id);
    if (key.Type == OverrideType::GrottoScrub ||
        (entry != nullptr && (entry->Name.find("Deku Scrub") != std::string::npos ||
                              entry->Name.ends_with(" Cow"))))
      return std::nullopt;
  }
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

std::optional<std::uint8_t> Session::ReplaceDirectGive(const LocationKey &key,
                                                      oot3d::recomp::a32::MemoryBus &memory) {
  (void)memory;
  std::lock_guard lock(mMutex);
  // The Great Fairies' spells and the Light Arrow cutscene are ordinary
  // locations of the seed: once its placements are known, the game's own item
  // becomes a recovery heart and the location's item comes from the server.
  // Saria's gift (shuffle_ocarinas) the same. Songs and the dungeon rewards
  // hold their own item with the supported options and stay as they are. A Gold Skulltula token's location
  // is replaced the same way when the seed shuffles it (it is reported from
  // the save's gsFlags, WatchTokens).
  const bool token = key.Type == OverrideType::Skull;
  if (!token && (key.Type != OverrideType::Delayed ||
      (key.Flag != kDelayedLightArrowCutscene && key.Flag != kDelayedSariasGift && key.Flag != kDelayedFairyZorasFountain &&
       key.Flag != kDelayedFairyHyruleCastle && key.Flag != kDelayedFairyColossus)))
    return std::nullopt;
  if (!mSidecar.has_value() || !mLocations.has_value())
    return std::nullopt;
  const auto id = mLocations->Find(key, nullptr);
  if (!id.has_value() || mSidecar->Placements.count(*id) == 0U)
    return std::nullopt;
  return kItemRecoveryHeart;
}

bool Session::CowGivesItem(const LocationKey &key) const {
  std::lock_guard lock(mMutex);
  if (!mSidecar.has_value() || !mLocations.has_value())
    return false;
  const auto id = mLocations->Find(key, nullptr);
  return id.has_value() && mSidecar->Placements.count(*id) != 0U && mSidecar->ReportedLocations.count(*id) == 0U;
}

bool Session::ForestExitOpen() const {
  std::lock_guard lock(mMutex);
  return mSidecar.has_value() && mWorld.OpenForestExit;
}

bool Session::Receiving() const {
  std::lock_guard lock(mMutex);
  return mReceiver.Offering();
}

void Session::WatchFairyFountain(oot3d::recomp::a32::MemoryBus &memory) {
  const auto now = ReadFairyUpgrades(memory);
  std::uint32_t play = 0;
  std::uint16_t scene = 0;
  const bool inFountain = memory.Read32(kGlobalContextPointer, &play) && play != 0U &&
                          memory.Read16(play + kPlaySceneNum, &scene) && scene == kSceneGreatFairyUpgrades;
  // A received magic upgrade changes the same fields; it is never a fairy's.
  if (!now.has_value() || !inFountain || mFrontend || !mSidecar.has_value() || !mLocations.has_value() ||
      !mFairyBaseline.has_value() || mReceiver.Offering()) {
    mFairyBaseline = now;
    return;
  }
  const auto flag = FairyRewardLocation(*mFairyBaseline, *now);
  if (!flag.has_value()) {
    mFairyBaseline = now;
    return;
  }
  const LocationKey key{kDelayedScene, OverrideType::Delayed, *flag};
  const auto id = mLocations->Find(key, nullptr);
  if (!id.has_value()) {
    ++mUnmapped;
    mFairyBaseline = now;
    return;
  }
  // The game's reward stays when the location holds this world's same
  // upgrade (the server's copy is then skipped); otherwise it is taken back
  // and the location's item comes from the server.
  const auto placement = mSidecar->Placements.find(*id);
  const std::string wanted =
      *flag == kDelayedFairyOutsideGanonsCastle ? "Double Defense" : "Progressive Magic Meter";
  const bool keep = placement != mSidecar->Placements.end() && mSlot.has_value() &&
                    placement->second.Player == mSlot->Slot && ItemName(placement->second.Item) == wanted;
  if (keep)
    mSidecar->GivenAtLocation.insert(*id);
  else
    (void)RevertFairyReward(*flag, *mFairyBaseline, memory);
  Log(std::string("Great Fairy: ") + Describe(key) + (keep ? ", her own reward kept" : ", her own reward taken back"));
  mFairyBaseline = ReadFairyUpgrades(memory);
  ReportLocation(*id);
}

// Every token the save records, reported once: the game sets the token's
// gsFlags bit (index and bit of its params, the randomizer's skull key)
// however the token was taken, and a token taken while disconnected counts
// too.
void Session::WatchTokens(oot3d::recomp::a32::MemoryBus &memory) {
  // Whether any scouted location of this slot is a token's, recounted when
  // the placements change.
  const std::size_t placements = mSidecar.has_value() ? mSidecar->Placements.size() : 0U;
  if (placements != mTokenPlacementCount) {
    mTokenPlacementCount = placements;
    mTokensShuffled = false;
    if (mSidecar.has_value() && mLocations.has_value())
      for (const auto &[location, placement] : mSidecar->Placements)
        if (const auto *entry = mLocations->ById(location); entry != nullptr && entry->Key.Type == OverrideType::Skull) {
          mTokensShuffled = true;
          break;
        }
    mTokenFlags.reset();
  }
  if (!mTokensShuffled || mFrontend || !mSidecar.has_value() || !mLocations.has_value())
    return;
  std::array<std::uint8_t, kSaveGsFlagCount> flags{};
  for (std::uint32_t i = 0; i < kSaveGsFlagCount; ++i)
    if (!memory.Read8(kSaveContext + kSaveGsFlags + i, &flags[i]))
      return;
  if (mTokenFlags.has_value() && *mTokenFlags == flags)
    return;
  mTokenFlags = flags;
  for (const LocationKey &key : TokenKeys(flags)) {
    const auto id = mLocations->Find(key, nullptr);
    if (id.has_value() && mSidecar->Placements.count(*id) != 0U && mSidecar->ReportedLocations.count(*id) == 0U)
      ReportLocation(*id);
  }
}

CheckObserver::ShopSlotState Session::ShopSlot(const LocationKey &key, oot3d::recomp::a32::MemoryBus &memory) {
  std::lock_guard lock(mMutex);
  CheckObserver::ShopSlotState state;
  if (!mSidecar.has_value() || !mSlot.has_value() || !mLocations.has_value())
    return state;
  const auto id = mLocations->Find(key, nullptr);
  if (!id.has_value())
    return state;
  const auto price = mShopPrices.find(*id);
  const auto placement = mSidecar->Placements.find(*id);
  if (price == mShopPrices.end() || placement == mSidecar->Placements.end())
    return state; // not a location of this seed: the game's shelf
  if (mSidecar->ReportedLocations.count(*id) != 0U) {
    state.Type = CheckObserver::ShopSlotState::Kind::SoldOut;
    return state;
  }
  // Like OverrideGetItem: this world's item with its own get-item, another
  // player's (or one the server gave already) as the placeholder.
  std::uint16_t getItem = kPlaceholderGetItem;
  mShownAtLocation.erase(*id);
  if (placement->second.Player == mSlot->Slot && mSidecar->GivenByServer.count(*id) == 0U && mItems.has_value()) {
    const ItemPlan plan = mItems->Plan(placement->second.Item, memory);
    if (const auto *give = std::get_if<GiveGetItem>(&plan)) {
      getItem = give->GetItemId;
      mShownAtLocation.insert(*id);
    }
  }
  state.Type = CheckObserver::ShopSlotState::Kind::Sell;
  state.Item = ShopSlotItem{getItem, static_cast<std::int16_t>(price->second)};
  return state;
}

void Session::ShopBought(const LocationKey &key) {
  std::lock_guard lock(mMutex);
  if (!mLocations.has_value())
    return;
  const auto id = mLocations->Find(key, nullptr);
  if (!id.has_value())
    return;
  ReportLocation(*id);
  // The shop gave this world's item itself; the server's copy is skipped.
  if (mSidecar.has_value() && mShownAtLocation.count(*id) != 0U && mSidecar->GivenAtLocation.insert(*id).second)
    Persist();
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
  std::uint32_t gameMode = 0U;
  mFrontend = memory.Read32(kSaveContext + kSaveGameMode, &gameMode) && gameMode != 0U;
  if (mClient) {
    for (const auto &event : mClient->TakeEvents())
      HandleEvent(event);
  }
  OpenSidecarIfReady(memory);
  SyncProgress(memory);
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
  if (mSidecar.has_value() && !mFrontend && !mWorld.Empty()) {
    const int set = ApplyWorldState(mWorld, memory);
    if (set > 0 && mWorldLoggedForSlot != mSidecar->Identity.SaveSlot) {
      Log("World settings of the seed: " + std::to_string(set) + " flags set");
      mWorldLoggedForSlot = mSidecar->Identity.SaveSlot;
    }
  }
  WatchTriforce();
  WatchFairyFountain(memory);
  WatchTokens(memory);
  GiveReceivedItems(memory);
  ApplyDeathLink(memory);
  WatchGoal(memory);
  // The count of this moment, for a savestate taken now.
  if (mSidecar.has_value() && !mFrontend && mReceiver.EnsurePage(memory, nullptr))
    (void)WriteProgressRecord(memory, {IdentityHash(mSidecar->Identity),
                                       static_cast<std::uint32_t>(mSidecar->AppliedItems)});
  FlushPersist();
}

// The save file the game writes for the loaded save slot.
std::filesystem::path Session::GameSavePath(int saveSlot) const {
  char name[16];
  std::snprintf(name, sizeof(name), "save%02d.bin", saveSlot);
  return mPaths.SidecarDirectory.parent_path() / name;
}

// Notes the count whenever the game writes its save file (checked twice a
// second): a later load of the file holds what was applied until then.
void Session::WatchGameSave() {
  if (--mSaveCheckCountdown > 0)
    return;
  mSaveCheckCountdown = 15;
  std::error_code failed;
  const auto stamp = std::filesystem::last_write_time(GameSavePath(mSidecar->Identity.SaveSlot), failed);
  if (failed)
    return;
  const bool known = mSaveStamp.has_value() && mSaveStampSlot == mSidecar->Identity.SaveSlot;
  const bool written = known && *mSaveStamp != stamp;
  mSaveStamp = stamp;
  mSaveStampSlot = mSidecar->Identity.SaveSlot;
  if (!written || mSidecar->AppliedAtLastSave == mSidecar->AppliedItems)
    return;
  mSidecar->AppliedAtLastSave = mSidecar->AppliedItems;
  Persist();
  Log("The game saved: " + std::to_string(mSidecar->AppliedItems) + " received items are in the save");
}

void Session::SyncProgress(oot3d::recomp::a32::MemoryBus &memory) {
  if (mFrontend)
    mSyncedIdentity.reset(); // the next gameplay comes from the file selection
  if (!mSidecar.has_value())
    return;
  WatchGameSave();
  if (mFrontend)
    return;
  // Gameplay started from the file selection, or this save's sidecar opened
  // for the first time in this run while playing: the save came from disk.
  const bool entered = !(mSyncedIdentity.has_value() && *mSyncedIdentity == mSidecar->Identity);
  mSyncedIdentity = mSidecar->Identity;
  std::optional<ProgressRecord> record;
  if (!entered && mReceiver.EnsurePage(memory, nullptr))
    record = ReadProgressRecord(memory);
  const auto decision = DecideProgress(*mSidecar, entered, entered && IsNewFile(memory), record);
  auto &sidecar = *mSidecar;
  switch (decision.Event) {
  case ProgressEvent::None:
    return;
  case ProgressEvent::NewFile:
    sidecar.AppliedItems = 0;
    sidecar.AppliedAtLastSave = 0;
    sidecar.GivenAtLocation.clear();
    sidecar.GivenByServer.clear();
    sidecar.GoalReached = false;
    Log("New file: every received item is given again");
    if (ApplyStartingHearts(mWorld, memory))
      Log("New file: " + std::to_string(mWorld.StartingHearts) + " starting hearts");
    if (!mServerItems.empty())
      Notify(Notification::Kind::Info,
             "New file: receiving " + std::to_string(mServerItems.size()) + " items again");
    break;
  case ProgressEvent::LoadedFromDisk:
    if (decision.AppliedItems == sidecar.AppliedItems)
      return;
    Log("Loaded save: items " + std::to_string(decision.AppliedItems) + " to " +
        std::to_string(sidecar.AppliedItems - 1) + " were given after it was saved and come again");
    sidecar.AppliedItems = decision.AppliedItems;
    break;
  case ProgressEvent::Savestate:
    Log("Savestate: received items counted from " + std::to_string(decision.AppliedItems) +
        " (was " + std::to_string(sidecar.AppliedItems) + ")");
    sidecar.AppliedItems = decision.AppliedItems;
    break;
  }
  mReceiver.Reset();
  Persist();
}

void Session::OnDeath() {
  std::lock_guard lock(mMutex);
  mDiedInScene = true;
  if (mLinkDeathFrames > 0) {
    mLinkDeathFrames = 0; // this game over is the received death itself
    return;
  }
  if (mDeathLink && mClient && mSlot.has_value()) {
    mClient->SendDeath(mSettings.Slot + " fell in Hyrule.");
    Log("DeathLink: your death was sent");
    Notify(Notification::Kind::Death, "DeathLink: your death was shared");
  }
}

void Session::OnBlueWarp(std::uint32_t pc, std::uint8_t scene, oot3d::recomp::a32::MemoryBus &memory) {
  std::lock_guard lock(mMutex);
  if (pc == kRutoBlueWarpDoneBlock) {
    // The game gave Zora's Sapphire itself; only its cutscene goes.
    if (mSkips.Cutscenes && ClearJabuJabuWarpCutscene(scene, memory))
      Log("Blue warp: Jabu-Jabu's cutscene skipped");
    return;
  }
  // Without a connected save the reward could not come from the server, so
  // the warp plays as usual and the game gives it.
  if (!mSkips.Cutscenes || !mSidecar.has_value() || !mLocations.has_value())
    return;
  const auto dungeon = SkipBlueWarpCutscene(scene, memory);
  if (!dungeon.has_value())
    return;
  const LocationKey key{kTempleScene, OverrideType::Temple, *dungeon};
  Log("Blue warp: cutscene skipped (" + Describe(key) + ")");
  if (const auto id = mLocations->Find(key, nullptr))
    ReportLocation(*id);
  else
    ++mUnmapped;
}

void Session::ApplyDeathLink(oot3d::recomp::a32::MemoryBus &memory) {
  if (mLinkDeathFrames > 0)
    --mLinkDeathFrames;
  if (!mKillPending || !LoadedSaveSlot(memory).has_value() || !PlayerCanTakeItem(memory))
    return;
  std::uint16_t health = 0;
  if (!memory.Read16(kSaveContext + kSaveHealth, &health) || static_cast<std::int16_t>(health) <= 0)
    return;
  // No health: the game's own death follows (a fairy in a bottle saves Link).
  if (memory.Write16(kSaveContext + kSaveHealth, 0U)) {
    mKillPending = false;
    mLinkDeathFrames = kLinkDeathFrames;
  }
}

// Triforce hunt: the goal is the required number of Triforce Pieces received
// (the server is the source; the pieces have no item in the game).
void Session::WatchTriforce() {
  if (mTriforceRequired <= 0 || !mSidecar.has_value() || mSidecar->GoalReached)
    return;
  int pieces = 0;
  for (const ReceivedItem &item : mServerItems)
    pieces += ItemName(item.Item) == "Triforce Piece" ? 1 : 0;
  if (pieces == mTriforcePieces)
    return;
  mTriforcePieces = pieces;
  if (pieces < mTriforceRequired) {
    Notify(Notification::Kind::Info, "Triforce Pieces: " + std::to_string(pieces) + " of " +
                                         std::to_string(mTriforceRequired));
    return;
  }
  mSidecar->GoalReached = true;
  Persist();
  Log("Goal: " + std::to_string(pieces) + " Triforce Pieces");
  Notify(Notification::Kind::Goal, "Goal complete: the Triforce is whole!");
  if (mClient)
    mClient->SendGoal();
}

void Session::WatchGoal(oot3d::recomp::a32::MemoryBus &memory) {
  std::uint32_t play = 0;
  std::uint16_t scene = 0;
  if (!memory.Read32(kGlobalContextPointer, &play) || play == 0U || PlayerActor(memory) == 0U ||
      !memory.Read16(play + kPlaySceneNum, &scene) || scene == mPreviousScene)
    return;
  // Leaving Ganon's battle alive and in gameplay is the end of the fight;
  // a game over there sends Link back into Ganon's Castle instead.
  std::uint32_t gameMode = 1;
  const bool won = mPreviousScene == kSceneGanonBattle && !mDiedInScene &&
                   memory.Read32(kSaveContext + kSaveGameMode, &gameMode) && gameMode == 0U;
  mPreviousScene = scene;
  mDiedInScene = false;
  if (!won || !mSidecar.has_value() || mSidecar->GoalReached)
    return;
  mSidecar->GoalReached = true;
  Persist();
  Log("Goal: Ganon is defeated");
  Notify(Notification::Kind::Goal, "Goal complete: Ganon is defeated!");
  if (mClient)
    mClient->SendGoal();
}

// Items found in this world come back without a message: the location
// showed them, or the placeholder's message named them on sending.
void Session::NotifyReceived(const ReceivedItem &item, const std::string &name) {
  if (!mSlot.has_value())
    return;
  if (item.Player != mSlot->Slot)
    Notify(Notification::Kind::ItemReceived,
           "Received " + name + " from " + (item.PlayerName.empty() ? std::string("another player") : item.PlayerName));
  else if (item.Location <= 0)
    Notify(Notification::Kind::ItemReceived, "Received " + name);
  else if (mSidecar.has_value() && mSidecar->GivenAtLocation.count(item.Location) == 0U)
    Notify(Notification::Kind::ItemReceived, "Got " + name);
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
    // Items of this world come from this slot's own locations; the starting
    // inventory and !getitem have none (negative location ids).
    const bool ours = item.Player == mSlot->Slot && item.Location > 0;
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
      NotifyReceived(item, name);
    } else if (const auto *direct = std::get_if<GiveDirect>(&plan)) {
      if (!ApplyDirect(*direct, memory))
        return;
      Log("Received " + name);
      NotifyReceived(item, name);
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
  FlushPersist(); // the previous save's sidecar, before it is replaced
  mWriter.WaitIdle(); // the file read next may still be queued
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
  if (mSidecar->GoalReached && mClient)
    mClient->SendGoal();
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
    if (changed->State != ConnectionState::SlotConnected && mWasConnected) {
      mWasConnected = false;
      Notify(Notification::Kind::Warning, changed->State == ConnectionState::Disconnected
                                              ? "Disconnected from Archipelago"
                                              : "Connection to Archipelago lost; reconnecting");
    }
  } else if (const auto *slot = std::get_if<NetworkEvent::SlotConnected>(&variant)) {
    mWasConnected = true;
    Notify(Notification::Kind::Info, "Connected to Archipelago as " + mSettings.Slot);
    mSlot = SidecarIdentity{slot->Seed, slot->Team, slot->Slot, -1};
    mServerChecked = {slot->CheckedLocations.begin(), slot->CheckedLocations.end()};
    mServerItems.clear();
    mPlacements.clear();
    mPlacementNames.clear();
    mSidecar.reset();
    const auto slotData = nlohmann::json::parse(slot->SlotDataJson, nullptr, false);
    mDeathLink = slotData.is_object() && slotData.value("death_link", false);
    mWorld = ParseWorldState(slot->SlotDataJson);
    mTriforceRequired = slotData.is_object() && slotData.value("triforce_hunt", 0) != 0
                            ? slotData.value("triforce_hunt_pieces_required", 0)
                            : 0;
    mTriforcePieces = 0;
    mShopPrices.clear();
    if (slotData.is_object() && mLocations.has_value())
      if (const auto prices = slotData.find("shop_prices"); prices != slotData.end() && prices->is_object())
        for (const auto &[name, price] : prices->items())
          if (const auto *entry = mLocations->ByName(name); entry != nullptr && price.is_number_integer())
            mShopPrices[entry->Id] = price.get<int>();
    mWorldLoggedForSlot = -1;
    if (mClient)
      mClient->SetDeathLink(mDeathLink);
    Log("Slot connected: seed " + slot->Seed + ", team " + std::to_string(slot->Team) +
        ", slot " + std::to_string(slot->Slot) + ", " +
        std::to_string(slot->CheckedLocations.size()) + " checked, " +
        std::to_string(slot->MissingLocations.size()) + " missing");
  } else if (const auto *refused = std::get_if<NetworkEvent::SlotRefused>(&variant)) {
    std::string errors;
    for (const auto &error : refused->Errors)
      errors += (errors.empty() ? "" : ", ") + error;
    Log("Slot refused: " + errors);
    Notify(Notification::Kind::Warning, "Archipelago refused the slot: " + errors);
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
    for (const auto &item : scouted->Items) {
      mPlacements[item.Location] = Placement{item.Item, item.Player, item.Flags};
      mPlacementNames[item.Location] = {item.ItemName, item.PlayerName};
    }
    if (mSidecar.has_value()) {
      for (const auto &[location, placement] : mPlacements)
        mSidecar->Placements[location] = placement;
      Persist();
    }
    Log("Scouted " + std::to_string(scouted->Items.size()) + " locations");
  } else if (const auto *checked = std::get_if<NetworkEvent::LocationsChecked>(&variant)) {
    mServerChecked.insert(checked->Locations.begin(), checked->Locations.end());
  } else if (const auto *death = std::get_if<NetworkEvent::DeathReceived>(&variant)) {
    if (mDeathLink) {
      mKillPending = true;
      Log("DeathLink: " + death->Source + " died" + (death->Cause.empty() ? "" : " (" + death->Cause + ")"));
      Notify(Notification::Kind::Death,
             "DeathLink: " + (death->Cause.empty() ? death->Source + " died" : death->Cause));
    }
  } else if (const auto *message = std::get_if<NetworkEvent::Message>(&variant)) {
    Log(message->Text);
  }
}

// Marks the sidecar for writing at the end of the frame (FlushPersist): a
// new file receiving many items, or the scouts on connecting, would
// otherwise write the whole file once per item.
void Session::Persist() { mPersistPending = mSidecar.has_value(); }

void Session::FlushPersist() {
  if (auto error = mWriter.TakeError())
    Log("Cannot save Archipelago progress: " + *error);
  if (!mPersistPending || !mSidecar.has_value())
    return;
  mPersistPending = false;
  mWriter.Write(*mSidecar);
}

} // namespace Oot3dArchipelago
