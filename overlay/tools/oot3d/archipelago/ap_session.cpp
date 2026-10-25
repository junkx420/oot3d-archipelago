#include "ap_session.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <random>
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

} // namespace

Session::Session(Paths paths) : mPaths(std::move(paths)) {
  LoadSettings();
  std::string error;
  mLocations = LocationTable::Load(mPaths.LocationTable, &error);
  if (mLocations.has_value())
    Log("Location table: " + std::to_string(mLocations->Size()) + " locations");
  else
    Log("No location table yet (" + error + "); checks are logged, not sent.");
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
                           {"uuid", mUuid}}
                .dump(1)
         << '\n';
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
}

void Session::OpenSidecarIfReady(oot3d::recomp::a32::MemoryBus &memory) {
  if (!mSlot.has_value())
    return;
  // The save slot is valid only while a file is loaded ("ZELDAZ" present).
  char newf[6] = {};
  for (std::uint32_t i = 0; i < 6U; ++i) {
    std::uint8_t byte = 0;
    if (!memory.Read8(kSaveNewf + i, &byte))
      return;
    newf[i] = static_cast<char>(byte);
  }
  std::uint32_t fileNum = 0;
  if (std::string_view(newf, 6) != "ZELDAZ" || !memory.Read32(kSaveFileNum, &fileNum) ||
      fileNum >= static_cast<std::uint32_t>(kSaveSlots))
    return;
  SidecarIdentity identity = *mSlot;
  identity.SaveSlot = static_cast<int>(fileNum);
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
    // Phase 2 records items only; Phase 4 gives them in game and advances
    // AppliedItems. The server resends from index 0 on every connection.
    for (const auto &item : items->Items) {
      if (item.Index < static_cast<std::int64_t>(mServerItems.size()))
        continue;
      mServerItems.push_back(item);
      Log("Item #" + std::to_string(item.Index) + ": id " + std::to_string(item.Item) +
          " from player " + std::to_string(item.Player));
    }
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
