#include "ap_network_client.h"

#include <apclient.hpp>

#include <algorithm>
#include <chrono>
#include <optional>

namespace Oot3dArchipelago {

namespace {

// Receive every item from the server, including this world's own and the
// starting inventory: the game gives nothing for a check by itself once
// Phase 4 overrides locations, so the server is the only item source.
constexpr int kItemsHandlingAll = 0b111;
constexpr auto kPollInterval = std::chrono::milliseconds(5);

} // namespace

std::chrono::seconds RetryDelay(int attempt) noexcept {
  const int exponent = std::clamp(attempt, 1, 5) - 1;
  return std::chrono::seconds(std::min(5 << exponent, 60));
}

const char *ToString(ConnectionState state) noexcept {
  switch (state) {
  case ConnectionState::Disconnected:
    return "disconnected";
  case ConnectionState::Connecting:
    return "connecting";
  case ConnectionState::SocketConnected:
    return "socket connected";
  case ConnectionState::SlotConnected:
    return "slot connected";
  }
  return "?";
}

// One connection attempt. Lives and dies on the network thread.
class NetworkClient::Session final {
public:
  Session(NetworkClient &owner, const ConnectionSettings &settings)
      : mOwner(owner), mSettings(settings),
        mClient(std::make_unique<APClient>(owner.mUuid, kGameName, settings.Server,
                                           owner.mCertStore)) {
    mClient->set_socket_connected_handler([this] {
      mLastSocketError.clear();
      mOwner.mState = ConnectionState::SocketConnected;
      mOwner.Emit(NetworkEvent::StateChanged{ConnectionState::SocketConnected, mSettings.Server});
    });
    // apclientpp retries a closed socket every few seconds; say each
    // different error once.
    mClient->set_socket_error_handler([this](const std::string &error) {
      if (error == mLastSocketError)
        return;
      mLastSocketError = error;
      mOwner.Emit(NetworkEvent::Message{"Socket error: " + error});
    });
    mClient->set_socket_disconnected_handler([this] {
      mOwner.mState = ConnectionState::Connecting;
      mOwner.Emit(NetworkEvent::StateChanged{ConnectionState::Connecting, "socket closed"});
    });
    mClient->set_room_info_handler([this] {
      mClient->ConnectSlot(mSettings.Slot, mSettings.Password, kItemsHandlingAll);
    });
    mClient->set_slot_connected_handler([this](const nlohmann::json &slotData) {
      mOwner.mState = ConnectionState::SlotConnected;
      NetworkEvent::SlotConnected event;
      event.Seed = mClient->get_seed();
      event.Team = mClient->get_team_number();
      event.Slot = mClient->get_player_number();
      event.SlotDataJson = slotData.dump();
      const auto &checked = mClient->get_checked_locations();
      event.CheckedLocations.assign(checked.begin(), checked.end());
      const auto &missing = mClient->get_missing_locations();
      event.MissingLocations.assign(missing.begin(), missing.end());
      // The game overrides what its locations give (Phase 4), so it needs
      // the placement of every location of this slot.
      std::list<std::int64_t> all(checked.begin(), checked.end());
      all.insert(all.end(), missing.begin(), missing.end());
      if (!all.empty())
        mClient->LocationScouts(std::move(all));
      mOwner.Emit(std::move(event));
      mOwner.Emit(NetworkEvent::StateChanged{ConnectionState::SlotConnected, mSettings.Slot});
    });
    mClient->set_location_info_handler([this](const std::list<APClient::NetworkItem> &items) {
      mScouted = items;
      EmitScouted();
    });
    // Scouts that arrived before the data package named nothing: name them
    // again once it is there.
    mClient->set_data_package_changed_handler([this](const nlohmann::json &) {
      if (!mScouted.empty())
        EmitScouted();
    });
    mClient->set_slot_refused_handler([this](const std::list<std::string> &errors) {
      mOwner.Emit(NetworkEvent::SlotRefused{{errors.begin(), errors.end()}});
    });
    mClient->set_slot_disconnected_handler([this] {
      mOwner.mState = ConnectionState::SocketConnected;
      mOwner.Emit(NetworkEvent::StateChanged{ConnectionState::SocketConnected, "slot disconnected"});
    });
    mClient->set_items_received_handler([this](const std::list<APClient::NetworkItem> &items) {
      NetworkEvent::ItemsReceived event;
      for (const auto &item : items)
        event.Items.push_back({item.index, item.item, item.location, item.player, item.flags,
                               Known(mClient->get_item_name(item.item, kGameName)),
                               Known(mClient->get_player_alias(item.player))});
      mOwner.Emit(std::move(event));
    });
    mClient->set_location_checked_handler([this](const std::list<std::int64_t> &locations) {
      mOwner.Emit(NetworkEvent::LocationsChecked{{locations.begin(), locations.end()}});
    });
    mClient->set_bounced_handler([this](const nlohmann::json &packet) {
      const auto tags = packet.value("tags", nlohmann::json::array());
      if (std::find(tags.begin(), tags.end(), "DeathLink") == tags.end())
        return;
      const auto data = packet.value("data", nlohmann::json::object());
      const std::string source = data.value("source", "");
      if (source == mSettings.Slot)
        return; // our own death, echoed to every DeathLink client
      mOwner.Emit(NetworkEvent::DeathReceived{source, data.value("cause", "")});
    });
    mClient->set_print_json_handler([this](const APClient::PrintJSONArgs &args) {
      mOwner.Emit(NetworkEvent::Message{mClient->render_json(args.data)});
    });
  }

  void Poll() { mClient->poll(); }
  APClient &Client() { return *mClient; }

private:
  // apclientpp names what its data package lacks "Unknown".
  static std::string Known(std::string name) { return name == "Unknown" ? std::string() : name; }

  void EmitScouted() {
    NetworkEvent::LocationsScouted event;
    for (const auto &item : mScouted)
      event.Items.push_back({0, item.item, item.location, item.player, item.flags,
                             Known(mClient->get_item_name(item.item, mClient->get_player_game(item.player))),
                             Known(mClient->get_player_alias(item.player))});
    mOwner.Emit(std::move(event));
  }

  NetworkClient &mOwner;
  ConnectionSettings mSettings;
  std::string mLastSocketError;
  std::list<APClient::NetworkItem> mScouted;
  std::unique_ptr<APClient> mClient;
};

NetworkClient::NetworkClient(std::string uuid, std::string certStore)
    : mUuid(std::move(uuid)), mCertStore(std::move(certStore)),
      mThread([this] { Run(); }) {}

NetworkClient::~NetworkClient() {
  mStop = true;
  if (mThread.joinable())
    mThread.join();
}

void NetworkClient::Connect(const ConnectionSettings &settings) {
  mState = ConnectionState::Connecting; // at once, not when the thread gets to it
  Command command;
  command.Type = Command::Kind::Connect;
  command.Settings = settings;
  Post(std::move(command));
}

void NetworkClient::Disconnect() { Post({Command::Kind::Disconnect}); }

void NetworkClient::SendLocationChecks(std::vector<std::int64_t> locations) {
  Command command;
  command.Type = Command::Kind::LocationChecks;
  command.Locations = std::move(locations);
  Post(std::move(command));
}

void NetworkClient::SendGoal() { Post({Command::Kind::Goal}); }

void NetworkClient::SetDeathLink(bool enabled) {
  Command command;
  command.Type = Command::Kind::DeathLink;
  command.Enabled = enabled;
  Post(std::move(command));
}

void NetworkClient::SendDeath(std::string cause) {
  Command command;
  command.Type = Command::Kind::Death;
  command.Text = std::move(cause);
  Post(std::move(command));
}

void NetworkClient::Say(std::string text) {
  Command command;
  command.Type = Command::Kind::Say;
  command.Text = std::move(text);
  Post(std::move(command));
}

std::vector<NetworkEventVariant> NetworkClient::TakeEvents() {
  std::lock_guard lock(mEventMutex);
  std::vector<NetworkEventVariant> events;
  events.swap(mEvents);
  return events;
}

void NetworkClient::Post(Command command) {
  std::lock_guard lock(mCommandMutex);
  mCommands.push_back(std::move(command));
}

void NetworkClient::Emit(NetworkEventVariant event) {
  std::lock_guard lock(mEventMutex);
  mEvents.push_back(std::move(event));
}

void NetworkClient::Run() {
  std::unique_ptr<Session> session;
  // Checks sent while the slot is not connected wait here; the server
  // ignores duplicates, so resending after a reconnect is safe.
  std::vector<std::int64_t> pendingChecks;
  bool pendingGoal = false;
  bool deathLink = false;
  bool tagsSent = false;
  // The last Connect, kept until Disconnect: a session that fails with an
  // error is started again after RetryDelay.
  std::optional<ConnectionSettings> wanted;
  int failures = 0;
  auto retryAt = std::chrono::steady_clock::time_point::max();
  const auto fail = [&](const std::exception &error) {
    session.reset();
    if (!wanted.has_value()) {
      mState = ConnectionState::Disconnected;
      Emit(NetworkEvent::StateChanged{ConnectionState::Disconnected, error.what()});
      return;
    }
    const auto delay = RetryDelay(++failures);
    retryAt = std::chrono::steady_clock::now() + delay;
    mState = ConnectionState::Connecting;
    Emit(NetworkEvent::StateChanged{ConnectionState::Connecting,
                                    std::string(error.what()) + "; retrying in " +
                                        std::to_string(delay.count()) + " s"});
  };
  while (!mStop) {
    std::deque<Command> commands;
    {
      std::lock_guard lock(mCommandMutex);
      commands.swap(mCommands);
    }
    for (auto &command : commands) {
      try {
        switch (command.Type) {
        case Command::Kind::Connect:
          session.reset();
          wanted = command.Settings;
          failures = 0;
          retryAt = std::chrono::steady_clock::time_point::max();
          mState = ConnectionState::Connecting;
          Emit(NetworkEvent::StateChanged{ConnectionState::Connecting, command.Settings.Server});
          session = std::make_unique<Session>(*this, command.Settings);
          break;
        case Command::Kind::Disconnect:
          wanted.reset();
          retryAt = std::chrono::steady_clock::time_point::max();
          session.reset();
          mState = ConnectionState::Disconnected;
          Emit(NetworkEvent::StateChanged{ConnectionState::Disconnected, "by request"});
          break;
        case Command::Kind::LocationChecks:
          pendingChecks.insert(pendingChecks.end(), command.Locations.begin(),
                               command.Locations.end());
          break;
        case Command::Kind::Goal:
          pendingGoal = true;
          break;
        case Command::Kind::Say:
          if (session && mState == ConnectionState::SlotConnected)
            session->Client().Say(command.Text);
          break;
        case Command::Kind::DeathLink:
          deathLink = command.Enabled;
          tagsSent = false;
          break;
        case Command::Kind::Death:
          // A death while not connected is not shared later: it is old news.
          if (session && mState == ConnectionState::SlotConnected && deathLink) {
            const double now = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            session->Client().Bounce({{"time", now},
                                      {"source", session->Client().get_slot()},
                                      {"cause", command.Text}},
                                     {}, {}, {"DeathLink"});
          }
          break;
        }
      } catch (const std::exception &error) {
        fail(error);
      }
    }
    if (!session && wanted.has_value() && std::chrono::steady_clock::now() >= retryAt) {
      retryAt = std::chrono::steady_clock::time_point::max();
      try {
        Emit(NetworkEvent::StateChanged{ConnectionState::Connecting, "retry " + std::to_string(failures)});
        session = std::make_unique<Session>(*this, *wanted);
      } catch (const std::exception &error) {
        fail(error);
      }
    }
    if (session) {
      try {
        session->Poll();
        if (mState != ConnectionState::SlotConnected)
          tagsSent = false; // a new slot connection starts without tags
        if (mState == ConnectionState::SlotConnected) {
          failures = 0;
          if (!tagsSent) {
            const std::list<std::string> tags = deathLink ? std::list<std::string>{"DeathLink"}
                                                          : std::list<std::string>{};
            tagsSent = session->Client().ConnectUpdate(false, 0, true, tags);
          }
          if (!pendingChecks.empty() &&
              session->Client().LocationChecks({pendingChecks.begin(), pendingChecks.end()}))
            pendingChecks.clear();
          if (pendingGoal && session->Client().StatusUpdate(APClient::ClientStatus::GOAL))
            pendingGoal = false;
        }
      } catch (const std::exception &error) {
        fail(error);
      }
    }
    std::this_thread::sleep_for(kPollInterval);
  }
  session.reset();
}

} // namespace Oot3dArchipelago
