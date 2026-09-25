# MCP 接入

项目提供本地 stdio MCP 服务，Agent 可通过具名工具操作现有 REST API，并读取只读资源或使用提示模板。MCP 服务不会自行启动后端；使用前先按 README 启动财迹后端。

## 构建

需要 Node.js 20 或更高版本。

```bash
cd mcp
npm ci
npm run build
npm start
```

MCP 协议消息独占 stdout，启动错误和运行诊断写入 stderr。进程应由 MCP Host 启动并保持运行。

## Agent 配置

下面示例适用于支持 `mcpServers` 配置的客户端。将工作目录改为本项目的绝对路径：

```json
{
  "mcpServers": {
    "wealth-trace": {
      "command": "node",
      "args": ["/绝对路径/wealth-trace/mcp/dist/index.js"],
      "cwd": "/绝对路径/wealth-trace"
    }
  }
}
```

默认 API 地址是 `http://127.0.0.1:8080/api`。如后端使用其他地址，可在 MCP Host 的进程环境中设置 `WEALTH_TRACE_API_URL`，值应包含 `/api`，例如 `http://127.0.0.1:8080/api`。

当前后端没有认证机制；此 stdio 服务面向本机 Agent，连接本机后端。

## 暴露能力

### 工具

工具名使用英文 `snake_case`，参数由 MCP 输入 Schema 校验。工具覆盖现有 REST API：

- `check_backend`、`get_metadata`；家庭与成员的查询、创建和更新；
- 账户与资产的查询、创建、更新、状态切换、专有明细维护和删除；
- 收入、支出、余额调整、转账、流水查询与删除；
- 家庭总览、区间和月度统计；
- 定投计划的查询、创建、更新、暂停/恢复、软删除、立即执行、执行历史和失败重试；
- 每日维护预览与执行。

不会提供 REST API 以外的任意 URL 调用工具。涉及删除、直接设置资产余额、执行定投或维护的工具会标注副作用。MCP 服务不会自动重试工具调用，避免重放写操作。记账和转账提示要求先核对字段，并在写入前向用户确认。

金额参数遵循 REST API 约定，单位为人民币分；利率沿用后端的定点整数约定。日期时间格式和资产字段要求由 Schema 描述，并最终由后端业务校验。

### 资源

| URI | 内容 |
| --- | --- |
| `wealth-trace://meta` | 枚举、分类、金额单位和业务日期 |
| `wealth-trace://households` | 家庭列表 |
| `wealth-trace://households/{household_id}/overview` | 家庭总览 |
| `wealth-trace://households/{household_id}/members` | 家庭成员 |
| `wealth-trace://households/{household_id}/accounts` | 家庭账户及余额 |
| `wealth-trace://households/{household_id}/assets` | 家庭资产 |

### 提示

- `review_household_finances`：汇总家庭财务现状，不写入数据。
- `record_income_or_expense`：指导 Agent 核对并记录收支。
- `reconcile_transfer`：指导 Agent 核对转账双方和金额。

## 错误处理

参数校验由 MCP Schema 和后端共同完成。后端失败会作为 MCP 工具错误返回，并带有可用的 HTTP 状态和业务错误码。无法连接后端时，错误会提示检查后端进程或 `WEALTH_TRACE_API_URL`。
