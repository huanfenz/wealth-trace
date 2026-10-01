// 密码学工具实现：SHA-256（FIPS 180-4）、HMAC-SHA256（RFC 2104）、
// PBKDF2-HMAC-SHA256（RFC 8018）、/dev/urandom 随机与常量时间比较。
#include "utils/crypto.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace wt::crypto {
namespace {

// SHA-256 轮常量：前 64 个素数立方根的小数部分（FIPS 180-4 第 4.2.2 节）。
constexpr std::uint32_t kRoundConstants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr std::uint32_t rotr(std::uint32_t value, int bits) {
  return (value >> bits) | (value << (32 - bits));
}

// 压缩函数：处理一个 64 字节块并更新链接状态 h[8]。
void compress_block(std::uint32_t h[8], const std::uint8_t block[64]) {
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(block[4 * i]) << 24) |
           (static_cast<std::uint32_t>(block[4 * i + 1]) << 16) |
           (static_cast<std::uint32_t>(block[4 * i + 2]) << 8) |
           static_cast<std::uint32_t>(block[4 * i + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
  std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
  for (int i = 0; i < 64; ++i) {
    const std::uint32_t sum1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const std::uint32_t choose = (e & f) ^ (~e & g);
    const std::uint32_t t1 = hh + sum1 + choose + kRoundConstants[i] + w[i];
    const std::uint32_t sum0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t t2 = sum0 + majority;
    hh = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d;
  h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

}  // namespace

Sha256Digest sha256(const std::uint8_t* data, std::size_t size) {
  // 初始链接状态：前 8 个素数平方根的小数部分（FIPS 180-4 第 5.3.3 节）。
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

  // 主循环：整块压缩。
  const std::size_t full_blocks = size / 64;
  for (std::size_t i = 0; i < full_blocks; ++i) {
    compress_block(h, data + i * 64);
  }

  // 填充：0x80、若干 0、8 字节大端位长度，最后一块不足 64 时补两个块。
  std::uint8_t tail[128] = {};
  const std::size_t remainder = size - full_blocks * 64;
  std::copy(data + full_blocks * 64, data + size, tail);
  tail[remainder] = 0x80;
  const std::size_t padded = remainder + 1 <= 56 ? 64 : 128;
  const std::uint64_t bits = static_cast<std::uint64_t>(size) * 8;
  for (int i = 0; i < 8; ++i) {
    tail[padded - 1 - i] = static_cast<std::uint8_t>(bits >> (8 * i));
  }
  compress_block(h, tail);
  if (padded == 128) {
    compress_block(h, tail + 64);
  }

  Sha256Digest digest{};
  for (int i = 0; i < 8; ++i) {
    digest[4 * i] = static_cast<std::uint8_t>(h[i] >> 24);
    digest[4 * i + 1] = static_cast<std::uint8_t>(h[i] >> 16);
    digest[4 * i + 2] = static_cast<std::uint8_t>(h[i] >> 8);
    digest[4 * i + 3] = static_cast<std::uint8_t>(h[i]);
  }
  return digest;
}

Sha256Digest sha256(std::string_view data) {
  return sha256(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

Sha256Digest hmac_sha256(std::string_view key, std::string_view data) {
  constexpr std::size_t kBlockSize = 64;
  std::uint8_t key_block[kBlockSize] = {};
  if (key.size() > kBlockSize) {
    // 超长密钥先哈希成 32 字节再展开。
    const auto digest = sha256(key);
    std::copy(digest.begin(), digest.end(), key_block);
  } else {
    std::copy(key.begin(), key.end(), reinterpret_cast<char*>(key_block));
  }

  std::uint8_t inner[kBlockSize];
  std::uint8_t outer[kBlockSize];
  for (std::size_t i = 0; i < kBlockSize; ++i) {
    inner[i] = key_block[i] ^ 0x36;
    outer[i] = key_block[i] ^ 0x5c;
  }

  // HMAC = H(K^opad || H(K^ipad || message))。
  std::vector<std::uint8_t> inner_input(inner, inner + kBlockSize);
  inner_input.insert(inner_input.end(), reinterpret_cast<const std::uint8_t*>(data.data()),
                     reinterpret_cast<const std::uint8_t*>(data.data()) + data.size());
  const auto inner_digest = sha256(inner_input.data(), inner_input.size());

  std::vector<std::uint8_t> outer_input(outer, outer + kBlockSize);
  outer_input.insert(outer_input.end(), inner_digest.begin(), inner_digest.end());
  return sha256(outer_input.data(), outer_input.size());
}

std::string pbkdf2_hmac_sha256(std::string_view password, std::string_view salt,
                               std::uint32_t iterations, std::size_t dk_len) {
  if (iterations == 0 || dk_len == 0) {
    throw std::invalid_argument("pbkdf2 requires positive iterations and output length");
  }
  std::string output;
  output.reserve(dk_len);
  // 逐块派生：每 32 字节一个块，块序号 i（从 1 起）以 4 字节大端拼进首轮输入。
  for (std::uint32_t block = 1; output.size() < dk_len; ++block) {
    std::string input(salt);
    input.push_back(static_cast<char>(block >> 24));
    input.push_back(static_cast<char>(block >> 16));
    input.push_back(static_cast<char>(block >> 8));
    input.push_back(static_cast<char>(block));

    auto u = hmac_sha256(password, input);
    std::string t(reinterpret_cast<const char*>(u.data()), u.size());
    for (std::uint32_t i = 1; i < iterations; ++i) {
      u = hmac_sha256(password,
                      std::string_view(reinterpret_cast<const char*>(u.data()), u.size()));
      for (std::size_t j = 0; j < u.size(); ++j) {
        t[j] = static_cast<char>(static_cast<std::uint8_t>(t[j]) ^ u[j]);
      }
    }
    output.append(t, 0, std::min(t.size(), dk_len - output.size()));
  }
  return output;
}

void random_bytes(std::uint8_t* out, std::size_t count) {
  if (count == 0) {
    return;
  }
  // /dev/urandom 在 Linux 上读取不阻塞，是用户态 CSPRNG 的标准来源。
  std::ifstream source("/dev/urandom", std::ios::binary);
  if (!source) {
    throw std::runtime_error("failed to open /dev/urandom");
  }
  source.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(count));
  if (!source || static_cast<std::size_t>(source.gcount()) != count) {
    throw std::runtime_error("failed to read random bytes");
  }
}

std::string random_hex_token(std::size_t bytes) {
  std::vector<std::uint8_t> buffer(bytes);
  random_bytes(buffer.data(), buffer.size());
  return to_hex(buffer.data(), buffer.size());
}

bool constant_time_equal(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  std::uint8_t difference = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    difference |= static_cast<std::uint8_t>(a[i]) ^ static_cast<std::uint8_t>(b[i]);
  }
  return difference == 0;
}

std::string to_hex(const std::uint8_t* data, std::size_t size) {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(size * 2);
  for (std::size_t i = 0; i < size; ++i) {
    hex.push_back(kDigits[data[i] >> 4]);
    hex.push_back(kDigits[data[i] & 0x0f]);
  }
  return hex;
}

std::string to_hex(const Sha256Digest& digest) {
  return to_hex(digest.data(), digest.size());
}

}  // namespace wt::crypto
