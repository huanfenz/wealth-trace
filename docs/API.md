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

## 数据库备份

### `GET /api/database/export`

下载包含在线 WAL 已提交数据的一致 SQLite 快照，响应为 `application/vnd.sqlite3`，文件名为 `wealth-trace.db`。

### `POST /api/database/import`

请求体为 SQLite `.db` 文件原始字节（`Content-Type: application/vnd.sqlite3`），最大 100 MiB。服务端校验数据库完整性、外键和迁移版本；旧版本会先迁移。导入成功后整体替换当前数据库，并在数据库目录的 `backups/` 留存覆盖前快照。

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
    "asset_types": ["CASH", "TERM_DEPOSIT", "STOCK_FUND", "BOND_FUND", "FLEXIBLE_TERM", "COMMERCIAL_PENSION", "INSURANCE", "LIABILITY", "OTHER"],
    "asset_statuses": ["ACTIVE", "CLOSED"],
    "transaction_types": ["INCOME", "EXPENSE", "TRANSFER", "INVESTMENT", "ADJUSTMENT"],
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

`income_categories` / `expense_categories` 是配置文件默认值的兼容字段。运行中的家庭分类请使用下方分类管理 API。

---

## 家庭 Household

### `GET /api/households`

返回家庭数组。

### `POST /api/households`

```json
{ "name": "我的家庭" }
```

## 收支分类

分类按家庭和收支类型隔离。查询分类时会把配置中的默认值补入该家庭；停用分类仍保留历史流水。

### `GET /api/households/{id}/categories?type=INCOME&include_inactive=false`

`type` 必须是 `INCOME` 或 `EXPENSE`。默认只返回启用分类；管理页可传 `include_inactive=true`。

### `POST /api/households/{id}/categories`

```json
{ "type": "EXPENSE", "name": "宠物" }
```

### `PUT /api/households/{household_id}/categories/{id}`

```json
{ "name": "宠物用品", "sort_order": 3 }
```

改名会同步更新历史流水显示名称和统计分组。

### `PUT /api/households/{household_id}/categories/{id}/status`

```json
{ "active": false }
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

返回资产数组，按 `asset_type` 附带对应明细块（`term_deposit` / `stock_fund` / `bond_fund` / `insurance`），
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
  "stock_fund": null,
  "bond_fund": null,
  "insurance": null,
  "created_at": "2026-09-19 10:00:00",
  "updated_at": "2026-09-19 10:00:00"
}
```

### `POST /api/households/{id}/assets`

创建资产。`account_id` 决定 `household_id` 与 `owner_member_id`（冗余字段自动从账户派生）。
可选传 `payment_asset_id`，从同一家庭的一项有效资产支付 `opening_balance`。
新资产仍以该金额作为期初本金；服务端会在同一事务中创建资产、扣除付款资产余额，
提供 `payment_asset_id` 时，会创建一笔 `INVESTMENT/BUY` 交易，在付款资产上写 `OUT` Entry、在新资产上写 `IN` Entry。付款资产须有足够余额，且不能是负债；
受转出限制的定活理财、商业养老金也须满足其转出条件。留空则直接创建金额，
不扣除其他资产。该流水不计入收入或支出统计。

现金/活期：

```json
{
  "account_id": 1,
  "name": "活期",
  "asset_type": "CASH",
  "opening_balance": 2000000,
  "payment_asset_id": null,
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
明细不再保存 `principal`，本金以资产的 `opening_balance` 为准；产品名称以 `asset.name` 为准。

股票基金：

```json
{
  "account_id": 1,
  "name": "某股票基金",
  "asset_type": "STOCK_FUND",
  "opening_balance": 5000000,
  "stock_fund": { "fund_code": "000001", "lock_end_date": "2027-03-01" }
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

定活理财（`asset_type=FLEXIBLE_TERM`）必须提供 `flexible_term` 明细。`holding_period_days`
仅支持 `180` 或 `360`，日期按申购确认日后的自然日计算：

```json
{
  "account_id": 1,
  "name": "180天定活理财",
  "asset_type": "FLEXIBLE_TERM",
  "opening_balance": 1000000,
  "flexible_term": { "purchase_date": "2026-09-23", "holding_period_days": 180 }
}
```

申购确认日满 30 个自然日后，每月 5 日可转出；持有期满当日起每天可转出。
返回明细含只读 `maturity_date`、`next_transfer_date`、`can_transfer`。
转账接口按服务端当前业务日期校验定活理财转出，非开放日返回冲突错误。

商业养老金（`asset_type=COMMERCIAL_PENSION`）必须提供 `commercial_pension` 明细。
买入时间与预约赎回提醒范围采用业务时区的 `YYYY-MM-DD HH:MM:SS`；持有周期由正整数
`holding_period_value` 和 `holding_period_unit`（`DAY` / `MONTH` / `YEAR`）组成。

```json
{
  "account_id": 1,
  "name": "商业养老金",
  "asset_type": "COMMERCIAL_PENSION",
  "opening_balance": 1000000,
  "commercial_pension": {
    "purchase_time": "2026-09-23 10:00:00",
    "holding_period_value": 1,
    "holding_period_unit": "YEAR",
    "reservation_window_start": "2027-08-01 00:00:00",
    "reservation_window_end": "2027-08-31 23:59:59"
  }
}
```

新建时默认到期续期。预约范围只用于提醒：开始前为 `UPCOMING`，范围内为 `OPEN`，
结束后为 `ENDED`；未设置为 `NOT_SET`。它不限制用户修改到期处理方式。
创建时或之后用 `PUT /api/assets/{id}/detail` 将 `redeem_at_maturity` 设为 `true`，
系统锁定下一次到期时间；到期后显示 `MATURED`，并允许从该资产转账。
未选择到期赎回时，按原到期时间连续续期，下一到期时间实时计算。
返回明细还包含只读 `maturity_time`、`reservation_status`、`status`。到期赎回不会自动生成流水，
实际转出需用户发起转账。

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

### `PUT /api/assets/{id}/balance`

资产管理中手动设置当前余额且不产生交易记录：

```json
{ "current_balance": 2500000 }
```

接口按余额差额同步修正 `opening_balance`。若用户选择产生记录，前端应改为调用
`POST /api/households/{household_id}/transactions/adjustment`，并传入差额作为 `amount`。

### `PUT /api/assets/{id}/status`

```json
{ "status": "CLOSED" }
```

关闭后不计入统计，且不能新增交易。

### `PUT /api/assets/{id}/detail`

更新专有明细，`detail_type` 必须与资产类型一致：

```json
{
  "detail_type": "STOCK_FUND",
  "stock_fund": { "fund_code": "000001" }
}
```

`detail_type=BOND_FUND` 时使用 `bond_fund` 块，字段与创建一致。编辑时若不传
`next_redeem_date`，服务端会沿用数据库中已被每日维护推进的值，不会重置回首期。

### `DELETE /api/assets/{id}`

删除无交易 Entry 的资产，成功返回 `data: null`。有交易历史的资产只能关闭，不能硬删除。

---

## 交易 Transaction

### `GET /api/households/{id}/transactions`

查询参数：

| 参数 | 说明 |
| --- | --- |
| `owner_member_id` | 按成员过滤 |
| `asset_id` | 按资产过滤 |
| `type` | INCOME/EXPENSE/TRANSFER/INVESTMENT/ADJUSTMENT |
| `from` / `to` | 时间范围（`YYYY-MM-DD HH:MM:SS`，含端点） |
| `limit` | 1..1000，默认 200 |
| `offset` | 默认 0 |

返回：

```json
{ "total": 1, "items": [ { "id": 1, "type": "TRANSFER", "title": "转账", "subtitle": "银行卡 → 支付宝", "amount": 500000, "direction": "NEUTRAL" } ] }
```

列表按业务交易计数和分页。轻量交易 DTO 包含 `title`、`subtitle`、`amount` 和展示方向；转账只出现一次。收支方向为 `IN` / `OUT`，转账和投资买入为 `NEUTRAL`。

### `GET /api/assets/{asset_id}/transactions?household_id={id}`

资产视角流水。转账会按当前资产对应 Entry 展示 `IN` 或 `OUT`，同时在副标题中给出对端资产名称。

### `POST /api/households/{id}/transactions`

通用交易写入接口。`entries` 中金额为正整数分，方向相对 Entry 对应资产：

```json
{
  "type": "EXPENSE",
  "category_id": 2,
  "entries": [{ "asset_id": 10, "direction": "OUT", "amount": 3500 }],
  "transaction_time": "2026-09-26 12:30:00",
  "remark": "午餐"
}
```

收入、支出和调整要求一个 Entry；转账与买入要求两个资产不同、方向相反且金额相等的 Entries。现有收入、支出、转账和调整专用接口保留为业务化封装。

### `POST /api/households/{id}/transactions/income`

```json
{ "asset_id": 1, "category_id": 1, "amount": 1000000, "transaction_time": "", "remark": "" }
```

### `POST /api/households/{id}/transactions/expense`

```json
{ "asset_id": 1, "category_id": 2, "amount": 3500 }
```

### `POST /api/households/{id}/transactions/adjustment`

`amount` 可正可负：

```json
{ "asset_id": 3, "amount": 100000, "remark": "股票基金估值调整" }
```

### `POST /api/households/{id}/transfers`

一次转账生成一个 `TRANSFER` Transaction 和两条 Entry：

```json
{
  "from_asset_id": 1,
  "to_asset_id": 2,
  "amount": 500000,
  "transaction_time": "",
  "remark": "活期转定期"
}
```

返回一个交易 DTO，方向为 `NEUTRAL`，详情中包含两条 Entry。

### `POST /api/households/{id}/investments/buy`

请求字段与转账一致，执行投资 `BUY`，记录 `INVESTMENT` 交易并关联投资资产明细。该类型不计入普通支出。
资金来源为定活理财或商业养老金时，同样遵守该资产的转出/赎回日期限制。

### `GET /api/transactions/{id}`

返回完整交易 DTO 和 Entries；`editable` 表示交易能否编辑。已关联定投执行记录的投资交易为 `false`。

### `PUT /api/transactions/{id}`

按创建接口的 `type` / `category_id` / `entries` / `transaction_time` / `remark` 更新交易。原交易的类型和投资动作不可更改。旧交易只有分类名称、没有分类 ID 时，可传 `preserve_legacy_category: true` 保留该名称；否则 `category_id: null` 会清空分类。`rollback_assets` 查询参数默认 `true`：先撤销旧 Entries 对余额的影响，再应用新 Entries；设为 `false` 时更新 Entries 但保留资产当前余额，新 Entries 的余额快照为空。金额、方向及资产均未变化时只更新交易信息，保留原 Entries 和余额快照。更新在同一 SQLite 事务中完成。
已关联成功定投执行记录的交易不能通过此接口编辑，以保持执行历史与交易一致。

### `PUT /api/transactions/{id}/category`

只修改收入或支出交易的分类，返回更新后的 DTO。请求体为 `{"category_id": 1}`；
传入 `null` 可清空分类。分类必须启用，且属于同一家庭及相同的收支类型。
Entry、历史余额快照和资产当前余额不变；转账、投资、余额调整不能通过此接口修改分类。

### `DELETE /api/transactions/{id}`

删除一笔完整交易，返回 `{"deleted": 1}`。`rollback_assets` 默认 `true` 并回滚所有 Entry 造成的余额变化；设为 `false` 时删除交易和 Entries、保留资产当前余额。定投执行记录通过 `transaction_id` 关联；回滚删除时标记为 `REVERSED`，保留余额删除时执行记录仍为 `SUCCESS` 且关联置空。

---

## 定投 Recurring investment

### `GET /api/households/{id}/investment-plans`

返回家庭定投计划。状态包括 `ACTIVE`、`PAUSED`、`DELETED`；删除为软删除，历史记录保留。

### `POST /api/households/{id}/investment-plans` / `PUT /api/investment-plans/{id}`

创建或编辑计划，金额单位为分。目标必须是活跃 `STOCK_FUND`；付款资产必须和目标属于同一家庭、同一成员，且为活跃非负债资产。

```json
{
  "target_asset_id": 12,
  "source_asset_id": 3,
  "amount": 50000,
  "frequency": "BIWEEKLY",
  "weekday": 1,
  "month_day": null,
  "start_date": "2026-10-05"
}
```

`frequency` 为 `DAILY` / `WEEKLY` / `BIWEEKLY` / `MONTHLY`；周频率的 `weekday` 使用 ISO 星期值 1～7（周一至周日），月频率的 `month_day` 为 1～28。当天符合计划时，创建或恢复计划会立即尝试执行。

### `PUT /api/investment-plans/{id}/status`

```json
{ "status": "PAUSED" }
```

状态可设为 `ACTIVE` 或 `PAUSED`。暂停日期跳过，不在恢复时补齐。

### `POST /api/investment-plans/{id}/execute`

立即执行计划一次，生成以当前业务日期记录的执行结果与转账流水。暂停计划也可手动执行；每个计划每天只能手动或按期执行一次，重复执行返回冲突。余额不足等业务失败会作为 `FAILED` 执行记录返回，可在执行历史中查看原因。

### `DELETE /api/investment-plans/{id}`

软删除计划并停止后续执行，保留计划及历史执行记录。

### `GET /api/investment-plans/{id}/executions`

返回各预定日期的执行快照和结果：`SUCCESS`、`FAILED` 或 `REVERSED`。余额不足会记录失败原因，不产生交易。

### `POST /api/investment-executions/{id}/retry`

手动重试失败期次；交易日期保持该期的预定日期。计划已删除或期次状态不是 `FAILED` 时返回冲突。

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
