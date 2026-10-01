// 密码学工具：SHA-256、HMAC-SHA256、PBKDF2、随机字节与常量时间比较。
// 自包含实现，不引入 OpenSSL（vendored 依赖结构下 arm64 交叉编译成本过高）；
// 全部为标准构造，backend/test/crypto_test.cpp 以 RFC 官方测试向量验证正确性。
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace wt::crypto {

// SHA-256 摘要固定 32 字节。
using Sha256Digest = std::array<std::uint8_t, 32>;

// 计算 SHA-256 摘要。
Sha256Digest sha256(std::string_view data);
Sha256Digest sha256(const std::uint8_t* data, std::size_t size);

// HMAC-SHA256（RFC 2104），密钥与数据均为原始字节。
Sha256Digest hmac_sha256(std::string_view key, std::string_view data);

// PBKDF2-HMAC-SHA256（RFC 8018）：由口令与盐派生 dk_len 字节。
// iterations 由调用方给定（口令哈希建议 200000）。
std::string pbkdf2_hmac_sha256(std::string_view password, std::string_view salt,
                               std::uint32_t iterations, std::size_t dk_len);

// 从系统 CSPRNG（/dev/urandom）填充 count 个字节；失败抛 std::runtime_error。
void random_bytes(std::uint8_t* out, std::size_t count);

// 生成 bytes 个随机字节的十六进制字符串（如 32 字节 -> 64 字符），可直接用作令牌。
std::string random_hex_token(std::size_t bytes);

// 常量时间比较：内容逐字节比较，不因前缀匹配程度提前返回。
// 长度不同直接返回 false（长度本身不是需要保护的秘密）。
bool constant_time_equal(std::string_view a, std::string_view b);

// 字节序列转小写十六进制字符串。
std::string to_hex(const std::uint8_t* data, std::size_t size);
std::string to_hex(const Sha256Digest& digest);

}  // namespace wt::crypto
