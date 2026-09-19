-- 001_init.sql
-- 财迹 (wealth-trace) V1 initial schema.
--
-- Conventions enforced across the project:
--   * monetary values : INTEGER, minor units (CNY fen, 1 yuan = 100 fen)
--   * interest rates  : INTEGER, fixed point with 6 decimals (0.0185 -> 18500)
--   * timestamps      : TEXT, UTC "YYYY-MM-DD HH:MM:SS"
--   * dates           : TEXT, "YYYY-MM-DD"
--
-- NOTE: `transaction` is a reserved SQL keyword, so it must always be quoted
-- as "transaction" in SQL statements.

PRAGMA foreign_keys = ON;

-- ---------------------------------------------------------------------------
-- household
-- ---------------------------------------------------------------------------
CREATE TABLE household (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  name          TEXT    NOT NULL,
  created_at    TEXT    NOT NULL,
  updated_at    TEXT    NOT NULL
);

-- ---------------------------------------------------------------------------
-- household_member
-- ---------------------------------------------------------------------------
CREATE TABLE household_member (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  household_id INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  name         TEXT    NOT NULL,
  role         TEXT    NOT NULL DEFAULT 'MEMBER',
  status       TEXT    NOT NULL DEFAULT 'ACTIVE',
  created_at   TEXT    NOT NULL,
  updated_at   TEXT    NOT NULL
);
CREATE INDEX idx_member_household ON household_member(household_id);

-- ---------------------------------------------------------------------------
-- account
-- ---------------------------------------------------------------------------
CREATE TABLE account (
  id                 INTEGER PRIMARY KEY AUTOINCREMENT,
  household_id       INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  owner_member_id    INTEGER NOT NULL REFERENCES household_member(id) ON DELETE RESTRICT,
  name               TEXT    NOT NULL,
  type               TEXT    NOT NULL,
  institution_name   TEXT,
  account_no_masked  TEXT,
  remark             TEXT,
  enabled            INTEGER NOT NULL DEFAULT 1,
  created_at         TEXT    NOT NULL,
  updated_at         TEXT    NOT NULL
);
CREATE INDEX idx_account_household_owner ON account(household_id, owner_member_id);
CREATE INDEX idx_account_owner ON account(owner_member_id);

-- ---------------------------------------------------------------------------
-- asset
-- ---------------------------------------------------------------------------
CREATE TABLE asset (
  id               INTEGER PRIMARY KEY AUTOINCREMENT,
  household_id     INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  owner_member_id  INTEGER NOT NULL REFERENCES household_member(id) ON DELETE RESTRICT,
  account_id       INTEGER NOT NULL REFERENCES account(id) ON DELETE CASCADE,
  name             TEXT    NOT NULL,
  asset_type       TEXT    NOT NULL,
  opening_balance  INTEGER NOT NULL DEFAULT 0,
  current_balance  INTEGER NOT NULL DEFAULT 0,
  status           TEXT    NOT NULL DEFAULT 'ACTIVE',
  remark           TEXT,
  created_at       TEXT    NOT NULL,
  updated_at       TEXT    NOT NULL
);
CREATE INDEX idx_asset_household_owner ON asset(household_id, owner_member_id);
CREATE INDEX idx_asset_account ON asset(account_id);

-- ---------------------------------------------------------------------------
-- term_deposit_detail (1:0..1 with asset)
-- ---------------------------------------------------------------------------
CREATE TABLE term_deposit_detail (
  asset_id             INTEGER PRIMARY KEY REFERENCES asset(id) ON DELETE CASCADE,
  principal            INTEGER NOT NULL DEFAULT 0,
  annual_interest_rate INTEGER NOT NULL DEFAULT 0,
  start_date           TEXT,
  maturity_date        TEXT,
  term_value           INTEGER,
  term_unit            TEXT,
  interest_type        TEXT,
  auto_rollover        INTEGER NOT NULL DEFAULT 0,
  maturity_action      TEXT
);

-- ---------------------------------------------------------------------------
-- fund_detail (1:0..1 with asset)
-- ---------------------------------------------------------------------------
CREATE TABLE fund_detail (
  asset_id       INTEGER PRIMARY KEY REFERENCES asset(id) ON DELETE CASCADE,
  fund_code      TEXT,
  fund_name      TEXT,
  fund_type      TEXT,
  lock_start_date TEXT,
  lock_end_date   TEXT
);

-- ---------------------------------------------------------------------------
-- bond_detail (1:0..1 with asset)
-- ---------------------------------------------------------------------------
CREATE TABLE bond_detail (
  asset_id           INTEGER PRIMARY KEY REFERENCES asset(id) ON DELETE CASCADE,
  bond_code          TEXT,
  bond_name          TEXT,
  principal          INTEGER NOT NULL DEFAULT 0,
  annual_coupon_rate INTEGER NOT NULL DEFAULT 0,
  purchase_date      TEXT,
  maturity_date      TEXT,
  lock_end_date      TEXT
);

-- ---------------------------------------------------------------------------
-- insurance_detail (1:0..1 with asset)
-- ---------------------------------------------------------------------------
CREATE TABLE insurance_detail (
  asset_id             INTEGER PRIMARY KEY REFERENCES asset(id) ON DELETE CASCADE,
  policy_no            TEXT,
  insurance_company    TEXT,
  product_name         TEXT,
  insurance_type       TEXT,
  effective_date       TEXT,
  maturity_date        TEXT,
  annual_premium       INTEGER NOT NULL DEFAULT 0,
  total_paid_premium   INTEGER NOT NULL DEFAULT 0,
  insured_amount       INTEGER NOT NULL DEFAULT 0,
  payment_years        INTEGER
);

-- ---------------------------------------------------------------------------
-- "transaction"
-- ---------------------------------------------------------------------------
CREATE TABLE "transaction" (
  id                INTEGER PRIMARY KEY AUTOINCREMENT,
  household_id      INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  owner_member_id   INTEGER NOT NULL REFERENCES household_member(id) ON DELETE RESTRICT,
  asset_id          INTEGER NOT NULL REFERENCES asset(id) ON DELETE CASCADE,
  type              TEXT    NOT NULL,
  category          TEXT,
  amount            INTEGER NOT NULL,
  transfer_group_id INTEGER,
  balance_before    INTEGER,
  balance_after     INTEGER,
  transaction_time  TEXT    NOT NULL,
  remark            TEXT,
  status            TEXT    NOT NULL DEFAULT 'NORMAL',
  created_at        TEXT    NOT NULL,
  updated_at        TEXT    NOT NULL
);
CREATE INDEX idx_transaction_household_owner_time
  ON "transaction"(household_id, owner_member_id, transaction_time);
CREATE INDEX idx_transaction_asset_time ON "transaction"(asset_id, transaction_time);
CREATE INDEX idx_transaction_transfer_group ON "transaction"(transfer_group_id);
