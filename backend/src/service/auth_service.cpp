// 鉴权服务实现：账号 PBKDF2 哈希、会话签发/校验/滑动续期、登录退避。
#include "service/auth_service.hpp"

#include <array>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <utility>
#include <vector>

#include "common/error.hpp"
#include "common/logging.hpp"
#include "database/database.hpp"
#include "database/statement.hpp"
#include "database/transaction.hpp"
#include "utils/crypto.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

// PBKDF2 参数：200k 次迭代在 arm64 上约百毫秒量级，对登录场景可接受。
constexpr std::uint32_t kPbkdf2Iterations = 200000;
constexpr std::size_t kPbkdf2OutputBytes = 32;
constexpr std::size_t kSaltBytes = 16;

// 会话有效期：90 天滑动（每次校验通过后按需续期，活跃使用即无需重登）。
constexpr std::int64_t kSessionLifetimeSeconds = 90LL * 24 * 3600;
// 续期写库的最小间隔：活跃会话最多每小时写一次 last_used/expires，避免每请求一写。
constexpr std::int64_t kRefreshIntervalSeconds = 3600;

// 登录退避：前 2 次失败不罚（容忍手滑），第 3 次起 2/4/8/… 秒，封顶 30 秒。
constexpr std::uint32_t kFreeFailures = 2;
constexpr std::int64_t kBackoffMaxSeconds = 30;

std::int64_t now_epoch() { return static_cast<std::int64_t>(std::time(nullptr)); }

// 口令哈希自描述格式：pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>。
// 参数保留在哈希串内，未来提高迭代数不影响旧记录校验。
std::string hash_password(const std::string& password) {
  std::array<std::uint8_t, kSaltBytes> salt{};
  crypto::random_bytes(salt.data(), salt.size());
  const std::string salt_hex = crypto::to_hex(salt.data(), salt.size());
  const auto derived =
      crypto::pbkdf2_hmac_sha256(password, salt_hex, kPbkdf2Iterations, kPbkdf2OutputBytes);
  const std::string hash_hex =
      crypto::to_hex(reinterpret_cast<const std::uint8_t*>(derived.data()), derived.size());
  return "pbkdf2_sha256$" + std::to_string(kPbkdf2Iterations) + "$" + salt_hex + "$" +
         hash_hex;
}

bool verify_password(const std::string& password, const std::string& stored) {
  // 按 '$' 拆成 4 段；格式不符一律校验失败（不抛错，避免泄露存储细节）。
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (true) {
    const std::size_t index = stored.find('$', start);
    if (index == std::string::npos) {
      parts.push_back(stored.substr(start));
      break;
    }
    parts.push_back(stored.substr(start, index - start));
    start = index + 1;
  }
  if (parts.size() != 4 || parts[0] != "pbkdf2_sha256") {
    return false;
  }
  char* end = nullptr;
  const unsigned long iterations = std::strtoul(parts[1].c_str(), &end, 10);
  if (end == parts[1].c_str() || iterations == 0 || iterations > 10000000) {
    return false;
  }
  const auto derived = crypto::pbkdf2_hmac_sha256(
      password, parts[2], static_cast<std::uint32_t>(iterations), kPbkdf2OutputBytes);
  const std::string hash_hex =
      crypto::to_hex(reinterpret_cast<const std::uint8_t*>(derived.data()), derived.size());
  return crypto::constant_time_equal(hash_hex, parts[3]);
}

void validate_credentials(const std::string& username, const std::string& password) {
  if (username.empty() || username.size() > 64) {
    throw invalid_request("username must be 1-64 characters");
  }
  if (password.size() < 8) {
    throw invalid_request("password must be at least 8 characters");
  }
  if (password.size() > 128) {
    throw invalid_request("password must be at most 128 characters");
  }
}

std::string token_hash_hex(const std::string& token) {
  return crypto::to_hex(crypto::sha256(token));
}

}  // namespace

AuthService::AuthService(Database& database, bool enabled)
    : database_(database), enabled_(enabled) {}

bool AuthService::initialized() {
  std::scoped_lock lock(database_.mutex());
  Statement statement(database_, "SELECT EXISTS(SELECT 1 FROM auth_user);");
  statement.step();
  return statement.get_int64(0) == 1;
}

AuthSessionInfo AuthService::setup(const std::string& username,
                                   const std::string& password) {
  validate_credentials(username, password);
  std::scoped_lock lock(database_.mutex());
  Statement exists(database_, "SELECT EXISTS(SELECT 1 FROM auth_user);");
  exists.step();
  if (exists.get_int64(0) == 1) {
    throw conflict("管理员账号已存在，请直接登录");
  }
  const auto now = time_util::now_iso8601();
  TransactionGuard transaction(database_);
  Statement insert(database_,
                   "INSERT INTO auth_user(username, password_hash, created_at, updated_at) "
                   "VALUES(?, ?, ?, ?);");
  insert.bind(1, username)
      .bind(2, hash_password(password))
      .bind(3, now)
      .bind(4, now)
      .run();
  const auto user_id = database_.last_insert_rowid();
  auto session = issue_session(user_id, username);
  transaction.commit();
  log_info("admin account created: " + username);
  return session;
}

AuthSessionInfo AuthService::login(const std::string& username,
                                   const std::string& password) {
  if (username.empty() || username.size() > 64 || password.empty() ||
      password.size() > 128) {
    throw invalid_request("用户名或密码格式不正确");
  }

  // 退避检查不持有数据库锁：处于惩罚期时直接拒绝并提示剩余秒数。
  {
    std::scoped_lock backoff_lock(backoff_mutex_);
    const auto entry = backoff_.find(username);
    if (entry != backoff_.end()) {
      const std::int64_t remaining = entry->second.second - now_epoch();
      if (remaining > 0) {
        throw ApiError(error_code::kTooManyRequests, 429,
                       "尝试过于频繁，请 " + std::to_string(remaining) + " 秒后再试");
      }
    }
  }

  std::scoped_lock lock(database_.mutex());
  std::string stored;
  const auto user_id = find_user(username, &stored);
  if (user_id == 0 || !verify_password(password, stored)) {
    // 无论用户不存在还是密码错误，统一提示并施加退避（不区分文案，避免枚举用户名）。
    std::scoped_lock backoff_lock(backoff_mutex_);
    auto& entry = backoff_[username];
    entry.first += 1;
    if (entry.first > kFreeFailures) {
      const auto shift = std::min(entry.first - kFreeFailures, 6U);
      const auto penalty = std::min(static_cast<std::int64_t>(1) << shift, kBackoffMaxSeconds);
      entry.second = now_epoch() + penalty;
    }
    log_warn("login failed for user '" + username + "' (attempt " +
             std::to_string(entry.first) + ")");
    throw unauthorized("用户名或密码错误");
  }
  {
    std::scoped_lock backoff_lock(backoff_mutex_);
    backoff_.erase(username);
  }
  prune_expired_sessions();
  return issue_session(user_id, username);
}

bool AuthService::validate(const std::string& token) {
  if (token.empty() || token.size() > 128) {
    return false;
  }
  try {
    std::scoped_lock lock(database_.mutex());
    const auto now = now_epoch();
    Statement statement(database_,
                        "SELECT expires_at, last_used_at FROM auth_session "
                        "WHERE token_hash = ?;");
    statement.bind(1, token_hash_hex(token));
    if (!statement.step() || statement.get_int64(0) <= now) {
      return false;
    }
    // 滑动续期：距上次刷新超过 1 小时才写库，避免每个请求一次 UPDATE。
    if (now - statement.get_int64(1) >= kRefreshIntervalSeconds) {
      Statement refresh(database_,
                        "UPDATE auth_session SET expires_at = ?, last_used_at = ? "
                        "WHERE token_hash = ?;");
      refresh.bind(1, now + kSessionLifetimeSeconds)
          .bind(2, now)
          .bind(3, token_hash_hex(token))
          .run();
    }
    return true;
  } catch (const std::exception& error) {
    // 中间件路径不抛异常：任何数据库错误都按未授权处理并记录。
    log_error(std::string("session validate failed: ") + error.what());
    return false;
  }
}

bool AuthService::logout(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  std::scoped_lock lock(database_.mutex());
  Statement statement(database_, "DELETE FROM auth_session WHERE token_hash = ?;");
  statement.bind(1, token_hash_hex(token)).run();
  return database_.changes() > 0;
}

void AuthService::change_password(const std::string& token,
                                  const std::string& old_password,
                                  const std::string& new_password) {
  if (new_password.size() < 8 || new_password.size() > 128) {
    throw invalid_request("新密码长度需在 8-128 位之间");
  }
  std::scoped_lock lock(database_.mutex());
  std::int64_t user_id = 0;
  std::string stored;
  {
    Statement statement(database_,
                        "SELECT u.id, u.password_hash FROM auth_session s "
                        "JOIN auth_user u ON u.id = s.user_id WHERE s.token_hash = ?;");
    statement.bind(1, token_hash_hex(token));
    if (!statement.step()) {
      throw unauthorized("登录状态已失效，请重新登录");
    }
    user_id = statement.get_int64(0);
    stored = statement.get_text(1);
  }
  if (!verify_password(old_password, stored)) {
    throw unauthorized("旧密码不正确");
  }
  TransactionGuard transaction(database_);
  Statement update(database_,
                   "UPDATE auth_user SET password_hash = ?, updated_at = ? WHERE id = ?;");
  update.bind(1, hash_password(new_password))
      .bind(2, time_util::now_iso8601())
      .bind(3, user_id)
      .run();
  // 改密后吊销全部会话（含当前），要求所有设备重新登录。
  Statement revoke(database_, "DELETE FROM auth_session WHERE user_id = ?;");
  revoke.bind(1, user_id).run();
  transaction.commit();
  log_info("password changed for user #" + std::to_string(user_id));
}

AuthSessionInfo AuthService::issue_session(std::int64_t user_id,
                                           const std::string& username) {
  // 32 字节随机令牌以 64 字符 hex 下发；库中仅存其 SHA-256 哈希。
  const std::string token = crypto::random_hex_token(32);
  const std::int64_t now = now_epoch();
  Statement insert(database_,
                   "INSERT INTO auth_session(token_hash, user_id, created_at, expires_at, "
                   "last_used_at) VALUES(?, ?, ?, ?, ?);");
  insert.bind(1, token_hash_hex(token))
      .bind(2, user_id)
      .bind(3, time_util::now_iso8601())
      .bind(4, now + kSessionLifetimeSeconds)
      .bind(5, now)
      .run();
  AuthSessionInfo info;
  info.token = token;
  info.user_id = user_id;
  info.username = username;
  info.expires_at = now + kSessionLifetimeSeconds;
  return info;
}

std::int64_t AuthService::find_user(const std::string& username,
                                    std::string* password_hash) {
  Statement statement(database_, "SELECT id, password_hash FROM auth_user WHERE username = ?;");
  statement.bind(1, username);
  if (!statement.step()) {
    return 0;
  }
  if (password_hash != nullptr) {
    *password_hash = statement.get_text(1);
  }
  return statement.get_int64(0);
}

void AuthService::prune_expired_sessions() {
  Statement statement(database_, "DELETE FROM auth_session WHERE expires_at < ?;");
  statement.bind(1, now_epoch()).run();
}

}  // namespace wt
