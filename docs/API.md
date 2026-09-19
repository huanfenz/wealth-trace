# API 参考

基础路径：`/api`。除 `GET /api/health` 外均返回统一响应包：

```json
{ "code": 0, "message": "success", "data": {} }
```

失败：

```json
{ "code": 40001, "message": "invalid request", "data": null }
```

错误码：

| code | HTTP | 含义 |
| --- | --- | --- |
| 0 | 200 | 成功 |
| 40001 | 400 | 请求参数错误 |
| 40401 | 404 | 资源不存在 |
| 40901 | 409 | 冲突（违反一致性约束） |
| 50001 | 500 | 数据库错误 |
| 50002 | 500 | 内部错误 |
| 50001 | 503 | 数据库忙 |

> 系统仅支持人民币（CNY，无币种字段）。所有金额字段单位均为**分**（整数）；
> 利率字段为 6 位定点整数（`18500 = 1.85%`）；
> 时间为 UTC 字符串 `YYYY-MM-DD HH:MM:SS`，日期为 `YYYY-MM-DD`。

---

## 元数据

### `GET /api/health`

```json
{ "code": 0, "message": "success", "data": { "status": "ok" } }
```

### `GET /api/meta`

返回枚举与默认分类：

```json
{
  "code": 0,
  "message": "success",
  "data": {
    "member_roles": ["OWNER", "MEMBER"],
    "member_statuses": ["ACTIVE", "INACTIVE"],
    "account_types": ["BANK", "ALIPAY", "WECHAT", "CASH", "SECURITIES", "INSURANCE", "OTHER"],
    "asset_types": ["CASH", "TERM_DEPOSIT", "FUND", "BOND", "INSURANCE", "LIABILITY", "OTHER"],
    "asset_statuses": ["ACTIVE", "CLOSED"],
    "transaction_types": ["INCOME", "EXPENSE", "TRANSFER_IN", "TRANSFER_OUT", "ADJUSTMENT"],
    "term_units": ["DAY", "MONTH", "YEAR"],
    "income_categories": ["工资", "奖金", "..."],
    "expense_categories": ["餐饮", "交通", "..."],
    "money": { "unit": "minor", "minor_units_per_yuan": 100 },
    "rate_scale": 1000000
  }
}
```

---

## 家庭 Household

### `GET /api/households`

返回家庭数组。

### `POST /api/households`

```json
{ "name": "我的家庭" }
```

### `GET /api/households/{id}` / `PUT /api/households/{id}`

`PUT` 请求体：

```json
{ "name": "我的家庭" }
```

---

## 成员 Member

### `GET /api/households/{id}/members`

### `POST /api/households/{id}/members`

```json
{ "name": "王鹏", "role": "OWNER", "status": "ACTIVE" }
```

### `GET /api/members/{id}` / `PUT /api/members/{id}`

```json
{ "name": "王鹏", "role": "OWNER", "status": "ACTIVE" }
```

---

## 账户 Account

### `GET /api/households/{id}/accounts?owner_member_id={memberId}`

返回账户数组，`balance` 为其有效资产余额之和，`asset_count` 为资产数量：

```json
{
  "id": 1,
  "household_id": 1,
  "owner_member_id": 1,
  "name": "工商银行",
  "type": "BANK",
  "institution_name": "工商银行",
  "account_no_masked": "****1234",
  "remark": null,
  "enabled": true,
  "balance": 1700000,
  "asset_count": 3,
  "created_at": "2026-09-19 10:00:00",
  "updated_at": "2026-09-19 10:00:00"
}
```

### `POST /api/households/{id}/accounts`

```json
{
  "owner_member_id": 1,
  "name": "工商银行",
  "type": "BANK",
  "institution_name": "工商银行",
  "account_no_masked": "****1234",
  "remark": null,
  "enabled": true
}
```

### `GET /api/accounts/{id}` / `PUT /api/accounts/{id}`

`PUT` 字段与创建一致。若账户下已有资产，修改 `owner_member_id` 返回 `40901`。

---

## 资产 Asset

### `GET /api/households/{id}/assets?owner_member_id=&account_id=`

返回资产数组，按 `asset_type` 附带对应明细块（`term_deposit` / `fund` / `bond` / `insurance`），
不适用时为 `null`：

```json
{
  "id": 1,
  "household_id": 1,
  "owner_member_id": 1,
  "account_id": 1,
  "name": "活期",
  "asset_type": "CASH",
  "opening_balance": 2000000,
  "current_balance": 2996500,
  "status": "ACTIVE",
  "remark": null,
  "term_deposit": null,
  "fund": null,
  "bond": null,
  "insurance": null,
  "created_at": "2026-09-19 10:00:00",
  "updated_at": "2026-09-19 10:00:00"
}
```

### `POST /api/households/{id}/assets`

创建资产。`account_id` 决定 `household_id` 与 `owner_member_id`（冗余字段自动从账户派生）。

现金/活期：

```json
{
  "account_id": 1,
  "name": "活期",
  "asset_type": "CASH",
  "opening_balance": 2000000,
  "remark": null
}
```

定期存款（明细类型必须与 `asset_type` 一致）：

```json
{
  "account_id": 1,
  "name": "三年定期",
  "asset_type": "TERM_DEPOSIT",
  "opening_balance": 10000000,
  "term_deposit": {
    "principal": 10000000,
    "annual_interest_rate": 18500,
    "start_date": "2026-09-19",
    "maturity_date": "2029-09-19",
    "term_value": 3,
    "term_unit": "YEAR",
    "auto_rollover": false
  }
}
```

基金：

```json
{
  "account_id": 1,
  "name": "某债券基金",
  "asset_type": "FUND",
  "opening_balance": 5000000,
  "fund": { "fund_code": "000001", "fund_type": "BOND", "lock_end_date": "2027-03-01" }
}
```

债券 / 保险同理，使用 `bond` / `insurance` 块。

负债（`opening_balance` 必须 ≤ 0）：

```json
{ "account_id": 2, "name": "信用卡", "asset_type": "LIABILITY", "opening_balance": 0 }
```

### `GET /api/assets/{id}` / `PUT /api/assets/{id}`

`PUT` 更新基本信息：

```json
{ "name": "活期", "opening_balance": 2500000, "remark": "工资卡" }
```

> 若该资产已有交易，修改 `opening_balance` 返回 `40901`。

### `PUT /api/assets/{id}/status`

```json
{ "status": "CLOSED" }
```

关闭后不计入统计，且不能新增交易。

### `PUT /api/assets/{id}/detail`

更新专有明细，`detail_type` 必须与资产类型一致：

```json
{
  "detail_type": "FUND",
  "fund": { "fund_code": "000001", "fund_name": "某债券基金" }
}
```

---

## 交易 Transaction

### `GET /api/households/{id}/transactions`

查询参数：

| 参数 | 说明 |
| --- | --- |
| `owner_member_id` | 按成员过滤 |
| `asset_id` | 按资产过滤 |
| `type` | INCOME/EXPENSE/TRANSFER_IN/TRANSFER_OUT/ADJUSTMENT |
| `from` / `to` | 时间范围（`YYYY-MM-DD HH:MM:SS`，含端点） |
| `limit` | 1..1000，默认 200 |
| `offset` | 默认 0 |

返回：

```json
{ "total": 4, "items": [ { "id": 1, "type": "INCOME", "amount": 1000000, "...": "..." } ] }
```

### `POST /api/households/{id}/transactions/income`

```json
{ "asset_id": 1, "category": "工资", "amount": 1000000, "transaction_time": "", "remark": "" }
```

### `POST /api/households/{id}/transactions/expense`

```json
{ "asset_id": 1, "category": "餐饮", "amount": 3500 }
```

### `POST /api/households/{id}/transactions/adjustment`

`amount` 可正可负：

```json
{ "asset_id": 3, "amount": 100000, "remark": "基金估值调整" }
```

### `POST /api/households/{id}/transfers`

一次转账生成 `TRANSFER_OUT` + `TRANSFER_IN` 两条流水，共用 `transfer_group_id`：

```json
{
  "from_asset_id": 1,
  "to_asset_id": 2,
  "amount": 500000,
  "transaction_time": "",
  "remark": "活期转定期"
}
```

返回：

```json
{
  "outgoing": { "type": "TRANSFER_OUT", "amount": 500000, "transfer_group_id": 10001, "...": "..." },
  "incoming": { "type": "TRANSFER_IN", "amount": 500000, "transfer_group_id": 10001, "...": "..." }
}
```

### `GET /api/transactions/{id}`

返回单条流水。

---

## 统计 Statistics

### `GET /api/households/{id}/statistics/overview?year=2026&month=9`

```json
{
  "total_assets": 2950000,
  "total_liabilities": 30000,
  "net_worth": 2920000,
  "month_income": 1000000,
  "month_expense": 80000,
  "month_balance": 920000,
  "by_member": [ { "id": 1, "name": "王鹏", "amount": 2920000 } ],
  "by_account": [ { "id": 1, "name": "工商银行", "amount": 2920000 } ],
  "by_type": [ { "asset_type": "CASH", "amount": 2950000 } ]
}
```

### `GET /api/households/{id}/statistics/period?from=2026-09-01 00:00:00&to=2026-09-30 23:59:59&owner_member_id=1`

```json
{
  "from": "2026-09-01 00:00:00",
  "to": "2026-09-30 23:59:59",
  "income": 1000000,
  "expense": 80000,
  "balance": 920000,
  "by_member": [ { "id": 1, "name": "王鹏", "amount": 920000 } ],
  "income_categories": [ { "category": "工资", "amount": 1000000 } ],
  "expense_categories": [ { "category": "餐饮", "amount": 50000 } ]
}
```

---

## 前端静态资源（可选）

当 `config.json` 中 `frontend.enabled=true` 时，后端同时托管 `frontend/dist`：

- `GET /` 返回 `index.html`；
- 未匹配到文件的非 API 路径回退到 `index.html`（SPA 路由）；
- `/api/*` 始终由 API 处理。
