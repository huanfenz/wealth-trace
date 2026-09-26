-- Replace one-row-per-asset transactions with business transactions and entries.
-- Invalid legacy pairs/asset purchases are rejected by MigrationRunner preflight.
PRAGMA foreign_keys = OFF;
ALTER TABLE "transaction" RENAME TO legacy_transaction;

CREATE TABLE transactions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  household_id INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  owner_member_id INTEGER NOT NULL REFERENCES household_member(id) ON DELETE RESTRICT,
  type TEXT NOT NULL CHECK(type IN ('INCOME','EXPENSE','TRANSFER','INVESTMENT','ADJUSTMENT')),
  category_id INTEGER REFERENCES transaction_category(id) ON DELETE SET NULL,
  category TEXT,
  action TEXT CHECK(action IS NULL OR action IN ('BUY','REDEEM','DIVIDEND','INTEREST','MATURITY','ROLL_OVER')),
  transaction_time TEXT NOT NULL,
  remark TEXT,
  status TEXT NOT NULL DEFAULT 'NORMAL' CHECK(status IN ('NORMAL','VOID')),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE transaction_entries (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  transaction_id INTEGER NOT NULL REFERENCES transactions(id) ON DELETE CASCADE,
  household_id INTEGER NOT NULL REFERENCES household(id) ON DELETE CASCADE,
  owner_member_id INTEGER NOT NULL REFERENCES household_member(id) ON DELETE RESTRICT,
  asset_id INTEGER NOT NULL REFERENCES asset(id) ON DELETE RESTRICT,
  direction TEXT NOT NULL CHECK(direction IN ('IN','OUT')),
  amount INTEGER NOT NULL CHECK(amount >= 0),
  balance_before INTEGER,
  balance_after INTEGER,
  created_at TEXT NOT NULL
);

CREATE TABLE investment_transaction_details (
  transaction_id INTEGER PRIMARY KEY REFERENCES transactions(id) ON DELETE CASCADE,
  asset_id INTEGER NOT NULL REFERENCES asset(id) ON DELETE RESTRICT,
  action TEXT NOT NULL CHECK(action IN ('BUY','REDEEM','DIVIDEND','INTEREST','MATURITY','ROLL_OVER')),
  principal INTEGER
);

-- Transfer transaction ID is the legacy outgoing row ID; standalone transaction IDs remain stable.
INSERT INTO transactions (id,household_id,owner_member_id,type,category_id,category,action,transaction_time,remark,status,created_at,updated_at)
SELECT o.id,o.household_id,o.owner_member_id,
       CASE WHEN EXISTS (SELECT 1 FROM recurring_investment_execution e
                         WHERE e.transfer_group_id=o.transfer_group_id AND e.status IN ('SUCCESS','REVERSED'))
            THEN 'INVESTMENT' ELSE 'TRANSFER' END,
       NULL,NULL,
       CASE WHEN EXISTS (SELECT 1 FROM recurring_investment_execution e
                         WHERE e.transfer_group_id=o.transfer_group_id AND e.status IN ('SUCCESS','REVERSED'))
            THEN 'BUY' ELSE NULL END,
       o.transaction_time,o.remark,o.status,o.created_at,o.updated_at
FROM legacy_transaction o WHERE o.type='TRANSFER_OUT';

INSERT INTO transactions (id,household_id,owner_member_id,type,category_id,category,action,transaction_time,remark,status,created_at,updated_at)
SELECT id,household_id,owner_member_id,
       CASE type WHEN 'ASSET_PURCHASE' THEN 'INVESTMENT' ELSE type END,
       category_id,category,
       CASE WHEN type='ASSET_PURCHASE' THEN 'BUY'
            WHEN type='TRANSFER_OUT' AND EXISTS (SELECT 1 FROM recurring_investment_execution e
                 WHERE e.transfer_group_id=legacy_transaction.transfer_group_id AND e.status IN ('SUCCESS','REVERSED'))
            THEN 'BUY' ELSE NULL END,
       transaction_time,remark,status,created_at,updated_at
FROM legacy_transaction WHERE type NOT IN ('TRANSFER_IN','TRANSFER_OUT');

INSERT INTO transaction_entries (transaction_id,household_id,owner_member_id,asset_id,direction,amount,balance_before,balance_after,created_at)
SELECT CASE WHEN type IN ('TRANSFER_IN','TRANSFER_OUT') THEN
         (SELECT id FROM legacy_transaction o WHERE o.transfer_group_id=legacy_transaction.transfer_group_id AND o.type='TRANSFER_OUT')
       ELSE id END,
       household_id,owner_member_id,asset_id,
       CASE WHEN type IN ('INCOME','TRANSFER_IN') THEN 'IN'
            WHEN type='ADJUSTMENT' AND amount>=0 THEN 'IN' ELSE 'OUT' END,
       ABS(amount),balance_before,balance_after,created_at
FROM legacy_transaction;

-- Legacy asset purchase recorded only the cash outflow. Add the acquired asset entry and
-- shift its opening balance by the same amount so the current balance does not change.
INSERT INTO transaction_entries (transaction_id,household_id,owner_member_id,asset_id,direction,amount,created_at)
SELECT t.id,t.household_id,a.owner_member_id,
       CAST(substr(substr(t.remark,instr(t.remark,'(#')+2),1,
                    instr(substr(t.remark,instr(t.remark,'(#')+2),')')-1) AS INTEGER),
       'IN',legacy.amount,legacy.created_at
FROM transactions t
JOIN legacy_transaction legacy ON legacy.id=t.id AND legacy.type='ASSET_PURCHASE'
JOIN asset a ON a.id=CAST(substr(substr(legacy.remark,instr(legacy.remark,'(#')+2),1,
                    instr(substr(legacy.remark,instr(legacy.remark,'(#')+2),')')-1) AS INTEGER);

INSERT INTO investment_transaction_details(transaction_id,asset_id,action,principal)
SELECT t.id,CAST(substr(substr(l.remark,instr(l.remark,'(#')+2),1,
                    instr(substr(l.remark,instr(l.remark,'(#')+2),')')-1) AS INTEGER),
       'BUY',l.amount
FROM transactions t JOIN legacy_transaction l ON l.id=t.id AND l.type='ASSET_PURCHASE';

INSERT INTO investment_transaction_details(transaction_id,asset_id,action,principal)
SELECT DISTINCT o.id,e.target_asset_id,'BUY',e.amount
FROM recurring_investment_execution e
JOIN legacy_transaction o ON o.transfer_group_id=e.transfer_group_id AND o.type='TRANSFER_OUT'
WHERE e.status IN ('SUCCESS','REVERSED');

UPDATE asset SET opening_balance=opening_balance-(
  SELECT COALESCE(SUM(l.amount),0) FROM legacy_transaction l
  WHERE l.type='ASSET_PURCHASE'
    AND CAST(substr(substr(l.remark,instr(l.remark,'(#')+2),1,
              instr(substr(l.remark,instr(l.remark,'(#')+2),')')-1) AS INTEGER)=asset.id
) WHERE id IN (SELECT asset_id FROM investment_transaction_details);

CREATE INDEX idx_transactions_household_owner_time ON transactions(household_id,owner_member_id,transaction_time);
CREATE INDEX idx_entries_asset_time ON transaction_entries(asset_id,transaction_id);
CREATE INDEX idx_entries_transaction ON transaction_entries(transaction_id);

-- Replace recurring execution's legacy transfer group reference with a direct business ID.
CREATE TABLE recurring_investment_execution_new (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  plan_id INTEGER NOT NULL REFERENCES recurring_investment_plan(id) ON DELETE CASCADE,
  scheduled_date TEXT NOT NULL,
  amount INTEGER NOT NULL,
  source_asset_id INTEGER,
  target_asset_id INTEGER,
  status TEXT NOT NULL CHECK(status IN ('SUCCESS','FAILED','REVERSED')),
  transaction_id INTEGER REFERENCES transactions(id) ON DELETE SET NULL,
  failure_reason TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  UNIQUE(plan_id,scheduled_date)
);
INSERT INTO recurring_investment_execution_new
  (id,plan_id,scheduled_date,amount,source_asset_id,target_asset_id,status,transaction_id,failure_reason,created_at,updated_at)
SELECT e.id,e.plan_id,e.scheduled_date,e.amount,e.source_asset_id,e.target_asset_id,e.status,
       (SELECT o.id FROM legacy_transaction o WHERE o.transfer_group_id=e.transfer_group_id AND o.type='TRANSFER_OUT'),
       e.failure_reason,e.created_at,e.updated_at
FROM recurring_investment_execution e;
DROP TABLE recurring_investment_execution;
ALTER TABLE recurring_investment_execution_new RENAME TO recurring_investment_execution;
CREATE INDEX idx_recurring_execution_plan_date ON recurring_investment_execution(plan_id,scheduled_date DESC);
CREATE INDEX idx_recurring_execution_transaction ON recurring_investment_execution(transaction_id);

DROP TABLE legacy_transaction;
PRAGMA foreign_keys = ON;
