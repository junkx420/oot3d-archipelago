// Network smoke test against a running Archipelago server, no game needed:
//   triaevum_archipelago_net_smoke <server> <slot> <location id> [password]
// Connects, sends one location check and waits for the item list. Then it
// reconnects: the server's checked_locations must now contain the check
// (apclientpp marks own checks locally, so no event confirms them earlier).
// Exit 0 only when both happened.

#include "ap_network_client.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

using namespace Oot3dArchipelago;

int main(int argc, char **argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <server> <slot> <location id> [password]\n", argv[0]);
    return 2;
  }
  const std::int64_t location = std::strtoll(argv[3], nullptr, 10);
  NetworkClient client("triaevum-net-smoke-0000000000000");
  client.Connect({argv[1], argv[2], argc > 4 ? argv[4] : ""});

  bool connected = false, sent = false, confirmed = false, items = false, reconnected = false;
  std::size_t scouted = 0;
  // With slot_data death_link: join DeathLink, send one death, and wait for
  // one from another client.
  bool deathLink = false, deathSent = false, deathReceived = false;
  auto deathAt = std::chrono::steady_clock::time_point::max();
  int connections = 0;
  auto reconnectAt = std::chrono::steady_clock::time_point::max();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (std::chrono::steady_clock::now() < deadline &&
         !(confirmed && items && scouted != 0U && (!deathLink || deathReceived))) {
    if (deathLink && !deathSent && std::chrono::steady_clock::now() >= deathAt) {
      client.SendDeath("net smoke test");
      deathSent = true;
      std::printf("death sent\n");
    }
    if (!reconnected && std::chrono::steady_clock::now() >= reconnectAt) {
      reconnected = true;
      std::printf("reconnecting\n");
      client.Connect({argv[1], argv[2], argc > 4 ? argv[4] : ""});
    }
    for (const auto &variant : client.TakeEvents()) {
      if (const auto *changed = std::get_if<NetworkEvent::StateChanged>(&variant)) {
        std::printf("state: %s %s\n", ToString(changed->State), changed->Detail.c_str());
      } else if (const auto *slot = std::get_if<NetworkEvent::SlotConnected>(&variant)) {
        connected = true;
        if (++connections == 2)
          for (const auto id : slot->CheckedLocations)
            confirmed |= id == location;
        std::printf("slot connected: seed %s team %d slot %d, %zu checked, %zu missing\n",
                    slot->Seed.c_str(), slot->Team, slot->Slot, slot->CheckedLocations.size(),
                    slot->MissingLocations.size());
        std::printf("slot_data: %s\n", slot->SlotDataJson.c_str());
        if (!deathLink && slot->SlotDataJson.find("\"death_link\":true") != std::string::npos) {
          deathLink = true;
          client.SetDeathLink(true);
          deathAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        }
      } else if (const auto *refused = std::get_if<NetworkEvent::SlotRefused>(&variant)) {
        for (const auto &error : refused->Errors)
          std::printf("refused: %s\n", error.c_str());
        return 1;
      } else if (const auto *received = std::get_if<NetworkEvent::ItemsReceived>(&variant)) {
        items = true;
        for (const auto &item : received->Items)
          std::printf("item #%lld: %lld from player %d (location %lld)\n",
                      static_cast<long long>(item.Index), static_cast<long long>(item.Item),
                      item.Player, static_cast<long long>(item.Location));
      } else if (const auto *scouts = std::get_if<NetworkEvent::LocationsScouted>(&variant)) {
        scouted += scouts->Items.size();
        for (const auto &item : scouts->Items)
          if (item.Location == location)
            std::printf("location %lld holds item %lld of player %d\n", static_cast<long long>(item.Location),
                        static_cast<long long>(item.Item), item.Player);
      } else if (const auto *checked = std::get_if<NetworkEvent::LocationsChecked>(&variant)) {
        for (const auto id : checked->Locations) {
          std::printf("server confirms location %lld\n", static_cast<long long>(id));
          confirmed |= id == location;
        }
      } else if (const auto *death = std::get_if<NetworkEvent::DeathReceived>(&variant)) {
        deathReceived = true;
        std::printf("death received from %s (%s)\n", death->Source.c_str(), death->Cause.c_str());
      } else if (const auto *message = std::get_if<NetworkEvent::Message>(&variant)) {
        std::printf("message: %s\n", message->Text.c_str());
      }
    }
    if (connected && !sent) {
      client.SendLocationChecks({location});
      sent = true;
    }
    if (items && !reconnected && reconnectAt == std::chrono::steady_clock::time_point::max())
      reconnectAt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  client.Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::printf("result: connected=%d sent=%d confirmed=%d items=%d scouted=%zu deathlink=%d sent=%d received=%d\n",
              connected, sent, confirmed, items, scouted, deathLink, deathSent, deathReceived);
  return connected && confirmed && items && scouted != 0U && (!deathLink || deathReceived) ? 0 : 1;
}
