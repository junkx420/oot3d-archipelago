#pragma once

// Minimal SHA-256 (FIPS 180-4) for the address self-check. It hashes short
// code windows only; it is not a general-purpose crypto API.

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace Oot3dArchipelago {

std::array<std::uint8_t, 32> Sha256(std::span<const std::uint8_t> data);
std::string Sha256Hex(std::span<const std::uint8_t> data);

} // namespace Oot3dArchipelago
