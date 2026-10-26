#pragma once

// Quick text after OoT3D_Randomizer's Quick Text setting (code/src/message.c
// and Settings_IsTurboText): text boxes fill at once, and holding B advances
// and closes them.
//
// Instant text: the randomizer patches two sites in the text parser; here
// each becomes a block-entry hook at a block start near its site
// (addresses.json, kind hook_block) that does what the patch's code does.
// Turbo: the randomizer patches the B checks of several message states. Not
// every state's check is a block start the hooks reach in every execution
// mode, so the B button itself is filtered instead: while a text box is open
// and B is held, the game sees B pressed every other game frame. Nothing is
// patched in the game's code.

#include "recomp/a32_runtime.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Oot3dArchipelago {

enum class QuickTextMode : std::uint8_t {
  Off,     // the game's own text
  Instant, // whole boxes at once, unskippable and slow parts removed
  Turbo,   // Instant, and holding B advances and closes text boxes
};
const char *ToString(QuickTextMode mode) noexcept;
std::optional<QuickTextMode> ParseQuickTextMode(std::string_view text) noexcept;

// Block starts (EUR execution addresses) and the randomizer sites they stand
// for.
inline constexpr std::uint32_t kQuickTextCheckControlCodeBlock = 0x002E0488U;  // CheckForTextControlCode - 8
inline constexpr std::uint32_t kQuickTextHandleControlCodeBlock = 0x002E0578U; // HandleTextControlCode - 4
// Message_SetInstantText, which this module performs itself.
inline constexpr std::uint32_t kMessageSetInstantText = 0x0040B608U;
// Message_GetState: it asks the message object at kMessageObject (its literal
// at kMessageGetStateObjectLiteral), whose byte at +0xF38 is 0 while no text
// box is open.
inline constexpr std::uint32_t kMessageGetState = 0x003769D8U;
inline constexpr std::uint32_t kMessageGetStateObjectLiteral = 0x00376A34U;
inline constexpr std::uint32_t kMessageObject = 0x005C1878U;
inline constexpr std::uint32_t kMessageObjectState = 0xF38U;

struct QuickTextSite {
  const char *Name;
  std::uint32_t Address;
  const char *WindowSha256; // first 32 bytes, as addresses.json records them
};
inline constexpr std::array<QuickTextSite, 4> kQuickTextSites{{
    {"QuickTextCheckControlCodeBlock", kQuickTextCheckControlCodeBlock,
     "d9c58e795e665a99cc82483cdde1ab9cd0bbd0ad5960f69d1d01dd2d5fc6869d"},
    {"QuickTextHandleControlCodeBlock", kQuickTextHandleControlCodeBlock,
     "bf8c4f9033b201aef02db0166fcefe830c56a78ffa47e2cfd9fb12e6e50612a6"},
    {"Message_SetInstantText", kMessageSetInstantText,
     "dd153e2cda29d11c7ca73de4a69e945c948f98ec705113e77ce5044b5acc6409"},
    {"Message_GetState", kMessageGetState,
     "3d537247888f13a51c534443bb9b114051cfcc82ea3aeb6e09ac81de8e226009"},
}};

// Text control codes (OoT3D_Randomizer shared/s_message.h).
inline constexpr std::uint8_t kTextControlCode = 0x7FU;
inline constexpr std::uint8_t kTextWaitForInput = 0x01U;
inline constexpr std::uint8_t kTextInstantOn = 0x04U;
inline constexpr std::uint8_t kTextInstantOff = 0x05U;
inline constexpr std::uint8_t kTextUnskippable = 0x19U;
// Turbo: B is let through for this many guest refreshes, then held back as
// long (the game reads input every other refresh).
inline constexpr std::uint32_t kTurboHalfPeriod = 2U;

class QuickText final {
public:
  // verify=false is for host tests whose fake memory holds no code.
  explicit QuickText(bool verify = true) : mVerified(!verify) {}

  // Sorted.
  static std::span<const std::uint32_t> HookPcs() noexcept;

  // Any thread.
  void SetMode(QuickTextMode mode) noexcept { mMode = mode; }
  [[nodiscard]] QuickTextMode Mode() const noexcept { return mMode; }

  // The game thread, at one of HookPcs. Checks the code once; on a
  // different image quick text stays off and Disabled() says why.
  void ObserveBlockEntry(std::uint32_t pc, oot3d::recomp::a32::GuestState &state,
                         oot3d::recomp::a32::MemoryBus &memory);
  // The window, once per guest refresh before the input reaches the game:
  // turbo with B (bButton, the HID mask) held while a text box is open.
  void FilterInput(std::uint32_t &buttons, std::uint32_t bButton, oot3d::recomp::a32::MemoryBus &memory);
  [[nodiscard]] const std::string &Disabled() const noexcept { return mDisabled; }

  // Message_SetInstantText(textObject), done from the host: appends the
  // instant-text command to the text object's command list.
  static void SetInstantText(std::uint32_t textObject, oot3d::recomp::a32::MemoryBus &memory);
  static bool TextBoxOpen(oot3d::recomp::a32::MemoryBus &memory);

  // Diagnostics for the observer's report.
  struct Hits {
    std::uint64_t Instant = 0;      // boxes made instant
    std::uint64_t ControlCodes = 0; // unskippable or instant-off codes replaced
    std::uint64_t TurboPresses = 0; // B presses given while B was held
  };
  [[nodiscard]] const Hits &Counts() const noexcept { return mHits; }

private:
  bool Verify(oot3d::recomp::a32::MemoryBus &memory);

  std::atomic<QuickTextMode> mMode{QuickTextMode::Off};
  bool mVerified = false;
  std::string mDisabled;
  std::uint32_t mTurboPhase = 0;
  Hits mHits{};
};

} // namespace Oot3dArchipelago
