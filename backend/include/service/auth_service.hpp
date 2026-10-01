// 鉴权服务：管理员账号（PBKDF2 哈希）与 Bearer 会话令牌（90 天滑动有效期）。
// 会话只存令牌的 SHA-256 哈希;每次校验通过后按需滑动续期(最多每小时写一次库)。
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace wt {

class Database;

// 一次成功登录/建号返回的会话信息。
struct AuthSessionInfo {
  std::string token;
  std::int64_t user_id = 0;
  std::string username;
  std::int64_t expires_at = 0;  // epoch 秒
};

class AuthService {
 public:
  AuthService(Database& database, bool enabled);

  bool enabled() const noexcept { return enabled_; }

  // 是否已创建管理员账号（未建号时 /api/auth/setup 才可用）。
  bool initialized();

  // 首次建号：仅当尚无账号时允许；成功后直接返回会话（建号者即登录）。
  AuthSessionInfo setup(const std::string& username, const std::string& password);

  // 登录：校验 PBKDF2 哈希。同一用户名连续失败会进入递增退避（立即拒绝并提示
  // 等待秒数），成功后清零。退避只记录在内存中，重启即清空。
  AuthSessionInfo login(const std::string& username, const std::string& password);

  // 校验 Bearer 令牌：有效则按需滑动续期并返回 true。不抛异常（供中间件调用）。
  bool validate(const std::string& token);

  // 登出：删除对应会话。返回是否删除了有效会话。
  bool logout(const std::string& token);

  // 修改密码：校验旧密码后更新哈希，并吊销该用户全部会话（调用方需重新登录）。
  void change_password(const std::string& token, const std::string& old_password,
                       const std::string& new_password);

 private:
  // 以下方法均要求调用方已持有 database_.mutex()。
  AuthSessionInfo issue_session(std::int64_t user_id, const std::string& username);
  std::int64_t find_user(const std::string& username, std::string* password_hash);
  void prune_expired_sessions();

  Database& database_;
  bool enabled_;

  // 登录失败退避状态（用户名 -> 失败次数与惩罚截止时间），独立小锁,
  // 不与数据库锁交叉持有可能（先查退避再进数据库锁）。
  std::mutex backoff_mutex_;
  std::unordered_map<std::string, std::pair<std::uint32_t, std::int64_t>> backoff_;
};

}  // namespace wt
