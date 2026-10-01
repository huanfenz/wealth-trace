#!/usr/bin/env python3
"""End-to-end API smoke test for the 财迹 wealth-trace backend.

Runs against a freshly started backend with a clean database and asserts the
core money flows (income / expense / transfer / statistics). Standard library
only, so it works in the minimal WSL environment.
"""
import json
import os
import sys
import urllib.error
import urllib.request

BASE = os.environ.get("WT_SMOKE_BASE_URL")
if not BASE:
    raise SystemExit("Run this test through scripts/run_api_smoke.sh")


def call(method, path, payload=None, token=None):
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(BASE + path, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            body = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        body = json.loads(error.read().decode("utf-8"))
    if body["code"] != 0:
        raise AssertionError(f"{method} {path} failed: {body}")
    return body["data"]


def call_status(method, path, payload=None, token=None):
    """Returns (http_status, parsed_body) without failing on error statuses."""
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(BASE + path, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            return response.status, json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        return error.code, json.loads(error.read().decode("utf-8"))


def backup_roundtrip():
    with urllib.request.urlopen(BASE + "/api/database/export", timeout=10) as response:
        backup = response.read()
    assert backup.startswith(b"SQLite format 3\x00")

    before = call("GET", "/api/households")
    household_id = before[0]["id"]
    marker = call("POST", f"/api/households/{household_id}/members",
                  {"name": "备份恢复测试", "role": "MEMBER"})
    request = urllib.request.Request(
        BASE + "/api/database/import", data=backup,
        headers={"Content-Type": "application/vnd.sqlite3"}, method="POST")
    with urllib.request.urlopen(request, timeout=10) as response:
        result = json.loads(response.read().decode("utf-8"))
    assert result["code"] == 0, result
    assert result["data"]["imported"] is True
    members = call("GET", f"/api/households/{household_id}/members")
    assert all(member["id"] != marker["id"] for member in members)


def auth_flow():
    """鉴权专项:建号 -> 未登录 401 -> 登录 -> 带令牌访问 -> 登出后失效。"""
    status = call("GET", "/api/auth/status")
    assert status["enabled"] is True, status
    assert status["initialized"] is False, status

    # 未携带令牌访问业务接口 -> 401。
    http_status, _ = call_status("GET", "/api/households")
    assert http_status == 401, http_status

    # 用户尚不存在时登录 -> 401（文案与密码错误一致，不泄露账号是否存在）。
    http_status, _ = call_status("POST", "/api/auth/login",
                                 {"username": "smoke-admin", "password": "wrong-password"})
    assert http_status == 401, http_status

    # 建号即登录。
    session = call("POST", "/api/auth/setup",
                   {"username": "smoke-admin", "password": "smoke-password-1"})
    assert session["token"] and session["user"]["username"] == "smoke-admin", session

    # 重复建号 -> 409。
    status, _ = call_status("POST", "/api/auth/setup",
                            {"username": "another", "password": "another-password"})
    assert status == 409, status

    # 带令牌访问业务接口 -> 200。
    households = call("GET", "/api/households", token=session["token"])
    assert households, households

    # 登出后旧令牌失效 -> 401。
    call("POST", "/api/auth/logout", {}, token=session["token"])
    status, _ = call_status("GET", "/api/households", token=session["token"])
    assert status == 401, status
    print("Auth smoke test passed")


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

    stock_fund = call("POST", f"/api/households/{household_id}/assets",
                {"account_id": account_id, "name": "某股票基金", "asset_type": "STOCK_FUND",
                 "opening_balance": 5000000,
                 "stock_fund": {"fund_code": "000001"}})
    assert stock_fund["stock_fund"]["fund_code"] == "000001"

    call("POST", f"/api/households/{household_id}/transactions/income",
         {"asset_id": cash["id"], "category": "工资", "amount": 1000000})
    call("POST", f"/api/households/{household_id}/transactions/expense",
         {"asset_id": cash["id"], "category": "餐饮", "amount": 3500})

    after = call("GET", f"/api/assets/{cash['id']}")
    assert after["current_balance"] == 2000000 + 1000000 - 3500, after

    transfer = call("POST", f"/api/households/{household_id}/transfers",
                    {"from_asset_id": cash["id"], "to_asset_id": stock_fund["id"],
                     "amount": 500000})
    assert transfer["type"] == "TRANSFER"
    assert transfer["direction"] == "NEUTRAL"
    assert len(transfer["entries"]) == 2
    assert {entry["direction"] for entry in transfer["entries"]} == {"IN", "OUT"}
    asset_rows = call("GET", f"/api/assets/{cash['id']}/transactions?household_id={household_id}")
    transfer_row = next(row for row in asset_rows["items"] if row["id"] == transfer["id"])
    assert transfer_row["direction"] == "OUT", transfer_row
    detail = call("GET", f"/api/transactions/{transfer['id']}")
    assert len(detail["entries"]) == 2

    cash_after = call("GET", f"/api/assets/{cash['id']}")
    fund_after = call("GET", f"/api/assets/{stock_fund['id']}")
    assert cash_after["current_balance"] == 2000000 + 1000000 - 3500 - 500000
    assert fund_after["current_balance"] == 5000000 + 500000

    transactions = call("GET", f"/api/households/{household_id}/transactions")
    assert transactions["total"] == 3, transactions["total"]

    overview = call("GET", f"/api/households/{household_id}/statistics/overview")
    assert overview["net_worth"] == cash_after["current_balance"] + fund_after["current_balance"]
    assert overview["month_income"] == 1000000
    assert overview["month_expense"] == 3500

    deleted = call("DELETE", f"/api/transactions/{transfer['id']}")
    assert deleted["deleted"] == 1, deleted
    restored_cash = call("GET", f"/api/assets/{cash['id']}")
    restored_fund = call("GET", f"/api/assets/{stock_fund['id']}")
    assert restored_cash["current_balance"] == 2000000 + 1000000 - 3500
    assert restored_fund["current_balance"] == 5000000

    kept_transfer = call("POST", f"/api/households/{household_id}/transfers",
                         {"from_asset_id": cash["id"], "to_asset_id": stock_fund["id"],
                          "amount": 100000})
    deleted = call("DELETE", f"/api/transactions/{kept_transfer['id']}?rollback_assets=false")
    assert deleted["deleted"] == 1, deleted
    kept_cash = call("GET", f"/api/assets/{cash['id']}")
    kept_fund = call("GET", f"/api/assets/{stock_fund['id']}")
    assert kept_cash["current_balance"] == restored_cash["current_balance"] - 100000
    assert kept_fund["current_balance"] == restored_fund["current_balance"] + 100000

    backup_roundtrip()
    print("API smoke test passed")


if __name__ == "__main__":
    try:
        if os.environ.get("WT_SMOKE_AUTH") == "1":
            auth_flow()
        else:
            main()
    except AssertionError as error:
        print("SMOKE TEST FAILED:", error)
        sys.exit(1)
