#include "ap_sha256.h"

#include <cstdio>
#include <vector>

namespace Oot3dArchipelago {
namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

constexpr std::uint32_t Rotr(std::uint32_t value, unsigned count) {
  return (value >> count) | (value << (32U - count));
}

} // namespace

std::array<std::uint8_t, 32> Sha256(std::span<const std::uint8_t> data) {
  std::array<std::uint32_t, 8> h = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
  std::vector<std::uint8_t> message(data.begin(), data.end());
  const std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8U;
  message.push_back(0x80U);
  while (message.size() % 64U != 56U)
    message.push_back(0U);
  for (int shift = 56; shift >= 0; shift -= 8)
    message.push_back(static_cast<std::uint8_t>(bits >> shift));

  for (std::size_t chunk = 0; chunk < message.size(); chunk += 64U) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16U; ++i)
      w[i] = (static_cast<std::uint32_t>(message[chunk + i * 4U]) << 24U) |
             (static_cast<std::uint32_t>(message[chunk + i * 4U + 1U]) << 16U) |
             (static_cast<std::uint32_t>(message[chunk + i * 4U + 2U]) << 8U) |
             static_cast<std::uint32_t>(message[chunk + i * 4U + 3U]);
    for (std::size_t i = 16U; i < 64U; ++i) {
      const std::uint32_t s0 = Rotr(w[i - 15U], 7U) ^ Rotr(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
      const std::uint32_t s1 = Rotr(w[i - 2U], 17U) ^ Rotr(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
      w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6],
                  k = h[7];
    for (std::size_t i = 0; i < 64U; ++i) {
      const std::uint32_t s1 = Rotr(e, 6U) ^ Rotr(e, 11U) ^ Rotr(e, 25U);
      const std::uint32_t ch = (e & f) ^ (~e & g);
      const std::uint32_t t1 = k + s1 + ch + kRound[i] + w[i];
      const std::uint32_t s0 = Rotr(a, 2U) ^ Rotr(a, 13U) ^ Rotr(a, 22U);
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t t2 = s0 + maj;
      k = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += k;
  }

  std::array<std::uint8_t, 32> digest{};
  for (std::size_t i = 0; i < 8U; ++i)
    for (std::size_t j = 0; j < 4U; ++j)
      digest[i * 4U + j] = static_cast<std::uint8_t>(h[i] >> (24U - j * 8U));
  return digest;
}

std::string Sha256Hex(std::span<const std::uint8_t> data) {
  const auto digest = Sha256(data);
  std::string text;
  text.reserve(64);
  char pair[3];
  for (const std::uint8_t byte : digest) {
    std::snprintf(pair, sizeof(pair), "%02x", byte);
    text += pair;
  }
  return text;
}

} // namespace Oot3dArchipelago
