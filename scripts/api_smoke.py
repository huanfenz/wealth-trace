#!/usr/bin/env python3
"""End-to-end API smoke test for the 财迹 wealth-trace backend.

Runs against a freshly started backend with a clean database and asserts the
core money flows (income / expense / transfer / statistics). Standard library
only, so it works in the minimal WSL environment.
"""
import json
import sys
import urllib.error
import urllib.request

BASE = "http://127.0.0.1:8080"


def call(method, path, payload=None):
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    request = urllib.request.Request(BASE + path, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            body = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        body = json.loads(error.read().decode("utf-8"))
    if body["code"] != 0:
        raise AssertionError(f"{method} {path} failed: {body}")
    return body["data"]


def main():
    health = call("GET", "/api/health")
    assert health["status"] == "ok"

    households = call("GET", "/api/households")
    assert households, "expected a default household"
    household_id = households[0]["id"]

    member = call("POST", f"/api/households/{household_id}/members",
                  {"name": "王鹏", "role": "OWNER"})
    assert member["name"] == "王鹏"

    account = call("POST", f"/api/households/{household_id}/accounts",
                   {"owner_member_id": member["id"], "name": "工商银行", "type": "BANK"})
    account_id = account["id"]

    assets = call("GET", f"/api/households/{household_id}/assets?account_id={account_id}")
    assert assets == []

    cash = call("POST", f"/api/households/{household_id}/assets",
                {"account_id": account_id, "name": "活期", "asset_type": "CASH",
                 "opening_balance": 2000000})
    assert cash["current_balance"] == 2000000

    fund = call("POST", f"/api/households/{household_id}/assets",
                {"account_id": account_id, "name": "某基金", "asset_type": "FUND",
                 "opening_balance": 5000000,
                 "fund": {"fund_code": "000001", "fund_type": "BOND"}})
    assert fund["fund"]["fund_code"] == "000001"

    call("POST", f"/api/households/{household_id}/transactions/income",
         {"asset_id": cash["id"], "category": "工资", "amount": 1000000})
    call("POST", f"/api/households/{household_id}/transactions/expense",
         {"asset_id": cash["id"], "category": "餐饮", "amount": 3500})

    after = call("GET", f"/api/assets/{cash['id']}")
    assert after["current_balance"] == 2000000 + 1000000 - 3500, after

    transfer = call("POST", f"/api/households/{household_id}/transfers",
                    {"from_asset_id": cash["id"], "to_asset_id": fund["id"],
                     "amount": 500000})
    assert transfer["outgoing"]["transfer_group_id"] == transfer["incoming"]["transfer_group_id"]

    cash_after = call("GET", f"/api/assets/{cash['id']}")
    fund_after = call("GET", f"/api/assets/{fund['id']}")
    assert cash_after["current_balance"] == 2000000 + 1000000 - 3500 - 500000
    assert fund_after["current_balance"] == 5000000 + 500000

    transactions = call("GET", f"/api/households/{household_id}/transactions")
    assert transactions["total"] == 4, transactions["total"]

    overview = call("GET", f"/api/households/{household_id}/statistics/overview")
    assert overview["net_worth"] == cash_after["current_balance"] + fund_after["current_balance"]
    assert overview["month_income"] == 1000000
    assert overview["month_expense"] == 3500

    print("API smoke test passed")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as error:
        print("SMOKE TEST FAILED:", error)
        sys.exit(1)
