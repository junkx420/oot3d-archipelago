#include "ap_check_observer.h"

#include "ap_sha256.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::GuestState;
using oot3d::recomp::a32::MemoryBus;

constexpr std::array<std::uint32_t, 3> kCheckHooks{kGameOverStartBlock, kActorOfferGetItem, kItemGive};
static_assert(kCheckHooks[0] < kCheckHooks[1] && kCheckHooks[1] < kCheckHooks[2]);

// Unreadable guest memory reads as zero: the observer must never fault the
// game, and a zero actor or scene simply forms no key.
std::uint8_t Read8(MemoryBus &memory, std::uint32_t address) {
  std::uint8_t value = 0;
  return address != 0U && memory.Read8(address, &value) ? value : 0U;
}

std::uint16_t Read16(MemoryBus &memory, std::uint32_t address) {
  std::uint16_t value = 0;
  return address != 0U && memory.Read16(address, &value) ? value : 0U;
}

std::uint32_t Read32(MemoryBus &memory, std::uint32_t address) {
  std::uint32_t value = 0;
  return address != 0U && memory.Read32(address, &value) ? value : 0U;
}

std::uint8_t AbsoluteGetItemId(std::uint32_t value) {
  const auto signedValue = static_cast<std::int8_t>(value & 0xFFU);
  return static_cast<std::uint8_t>(signedValue < 0 ? -signedValue : signedValue);
}

} // namespace

CheckObserver::~CheckObserver() {
  if (!mReportSink)
    return;
  char text[320];
  const auto &quick = mQuickText.Counts();
  std::snprintf(text, sizeof(text),
                "[archipelago] hook hits: verified=%d probe(0x%08X)=%llu "
                "Actor_OfferGetItem=%llu Item_Give=%llu GameOver=%llu "
                "quick_text(%s): instant=%llu control_codes=%llu turbo=%llu%s%s",
                mVerified ? 1 : 0, mProbePc.value_or(0U),
                static_cast<unsigned long long>(mHits.Probe),
                static_cast<unsigned long long>(mHits.ActorOfferGetItem),
                static_cast<unsigned long long>(mHits.ItemGive),
                static_cast<unsigned long long>(mHits.GameOver), ToString(mQuickText.Mode()),
                static_cast<unsigned long long>(quick.Instant),
                static_cast<unsigned long long>(quick.ControlCodes),
                static_cast<unsigned long long>(quick.TurboPresses),
                mQuickText.Disabled().empty() ? "" : " disabled: ", mQuickText.Disabled().c_str());
  mReportSink(text);
}

std::vector<std::uint32_t> CheckObserver::HookPcs(std::optional<std::uint32_t> probePc) {
  std::vector<std::uint32_t> pcs(kCheckHooks.begin(), kCheckHooks.end());
  const auto text = QuickText::HookPcs();
  pcs.insert(pcs.end(), text.begin(), text.end());
  if (probePc.has_value())
    pcs.push_back(*probePc);
  std::sort(pcs.begin(), pcs.end());
  pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
  return pcs;
}

bool CheckObserver::VerifyAddresses(MemoryBus &memory, std::string *error) {
  for (const GuestFunction &function : kGuestFunctions) {
    std::array<std::uint8_t, 32> window{};
    for (std::size_t i = 0; i < window.size(); ++i) {
      if (!memory.Read8(function.Address + static_cast<std::uint32_t>(i), &window[i])) {
        if (error != nullptr) {
          char text[128];
          std::snprintf(text, sizeof(text), "Archipelago: %s at 0x%08X is not mapped",
                        function.Name, function.Address);
          *error = text;
        }
        return false;
      }
    }
    if (Sha256Hex(window) != function.WindowSha256) {
      if (error != nullptr) {
        char text[192];
        std::snprintf(text, sizeof(text),
                      "Archipelago: %s at 0x%08X does not match its signature; "
                      "the running code image is not the supported EUR image",
                      function.Name, function.Address);
        *error = text;
      }
      return false;
    }
  }
  return true;
}

void CheckObserver::ObserveBlockEntry(std::uint32_t pc, GuestState &state, MemoryBus &memory) {
  if (!mVerified) {
    std::string error;
    if (!VerifyAddresses(memory, &error))
      throw std::runtime_error(error);
    mVerified = true;
  }
  if (mProbePc.has_value() && pc == *mProbePc)
    ++mHits.Probe;
  switch (pc) {
  case kActorOfferGetItem:
    ++mHits.ActorOfferGetItem;
    OnOffer(state, memory);
    break;
  case kItemGive:
    ++mHits.ItemGive;
    OnItemGive(state, memory);
    break;
  case kGameOverStartBlock:
    ++mHits.GameOver;
    if (mDeathSink)
      mDeathSink();
    break;
  default:
    mQuickText.ObserveBlockEntry(pc, state, memory);
    break;
  }
}

// Actor_OfferGetItem(Actor *actor, PlayState *play, s32 getItemId, ...).
// The game offers every frame while the player is in range; the latest offer
// is remembered until an Item_Give shows which one was accepted.
void CheckObserver::OnOffer(GuestState &state, MemoryBus &memory) {
  const std::uint32_t actor = state.r[0];
  const std::uint32_t play = state.r[1];
  if (actor == 0U || play == 0U)
    return;
  Offer offer;
  offer.Actor = actor;
  offer.Source.ActorId = Read16(memory, actor + kActorId);
  offer.Source.ActorParams = Read16(memory, actor + kActorParams);
  offer.Source.Scene = static_cast<std::uint8_t>(Read16(memory, play + kPlaySceneNum));
  offer.Source.GetItemId = AbsoluteGetItemId(state.r[2]);
  if (offer.Source.ActorId == kActorEnItem00)
    offer.Source.CollectibleFlag = Read16(memory, actor + kEnItem00CollectibleFlag);
  offer.Source.RespawnReturnScene = Read8(memory, kSaveRespawnReturnData);
  if (mOverride && offer.Source.GetItemId != 0U) {
    if (const auto key = KeyForOffer(offer.Source)) {
      if (const auto replacement = mOverride(*key, memory); replacement.has_value() && *replacement != 0U) {
        const auto value = static_cast<std::int32_t>(*replacement & 0xFFU);
        const bool negative = static_cast<std::int32_t>(state.r[2]) < 0;
        state.r[2] = static_cast<std::uint32_t>(negative ? -value : value);
        offer.OverrideGetItemId = static_cast<std::uint8_t>(value);
      }
    }
  }
  mLastOffer = offer;
}

// Item_Give(PlayState *play, u8 item).
void CheckObserver::OnItemGive(GuestState &state, MemoryBus &memory) {
  const std::uint32_t play = state.r[0];
  CheckEvent event;
  event.Sequence = ++mSequence;
  event.ItemId = static_cast<std::uint8_t>(state.r[1] & 0xFFU);
  event.Scene = static_cast<std::uint8_t>(Read16(memory, play + kPlaySceneNum));
  const std::uint32_t player = Read32(memory, play + kPlayPlayerActor);
  event.PlayerGetItemId = AbsoluteGetItemId(Read8(memory, player + kPlayerGetItemId));
  const std::uint32_t interactActor = Read32(memory, player + kPlayerInteractRangeActor);

  // A collected token reaches Item_Give after the player has already cleared
  // getItemId and interactRangeActor (seen in play, 2026-10-25), so it
  // matches the latest offer by actor kind instead.
  const bool tokenFromOffer = mLastOffer.has_value() && event.ItemId == kItemSkullToken &&
                              mLastOffer->Source.ActorId == kActorSkullToken;
  const bool offerAccepted =
      mLastOffer.has_value() &&
      (interactActor == mLastOffer->Actor || tokenFromOffer ||
       (event.PlayerGetItemId != 0U && (event.PlayerGetItemId == mLastOffer->Source.GetItemId ||
                                        event.PlayerGetItemId == mLastOffer->OverrideGetItemId)));
  if (offerAccepted) {
    event.Source = "offer";
    event.ActorId = mLastOffer->Source.ActorId;
    event.ActorParams = mLastOffer->Source.ActorParams;
    event.OfferGetItemId = mLastOffer->Source.GetItemId;
    event.OverrideGetItemId = mLastOffer->OverrideGetItemId;
    event.Key = KeyForOffer(mLastOffer->Source);
    // One offer, one check: a later give without its own offer must not
    // inherit this one.
    mLastOffer.reset();
  } else if (const auto key = KeyForDirectGive(event.ItemId)) {
    event.Source = "direct";
    event.Key = key;
  }
  if (mSink)
    mSink(event);
}

std::string FormatEvent(const CheckEvent &event) {
  char text[256];
  std::snprintf(text, sizeof(text),
                "[archipelago] check #%llu source=%s scene=0x%02X item=0x%02X "
                "player_gi=0x%02X actor=0x%04X params=0x%04X offer_gi=0x%02X override_gi=0x%02X",
                static_cast<unsigned long long>(event.Sequence), event.Source, event.Scene,
                event.ItemId, event.PlayerGetItemId, event.ActorId, event.ActorParams,
                event.OfferGetItemId, event.OverrideGetItemId);
  std::string line = text;
  line += event.Key.has_value() ? " " + Describe(*event.Key) : " key=none";
  return line;
}

} // namespace Oot3dArchipelago
