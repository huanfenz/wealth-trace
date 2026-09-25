CREATE TABLE flexible_term_detail (
  asset_id INTEGER PRIMARY KEY REFERENCES asset(id) ON DELETE CASCADE,
  purchase_date TEXT NOT NULL,
  holding_period_days INTEGER NOT NULL CHECK (holding_period_days IN (180, 360))
);
