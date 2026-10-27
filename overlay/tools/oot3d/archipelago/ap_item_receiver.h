#pragma once

// Gives Link a get-item the way OoT3D_Randomizer gives pending items
// (code/src/item_override.c, ItemOverride_Update): a minimal actor in guest
// memory (update set, parent null) is offered to Player as
// interactRangeActor together with getItemId while Link can take an item.
// Player's own code then plays the get-item, calls Item_Give and sets the
// actor's parent, which confirms the give. Nothing is patched.

#include "ap_location_key.h"
#include "recomp/a32_runtime.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Oot3dArchipelago {

// Maps the client's guest page (kClientRegionBase); returns false with a
// message. The window passes NativeA32Memory::MapRegion.
using GuestRegionMapper = std::function<bool(std::uint32_t base, std::uint32_t size, std::string *error)>;

// The play state's player actor, or 0 outside gameplay.
std::uint32_t PlayerActor(oot3d::recomp::a32::MemoryBus &memory);

// ItemOverride_CheckPlayerReadiness, standing case only: no blocking state,
// visible, on the ground and not submerged, no title card, no scene load,
// no get-item or interaction in progress.
bool PlayerCanTakeItem(oot3d::recomp::a32::MemoryBus &memory);

// Whether the save records a chest or collectible location as taken: the
// current scene's flags live in the play state until the scene changes.
// Other location types have no single flag; they return nullopt.
std::optional<bool> LocationTakenInSave(const LocationKey &key, oot3d::recomp::a32::MemoryBus &memory);

class ItemReceiver final {
public:
  explicit ItemReceiver(GuestRegionMapper mapper) : mMapper(std::move(mapper)) {}

  enum class Result : std::uint8_t {
    Waiting,     // Link is not ready, or the offer is not taken yet
    Given,       // Player took the item
    Unavailable, // the client page cannot be mapped; nothing can be given
  };
  // Call once per frame with the same get-item until it returns Given.
  Result Give(std::uint16_t getItemId, oot3d::recomp::a32::MemoryBus &memory, std::string *error);
  // Withdraws an offer that was not taken (the item changed, or the save
  // was closed).
  void Cancel(oot3d::recomp::a32::MemoryBus &memory);
  // Maps the client page if it is not (again); false if it cannot be.
  bool EnsurePage(oot3d::recomp::a32::MemoryBus &memory, std::string *error);
  // Forgets the actor and any offer: guest memory was replaced (savestate).
  void Reset() noexcept;
  // An item is offered and not yet confirmed taken.
  [[nodiscard]] bool Offering() const noexcept { return mOffered; }

private:
  bool EnsureActor(oot3d::recomp::a32::MemoryBus &memory, std::string *error);

  GuestRegionMapper mMapper;
  bool mActorReady = false;
  bool mUnavailable = false;
  bool mOffered = false;
};

} // namespace Oot3dArchipelago
