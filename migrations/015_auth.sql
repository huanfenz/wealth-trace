-- 鉴权:管理员账号与登录会话。
-- auth_session 只存令牌的 SHA-256 哈希,导出/备份的 .db 文件泄露不会泄露可用会话。
-- expires_at / last_used_at 为 epoch 秒(整数),便于过期比较与滑动续期;
-- created_at / updated_at 沿用全项目的 UTC ISO8601 文本格式。
CREATE TABLE auth_user (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE auth_session (
  token_hash TEXT PRIMARY KEY,
  user_id INTEGER NOT NULL REFERENCES auth_user(id) ON DELETE CASCADE,
  created_at TEXT NOT NULL,
  expires_at INTEGER NOT NULL,
  last_used_at INTEGER NOT NULL
);

CREATE INDEX idx_auth_session_user ON auth_session(user_id);
