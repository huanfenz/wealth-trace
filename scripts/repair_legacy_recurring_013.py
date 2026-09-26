#!/usr/bin/env python3
"""One-time repair for the known caiji.db recurring-investment history before migration 013.

The six successful buys affected balances, but their old transfer rows were deleted and
their group IDs were reused. Rebuild those historical rows without changing balances.
"""

import argparse
import sqlite3


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def repair(path: str) -> None:
    db = sqlite3.connect(path)
    db.row_factory = sqlite3.Row
    db.execute("PRAGMA foreign_keys=ON")
    try:
        require(db.execute("SELECT max(version) FROM schema_migration").fetchone()[0] == 12,
                "expected schema version 12")
        rows = db.execute("SELECT * FROM recurring_investment_execution ORDER BY id").fetchall()
        reversed_rows = [row for row in rows if row["status"] == "REVERSED"]
        successful = [row for row in rows if row["status"] == "SUCCESS"]
        require([row["id"] for row in reversed_rows] == list(range(1, 7)),
                "reversed execution IDs changed")
        require([row["id"] for row in successful] == list(range(7, 13)),
                "successful execution IDs changed")
        require(all(row["amount"] == 1000 and row["source_asset_id"] == 3 and
                    row["transfer_group_id"] == row["plan_id"] for row in rows),
                "execution amounts, source, or old group IDs changed")
        require(all(row["scheduled_date"] == "2026-09-25" for row in reversed_rows) and
                all(row["scheduled_date"] == "2026-09-26" for row in successful),
                "execution dates changed")
        existing = db.execute('SELECT id,asset_id,type,amount,transfer_group_id FROM "transaction" ORDER BY id').fetchall()
        require([(r["id"], r["asset_id"], r["type"], r["amount"], r["transfer_group_id"])
                 for r in existing] == [
                    (16, 3, "ASSET_PURCHASE", 1000000, None),
                    (29, 3, "EXPENSE", 28320, None),
                    (30, 3, "TRANSFER_OUT", 100, 1),
                    (31, 4, "TRANSFER_IN", 100, 1),
                 ], "legacy transaction rows changed")
        source_start = db.execute('SELECT balance_after FROM "transaction" WHERE id=16').fetchone()[0]
        source_end = db.execute('SELECT balance_before FROM "transaction" WHERE id=29').fetchone()[0]
        require(source_start - source_end == sum(row["amount"] for row in successful),
                "source balance gap does not equal successful investment total")

        details = []
        for index, execution in enumerate(successful, start=2):
            source = db.execute("SELECT * FROM asset WHERE id=?", (execution["source_asset_id"],)).fetchone()
            target = db.execute("SELECT * FROM asset WHERE id=?", (execution["target_asset_id"],)).fetchone()
            plan = db.execute("SELECT * FROM recurring_investment_plan WHERE id=?", (execution["plan_id"],)).fetchone()
            require(source is not None and target is not None and plan is not None,
                    f"execution {execution['id']} has a missing asset or plan")
            require(source["household_id"] == target["household_id"] == plan["household_id"] and
                    source["owner_member_id"] == target["owner_member_id"] == plan["owner_member_id"],
                    f"execution {execution['id']} has inconsistent ownership")
            require(target["id"] == plan["target_asset_id"] and
                    source["id"] == plan["source_asset_id"] and
                    target["current_balance"] - target["opening_balance"] == execution["amount"],
                    f"execution {execution['id']} does not match asset balances")
            details.append((execution, source, target, index))

        db.execute("BEGIN IMMEDIATE")
        db.execute("UPDATE recurring_investment_execution SET transfer_group_id=NULL WHERE status='REVERSED'")
        source_before = source_start
        for execution, source, target, group_id in details:
            amount = execution["amount"]
            source_after = source_before - amount
            for asset, tx_type, before, after in (
                (source, "TRANSFER_OUT", source_before, source_after),
                (target, "TRANSFER_IN", target["current_balance"] - amount, target["current_balance"]),
            ):
                db.execute('''INSERT INTO "transaction"
                    (household_id,owner_member_id,asset_id,type,category,amount,
                     transfer_group_id,balance_before,balance_after,transaction_time,
                     remark,status,created_at,updated_at)
                    VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)''',
                    (asset["household_id"], asset["owner_member_id"], asset["id"], tx_type,
                     None, amount, group_id, before, after, execution["created_at"],
                     "股票基金定投", "NORMAL", execution["created_at"], execution["updated_at"]))
            db.execute("UPDATE recurring_investment_execution SET transfer_group_id=? WHERE id=?",
                       (group_id, execution["id"]))
            source_before = source_after
        require(source_before == source_end, "reconstructed source balance chain is incomplete")
        require(db.execute("PRAGMA foreign_key_check").fetchall() == [], "foreign key check failed")
        require(db.execute("PRAGMA integrity_check").fetchone()[0] == "ok", "integrity check failed")
        db.commit()
        print("Repaired 6 successful buys and detached 6 reversed execution links; asset balances unchanged.")
    except Exception:
        db.rollback()
        raise
    finally:
        db.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", help="schema-12 database to repair")
    args = parser.parse_args()
    repair(args.database)
