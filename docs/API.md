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
> 审计时间戳（`created_at` / `updated_at` / `transaction_time`）为 UTC 字符串 `YYYY-MM-DD HH:MM:SS`；
> 业务日期字段（如债券基金赎回日）为 `YYYY-MM-DD`，按业务时区 `business_timezone`（默认 `Asia/Shanghai`）口径。

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
    "asset_types": ["CASH", "TERM_DEPOSIT", "FUND", "BOND", "BOND_FUND", "INSURANCE", "LIABILITY", "OTHER"],
    "asset_statuses": ["ACTIVE", "CLOSED"],
    "transaction_types": ["INCOME", "EXPENSE", "TRANSFER_IN", "TRANSFER_OUT", "ADJUSTMENT"],
    "term_units": ["DAY", "MONTH", "YEAR"],
    "income_categories": ["工资", "奖金", "..."],
    "expense_categories": ["餐饮", "交通", "..."],
    "money": { "unit": "minor", "minor_units_per_yuan": 100 },
    "rate_scale": 1000000,
    "business_timezone": "Asia/Shanghai",
    "business_date": "2026-09-22"
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

> 现金/支付宝/微信（`CASH` / `ALIPAY` / `WECHAT`）账户若未提供 `institution_name`，
> 服务端会自动填入「现金 / 支付宝 / 微信」作为默认机构名。

### `DELETE /api/accounts/{id}`

删除账户。若账户下仍有资产，返回 `40901`（需先删除或转移其下资产），成功返回 `data: null`。

---

## 资产 Asset

### `GET /api/households/{id}/assets?owner_member_id=&account_id=`

返回资产数组，按 `asset_type` 附带对应明细块（`term_deposit` / `fund` / `bond` / `bond_fund` / `insurance`），
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
  "bond_fund": null,
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

定期存款（明细类型必须与 `asset_type` 一致）。`start_date` / `term_value` / `term_unit` 必填，
`maturity_date` 可留空，由服务端按存期（自然月 / 自然年，目标月无对应日取月末）计算，也可手工修正：

```json
{
  "account_id": 1,
  "name": "三年定期",
  "asset_type": "TERM_DEPOSIT",
  "opening_balance": 10000000,
  "term_deposit": {
    "annual_interest_rate": 18500,
    "start_date": "2026-09-19",
    "term_value": 3,
    "term_unit": "YEAR",
    "auto_rollover": false
  }
}
```

返回体中的 `term_deposit` 还带两个只读派生字段：`status`
（`ACTIVE` 存续中 / `MATURED` 已到期 / `UNKNOWN`）与 `days_until_maturity`（距到期天数，可空）。
`auto_rollover=true` 时，每日维护在到期当天（业务日期 `today >= maturity_date`）自动推进到下一存期，
同步更新 `start_date` 为当前存期起始日；不修改本金（即 `opening_balance`）、当前金额与利率。
明细不再保存 `principal`，本金以资产的 `opening_balance` 为准（债券同理，名称以 `asset.name` 为准）。

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

债券基金（`asset_type=BOND_FUND`）。`purchase_date` / `holding_mode` / `holding_period_days` 必填；
`first_redeem_date` / `next_redeem_date` 可留空由服务端计算，也可手工修正（节假日顺延）：

```json
{
  "account_id": 1,
  "name": "某90天滚动持有债券基金",
  "asset_type": "BOND_FUND",
  "opening_balance": 10235025,
  "bond_fund": {
    "fund_code": "012345",
    "expected_annual_yield_rate": 35000,
    "purchase_date": "2026-09-22",
    "holding_mode": "ROLLING",
    "holding_period_days": 90
  }
}
```

> 债券基金的本金以资产的「初始金额」为准，名称以 `asset.name` 为准；
> 明细不再接收 `principal` / `fund_name`（传入会被忽略）。

- `holding_mode=MIN_HOLDING`（持有期）：返回 `first_redeem_date = 购买日期 + 持有周期`，`next_redeem_date` 为 `null`；
- `holding_mode=ROLLING`（滚动持有）：额外返回 `next_redeem_date`，初值等于 `first_redeem_date`，
  之后由每日维护按持有周期推进；
- `expected_annual_yield_rate` 为定点整数（`35000 = 3.5%`），可空；
- 返回体中的 `bond_fund` 还带两个**只读派生字段**（由后端按业务时区计算，前端直接展示即可）：
  - `status`：`LOCKED`（锁定）/ `REDEEMABLE`（持有期已满足）/ `REDEEMABLE_TODAY`（今日可赎回）/
    `PENDING`（滚动型已过赎回日、等待每日维护推进）/ `UNKNOWN`；
  - `days_until_redeem`：距可赎回日的天数（有符号整数，可空）。

> 添加时维护：创建滚动债基（`ROLLING`）或自动续存定存（`auto_rollover=true`）时，若按上述规则
> 算出的 `next_redeem_date` / `maturity_date` 已过期，可传 `maintain_on_create: true`，
> 服务端会在写入前按与每日维护相同的规则把日期推进到当前周期，避免「新建即 `PENDING`」。
> 前端可先调用下面的 `POST .../assets/maintenance-preview` 预览将要产生的推进，并向用户确认。

债券 / 保险同理，使用 `bond` / `insurance` 块。

负债（`opening_balance` 必须 ≤ 0）：

```json
{ "account_id": 2, "name": "信用卡", "asset_type": "LIABILITY", "opening_balance": 0 }
```

### `POST /api/households/{id}/assets/maintenance-preview`

创建前的「添加时维护」预览：请求体与 `POST /api/households/{id}/assets` 相同（至少含
`asset_type` 与对应明细块），只读取入参、不落库、不校验账户。返回是否需要维护及推进前后值：

```json
{
  "required": true,
  "asset_type": "BOND_FUND",
  "changes": [
    { "field": "next_redeem_date", "before": "2026-06-05", "after": "2026-12-02" }
  ]
}
```

`field` 取值：`next_redeem_date`（滚动债基）或 `start_date` / `maturity_date`（自动续存定存）。
前端在创建前调用本接口，`required=true` 时弹框让用户确认，确认后再带
`maintain_on_create: true` 提交创建；用户取消则放弃本次新增。

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
  "fund": { "fund_code": "000001", "fund_type": "BOND" }
}
```

`detail_type=BOND_FUND` 时使用 `bond_fund` 块，字段与创建一致。编辑时若不传
`next_redeem_date`，服务端会沿用数据库中已被每日维护推进的值，不会重置回首期。

### `DELETE /api/assets/{id}`

删除资产，成功返回 `data: null`。其名下全部流水与明细块会级联删除，不可恢复。

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

### `DELETE /api/transactions/{id}`

删除流水并回滚资产余额，返回 `{"deleted": 1}`。若该流水属于某次转账
（`transfer_group_id` 非空），会同组删除配对的两条，返回 `{"deleted": 2}`。

---

## 维护 Maintenance

每日维护让「与时间相关的状态」收敛到当前业务日期应有的状态（滚动债基下一赎回日、
自动续存定存的本期起止日期）。除启动补跑与每天 0 点调度外，还支持手动预览与触发。

### `GET /api/maintenance/preview`

预览按当前业务日期执行维护将产生的变更，**不落库**：

```json
{
  "business_date": "2026-09-23",
  "required": true,
  "changes": [
    {
      "asset_id": 1,
      "asset_name": "滚动债基A",
      "asset_type": "BOND_FUND",
      "field": "next_redeem_date",
      "before": "2026-06-05",
      "after": "2026-12-02"
    }
  ]
}
```

### `POST /api/maintenance/run`

强制执行一次每日维护（幂等），返回各类被推进的资产条数：

```json
{ "business_date": "2026-09-23", "bond_funds": 1, "term_deposits": 1 }
```

维护规则与启动补跑、零点调度完全一致（滚动债基 `today > next_redeem_date` 才推进；
自动续存定存 `today >= maturity_date` 即续期），整个任务在一个事务内完成并更新
`system_state.daily_maintenance_last_run`。

---

## 统计 Statistics

### `GET /api/households/{id}/statistics/overview?year=2026&month=9`

`year` / `month` 缺省时取后端业务日期的当前年月；统计区间按业务时区边界计算。

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

`from` / `to` 按业务时区（`business_timezone`）本地时间解释，后端换算成 UTC 后匹配
以 UTC 存储的 `transaction_time`；返回的 `from` / `to` 原样回显。

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

### `GET /api/households/{id}/statistics/monthly?months=6`

近 N 个月收支趋势（`months` 缺省 6，范围 1..36）。按月升序返回，缺月补零：

```json
[
  { "month": "2026-04", "income": 0, "expense": 0, "balance": 0 },
  { "month": "2026-05", "income": 1000000, "expense": 80000, "balance": 920000 }
]
```

---

## 前端静态资源（可选）

当 `config.json` 中 `frontend.enabled=true` 时，后端同时托管 `frontend/dist`：

- `GET /` 返回 `index.html`；
- 未匹配到文件的非 API 路径回退到 `index.html`（SPA 路由）；
- `/api/*` 始终由 API 处理。
