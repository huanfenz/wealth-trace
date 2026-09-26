import { McpServer, ResourceTemplate } from '@modelcontextprotocol/server';
import { serveStdio } from '@modelcontextprotocol/server/stdio';
import * as z from 'zod/v4';

const apiBase = (process.env.WEALTH_TRACE_API_URL ?? 'http://127.0.0.1:8080/api').replace(/\/+$/, '');

type ApiEnvelope<T = unknown> = { code: number; message: string; data: T };

class ApiFailure extends Error {
  constructor(readonly status: number, readonly code: number | undefined, message: string) {
    super(message);
    this.name = 'ApiFailure';
  }
}

async function api<T = unknown>(path: string, method = 'GET', body?: unknown): Promise<T> {
  let response: Response;
  try {
    response = await fetch(`${apiBase}${path}`, {
      method,
      headers: body === undefined ? undefined : { 'Content-Type': 'application/json' },
      body: body === undefined ? undefined : JSON.stringify(body),
      signal: AbortSignal.timeout(15_000),
    });
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    throw new ApiFailure(0, undefined, `无法连接财迹后端 ${apiBase}：${detail}。请先启动后端或检查 WEALTH_TRACE_API_URL。`);
  }

  let envelope: ApiEnvelope<T>;
  try {
    envelope = await response.json() as ApiEnvelope<T>;
  } catch {
    throw new ApiFailure(response.status, undefined, `后端返回了无法解析的响应（HTTP ${response.status}）`);
  }
  if (!response.ok || envelope.code !== 0) {
    throw new ApiFailure(response.status, envelope.code, envelope.message || response.statusText);
  }
  return envelope.data;
}

type ToolOptions = {
  description: string;
  readOnly?: boolean;
  destructive?: boolean;
};

function registerTool<T extends z.ZodObject<any>>(
  server: McpServer,
  name: string,
  options: ToolOptions & { inputSchema: T },
  handler: (input: z.infer<T>) => Promise<unknown>,
) {
  const schemaShape = options.inputSchema.shape;
  server.registerTool(name, {
    description: options.description,
    inputSchema: schemaShape,
    annotations: {
      readOnlyHint: options.readOnly ?? false,
      destructiveHint: options.destructive ?? false,
      idempotentHint: options.readOnly ?? false,
      openWorldHint: false,
    },
  }, async (input: z.infer<T>) => {
    try {
      const data = await handler(input as z.infer<T>);
      return {
        content: [{ type: 'text' as const, text: JSON.stringify(data, null, 2) }],
      };
    } catch (error) {
      const message = error instanceof ApiFailure
        ? `${error.message} (HTTP ${error.status || '连接失败'}${error.code === undefined ? '' : `，业务码 ${error.code}`}）`
        : error instanceof Error ? error.message : String(error);
      return { isError: true, content: [{ type: 'text' as const, text: message }] };
    }
  });
}

const id = z.number().int().positive();
const maybeText = z.string().nullable().optional();
const empty = z.object({});
const householdId = z.object({ household_id: id });
const entityId = z.object({ id });
const memberFields = { name: z.string().min(1).max(100), role: z.enum(['OWNER', 'MEMBER']).optional(), status: z.enum(['ACTIVE', 'INACTIVE']).optional() };
const accountFields = {
  owner_member_id: id.optional(), name: z.string().min(1).max(100).optional(),
  type: z.enum(['BANK', 'ALIPAY', 'WECHAT', 'CASH', 'SECURITIES', 'INSURANCE', 'OTHER']).optional(),
  institution_name: maybeText, account_no_masked: maybeText, remark: z.string().max(500).nullable().optional(), enabled: z.boolean().optional(),
};
const transactionTime = z.string().max(19).optional();
const transactionRemark = z.string().max(500).nullable().optional();

function createServer() {
  const server = new McpServer({ name: 'wealth-trace', version: '1.0.0' });

  // Status, metadata and household operations.
  registerTool(server, 'check_backend', { description: '检查财迹后端是否可用。', inputSchema: empty, readOnly: true }, () => api('/health'));
  registerTool(server, 'get_metadata', { description: '读取枚举、收支分类、金额单位和业务日期。', inputSchema: empty, readOnly: true }, () => api('/meta'));
  registerTool(server, 'list_households', { description: '列出所有家庭。', inputSchema: empty, readOnly: true }, () => api('/households'));
  registerTool(server, 'create_household', { description: '创建家庭。', inputSchema: z.object({ name: z.string().min(1).max(100).optional() }) }, (v) => api('/households', 'POST', v));
  registerTool(server, 'get_household', { description: '读取家庭详情。', inputSchema: entityId, readOnly: true }, (v) => api(`/households/${v.id}`));
  registerTool(server, 'update_household', { description: '更新家庭名称。', inputSchema: z.object({ id, name: z.string().min(1).max(100) }) }, (v) => api(`/households/${v.id}`, 'PUT', { name: v.name }));

  // Members.
  registerTool(server, 'list_members', { description: '列出家庭成员。', inputSchema: householdId, readOnly: true }, (v) => api(`/households/${v.household_id}/members`));
  registerTool(server, 'create_member', { description: '在家庭中创建成员。', inputSchema: householdId.extend(memberFields).required({ name: true }) }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/members`, 'POST', body);
  });
  registerTool(server, 'get_member', { description: '读取成员详情。', inputSchema: entityId, readOnly: true }, (v) => api(`/members/${v.id}`));
  registerTool(server, 'update_member', { description: '更新成员姓名、角色或状态。', inputSchema: entityId.extend(memberFields).partial().required({ id: true }) }, (v) => {
    const { id: memberId, ...body } = v;
    return api(`/members/${memberId}`, 'PUT', body);
  });

  // Accounts.
  registerTool(server, 'list_accounts', { description: '列出家庭账户，可按成员筛选；金额单位为分。', inputSchema: householdId.extend({ owner_member_id: id.optional() }), readOnly: true }, (v) => {
    const query = v.owner_member_id ? `?owner_member_id=${v.owner_member_id}` : '';
    return api(`/households/${v.household_id}/accounts${query}`);
  });
  registerTool(server, 'create_account', { description: '在家庭中创建账户。', inputSchema: householdId.extend({
    owner_member_id: id, name: z.string().min(1).max(100), type: accountFields.type,
    institution_name: maybeText, account_no_masked: maybeText, remark: transactionRemark, enabled: z.boolean().optional(),
  }) }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/accounts`, 'POST', body);
  });
  registerTool(server, 'get_account', { description: '读取账户详情和余额。', inputSchema: entityId, readOnly: true }, (v) => api(`/accounts/${v.id}`));
  registerTool(server, 'update_account', { description: '更新账户信息。', inputSchema: entityId.extend(accountFields) }, (v) => {
    const { id: accountId, ...body } = v;
    return api(`/accounts/${accountId}`, 'PUT', body);
  });
  registerTool(server, 'delete_account', { description: '删除账户；账户仍有资产时后端会拒绝。', inputSchema: entityId, destructive: true }, (v) => api(`/accounts/${v.id}`, 'DELETE'));

  // Assets and asset details.
  registerTool(server, 'list_assets', { description: '列出家庭资产，可按成员或账户筛选。', inputSchema: householdId.extend({ owner_member_id: id.optional(), account_id: id.optional() }), readOnly: true }, (v) => {
    const query = new URLSearchParams();
    if (v.owner_member_id) query.set('owner_member_id', String(v.owner_member_id));
    if (v.account_id) query.set('account_id', String(v.account_id));
    return api(`/households/${v.household_id}/assets${query.size ? `?${query}` : ''}`);
  });
  const termDeposit = z.object({
    annual_interest_rate: z.number().int().optional(), start_date: z.string().length(10).optional(),
    maturity_date: z.string().length(10).nullable().optional(), term_value: z.number().int().optional(),
    term_unit: z.enum(['DAY', 'MONTH', 'YEAR']).optional(), interest_type: maybeText,
    auto_rollover: z.boolean().optional(), maturity_action: maybeText,
  }).passthrough();
  const stockFund = z.object({ fund_code: z.string().max(32).nullable().optional(), lock_start_date: z.string().length(10).nullable().optional(), lock_end_date: z.string().length(10).nullable().optional() }).passthrough();
  const bondFund = z.object({
    fund_code: z.string().max(32).nullable().optional(), expected_annual_yield_rate: z.number().int().nullable().optional(),
    purchase_date: z.string().length(10).optional(), holding_mode: z.enum(['MIN_HOLDING', 'ROLLING']).optional(),
    holding_period_days: z.number().int().optional(), first_redeem_date: z.string().length(10).nullable().optional(),
    next_redeem_date: z.string().length(10).nullable().optional(), maturity_date: z.string().length(10).nullable().optional(),
  }).passthrough();
  const flexibleTerm = z.object({ purchase_date: z.string().length(10).optional(), holding_period_days: z.number().int().optional() }).passthrough();
  const commercialPension = z.object({
    purchase_time: z.string().length(19).optional(), holding_period_value: z.number().int().optional(),
    holding_period_unit: z.enum(['DAY', 'MONTH', 'YEAR']).optional(), reservation_window_start: z.string().length(19).nullable().optional(),
    reservation_window_end: z.string().length(19).nullable().optional(), redeem_at_maturity: z.boolean().optional(),
  }).passthrough();
  const insurance = z.object({
    policy_no: z.string().max(64).nullable().optional(), insurance_company: z.string().max(100).nullable().optional(),
    product_name: z.string().max(100).nullable().optional(), insurance_type: z.string().max(32).nullable().optional(),
    effective_date: z.string().length(10).nullable().optional(), maturity_date: z.string().length(10).nullable().optional(),
    annual_premium: z.number().int().optional(), total_paid_premium: z.number().int().optional(),
    insured_amount: z.number().int().optional(), payment_years: z.number().int().nullable().optional(),
  }).passthrough();
  const assetDetail = z.object({
    term_deposit: termDeposit.nullable().optional(), stock_fund: stockFund.nullable().optional(),
    bond_fund: bondFund.nullable().optional(), flexible_term: flexibleTerm.nullable().optional(),
    commercial_pension: commercialPension.nullable().optional(), insurance: insurance.nullable().optional(),
  });
  const assetCreate = householdId.extend({
    account_id: id, name: z.string().min(1).max(100),
    asset_type: z.enum(['CASH', 'TERM_DEPOSIT', 'STOCK_FUND', 'BOND_FUND', 'FLEXIBLE_TERM', 'COMMERCIAL_PENSION', 'INSURANCE', 'LIABILITY', 'OTHER']),
    opening_balance: z.number().int().optional(), payment_asset_id: id.optional(), remark: transactionRemark,
    maintain_on_create: z.boolean().optional(),
  }).merge(assetDetail);
  registerTool(server, 'create_asset', { description: '创建资产；opening_balance 金额单位为分，可包含资产类型专有明细。提供 payment_asset_id 时会扣减付款资产余额。', inputSchema: assetCreate, destructive: true }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/assets`, 'POST', body);
  });
  registerTool(server, 'preview_asset_maintenance', { description: '预览创建资产时的日期维护，不会写入数据。', inputSchema: householdId.extend({
    asset_type: assetCreate.shape.asset_type,
  }).merge(assetDetail), readOnly: true }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/assets/maintenance-preview`, 'POST', body);
  });
  registerTool(server, 'get_asset', { description: '读取资产及其专有明细。', inputSchema: entityId, readOnly: true }, (v) => api(`/assets/${v.id}`));
  registerTool(server, 'update_asset', { description: '更新资产名称、期初余额或备注。', inputSchema: entityId.extend({ name: z.string().min(1).max(100).optional(), opening_balance: z.number().int().optional(), remark: z.string().max(500).nullable().optional() }) }, (v) => {
    const { id: assetId, ...body } = v;
    return api(`/assets/${assetId}`, 'PUT', body);
  });
  registerTool(server, 'set_asset_balance', { description: '直接设置资产当前余额（分），不会生成交易流水。', inputSchema: entityId.extend({ current_balance: z.number().int() }), destructive: true }, (v) => api(`/assets/${v.id}/balance`, 'PUT', { current_balance: v.current_balance }));
  registerTool(server, 'set_asset_status', { description: '启用或关闭资产。', inputSchema: entityId.extend({ status: z.enum(['ACTIVE', 'CLOSED']) }) }, (v) => api(`/assets/${v.id}/status`, 'PUT', { status: v.status }));
  registerTool(server, 'update_asset_detail', { description: '更新资产类型专有明细。', inputSchema: entityId.extend({ detail_type: z.enum(['TERM_DEPOSIT', 'STOCK_FUND', 'BOND_FUND', 'FLEXIBLE_TERM', 'COMMERCIAL_PENSION', 'INSURANCE']) }).merge(assetDetail) }, (v) => {
    const { id: assetId, ...body } = v;
    return api(`/assets/${assetId}/detail`, 'PUT', body);
  });
  registerTool(server, 'delete_asset', { description: '删除没有交易 Entry 的资产。有交易历史的资产应关闭，不能硬删除。', inputSchema: entityId, destructive: true }, (v) => api(`/assets/${v.id}`, 'DELETE'));

  // Transactions.
  registerTool(server, 'list_transactions', { description: '分页查询家庭流水，可按成员、资产、类型、时间筛选。时间使用后端接受的日期时间格式。', inputSchema: householdId.extend({
    owner_member_id: id.optional(), asset_id: id.optional(), type: z.enum(['INCOME', 'EXPENSE', 'TRANSFER', 'INVESTMENT', 'ADJUSTMENT']).optional(),
    from: z.string().optional(), to: z.string().optional(), limit: z.number().int().min(1).max(1000).optional(), offset: z.number().int().min(0).optional(),
  }), readOnly: true }, (v) => {
    const query = new URLSearchParams();
    for (const key of ['owner_member_id', 'asset_id', 'type', 'from', 'to', 'limit', 'offset'] as const) {
      if (v[key] !== undefined) query.set(key, String(v[key]));
    }
    return api(`/households/${v.household_id}/transactions?${query}`);
  });
  registerTool(server, 'list_asset_transactions', { description: '按单项资产视角查询流水，转入和转出方向按该资产 Entry 返回。', inputSchema: householdId.extend({ asset_id: id, owner_member_id: id.optional(), type: z.enum(['INCOME', 'EXPENSE', 'TRANSFER', 'INVESTMENT', 'ADJUSTMENT']).optional(), from: z.string().optional(), to: z.string().optional(), limit: z.number().int().min(1).max(1000).optional(), offset: z.number().int().min(0).optional() }), readOnly: true }, (v) => {
    const query = new URLSearchParams({ household_id: String(v.household_id) });
    for (const key of ['owner_member_id', 'type', 'from', 'to', 'limit', 'offset'] as const) if (v[key] !== undefined) query.set(key, String(v[key]));
    return api(`/assets/${v.asset_id}/transactions?${query}`);
  });
  const categoryInput = z.object({ household_id: id, type: z.enum(['INCOME', 'EXPENSE']), name: z.string().min(1).max(64) });
  registerTool(server, 'list_transaction_categories', { description: '读取家庭的收入或支出分类。', inputSchema: householdId.extend({ type: z.enum(['INCOME', 'EXPENSE']), include_inactive: z.boolean().optional() }), readOnly: true }, (v) => {
    const query = new URLSearchParams({ type: v.type, include_inactive: String(v.include_inactive ?? false) });
    return api(`/households/${v.household_id}/categories?${query}`);
  });
  registerTool(server, 'create_transaction_category', { description: '新增家庭收入或支出分类。', inputSchema: categoryInput }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/categories`, 'POST', body);
  });
  registerTool(server, 'update_transaction_category', { description: '修改分类名称和排序；历史流水会显示新名称。', inputSchema: z.object({ household_id: id, id, name: z.string().min(1).max(64), sort_order: z.number().int().min(0) }) }, (v) => api(`/households/${v.household_id}/categories/${v.id}`, 'PUT', { name: v.name, sort_order: v.sort_order }));
  registerTool(server, 'set_transaction_category_status', { description: '启用或停用分类；停用不影响历史流水。', inputSchema: z.object({ household_id: id, id, active: z.boolean() }) }, (v) => api(`/households/${v.household_id}/categories/${v.id}/status`, 'PUT', { active: v.active }));
  const txInput = z.object({ household_id: id, asset_id: id, amount: z.number().int(), category_id: id.nullable().optional(), transaction_time: transactionTime, remark: transactionRemark });
  registerTool(server, 'record_income', { description: '记录收入，amount 使用人民币分。', inputSchema: txInput }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/transactions/income`, 'POST', body);
  });
  registerTool(server, 'record_expense', { description: '记录支出，amount 使用人民币分。', inputSchema: txInput }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/transactions/expense`, 'POST', body);
  });
  registerTool(server, 'record_adjustment', { description: '记录余额调整，amount 使用人民币分，可为负数。', inputSchema: z.object({ household_id: id, asset_id: id, amount: z.number().int(), transaction_time: transactionTime, remark: transactionRemark }) }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/transactions/adjustment`, 'POST', body);
  });
  registerTool(server, 'transfer', { description: '在同一家庭资产之间转账，amount 使用人民币分。', inputSchema: z.object({ household_id: id, from_asset_id: id, to_asset_id: id, amount: z.number().int().positive(), transaction_time: transactionTime, remark: transactionRemark }) }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/transfers`, 'POST', body);
  });
  registerTool(server, 'investment_buy', { description: '记录投资买入；资金资产 OUT、投资资产 IN，amount 使用人民币分。', inputSchema: z.object({ household_id: id, from_asset_id: id, to_asset_id: id, amount: z.number().int().positive(), transaction_time: transactionTime, remark: transactionRemark }) }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/investments/buy`, 'POST', body);
  });
  registerTool(server, 'get_transaction', { description: '读取单条交易流水。', inputSchema: entityId, readOnly: true }, (v) => api(`/transactions/${v.id}`));
  registerTool(server, 'set_transaction_category', { description: '修改收入或支出流水的分类；category_id 为 null 时清空分类。', inputSchema: entityId.extend({ category_id: id.nullable() }) }, (v) => api(`/transactions/${v.id}/category`, 'PUT', { category_id: v.category_id }));
  registerTool(server, 'delete_transaction', { description: '删除一笔完整交易；rollback_assets 默认 true，为 false 时删除 Entry 但保留当前资产余额。', inputSchema: entityId.extend({ rollback_assets: z.boolean().optional() }), destructive: true }, (v) => api(`/transactions/${v.id}${v.rollback_assets === undefined ? '' : `?rollback_assets=${v.rollback_assets}`}`, 'DELETE'));

  // Statistics, maintenance and recurring investments.
  registerTool(server, 'get_overview', { description: '读取家庭总资产、负债、净资产和当月收支。', inputSchema: householdId.extend({ year: z.number().int().optional(), month: z.number().int().min(1).max(12).optional() }), readOnly: true }, (v) => {
    const query = new URLSearchParams();
    if (v.year) query.set('year', String(v.year));
    if (v.month) query.set('month', String(v.month));
    return api(`/households/${v.household_id}/statistics/overview${query.size ? `?${query}` : ''}`);
  });
  registerTool(server, 'get_period_statistics', { description: '按日期区间读取收支统计；from/to 按业务时区本地时间解释。', inputSchema: householdId.extend({ from: z.string().min(19).max(19), to: z.string().min(19).max(19), owner_member_id: id.optional() }), readOnly: true }, (v) => {
    const query = new URLSearchParams({ from: v.from, to: v.to });
    if (v.owner_member_id) query.set('owner_member_id', String(v.owner_member_id));
    return api(`/households/${v.household_id}/statistics/period?${query}`);
  });
  registerTool(server, 'get_monthly_statistics', { description: '读取近 N 个月收支趋势。', inputSchema: householdId.extend({ months: z.number().int().min(1).max(36).optional() }), readOnly: true }, (v) => api(`/households/${v.household_id}/statistics/monthly${v.months ? `?months=${v.months}` : ''}`));
  registerTool(server, 'preview_maintenance', { description: '预览每日维护将产生的变更，不会写入数据。', inputSchema: empty, readOnly: true }, () => api('/maintenance/preview'));
  registerTool(server, 'run_maintenance', { description: '执行每日资产维护，操作幂等。', inputSchema: empty, destructive: true }, () => api('/maintenance/run', 'POST'));

  registerTool(server, 'list_investment_plans', { description: '列出家庭定投计划。', inputSchema: householdId, readOnly: true }, (v) => api(`/households/${v.household_id}/investment-plans`));
  const planFields = { target_asset_id: id, source_asset_id: id, amount: z.number().int().positive(), frequency: z.enum(['DAILY', 'WEEKLY', 'BIWEEKLY', 'MONTHLY']), weekday: z.number().int().min(1).max(7).nullable().optional(), month_day: z.number().int().min(1).max(28).nullable().optional(), start_date: z.string().length(10) };
  registerTool(server, 'create_investment_plan', { description: '创建定投计划，amount 使用人民币分。符合日期时可能立即执行。', inputSchema: householdId.extend(planFields), destructive: true }, (v) => {
    const { household_id, ...body } = v;
    return api(`/households/${household_id}/investment-plans`, 'POST', body);
  });
  registerTool(server, 'update_investment_plan', { description: '更新定投计划。', inputSchema: entityId.extend(planFields) }, (v) => {
    const { id: planId, ...body } = v;
    return api(`/investment-plans/${planId}`, 'PUT', body);
  });
  registerTool(server, 'set_investment_plan_status', { description: '暂停或恢复定投计划；恢复时若当日符合计划可能立即执行。', inputSchema: entityId.extend({ status: z.enum(['ACTIVE', 'PAUSED']) }), destructive: true }, (v) => api(`/investment-plans/${v.id}/status`, 'PUT', { status: v.status }));
  registerTool(server, 'delete_investment_plan', { description: '软删除定投计划，保留历史执行记录。', inputSchema: entityId, destructive: true }, (v) => api(`/investment-plans/${v.id}`, 'DELETE'));
  registerTool(server, 'get_investment_executions', { description: '读取定投执行历史。', inputSchema: entityId, readOnly: true }, (v) => api(`/investment-plans/${v.id}/executions`));
  registerTool(server, 'execute_investment_plan', { description: '立即执行一次定投；同一计划每天最多执行一次。', inputSchema: entityId, destructive: true }, (v) => api(`/investment-plans/${v.id}/execute`, 'POST'));
  registerTool(server, 'retry_investment_execution', { description: '重试失败的定投期次。', inputSchema: entityId, destructive: true }, (v) => api(`/investment-executions/${v.id}/retry`, 'POST'));

  // Read-only MCP resources.
  const asJsonResource = (uri: URL, data: unknown) => ({ contents: [{ uri: uri.href, mimeType: 'application/json', text: JSON.stringify(data, null, 2) }] });
  server.registerResource('metadata', 'wealth-trace://meta', { title: '财迹元数据', description: '财迹枚举、分类和单位约定。', mimeType: 'application/json' }, async (uri) => asJsonResource(uri, await api('/meta')));
  server.registerResource('households', 'wealth-trace://households', { title: '家庭列表', description: '财迹中的家庭。', mimeType: 'application/json' }, async (uri) => asJsonResource(uri, await api('/households')));
  server.registerResource('household-overview', new ResourceTemplate('wealth-trace://households/{household_id}/overview', { list: undefined }), { title: '家庭财务总览', description: '指定家庭的资产、负债、净资产和当月收支。', mimeType: 'application/json' }, async (uri, variables) => asJsonResource(uri, await api(`/households/${encodeURIComponent(String(variables.household_id))}/statistics/overview`)));
  server.registerResource('household-members', new ResourceTemplate('wealth-trace://households/{household_id}/members', { list: undefined }), { title: '家庭成员', description: '指定家庭的成员列表。', mimeType: 'application/json' }, async (uri, variables) => asJsonResource(uri, await api(`/households/${encodeURIComponent(String(variables.household_id))}/members`)));
  server.registerResource('household-accounts', new ResourceTemplate('wealth-trace://households/{household_id}/accounts', { list: undefined }), { title: '家庭账户', description: '指定家庭的账户与余额。', mimeType: 'application/json' }, async (uri, variables) => asJsonResource(uri, await api(`/households/${encodeURIComponent(String(variables.household_id))}/accounts`)));
  server.registerResource('household-assets', new ResourceTemplate('wealth-trace://households/{household_id}/assets', { list: undefined }), { title: '家庭资产', description: '指定家庭的资产列表。', mimeType: 'application/json' }, async (uri, variables) => asJsonResource(uri, await api(`/households/${encodeURIComponent(String(variables.household_id))}/assets`)));

  // Reusable task prompts; they guide the model but do not write data by themselves.
  server.registerPrompt('review_household_finances', { title: '查看家庭财务概况', description: '读取家庭概况、账户和资产，并简明总结现状。', argsSchema: z.object({ household_id: id }) }, ({ household_id }) => ({
    messages: [{ role: 'user', content: { type: 'text', text: `请使用财迹 MCP 工具读取家庭 ${household_id} 的总览、账户、资产和近期流水，汇总净资产、主要构成及需要关注的变化。金额以元展示，同时保留分级精度；不要修改任何数据。` } }],
  }));
  server.registerPrompt('record_income_or_expense', { title: '记录一笔收支', description: '根据用户提供的信息补齐必要字段，再调用收支工具。', argsSchema: z.object({ direction: z.enum(['收入', '支出']).optional(), description: z.string().optional() }) }, ({ direction, description }) => ({
    messages: [{ role: 'user', content: { type: 'text', text: `帮助用户记录${direction ?? '收入或支出'}。${description ? `用户描述：${description}。` : ''}先确认家庭、资产、金额、分类和发生时间；查询可用 ID 后再操作。金额工具参数单位是分。提交写操作前向用户复述关键字段并取得明确确认；不要猜测金额或账户。` } }],
  }));
  server.registerPrompt('reconcile_transfer', { title: '核对并记录转账', description: '确认转出和转入资产后执行转账。', argsSchema: z.object({ household_id: id, description: z.string().optional() }) }, ({ household_id, description }) => ({
    messages: [{ role: 'user', content: { type: 'text', text: `协助家庭 ${household_id} 记录转账。${description ? `用户描述：${description}。` : ''}先查询该家庭账户和资产，确认转出资产、转入资产、金额及时间，确认两项资产属于同一家庭。调用 transfer 前向用户复述并取得明确确认；金额单位为分。` } }],
  }));

  return server;
}

serveStdio(createServer);
