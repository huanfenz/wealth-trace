-- 将流水文本分类迁移到家庭分类表。保留 category 文本列作为兼容快照。
CREATE TABLE transaction_category (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  household_id INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  type         TEXT    NOT NULL CHECK (type IN ('INCOME', 'EXPENSE')),
  name         TEXT    NOT NULL,
  sort_order   INTEGER NOT NULL DEFAULT 0,
  active       INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
  created_at   TEXT    NOT NULL,
  updated_at   TEXT    NOT NULL,
  UNIQUE (household_id, type, name),
  UNIQUE (id, household_id, type)
);

CREATE INDEX idx_transaction_category_household_type
  ON transaction_category(household_id, type, active, sort_order, id);

CREATE TABLE household_category_seed (
  household_id INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  type         TEXT NOT NULL CHECK (type IN ('INCOME', 'EXPENSE')),
  PRIMARY KEY (household_id, type)
);

ALTER TABLE "transaction" ADD COLUMN category_id INTEGER REFERENCES transaction_category(id) ON DELETE SET NULL;

-- 旧版本允许直接提交任意文本分类；迁移时逐个家庭、收支类型保留所有历史名称。
INSERT OR IGNORE INTO transaction_category
  (household_id, type, name, sort_order, active, created_at, updated_at)
SELECT household_id, type, category, 0, 1, MIN(created_at), MIN(updated_at)
FROM "transaction"
WHERE type IN ('INCOME', 'EXPENSE') AND category IS NOT NULL AND TRIM(category) <> ''
GROUP BY household_id, type, category;

UPDATE "transaction"
SET category_id = (
  SELECT c.id FROM transaction_category c
  WHERE c.household_id = "transaction".household_id
    AND c.type = "transaction".type
    AND c.name = "transaction".category
)
WHERE category IS NOT NULL AND type IN ('INCOME', 'EXPENSE');
