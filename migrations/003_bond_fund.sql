-- 债券基金明细（asset_type = 'BOND_FUND' 时使用）。
-- 两类债券基金共用一张表，通过 holding_mode 区分：
--   MIN_HOLDING 持有期债基：只关心 first_redeem_date，next_redeem_date 恒为 NULL；
--   ROLLING     滚动持有债基：first_redeem_date 固定，next_redeem_date 由每日维护推进。
-- 日期语义见「BOND_FUND 债券基金最终实现方案」。
CREATE TABLE bond_fund_detail (
  asset_id                    INTEGER PRIMARY KEY REFERENCES asset(id) ON DELETE CASCADE,
  fund_code                   TEXT,
  fund_name                   TEXT,
  principal                   INTEGER NOT NULL DEFAULT 0,            -- 本金（分）
  expected_annual_yield_rate  INTEGER,                              -- 预期年化收益率，定点 RATE_SCALE=1000000
  purchase_date               TEXT    NOT NULL,                     -- 买入/申购确认日期 YYYY-MM-DD
  holding_mode                TEXT    NOT NULL,                     -- MIN_HOLDING / ROLLING
  holding_period_days         INTEGER NOT NULL,                     -- 持有周期，统一按天
  first_redeem_date           TEXT,                                 -- 首次可赎回日期 YYYY-MM-DD
  next_redeem_date            TEXT,                                 -- 下一次可赎回日期（仅 ROLLING）
  maturity_date               TEXT                                  -- 产品最终到期日 YYYY-MM-DD（可空）
);

-- 每日维护只需扫描滚动型债基。
CREATE INDEX idx_bond_fund_holding_mode ON bond_fund_detail(holding_mode);

-- 系统键值元数据表。当前用于记录每日维护的最近执行日期：
--   key = 'daily_maintenance_last_run'，value = 'YYYY-MM-DD'。
CREATE TABLE system_state (
  key   TEXT PRIMARY KEY,
  value TEXT
);
