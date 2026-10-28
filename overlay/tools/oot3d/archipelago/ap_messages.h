#pragma once

// Item text for placeholders: where a location gives another player's item
// (or one of this world's the game has no get-item for), the game shows a
// recovery heart; its text box says what was found instead.
//
// As OoT3D_Randomizer's Message_GetCustomEntry (message.c), which replaces
// the two Message_GetEntry calls of the text system: the block after each
// call reads the entry found (r0). The client points r0 at an entry of its
// own in the client page whose texts it writes there. Message_GetText adds
// an entry's offset to the loaded text file's base, so the client's offsets
// are its text's address minus that base. Nothing is patched.

#include "recomp/a32_runtime.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace Oot3dArchipelago {

// The blocks after the calls (GetCustomMessageEntryOne and Two + 4) and the
// register that holds the text object owner there (the object is it + 0x1C).
inline constexpr std::uint32_t kMessageEntryOneBlock = 0x002CD3A8U; // r5
inline constexpr std::uint32_t kMessageEntryTwoBlock = 0x0044D588U; // r4
inline constexpr std::uint32_t kMessageObjectOffset = 0x1CU;
// The text object's loaded file: the base Message_GetText adds offsets to.
inline constexpr std::uint32_t kMessageObjectTextBase = 0x04U;

// MessageEntry (OoT3D_Randomizer shared/s_message.h): id, three header
// words (box type and position), then ten languages of {offset, length}.
inline constexpr std::uint32_t kMessageEntrySize = 0x60U;
inline constexpr std::uint32_t kMessageEntryHeader = 0x10U;
inline constexpr std::uint32_t kMessageLanguages = 10U;

// "You got a Recovery Heart!": the get-item text of the placeholder.
inline constexpr std::uint32_t kTextRecoveryHeart = 0x0055U;

// Text control codes (OoT3D_Randomizer custom_messages.cpp).
inline constexpr std::uint8_t kTextColorWhite = 0x00U;
inline constexpr std::uint8_t kTextColorRed = 0x41U;
inline constexpr std::uint8_t kTextColorBlue = 0x43U;
// Widest line of a text box, in pixels (NextLineLength).
inline constexpr std::uint32_t kTextLineWidth = 287U;
inline constexpr std::size_t kTextMaximumLines = 4U;

// The width the text box gives a UTF-8 string (characters the font lacks
// count as '?').
std::uint32_t TextPixelWidth(std::string_view text) noexcept;
// Replaces what the font cannot show with '?' and cuts the result to
// maximumBytes at a character boundary.
std::string FontSafe(std::string_view text, std::size_t maximumBytes);

// "You found <item> for <player>!" (player empty: "You found <item>!"), with
// the item in red and the player in blue, broken into lines that fit a text
// box, ending with the message end code.
std::string FormatItemFoundText(std::string_view item, std::string_view player);

// Writes an entry at entryAddress that keeps the header of the entry at
// originalEntry and gives every language the text, written at textAddress
// (padded to four bytes, at most capacity). textBase is the loaded file's
// base of the text object. False if guest memory cannot be read or written.
bool WriteMessageEntry(oot3d::recomp::a32::MemoryBus &memory, std::uint32_t originalEntry, std::uint32_t textBase,
                       const std::string &text, std::uint32_t entryAddress, std::uint32_t textAddress,
                       std::uint32_t capacity);

} // namespace Oot3dArchipelago
