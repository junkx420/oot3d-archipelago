#include "ap_messages.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

// Pixel widths of the text box font, from OoT3D_Randomizer's pixelWidthTable
// (custom_messages.cpp): printable ASCII from ' ', then Latin-1 and a few
// others. 0: the font has no such character.
constexpr std::array<std::uint8_t, 95> kAsciiWidths{
    4, 3, 5, 9, 9, 11, 9, 4, 5, 5, 6, 8, 4, 6, 3, 6,      // ' ' to '/'
    8, 4, 8, 8, 8, 8, 8, 8, 8, 8, 3, 3, 8, 9, 8, 7,       // '0' to '?'
    11, 11, 9, 10, 9, 8, 8, 10, 9, 3, 8, 9, 7, 11, 9, 11, // '@' to 'O'
    9, 11, 9, 9, 9, 9, 10, 12, 9, 10, 9, 5, 6, 5, 7, 8,   // 'P' to '_'
    4, 8, 8, 8, 8, 8, 5, 8, 7, 3, 4, 7, 3, 10, 7, 9,      // '`' to 'o'
    8, 8, 5, 7, 5, 7, 8, 9, 8, 8, 8, 5, 3, 5, 0};         // 'p' to '~'
// U+00A0 to U+00FF.
constexpr std::array<std::uint8_t, 96> kLatin1Widths{
    0, 5, 7, 8, 7, 8, 3, 12, 12, 10, 5, 8, 7, 6, 10, 5,          // A0 to AF
    12, 12, 5, 5, 0, 6, 8, 4, 0, 4, 5, 9, 9, 9, 10, 7,          // B0 to BF
    11, 11, 11, 11, 11, 11, 12, 10, 8, 8, 8, 8, 5, 5, 5, 5,     // C0 to CF
    10, 9, 11, 11, 11, 11, 11, 9, 11, 9, 9, 9, 9, 10, 8, 8,     // D0 to DF
    8, 8, 8, 8, 8, 8, 11, 8, 8, 8, 8, 8, 5, 5, 5, 5,            // E0 to EF
    9, 7, 9, 9, 9, 9, 9, 11, 9, 7, 7, 7, 7, 8, 8, 8};           // F0 to FF

std::uint32_t Width(char32_t c) noexcept {
  if (c >= 0x20U && c <= 0x7EU)
    return kAsciiWidths[c - 0x20U];
  if (c >= 0xA0U && c <= 0xFFU)
    return kLatin1Widths[c - 0xA0U];
  switch (c) {
  case 0x152U: // Œ
  case 0x153U: // œ
    return 11U;
  case 0x178U: // Ÿ
  case 0x20ACU: // €
    return 10U;
  case 0x201DU: // ”
  case 0x201EU: // „
    return 5U;
  default:
    return 0U;
  }
}

// One UTF-8 character from text at position, or nullopt (a broken
// sequence, counted as one byte).
std::optional<char32_t> Decode(std::string_view text, std::size_t &position) {
  const auto lead = static_cast<unsigned char>(text[position]);
  std::size_t length = lead < 0x80U ? 1U : (lead >> 5U) == 0x6U ? 2U : (lead >> 4U) == 0xEU ? 3U : (lead >> 3U) == 0x1EU ? 4U : 0U;
  if (length == 0U || position + length > text.size()) {
    ++position;
    return std::nullopt;
  }
  char32_t value = length == 1U ? lead : lead & (0x7FU >> length);
  for (std::size_t i = 1; i < length; ++i) {
    const auto next = static_cast<unsigned char>(text[position + i]);
    if ((next & 0xC0U) != 0x80U) {
      ++position;
      return std::nullopt;
    }
    value = (value << 6U) | (next & 0x3FU);
  }
  position += length;
  return value;
}

struct Word {
  std::string Text;
  std::uint8_t Color = kTextColorWhite;
  bool Glued = false; // no space before it
};

void AddWords(std::vector<Word> &words, std::string_view text, std::uint8_t color) {
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = std::min(text.find(' ', start), text.size());
    if (end > start)
      words.push_back({std::string(text.substr(start, end - start)), color, false});
    start = end + 1U;
  }
}

std::string Color(std::uint8_t color) { return std::string("\x7F\x1D", 2) + static_cast<char>(color); }

} // namespace

std::uint32_t TextPixelWidth(std::string_view text) noexcept {
  std::uint32_t width = 0;
  for (std::size_t position = 0; position < text.size();) {
    const auto c = Decode(text, position);
    const std::uint32_t w = c.has_value() ? Width(*c) : 0U;
    width += w != 0U ? w : Width(U'?');
  }
  return width;
}

std::string FontSafe(std::string_view text, std::size_t maximumBytes) {
  std::string safe;
  for (std::size_t position = 0; position < text.size();) {
    const std::size_t start = position;
    const auto c = Decode(text, position);
    const bool shown = c.has_value() && Width(*c) != 0U;
    const std::size_t length = shown ? position - start : 1U;
    if (safe.size() + length > maximumBytes)
      break;
    if (shown)
      safe.append(text.substr(start, length));
    else
      safe.push_back('?');
  }
  return safe;
}

std::string FormatItemFoundText(std::string_view item, std::string_view player) {
  std::vector<Word> words;
  AddWords(words, "You found", kTextColorWhite);
  AddWords(words, FontSafe(item.empty() ? std::string_view("an item") : item, 64U), kTextColorRed);
  if (!player.empty()) {
    AddWords(words, "for", kTextColorWhite);
    AddWords(words, FontSafe(player, 32U), kTextColorBlue);
  }
  words.push_back({"!", kTextColorWhite, true});

  // Lines that fit the box; a word wider than a line is cut by characters.
  std::vector<std::vector<Word>> lines(1);
  std::uint32_t lineWidth = 0;
  for (auto &word : words) {
    const std::uint32_t space = word.Glued || lines.back().empty() ? 0U : Width(U' ');
    std::uint32_t width = TextPixelWidth(word.Text);
    if (lineWidth + space + width > kTextLineWidth && !lines.back().empty() && !word.Glued) {
      lines.emplace_back();
      lineWidth = 0;
    } else {
      lineWidth += space;
    }
    while (lineWidth + width > kTextLineWidth) {
      std::string head;
      std::uint32_t headWidth = 0;
      std::size_t position = 0;
      while (position < word.Text.size()) {
        std::size_t next = position;
        const auto c = Decode(word.Text, next);
        const std::uint32_t w = c.has_value() && Width(*c) != 0U ? Width(*c) : Width(U'?');
        if (lineWidth + headWidth + w > kTextLineWidth)
          break;
        headWidth += w;
        position = next;
      }
      if (position == 0U)
        break; // not even one character fits
      lines.back().push_back({word.Text.substr(0, position), word.Color, word.Glued});
      word.Text.erase(0, position);
      word.Glued = false;
      lines.emplace_back();
      lineWidth = 0;
      width = TextPixelWidth(word.Text);
    }
    lines.back().push_back(word);
    lineWidth += width;
  }
  if (lines.size() > kTextMaximumLines)
    lines.resize(kTextMaximumLines);

  std::string text;
  std::uint8_t color = kTextColorWhite;
  for (std::size_t line = 0; line < lines.size(); ++line) {
    if (line > 0U)
      text += std::string("\x7F\x1C", 2); // new line
    for (std::size_t index = 0; index < lines[line].size(); ++index) {
      const Word &word = lines[line][index];
      if (index > 0U && !word.Glued)
        text += ' ';
      if (word.Color != color) {
        text += Color(word.Color);
        color = word.Color;
      }
      text += word.Text;
    }
  }
  if (color != kTextColorWhite)
    text += Color(kTextColorWhite);
  text += std::string("\x7F\x00", 2); // message end
  return text;
}

bool WriteMessageEntry(MemoryBus &memory, std::uint32_t originalEntry, std::uint32_t textBase, const std::string &text,
                       std::uint32_t entryAddress, std::uint32_t textAddress, std::uint32_t capacity) {
  const auto padded = static_cast<std::uint32_t>((text.size() + 3U) & ~std::size_t{3U});
  if (padded > capacity)
    return false;
  for (std::uint32_t offset = 0; offset < kMessageEntryHeader; offset += 4U) {
    std::uint32_t word = 0;
    if (!memory.Read32(originalEntry + offset, &word) || !memory.Write32(entryAddress + offset, word))
      return false;
  }
  for (std::uint32_t language = 0; language < kMessageLanguages; ++language) {
    const std::uint32_t info = entryAddress + kMessageEntryHeader + language * 8U;
    if (!memory.Write32(info, textAddress - textBase) || !memory.Write32(info + 4U, padded))
      return false;
  }
  for (std::uint32_t i = 0; i < padded; ++i)
    if (!memory.Write8(textAddress + i, i < text.size() ? static_cast<std::uint8_t>(text[i]) : 0U))
      return false;
  return true;
}

} // namespace Oot3dArchipelago
