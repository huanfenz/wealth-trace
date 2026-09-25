CREATE TABLE commercial_pension_detail (
  asset_id INTEGER PRIMARY KEY REFERENCES asset(id) ON DELETE CASCADE,
  purchase_time TEXT NOT NULL,
  holding_period_value INTEGER NOT NULL CHECK (holding_period_value > 0),
  holding_period_unit TEXT NOT NULL CHECK (holding_period_unit IN ('DAY', 'MONTH', 'YEAR')),
  change_window_start TEXT,
  change_window_end TEXT,
  redeem_at_maturity INTEGER NOT NULL DEFAULT 0 CHECK (redeem_at_maturity IN (0, 1)),
  redeem_at TEXT,
  CHECK ((change_window_start IS NULL) = (change_window_end IS NULL)),
  CHECK (redeem_at_maturity = 0 OR redeem_at IS NOT NULL)
);
