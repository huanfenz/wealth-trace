// crypto 工具测试：全部使用可复现的权威测试向量。
// - SHA-256 / HMAC-SHA256:RFC 6234 测试向量 + Python hashlib 参考值（边界长度）
// - PBKDF2-HMAC-SHA256:Python hashlib.pbkdf2_hmac 参考值（与公开向量集一致）
#include <gtest/gtest.h>

#include <string>

#include "utils/crypto.hpp"

namespace {

using namespace wt::crypto;

std::string Sha(std::string_view data) { return to_hex(sha256(data)); }

std::string Hex(const std::string& bytes) {
  return to_hex(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
}

TEST(CryptoTest, Sha256Rfc6234Vectors) {
  // RFC 6234 Appendix B 的官方向量。
  EXPECT_EQ(Sha("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(Sha(""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(Sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  // 100 万个 'a'：覆盖多块处理路径。
  EXPECT_EQ(Sha(std::string(1000000, 'a')),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(CryptoTest, Sha256PaddingBoundaries) {
  // 55/56/63/64 字节正好处在填充分支的边界（一块 vs 两块），
  // 参考值由 Python hashlib.sha256 生成。
  EXPECT_EQ(Sha(std::string(55, 'b')),
            "eb2c86e932179f4ba13fe8715a26124b77d6bad290b9b4c1cc140cf633300c19");
  EXPECT_EQ(Sha(std::string(56, 'b')),
            "a5fc6e203a4c2b657d0d153885932414b2ffc6a93f0f8bf8b3183315e5a7212c");
  EXPECT_EQ(Sha(std::string(63, 'b')),
            "94e419fabac7f930810f9636354042f8c1426d2f834d4ab65c93dc1e69326b13");
  EXPECT_EQ(Sha(std::string(64, 'b')),
            "a0fab1377f49a759b57f63318262ebe89fabfc990e8e93ceac2984561482b9d4");
}

TEST(CryptoTest, HmacSha256Rfc6234Vectors) {
  auto Hmac = [](std::string_view key, std::string_view data) {
    return to_hex(hmac_sha256(key, data));
  };
  // RFC 6234 Appendix B, HMAC-SHA-256 用例 #1/#2/#6。
  EXPECT_EQ(Hmac(std::string(20, '\x0b'), "Hi There"),
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
  EXPECT_EQ(Hmac("Jefe", "what do ya want for nothing?"),
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
  EXPECT_EQ(Hmac(std::string(131, '\xaa'),
                 "Test Using Larger Than Block-Size Key - Hash Key First"),
            "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
  // 超长密钥 + 超长消息（155 字节 > 64）：同时覆盖密钥先哈希与多块消息。
  // 期望值由 Python hmac.new(key, data, hashlib.sha256) 生成。
  EXPECT_EQ(
      Hmac(std::string(131, '\xaa'),
           "This is a test using a larger than block-size key and a larger "
           "than block-size message. The key needs to be hashed before being "
           "used by the HMAC algorithm."),
      "71aba8ffcbd7fc3e3e1cce3b169c2279fd486c6d602c312f2338e40f8555ad0c");
}

TEST(CryptoTest, Pbkdf2HmacSha256Vectors) {
  // 参考值由 Python hashlib.pbkdf2_hmac('sha256', ...) 生成，与公开向量集一致。
  EXPECT_EQ(Hex(pbkdf2_hmac_sha256("password", "salt", 1, 32)),
            "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
  EXPECT_EQ(Hex(pbkdf2_hmac_sha256("password", "salt", 2, 32)),
            "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
  EXPECT_EQ(Hex(pbkdf2_hmac_sha256("password", "salt", 4096, 32)),
            "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
  // dkLen=40 > 单块 32 字节，覆盖多块派生（块序号拼接）路径。
  EXPECT_EQ(Hex(pbkdf2_hmac_sha256("passwordPASSWORDpassword",
                                   "saltSALTsaltSALTsaltSALTsalt", 4096, 40)),
            "77575734398a5ad8cc515b7082b8905a0a626d2b2b7386d901de137e3ec407af"
            "de50d983cc177da7");
  EXPECT_THROW(pbkdf2_hmac_sha256("p", "s", 0, 32), std::invalid_argument);
}

TEST(CryptoTest, RandomAndHelpers) {
  const auto first = random_hex_token(32);
  const auto second = random_hex_token(32);
  EXPECT_EQ(first.size(), 64u);
  EXPECT_NE(first, second);

  EXPECT_TRUE(constant_time_equal("same", "same"));
  EXPECT_FALSE(constant_time_equal("same", "diff"));
  EXPECT_FALSE(constant_time_equal("short", "longer-string"));
  EXPECT_FALSE(constant_time_equal("", "x"));
}

}  // namespace
