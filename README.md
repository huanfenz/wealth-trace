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
├── migrations/                 # SQL migration（001_init.sql）
├── data/                       # SQLite 数据库文件（运行时生成）
├── docs/                       # API 与设计问题说明
└── scripts/                    # 冒烟测试脚本
```

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

## 4. 后端构建与运行

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
5. 注册 REST 路由并启动 Crow；
6. 若 `frontend.enabled=true` 且已构建前端，则同时托管 `frontend/dist`（SPA 回退到 `index.html`）。

### 数据库初始化与 Migration

- 数据库文件默认 `data/caiji.db`，可通过配置修改；
- 表结构全部由 `migrations/*.sql` 管理，不在业务代码中散落建表 SQL；
- 迁移记录表：`schema_migration(version, name, applied_at)`；
- 迁移文件命名：`<版本号>_<描述>.sql`，按版本升序执行；
- 每个迁移在独立事务中执行，失败即整体回滚并停止启动。

---

## 5. 前端启动

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

## 6. 测试

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

## 7. 配置说明（config.json）

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
| `categories.expense` | 默认支出分类 | 餐饮/交通/... |
| `categories.income` | 默认收入分类 | 工资/奖金/... |

配置文件路径可通过命令行参数或环境变量 `WEALTH_TRACE_CONFIG` 指定：

```bash
./build/backend/wealth-trace /path/to/config.json
WEALTH_TRACE_CONFIG=/path/to/config.json ./build/backend/wealth-trace
```

---

## 8. 金额 / 利率 / 时间约定

全项目统一，不允许混用：

- **币种**：V1 仅支持人民币（CNY），数据库中不保存币种字段，不做汇率换算。
- **金额**：最小货币单位整数（人民币 1 元 = 100 分），C++ `std::int64_t`，SQLite `INTEGER`，API 也使用整数分。
- **利率**：定点整数，`RATE_SCALE = 1000000`。例如 1.85% 存为 `18500`。
- **时间**：UTC，`YYYY-MM-DD HH:MM:SS`；日期 `YYYY-MM-DD`。
- 负债的 `current_balance` 为负数；净资产 = `SUM(asset.current_balance)`。

前端在展示时把「分」换算成「元」，金额输入换算成「分」后提交。

---

## 9. API 简介

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
| GET/POST | `/api/households` | 家庭列表/创建 |
| GET/PUT | `/api/households/{id}` | 家庭详情/更新 |
| GET/POST | `/api/households/{id}/members` | 成员列表/创建 |
| GET/PUT | `/api/members/{id}` | 成员详情/更新 |
| GET/POST | `/api/households/{id}/accounts` | 账户列表（含余额）/创建 |
| GET/PUT | `/api/accounts/{id}` | 账户详情（含余额）/更新 |
| GET/POST | `/api/households/{id}/assets` | 资产列表/创建（可带明细） |
| GET/PUT | `/api/assets/{id}` | 资产详情/更新 |
| PUT | `/api/assets/{id}/status` | 启用/关闭资产 |
| PUT | `/api/assets/{id}/detail` | 更新资产专有明细 |
| GET | `/api/households/{id}/transactions` | 流水查询（成员/资产/类型/时间） |
| POST | `/api/households/{id}/transactions/income` | 记账收入 |
| POST | `/api/households/{id}/transactions/expense` | 记账支出 |
| POST | `/api/households/{id}/transactions/adjustment` | 余额调整 |
| POST | `/api/households/{id}/transfers` | 转账（两条流水） |
| GET | `/api/transactions/{id}` | 单条流水 |
| GET | `/api/households/{id}/statistics/overview` | 家庭总览 |
| GET | `/api/households/{id}/statistics/period` | 区间收支统计 |

---

## 10. 设计文档与实现说明

- 需求与总体设计：[`家庭资产管理系统-需求分析与总体设计-V1.md`](./家庭资产管理系统-需求分析与总体设计-V1.md)
- 实现中发现的设计问题与处理：[`docs/设计问题说明.md`](./docs/设计问题说明.md)
- API 明细：[`docs/API.md`](./docs/API.md)

> V1 范围说明：不包含复杂权限、私人账单、复式记账、基金份额/净值、行情同步、AI 记账等。
