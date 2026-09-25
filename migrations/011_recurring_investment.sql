-- 股票基金定投计划与执行记录。
CREATE TABLE recurring_investment_plan (
  id                INTEGER PRIMARY KEY AUTOINCREMENT,
  household_id      INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  owner_member_id   INTEGER NOT NULL REFERENCES household_member(id) ON DELETE RESTRICT,
  target_asset_id   INTEGER REFERENCES asset(id) ON DELETE SET NULL,
  source_asset_id   INTEGER REFERENCES asset(id) ON DELETE SET NULL,
  amount            INTEGER NOT NULL CHECK (amount > 0),
  frequency         TEXT NOT NULL CHECK (frequency IN ('DAILY', 'WEEKLY', 'BIWEEKLY', 'MONTHLY')),
  weekday           INTEGER CHECK (weekday BETWEEN 1 AND 7),
  month_day         INTEGER CHECK (month_day BETWEEN 1 AND 28),
  start_date        TEXT NOT NULL,
  next_due_date     TEXT NOT NULL,
  status            TEXT NOT NULL DEFAULT 'ACTIVE' CHECK (status IN ('ACTIVE', 'PAUSED', 'DELETED')),
  created_at        TEXT NOT NULL,
  updated_at        TEXT NOT NULL
);
CREATE INDEX idx_recurring_plan_due ON recurring_investment_plan(status, next_due_date);
CREATE INDEX idx_recurring_plan_household ON recurring_investment_plan(household_id, status);

CREATE TABLE recurring_investment_execution (
  id                INTEGER PRIMARY KEY AUTOINCREMENT,
  plan_id           INTEGER NOT NULL REFERENCES recurring_investment_plan(id) ON DELETE CASCADE,
  scheduled_date    TEXT NOT NULL,
  amount            INTEGER NOT NULL,
  source_asset_id   INTEGER,
  target_asset_id   INTEGER,
  status            TEXT NOT NULL CHECK (status IN ('SUCCESS', 'FAILED', 'REVERSED')),
  transfer_group_id INTEGER,
  failure_reason    TEXT,
  created_at        TEXT NOT NULL,
  updated_at        TEXT NOT NULL,
  UNIQUE(plan_id, scheduled_date)
);
CREATE INDEX idx_recurring_execution_plan_date
  ON recurring_investment_execution(plan_id, scheduled_date DESC);
CREATE INDEX idx_recurring_execution_transfer
  ON recurring_investment_execution(transfer_group_id);
