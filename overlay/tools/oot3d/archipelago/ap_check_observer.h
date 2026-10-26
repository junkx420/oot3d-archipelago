#pragma once

// Observes item checks (Phase 1) and, with an override set, changes what a
// location gives (Phase 4).
//
// Hooks are guest function entries, so they are block starts of every
// execution mode (interpreter, compiled functions, whole-AOT) and need no
// title-module rebuild. The observer reads guest state; its one write is the
// get-item argument of Actor_OfferGetItem (r2, which block-entry callbacks
// may change) when the override names another item. It never writes memory
// and never raises an observable exit.

#include "ap_guest_layout.h"
#include "ap_location_key.h"
#include "ap_quick_text.h"
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
// Not a function: the block holding OoT3D_Randomizer's GameOverStart patch
// site, which runs once when Link dies without a fairy (addresses.json kind
// hook_block). It starts after a call, so every execution mode enters it.
inline constexpr std::uint32_t kGameOverStartBlock = 0x00252420U;

inline constexpr std::array<GuestFunction, 3> kGuestFunctions{{
    {"Actor_OfferGetItem", kActorOfferGetItem,
     "6c88ffb201ce9bba5920ffb4a4fca8247ae3859eab9e30dc8be729388888ecc4"},
    {"Item_Give", kItemGive,
     "e4f40f57614484da8e6cad178fc76bc97f1a26418815587c7c735c1be5c401c5"},
    {"GameOverStartBlock", kGameOverStartBlock,
     "6a643f94311062f2ce147592b043023f63114857eef0a2b5aacb4254f8db40cd"},
}};

struct CheckEvent {
  std::uint64_t Sequence = 0;
  std::uint8_t Scene = 0;
  std::uint8_t ItemId = 0;          // Item_Give argument
  std::uint8_t PlayerGetItemId = 0; // player->getItemId at Item_Give, absolute
  std::uint16_t ActorId = 0;        // source actor, 0 when none
  std::uint16_t ActorParams = 0;
  std::uint8_t OfferGetItemId = 0;  // the matched offer's get-item id
  std::uint8_t OverrideGetItemId = 0; // what the override gave instead, 0 for none
  std::optional<LocationKey> Key;
  const char *Source = "none"; // "offer", "direct" or "none"
};

std::string FormatEvent(const CheckEvent &event);

class CheckObserver final {
public:
  // verifyAddresses=false is for host tests whose fake memory holds no code.
  explicit CheckObserver(bool verifyAddresses = true)
      : mVerified(!verifyAddresses), mQuickText(verifyAddresses) {}
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
  // Called when Link dies without a fairy.
  void SetDeathSink(std::function<void()> sink) { mDeathSink = std::move(sink); }
  // Called for every offer that forms a location key; a value replaces the
  // offered get-item id (its sign is kept).
  using GetItemOverride = std::function<std::optional<std::uint16_t>(const LocationKey &key,
                                                                     oot3d::recomp::a32::MemoryBus &memory)>;
  void SetOverride(GetItemOverride override) { mOverride = std::move(override); }
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
    std::uint64_t GameOver = 0;
  };
  [[nodiscard]] const HookHits &Hits() const noexcept { return mHits; }

  // Quick text rides on the same hooks (HookPcs includes its blocks).
  [[nodiscard]] QuickText &Text() noexcept { return mQuickText; }

private:
  struct Offer {
    std::uint32_t Actor = 0;
    OfferSource Source{};
    std::uint8_t OverrideGetItemId = 0;
  };
  void OnOffer(oot3d::recomp::a32::GuestState &state, oot3d::recomp::a32::MemoryBus &memory);
  void OnItemGive(oot3d::recomp::a32::GuestState &state, oot3d::recomp::a32::MemoryBus &memory);

  std::function<void(const CheckEvent &)> mSink;
  std::function<void(const std::string &)> mReportSink;
  GetItemOverride mOverride;
  std::function<void()> mDeathSink;
  std::optional<Offer> mLastOffer;
  std::optional<std::uint32_t> mProbePc;
  HookHits mHits{};
  std::uint64_t mSequence = 0;
  bool mVerified = false;
  QuickText mQuickText;
};

} // namespace Oot3dArchipelago
