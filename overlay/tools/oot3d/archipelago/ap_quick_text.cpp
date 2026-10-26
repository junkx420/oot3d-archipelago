#include "ap_quick_text.h"

#include "ap_sha256.h"

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::GuestState;
using oot3d::recomp::a32::MemoryBus;

constexpr std::array<std::uint32_t, 2> kHooks{kQuickTextCheckControlCodeBlock, kQuickTextHandleControlCodeBlock};
static_assert(kHooks[0] < kHooks[1]);

// The instant-text command's type tag (Message_SetInstantText's literal).
constexpr std::uint32_t kInstantTextCommandTag = 0x004ED174U;
constexpr std::uint8_t kInstantTextCommand = 6U;

std::uint32_t Read32(MemoryBus &memory, std::uint32_t address) {
  std::uint32_t value = 0;
  return memory.Read32(address, &value) ? value : 0U;
}

std::uint8_t Read8(MemoryBus &memory, std::uint32_t address) {
  std::uint8_t value = 0;
  return memory.Read8(address, &value) ? value : 0U;
}

} // namespace

const char *ToString(QuickTextMode mode) noexcept {
  switch (mode) {
  case QuickTextMode::Off:
    return "off";
  case QuickTextMode::Instant:
    return "instant";
  case QuickTextMode::Turbo:
    return "turbo";
  }
  return "off";
}

std::optional<QuickTextMode> ParseQuickTextMode(std::string_view text) noexcept {
  for (const auto mode : {QuickTextMode::Off, QuickTextMode::Instant, QuickTextMode::Turbo})
    if (text == ToString(mode))
      return mode;
  return std::nullopt;
}

std::span<const std::uint32_t> QuickText::HookPcs() noexcept { return kHooks; }

// Once the code is mapped: every site's window and the message object
// literal. Before that, nothing is decided.
bool QuickText::Verify(MemoryBus &memory) {
  if (mVerified || !mDisabled.empty())
    return mVerified;
  for (const auto &site : kQuickTextSites) {
    std::array<std::uint8_t, 32> window{};
    for (std::size_t i = 0; i < window.size(); ++i)
      if (!memory.Read8(site.Address + static_cast<std::uint32_t>(i), &window[i]))
        return false; // not mapped yet
    if (Sha256Hex(window) != site.WindowSha256) {
      mDisabled = std::string(site.Name) + " does not match the supported EUR image";
      return false;
    }
  }
  if (Read32(memory, kMessageGetStateObjectLiteral) != kMessageObject) {
    mDisabled = "Message_GetState does not ask the expected message object";
    return false;
  }
  mVerified = true;
  return true;
}

// Message_SetInstantText (0x0040B608):
//   if (count < 0x200) { next = used + 8; if (next <= 0x4000) { used = next;
//     entry = pool + old used; entry->tag = ...; entry->type = 6;
//     list[count++] = entry; } }
void QuickText::SetInstantText(std::uint32_t text, MemoryBus &memory) {
  const auto count = static_cast<std::int32_t>(Read32(memory, text + 0xD8U));
  if (count >= 0x200)
    return;
  const std::uint32_t used = Read32(memory, text + 0xE0U);
  const std::uint32_t next = used + 8U;
  if (next > 0x4000U)
    return;
  const std::uint32_t entry = used + Read32(memory, text + 0xDCU);
  (void)memory.Write32(text + 0xE0U, next);
  if (entry == 0U)
    return;
  (void)memory.Write8(entry + 4U, kInstantTextCommand);
  (void)memory.Write32(entry, kInstantTextCommandTag);
  const std::uint32_t list = Read32(memory, text + 0xD4U);
  (void)memory.Write32(list + static_cast<std::uint32_t>(count) * 4U, entry);
  (void)memory.Write32(text + 0xD8U, static_cast<std::uint32_t>(count + 1));
}

bool QuickText::TextBoxOpen(MemoryBus &memory) {
  return Read8(memory, kMessageObject + kMessageObjectState) != 0U;
}

void QuickText::ObserveBlockEntry(std::uint32_t pc, GuestState &state, MemoryBus &memory) {
  if (mMode == QuickTextMode::Off || !Verify(memory))
    return;
  switch (pc) {
  case kQuickTextCheckControlCodeBlock: {
    // Before Message_CheckForControlCodes(r6, nextChars = r8): r5 is the text
    // object, r9 the character index. Instant from a box's first character
    // and right after a box break (0x7F 0x01).
    const std::uint32_t next = state.r[8];
    const std::uint32_t index = state.r[9];
    if (index == 0U || (index >= 2U && Read8(memory, next - 2U) == kTextControlCode &&
                        Read8(memory, next - 1U) == kTextWaitForInput)) {
      SetInstantText(state.r[5], memory);
      ++mHits.Instant;
    }
    break;
  }
  case kQuickTextHandleControlCodeBlock: {
    // The control code about to be dispatched is the byte at r6 + 4. The
    // randomizer bypasses "instant off" and "unskippable"; "instant on" in
    // their place keeps the text instant and does nothing else.
    const std::uint8_t code = Read8(memory, state.r[6] + 4U);
    if (code == kTextInstantOff || code == kTextUnskippable) {
      (void)memory.Write8(state.r[6] + 4U, kTextInstantOn);
      ++mHits.ControlCodes;
    }
    break;
  }
  default:
    break;
  }
}

void QuickText::FilterInput(std::uint32_t &buttons, std::uint32_t bButton, MemoryBus &memory) {
  if (mMode != QuickTextMode::Turbo || (buttons & bButton) == 0U || !Verify(memory) ||
      !TextBoxOpen(memory)) {
    mTurboPhase = 0;
    return;
  }
  // Pressed for the first half period, released for the second: the game
  // sees a new press every other game frame while B stays held.
  if (mTurboPhase % (2U * kTurboHalfPeriod) == 0U && mTurboPhase != 0U)
    ++mHits.TurboPresses;
  if (mTurboPhase % (2U * kTurboHalfPeriod) >= kTurboHalfPeriod)
    buttons &= ~bButton;
  ++mTurboPhase;
}

} // namespace Oot3dArchipelago
