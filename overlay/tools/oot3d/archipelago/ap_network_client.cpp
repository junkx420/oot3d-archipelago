#include "ap_network_client.h"

#include <apclient.hpp>

#include <chrono>

namespace Oot3dArchipelago {

namespace {

// Receive every item from the server, including this world's own and the
// starting inventory: the game gives nothing for a check by itself once
// Phase 4 overrides locations, so the server is the only item source.
constexpr int kItemsHandlingAll = 0b111;
constexpr auto kPollInterval = std::chrono::milliseconds(5);

} // namespace

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
      mOwner.mState = ConnectionState::SocketConnected;
      mOwner.Emit(NetworkEvent::StateChanged{ConnectionState::SocketConnected, mSettings.Server});
    });
    mClient->set_socket_error_handler([this](const std::string &error) {
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
      mOwner.Emit(std::move(event));
      mOwner.Emit(NetworkEvent::StateChanged{ConnectionState::SlotConnected, mSettings.Slot});
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
        event.Items.push_back({item.index, item.item, item.location, item.player, item.flags});
      mOwner.Emit(std::move(event));
    });
    mClient->set_location_checked_handler([this](const std::list<std::int64_t> &locations) {
      mOwner.Emit(NetworkEvent::LocationsChecked{{locations.begin(), locations.end()}});
    });
    mClient->set_print_json_handler([this](const APClient::PrintJSONArgs &args) {
      mOwner.Emit(NetworkEvent::Message{mClient->render_json(args.data)});
    });
  }

  void Poll() { mClient->poll(); }
  APClient &Client() { return *mClient; }

private:
  NetworkClient &mOwner;
  ConnectionSettings mSettings;
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
          mState = ConnectionState::Connecting;
          Emit(NetworkEvent::StateChanged{ConnectionState::Connecting, command.Settings.Server});
          session = std::make_unique<Session>(*this, command.Settings);
          break;
        case Command::Kind::Disconnect:
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
        }
      } catch (const std::exception &error) {
        session.reset();
        mState = ConnectionState::Disconnected;
        Emit(NetworkEvent::StateChanged{ConnectionState::Disconnected, error.what()});
      }
    }
    if (session) {
      try {
        session->Poll();
        if (mState == ConnectionState::SlotConnected) {
          if (!pendingChecks.empty() &&
              session->Client().LocationChecks({pendingChecks.begin(), pendingChecks.end()}))
            pendingChecks.clear();
          if (pendingGoal && session->Client().StatusUpdate(APClient::ClientStatus::GOAL))
            pendingGoal = false;
        }
      } catch (const std::exception &error) {
        session.reset();
        mState = ConnectionState::Disconnected;
        Emit(NetworkEvent::StateChanged{ConnectionState::Disconnected, error.what()});
      }
    }
    std::this_thread::sleep_for(kPollInterval);
  }
  session.reset();
}

} // namespace Oot3dArchipelago
