// AuthService 测试：建号/登录/会话校验/登出/改密吊销/登录退避/过期。
#include <gtest/gtest.h>

#include <mutex>
#include <string>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/migration.hpp"
#include "database/statement.hpp"
#include "service/auth_service.hpp"

namespace {

using namespace wt;

std::string migrations_dir() {
#ifdef WT_TEST_SOURCE_DIR
  return std::string(WT_TEST_SOURCE_DIR) + "/migrations";
#else
  return "migrations";
#endif
}

class AuthFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    database_.open(":memory:");
    MigrationRunner(database_).run(migrations_dir());
  }

  Database database_;
};

TEST_F(AuthFixture, SetupLoginValidateLogoutLifecycle) {
  AuthService auth(database_, true);
  EXPECT_FALSE(auth.initialized());
  EXPECT_FALSE(auth.validate("unknown-token"));

  const auto session = auth.setup("admin", "correct horse");
  EXPECT_NE(session.token, "");
  EXPECT_TRUE(auth.initialized());

  // 建号即登录：令牌立即可用。
  EXPECT_TRUE(auth.validate(session.token));
  EXPECT_FALSE(auth.validate("not-a-real-token"));

  // 登出后令牌失效；重复登出幂等。
  EXPECT_TRUE(auth.logout(session.token));
  EXPECT_FALSE(auth.validate(session.token));
  EXPECT_FALSE(auth.logout(session.token));
}

TEST_F(AuthFixture, SetupOnlyOnce) {
  AuthService auth(database_, true);
  auth.setup("admin", "correct horse");
  try {
    auth.setup("other", "another password");
    FAIL() << "second setup should be rejected";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.http_status(), 409);
  }
}

TEST_F(AuthFixture, LoginRejectsWrongPassword) {
  AuthService auth(database_, true);
  auth.setup("admin", "correct horse");
  try {
    auth.login("admin", "wrong password");
    FAIL() << "wrong password should be rejected";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.http_status(), 401);
  }
  // 正确密码仍可登录（前 2 次失败不触发退避）。
  const auto session = auth.login("admin", "correct horse");
  EXPECT_TRUE(auth.validate(session.token));
}

TEST_F(AuthFixture, LoginBackoffAfterRepeatedFailures) {
  AuthService auth(database_, true);
  auth.setup("admin", "correct horse");
  // 连续失败 3 次（前 2 次免罚）后进入退避：连正确密码也会被 429 拒绝。
  for (int i = 0; i < 3; ++i) {
    try {
      auth.login("admin", "wrong password");
    } catch (const ApiError&) {
    }
  }
  try {
    auth.login("admin", "correct horse");
    FAIL() << "backoff should reject even correct password";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.http_status(), 429);
  }
  // 退避只影响对应用户名：换一个已存在用户不受牵连的验证因单账号限制无法进行，
  // 这里退而验证退避不改变数据库状态（会话表仍只有建号会话）。
  std::scoped_lock lock(database_.mutex());
  Statement count(database_, "SELECT COUNT(*) FROM auth_session;");
  count.step();
  EXPECT_EQ(count.get_int64(0), 1);
}

TEST_F(AuthFixture, ChangePasswordRevokesAllSessions) {
  AuthService auth(database_, true);
  auth.setup("admin", "old password");
  const auto first = auth.login("admin", "old password");
  const auto second = auth.login("admin", "old password");
  EXPECT_TRUE(auth.validate(first.token));
  EXPECT_TRUE(auth.validate(second.token));

  // 旧密码错误时拒绝修改。
  try {
    auth.change_password(first.token, "wrong old", "new password 9");
    FAIL() << "wrong old password should be rejected";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.http_status(), 401);
  }

  auth.change_password(first.token, "old password", "new password 9");
  // 全部会话（含当前）被吊销。
  EXPECT_FALSE(auth.validate(first.token));
  EXPECT_FALSE(auth.validate(second.token));
  // 旧密码不能再登录，新密码可以。
  try {
    auth.login("admin", "old password");
    FAIL() << "old password must stop working";
  } catch (const ApiError&) {
  }
  EXPECT_NE(auth.login("admin", "new password 9").token, "");
}

TEST_F(AuthFixture, SessionExpiry) {
  AuthService auth(database_, true);
  const auto session = auth.setup("admin", "correct horse");
  ASSERT_TRUE(auth.validate(session.token));

  // 直接把会话过期时间改为过去：校验失败（模拟 90 天不活跃后过期）。
  {
    std::scoped_lock lock(database_.mutex());
    Statement update(database_, "UPDATE auth_session SET expires_at = 1;");
    update.run();
  }
  EXPECT_FALSE(auth.validate(session.token));
}

TEST_F(AuthFixture, WeakPasswordRejected) {
  AuthService auth(database_, true);
  try {
    auth.setup("admin", "short");
    FAIL() << "short password should be rejected";
  } catch (const ApiError& error) {
    EXPECT_EQ(error.http_status(), 400);
  }
}

}  // namespace
