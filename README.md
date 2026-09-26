# 财迹 · wealth-trace

> 轻量级家庭资产管理与记账系统 V1

财迹用于管理一个家庭中不同成员的账户、资产、负债、收入、支出与转账，并回答四个问题：

1. 家庭的钱在哪里？（Account）
2. 这些钱属于哪个成员？（Member）
3. 这些钱是什么类型的资产？（Asset）
4. 这些钱为什么发生变化？（Transaction）

业务模型、数据库字段、转账规则与统计规则以设计文档为准：

- [`家庭资产管理系统-需求分析与总体设计-V1.md`](./家庭资产管理系统-需求分析与总体设计-V1.md)

---

## 1. 技术栈

| 层 | 技术 |
| --- | --- |
| 后端语言 | C++23 |
| 构建系统 | Meson（`cpp_std=c++23`，`warning_level=3`） |
| Web 框架 | Crow（header-only RESTful JSON API） |
| JSON | nlohmann/json |
| 数据库 | SQLite3（WAL、外键、Prepared Statement） |
| 测试 | GoogleTest + `meson test` |
| 前端 | Vue 3 + TypeScript + Vite + Vue Router + Pinia + Axios + Element Plus |

主项目使用 Meson 构建，未使用 CMake。

### 第三方依赖（已内置）

为了让项目在**没有 sudo、没有系统 `-dev` 包、甚至无法访问 GitHub** 的环境中也能直接构建，依赖已放入 `subprojects/`：

```text
subprojects/
├── asio/           # standalone asio 1.30.2（Crow 的网络层）
├── crow/           # Crow 1.2.1（header-only）
├── nlohmann_json/  # nlohmann/json 3.11.3 单头文件
├── sqlite3/        # SQLite amalgamation 3.46.1（编译为静态库）
└── googletest/     # GoogleTest 1.14.0
```

无需 `pkg-config`，也无需系统安装这些库。

---

## 2. 目录结构

```text
wealth-trace/
├── meson.build                 # 顶层构建定义
├── meson_options.txt           # enable_tests 选项
├── config.json                 # 运行配置
│
├── backend/
│   ├── meson.build
│   ├── include/                # 头文件（common/config/utils/database/model/repository/service/dto/controller）
│   ├── src/                    # 实现
│   │   ├── main.cpp
│   │   ├── common/             # 日志、统一响应、错误
│   │   ├── config/             # 配置加载
│   │   ├── utils/              # 金额、利率、时间、字符串
│   │   ├── database/           # Database / Statement / TransactionGuard / Migration
│   │   ├── model/              # 枚举与实体
│   │   ├── repository/         # SQL 访问
│   │   ├── service/            # 业务逻辑与事务
│   │   ├── dto/                # JSON 解析与序列化
│   │   └── controller/         # HTTP 路由
│   └── test/                   # GoogleTest 测试
│
├── frontend/                   # Vue 3 + TS + Vite
│   ├── src/{api,components,views,router,stores,types,utils}/
│   └── vite.config.ts          # /api 代理到后端
│
├── migrations/                 # SQL migration（001–011，含资产类型与定投计划）
├── data/                       # SQLite 数据库文件（运行时生成）
├── docs/                       # API 与设计问题说明
└── scripts/                    # 冒烟测试脚本
```

网页左侧“数据备份”可下载完整 SQLite 数据库，或导入备份并整体恢复。导入会先验证完整性和外键，自动迁移旧版备份，并在数据库目录的 `backups/` 保存覆盖前快照。上传文件上限为 100 MiB。

---

## 3. 依赖与环境

后端：

- C++ 编译器：GCC ≥ 11（需要支持 `-std=c++23`；已在 GCC 11.4 验证）
- Meson ≥ 1.3（`cpp_std=c++23` 支持），Ninja
- 第三方库已内置，无需额外安装

如果系统 Meson 过旧（例如 Ubuntu 22.04 自带 0.61），用 pip 升级：

```bash
pip3 install --user -U meson
```

前端：

- Node.js ≥ 18（已在 Node 24 验证）、npm

---

## 4. 一键构建与运行

在项目根目录执行：

```bash
# 开发：构建后端，启动后端 :8080 和 Vite :5173（支持热更新）
bash scripts/run.sh dev

# 生产：构建前后端；后端在 :8080 托管 frontend/dist
bash scripts/run.sh prod
```

也可以在模式后传入自定义配置文件：`bash scripts/run.sh prod /path/to/config.json`。
首次运行时，脚本会在缺少 `frontend/node_modules` 的情况下自动安装前端依赖。开发模式访问 `http://127.0.0.1:5173`；生产模式访问 `http://127.0.0.1:8080`。按 `Ctrl-C` 停止服务。
启动前脚本会自动检测并停止仍占用后端/Vite 端口的旧进程，避免新服务因端口被占而启动失败。

### ARM64 交叉编译与部署

推荐使用 x86_64 Ubuntu 22.04 开发机。先准备 Node.js ≥ 18、npm、Ninja、rsync 和 Meson ≥ 1.3（Ubuntu 22.04 自带的 Meson 0.61 不够新）：

```bash
sudo apt-get install ninja-build python3-pip nodejs npm rsync
python3 -m pip install --user 'meson>=1.3'
```

若系统没有 ARM64 交叉编译器，首次执行构建脚本会用 `apt download` 将 Ubuntu 22.04 工具链装入仓库内的 `.toolchains/arm64/`，不需要 sudo；后续构建复用它。也可以提前用系统包安装 `gcc-aarch64-linux-gnu g++-aarch64-linux-gnu libc6-dev-arm64-cross`。首次构建缺少前端依赖时，脚本会运行 `npm ci`。

目标服务器需要 ARM64 Linux、systemd、Python 3、`curl`、`tar`、`rsync`，以及可免密登录的 root（或可执行 `sudo -n` 的账号）。前后端都在本机构建，目标机不需要 Node.js、Meson 或 C++ 编译器。交叉编译器使用 Ubuntu 22.04 的 ARM64 运行库；目标机应使用兼容的 Linux/运行库版本，当前目标机为 ARM64 Ubuntu 22.04。

```bash
# 只构建：生成 dist/wealth-trace-linux-arm64.tar.gz
bash scripts/build-arm64.sh

# 一键构建并部署到默认目标 root@192.168.50.142:8081
bash scripts/deploy-arm64.sh

# 使用已有构建包部署到指定目标
bash scripts/deploy-arm64.sh --skip-build --host 192.168.50.142 --user root --port 8081
```

若通过域名连接服务器，另用 `--bind` 指定服务器上的 IPv4 监听地址；例如 `--host my-server.local --bind 192.168.50.142`。

脚本在目标机的 `/opt/wealth-trace/releases/` 保存每次发布的完整后端程序、前端静态文件和数据库迁移文件；`/opt/wealth-trace/current` 指向当前版本。systemd 服务使用专用 `wealthtrace` 账号，数据库保存在 `/var/lib/wealth-trace/caiji.db`，服务器配置保存在 `/etc/wealth-trace/config.json`。每次发布前会将现有 SQLite 数据库备份到 `/var/lib/wealth-trace/backups/`，启动后检查健康接口；启动失败会恢复之前的程序及服务配置。数据库迁移一旦执行，旧程序可能不兼容新结构，此时应结合备份处理数据回退。

查看服务状态：`ssh root@192.168.50.142 systemctl status wealth-trace.service`。默认访问地址：`http://192.168.50.142:8081/`。当前应用没有登录鉴权，请只在可信网络内开放此端口。

---

## 5. 后端构建与运行

```bash
# 1. 配置
meson setup build

# 2. 编译
meson compile -C build

# 3. 运行（首次启动会自动创建数据库并执行 migration）
./build/backend/wealth-trace config.json
```

默认监听 `http://127.0.0.1:8080`。

> 工作目录应为项目根目录，因为 `config.json` 中的 `database.migrations_dir` 默认为相对路径 `migrations`。

`main.cpp` 启动流程：

1. 读取 `config.json`；
2. 打开 SQLite 数据库并设置 `PRAGMA foreign_keys=ON`、`journal_mode=WAL`、`busy_timeout`；
3. 检查并执行尚未执行的 migration；
4. 若不存在家庭，创建默认家庭「我的家庭」；
5. 执行每日资产维护补跑（`system_state.daily_maintenance_last_run` 不是当天时）；
6. 注册 REST 路由并启动 Crow，同时启动每日维护调度线程（等到下一个业务时区 0 点执行）；
7. 若 `frontend.enabled=true` 且已构建前端，则同时托管 `frontend/dist`（SPA 回退到 `index.html`）。

### 数据库初始化与 Migration

- 数据库文件默认 `data/caiji.db`，可通过配置修改；
- 表结构全部由 `migrations/*.sql` 管理，不在业务代码中散落建表 SQL；
- 迁移记录表：`schema_migration(version, name, applied_at)`；
- 迁移文件命名：`<版本号>_<描述>.sql`，按版本升序执行；
- 每个迁移在独立事务中执行，失败即整体回滚并停止启动。

---

## 6. 前端启动

开发模式（Vite dev server，自动把 `/api` 代理到 `127.0.0.1:8080`）：

```bash
cd frontend
npm install
npm run dev          # http://127.0.0.1:5173
```

生产构建（构建产物由后端静态托管）：

```bash
cd frontend
npm run build        # 生成 frontend/dist
# 然后启动后端，访问 http://127.0.0.1:8080
```

> 若 npm 被配置了不可用的代理（例如 `~/.npmrc` 中的 `proxy`），`npm install` 会失败。
> 可临时使用：`npm_config_proxy= npm_config_https_proxy= npm install`。

---

## 7. 测试

```bash
meson test -C build
```

测试覆盖金额/利率换算、时间格式、数据库与 migration、外键、事务回滚，以及核心财务流程：

- 收入/支出对余额的影响
- 转账生成两条 `TRANSFER_OUT`/`TRANSFER_IN` 且 `transfer_group_id` 一致
- 跨家庭转账被拒绝
- 余额调整可正可负
- 关闭资产后禁止交易
- 负债支出使余额变负
- 统计（总资产/总负债/净资产/本月收支）
- 账户存在资产时禁止变更属主
- 有交易后禁止修改初始金额
- 明细类型必须与资产类型一致

冒烟脚本（需要先构建后端）：

```bash
bash scripts/run_api_smoke.sh    # 启动后端 + 跑完整 API 收支/转账/统计流程
bash scripts/dev_smoke.sh        # 后端 + Vite 开发服务器联调
bash scripts/static_smoke.sh     # 验证后端静态托管前端
```

---

## 8. 配置说明（config.json）

| 字段 | 说明 | 默认 |
| --- | --- | --- |
| `server.host` | 监听地址 | `127.0.0.1` |
| `server.port` | 监听端口 | `8080` |
| `server.threads` | 工作线程数（Crow concurrency，最小 2） | `4` |
| `database.path` | SQLite 文件路径 | `data/caiji.db` |
| `database.migrations_dir` | migration 目录 | `migrations` |
| `database.busy_timeout_ms` | 忙等待超时 | `5000` |
| `database.wal` | 是否启用 WAL | `true` |
| `log.level` | 日志级别 trace/debug/info/warn/error/critical | `info` |
| `frontend.enabled` | 是否托管前端构建产物 | `true` |
| `frontend.dir` | 前端构建目录 | `frontend/dist` |
| `business_timezone` | 业务时区（IANA 名称），影响业务日期与每日维护调度 | `Asia/Shanghai` |
| `categories.expense` | 新家庭的初始支出分类 | 餐饮/交通/... |
| `categories.income` | 新家庭的初始收入分类 | 工资/奖金/... |

分类保存于数据库，可在应用的“分类管理”页面新增、改名和停用。配置文件分类仅作为初始模板。

配置文件路径可通过命令行参数或环境变量 `WEALTH_TRACE_CONFIG` 指定：

```bash
./build/backend/wealth-trace /path/to/config.json
WEALTH_TRACE_CONFIG=/path/to/config.json ./build/backend/wealth-trace
```

---

## 9. 金额 / 利率 / 时间约定

全项目统一，不允许混用：

- **币种**：V1 仅支持人民币（CNY），数据库中不保存币种字段，不做汇率换算。
- **金额**：最小货币单位整数（人民币 1 元 = 100 分），C++ `std::int64_t`，SQLite `INTEGER`，API 也使用整数分。
- **利率**：定点整数，`RATE_SCALE = 1000000`。例如 1.85% 存为 `18500`。
- **时间**：审计时间戳以 UTC 存储，`YYYY-MM-DD HH:MM:SS`；日期 `YYYY-MM-DD`。
- **业务时区**：`business_timezone`（默认 `Asia/Shanghai`）决定「业务日期」口径，
  用于债券基金赎回日推进、可赎回状态判定与统计的「自然月」；查询区间按业务时区边界
  换算成 UTC 后匹配 `transaction_time`。前端不自行推算，统一使用后端返回的结果。
- 负债的 `current_balance` 为负数；净资产 = `SUM(asset.current_balance)`。

前端在展示时把「分」换算成「元」，金额输入换算成「分」后提交。

---

## 10. API 简介

统一响应结构：

```json
{ "code": 0, "message": "success", "data": {} }
```

失败：

```json
{ "code": 40001, "message": "invalid request", "data": null }
```

主要接口（完整列表见 [`docs/API.md`](./docs/API.md)）：

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/api/health` | 健康检查 |
| GET | `/api/meta` | 枚举与默认分类 |
| GET | `/api/database/export` | 下载完整 SQLite 数据库备份 |
| POST | `/api/database/import` | 校验并整体导入数据库备份（最大 100 MiB） |
| GET/POST | `/api/households` | 家庭列表/创建 |
| GET/PUT | `/api/households/{id}` | 家庭详情/更新 |
| GET/POST | `/api/households/{id}/members` | 成员列表/创建 |
| GET/PUT | `/api/members/{id}` | 成员详情/更新 |
| GET/POST | `/api/households/{id}/accounts` | 账户列表（含余额）/创建 |
| GET/PUT/DELETE | `/api/accounts/{id}` | 账户详情（含余额）/更新/删除 |
| GET/POST | `/api/households/{id}/assets` | 资产列表/创建（可带明细） |
| POST | `/api/households/{id}/assets/maintenance-preview` | 创建资产时的「添加时维护」预览 |
| GET/PUT | `/api/assets/{id}` | 资产详情/更新 |
| DELETE | `/api/assets/{id}` | 删除资产（级联删除流水与明细） |
| PUT | `/api/assets/{id}/status` | 启用/关闭资产 |
| PUT | `/api/assets/{id}/detail` | 更新资产专有明细 |
| GET | `/api/households/{id}/transactions` | 流水查询（成员/资产/类型/时间） |
| POST | `/api/households/{id}/transactions/income` | 记账收入 |
| POST | `/api/households/{id}/transactions/expense` | 记账支出 |
| POST | `/api/households/{id}/transactions/adjustment` | 余额调整 |
| POST | `/api/households/{id}/transfers` | 转账（两条流水） |
| GET/POST | `/api/households/{id}/investment-plans` | 定投计划列表 / 创建 |
| PUT/DELETE | `/api/investment-plans/{id}` | 更新 / 删除定投计划 |
| PUT | `/api/investment-plans/{id}/status` | 暂停 / 恢复定投计划 |
| POST | `/api/investment-plans/{id}/execute` | 立即执行一次定投 |
| GET | `/api/investment-plans/{id}/executions` | 定投执行历史 |
| POST | `/api/investment-executions/{id}/retry` | 重试失败定投期次 |
| GET | `/api/transactions/{id}` | 单条流水 |
| DELETE | `/api/transactions/{id}` | 删除流水（回滚余额，转账成对删除） |
| GET | `/api/households/{id}/statistics/overview` | 家庭总览 |
| GET | `/api/households/{id}/statistics/period` | 区间收支统计 |
| GET | `/api/households/{id}/statistics/monthly` | 近 N 个月收支趋势 |
| GET | `/api/maintenance/preview` | 每日维护变更预览（不落库） |
| POST | `/api/maintenance/run` | 手动执行每日维护（幂等） |

---

## 11. 设计文档与实现说明

- MCP 接入：[`docs/MCP.md`](./docs/MCP.md)
- 需求与总体设计：[`家庭资产管理系统-需求分析与总体设计-V1.md`](./家庭资产管理系统-需求分析与总体设计-V1.md)
- 实现中发现的设计问题与处理：[`docs/设计问题说明.md`](./docs/设计问题说明.md)
- API 明细：[`docs/API.md`](./docs/API.md)

债券基金（`BOND_FUND`）：通过 `holding_mode` 区分持有期（`MIN_HOLDING`）与滚动持有（`ROLLING`）。
`first_redeem_date` / `next_redeem_date` 由服务端按「购买日期 + 持有周期」计算并允许手工修正；
滚动型的下一赎回日由每日维护推进（`today > next_redeem_date` 才滚动，当天仍是有效赎回日）。
持有状态不落库，由后端按业务时区（默认 `Asia/Shanghai`）实时推导并随资产 JSON 返回
（`status` / `days_until_redeem`）。详见 [`docs/设计问题说明.md`](./docs/设计问题说明.md) 第 14–16 节。

维护时机共有四处，规则统一（见 [`docs/设计问题说明.md`](./docs/设计问题说明.md) 第 19 节）：
启动补跑、每天 0 点调度、新增资产时的「添加时维护」（`maintain_on_create`，前端预览并确认后传入）、
以及资产页的「手动维护」按钮（`/api/maintenance/preview` + `/api/maintenance/run`）。

定期存款（`TERM_DEPOSIT`）：`start_date` / `term_value` / `term_unit` 必填，`maturity_date`
由后端按自然月 / 自然年计算（目标月无对应日取月末），也可手工修正。开启自动续存的存款，
每日维护在到期当天（`today >= maturity_date`，与滚动债基的 `>` 不同）推进到下一存期并同步
`start_date`；到期状态同样不落库，实时推导并返回 `status` / `days_until_maturity`。
详见 [`docs/设计问题说明.md`](./docs/设计问题说明.md) 第 17 节。

定活理财（`FLEXIBLE_TERM`）：选择 180 或 360 天持有期。申购确认满 30 个自然日后，
每月 5 日开放转出；持有期满后每天可转出。资产页显示下一次可转出日期，转账接口校验开放日。

商业养老金（`COMMERCIAL_PENSION`）：填写买入时间及持有周期（天、月、年），默认到期续期。
可设置预约赎回的日期时间提醒范围；它不限制修改到期处理方式。用户可选择到期赎回，
届时停止续期并允许转出。

> V1 范围说明：不包含复杂权限、私人账单、复式记账、股票基金份额/净值、行情同步、AI 记账等。
