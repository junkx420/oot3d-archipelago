#pragma once

// Phase 1 of the Archipelago integration: observe item checks, change nothing.
//
// Hooks are guest function entries, so they are block starts of every
// execution mode (interpreter, compiled functions, whole-AOT) and need no
// title-module rebuild. The observer only reads guest state; it never writes
// registers or memory and never raises an observable exit.

#include "ap_location_key.h"
#include "recomp/a32_runtime.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Oot3dArchipelago {

// Guest functions the observer hooks, on the EUR execution image
// 16a6b0aa... Addresses and window hashes come from addresses.json
// (OoT3D_Randomizer oot.ld, EUR offsets); the tests keep both in sync.
struct GuestFunction {
  const char *Name;
  std::uint32_t Address;
  const char *WindowSha256; // first 32 bytes at Address
};

inline constexpr std::uint32_t kActorOfferGetItem = 0x003724DCU;
inline constexpr std::uint32_t kItemGive = 0x00376A78U;

inline constexpr std::array<GuestFunction, 2> kGuestFunctions{{
    {"Actor_OfferGetItem", kActorOfferGetItem,
     "6c88ffb201ce9bba5920ffb4a4fca8247ae3859eab9e30dc8be729388888ecc4"},
    {"Item_Give", kItemGive,
     "e4f40f57614484da8e6cad178fc76bc97f1a26418815587c7c735c1be5c401c5"},
}};

// Guest layout (OoT3D_Randomizer code/include/z3D, EUR = USA here).
inline constexpr std::uint32_t kSaveContext = 0x00587958U;
inline constexpr std::uint32_t kSaveRespawnReturnData = kSaveContext + 0x14F0U + 0x1CU + 0x13U;
inline constexpr std::uint32_t kPlaySceneNum = 0x0104U;
// actorCtx (0x208C) + actorList (0x0C) + ACTORTYPE_PLAYER (2) * 8 + first (4)
inline constexpr std::uint32_t kPlayPlayerActor = 0x208CU + 0x0CU + 2U * 8U + 4U;
inline constexpr std::uint32_t kActorId = 0x000U;
inline constexpr std::uint32_t kActorParams = 0x01CU;
inline constexpr std::uint32_t kEnItem00CollectibleFlag = 0x1A8U;
inline constexpr std::uint32_t kPlayerGetItemId = 0x12ACU;
inline constexpr std::uint32_t kPlayerInteractRangeActor = 0x12B0U;

struct CheckEvent {
  std::uint64_t Sequence = 0;
  std::uint8_t Scene = 0;
  std::uint8_t ItemId = 0;          // Item_Give argument
  std::uint8_t PlayerGetItemId = 0; // player->getItemId at Item_Give, absolute
  std::uint16_t ActorId = 0;        // source actor, 0 when none
  std::uint16_t ActorParams = 0;
  std::uint8_t OfferGetItemId = 0;  // the matched offer's get-item id
  std::optional<LocationKey> Key;
  const char *Source = "none"; // "offer", "direct" or "none"
};

std::string FormatEvent(const CheckEvent &event);

class CheckObserver final {
public:
  // verifyAddresses=false is for host tests whose fake memory holds no code.
  explicit CheckObserver(bool verifyAddresses = true) : mVerified(!verifyAddresses) {}
  // With a report sink, the hook hit counts are reported once on destruction
  // (the end of the game session).
  ~CheckObserver();
  CheckObserver(const CheckObserver &) = delete;
  CheckObserver &operator=(const CheckObserver &) = delete;
  void SetReportSink(std::function<void(const std::string &)> sink) {
    mReportSink = std::move(sink);
  }

  // Sorted guest pcs to add to the block-entry hook list. A probe pc (the
  // hook experiment, diagnostics only) is counted but not interpreted; it is
  // not signature-checked, so it must be an entry of the running image.
  static std::vector<std::uint32_t> HookPcs(std::optional<std::uint32_t> probePc = {});
  void SetProbePc(std::optional<std::uint32_t> probePc) noexcept { mProbePc = probePc; }

  // Compares every hooked function's window hash with the running image.
  // Returns false with a message naming the first mismatch.
  static bool VerifyAddresses(oot3d::recomp::a32::MemoryBus &memory, std::string *error);

  void SetSink(std::function<void(const CheckEvent &)> sink) { mSink = std::move(sink); }
  // Called for every hooked block entry of the window, not only the
  // observer's own pcs: the first call runs VerifyAddresses (guest code is
  // mapped by then) and throws std::runtime_error on a mismatch.
  void ObserveBlockEntry(std::uint32_t pc, oot3d::recomp::a32::GuestState &state,
                         oot3d::recomp::a32::MemoryBus &memory);
  [[nodiscard]] bool Verified() const noexcept { return mVerified; }

  struct HookHits {
    std::uint64_t Probe = 0;
    std::uint64_t ActorOfferGetItem = 0;
    std::uint64_t ItemGive = 0;
  };
  [[nodiscard]] const HookHits &Hits() const noexcept { return mHits; }

private:
  struct Offer {
    std::uint32_t Actor = 0;
    OfferSource Source{};
  };
  void OnOffer(oot3d::recomp::a32::GuestState &state, oot3d::recomp::a32::MemoryBus &memory);
  void OnItemGive(oot3d::recomp::a32::GuestState &state, oot3d::recomp::a32::MemoryBus &memory);

  std::function<void(const CheckEvent &)> mSink;
  std::function<void(const std::string &)> mReportSink;
  std::optional<Offer> mLastOffer;
  std::optional<std::uint32_t> mProbePc;
  HookHits mHits{};
  std::uint64_t mSequence = 0;
  bool mVerified = false;
};

} // namespace Oot3dArchipelago
