#pragma once

// Archipelago network client (Phase 2).
//
// One background thread owns the apclientpp APClient: it connects, polls and
// runs every apclientpp callback. The game thread never touches the client;
// it posts commands and drains events through mutex-guarded queues, once per
// frame. Nothing here reads or writes guest memory.

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace Oot3dArchipelago {

// Must equal the apworld's game name exactly.
inline constexpr const char *kGameName = "Ocarina of Time 3D";

struct ConnectionSettings {
  std::string Server = "archipelago.gg:38281";
  std::string Slot;
  std::string Password;
};

enum class ConnectionState : std::uint8_t {
  Disconnected,
  Connecting,
  SocketConnected,
  SlotConnected,
};

const char *ToString(ConnectionState state) noexcept;

struct ReceivedItem {
  std::int64_t Index = 0;    // position in the server's item list
  std::int64_t Item = 0;     // AP item id
  std::int64_t Location = 0; // AP location id it came from
  int Player = 0;            // ReceivedItems: finding player's slot; scouts: owner
  unsigned Flags = 0;
};

namespace NetworkEvent {
struct StateChanged {
  ConnectionState State;
  std::string Detail;
};
struct SlotConnected {
  std::string Seed;
  int Team = 0;
  int Slot = 0;
  std::string SlotDataJson;
  std::vector<std::int64_t> CheckedLocations;
  std::vector<std::int64_t> MissingLocations;
};
struct SlotRefused {
  std::vector<std::string> Errors;
};
struct ItemsReceived {
  std::vector<ReceivedItem> Items;
};
struct LocationsChecked {
  std::vector<std::int64_t> Locations;
};
// What this slot's locations hold (LocationScouts, sent on every slot
// connection for all of them). Item is the id in the owning player's game.
struct LocationsScouted {
  std::vector<ReceivedItem> Items; // Index unused; Player owns the item
};
struct Message {
  std::string Text;
};
// Another DeathLink player died (Bounced with the DeathLink tag).
struct DeathReceived {
  std::string Source;
  std::string Cause;
};
} // namespace NetworkEvent

using NetworkEventVariant =
    std::variant<NetworkEvent::StateChanged, NetworkEvent::SlotConnected,
                 NetworkEvent::SlotRefused, NetworkEvent::ItemsReceived,
                 NetworkEvent::LocationsChecked, NetworkEvent::LocationsScouted,
                 NetworkEvent::Message, NetworkEvent::DeathReceived>;

class NetworkClient final {
public:
  // uuid identifies this installation to the server; certStore is a PEM file
  // for wss:// (empty: the system store, as Ship of Harkinian does).
  NetworkClient(std::string uuid, std::string certStore = {});
  ~NetworkClient();
  NetworkClient(const NetworkClient &) = delete;
  NetworkClient &operator=(const NetworkClient &) = delete;

  // Game thread. All calls only queue work and return immediately.
  void Connect(const ConnectionSettings &settings);
  void Disconnect();
  void SendLocationChecks(std::vector<std::int64_t> locations);
  void SendGoal();
  // Joins or leaves DeathLink (the slot's tags); kept across reconnects.
  void SetDeathLink(bool enabled);
  void SendDeath(std::string cause);
  void Say(std::string text);

  // Game thread: events since the last call, in order.
  std::vector<NetworkEventVariant> TakeEvents();
  [[nodiscard]] ConnectionState State() const noexcept { return mState.load(); }

private:
  struct Command {
    enum class Kind : std::uint8_t { Connect, Disconnect, LocationChecks, Goal, Say, DeathLink, Death };
    Kind Type = Kind::Disconnect;
    ConnectionSettings Settings;
    std::vector<std::int64_t> Locations;
    std::string Text;
    bool Enabled = false;
  };
  class Session;
  void Post(Command command);
  void Run();
  void Emit(NetworkEventVariant event);

  std::string mUuid;
  std::string mCertStore;
  std::atomic<ConnectionState> mState{ConnectionState::Disconnected};
  std::atomic<bool> mStop{false};
  std::mutex mCommandMutex;
  std::deque<Command> mCommands;
  std::mutex mEventMutex;
  std::vector<NetworkEventVariant> mEvents;
  std::thread mThread;
};

} // namespace Oot3dArchipelago
