<!-- 资产管理页：按成员/账户筛选资产，弹窗根据资产类型动态渲染对应明细块（定期/基金/债券/保险）。 -->
<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" :icon="Plus" @click="openCreate">新增资产</el-button>
      <el-select v-model="filterMember" clearable placeholder="按成员" style="width: 150px" @change="load">
        <el-option v-for="m in store.members" :key="m.id" :label="m.name" :value="m.id" />
      </el-select>
      <el-select v-model="filterAccount" clearable placeholder="按账户" style="width: 180px" @change="load">
        <el-option v-for="a in store.accounts" :key="a.id" :label="a.name" :value="a.id" />
      </el-select>
      <div class="spacer" />
    </div>

    <el-card shadow="never">
      <el-table :data="assets" v-loading="loading">
        <el-table-column prop="name" label="资产" />
        <el-table-column label="类型" width="110">
          <template #default="{ row }">{{ assetTypeLabels[row.asset_type as AssetType] }}</template>
        </el-table-column>
        <el-table-column label="属主" width="110">
          <template #default="{ row }">{{ store.memberName(row.owner_member_id) }}</template>
        </el-table-column>
        <el-table-column label="账户" width="150">
          <template #default="{ row }">{{ store.accountName(row.account_id) }}</template>
        </el-table-column>
        <el-table-column label="当前价值" width="150" align="right">
          <template #default="{ row }"><AmountText :value="row.current_balance" /></template>
        </el-table-column>
        <el-table-column label="期限状态" width="210">
          <template #default="{ row }">
            <span v-if="row.asset_type === 'BOND_FUND'">{{ bondFundInfo(row) }}</span>
            <span v-else-if="row.asset_type === 'TERM_DEPOSIT'">{{ termDepositInfo(row) }}</span>
            <span v-else>—</span>
          </template>
        </el-table-column>
        <el-table-column label="状态" width="90">
          <template #default="{ row }">
            <el-tag :type="row.status === 'ACTIVE' ? 'success' : 'info'" size="small">
              {{ assetStatusLabels[row.status as AssetStatus] }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="190" align="right">
          <template #default="{ row }">
            <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
            <el-button
              link
              :type="row.status === 'ACTIVE' ? 'warning' : 'success'"
              @click="toggleStatus(row)"
            >
              {{ row.status === 'ACTIVE' ? '关闭' : '启用' }}
            </el-button>
            <el-button link type="danger" @click="remove(row)">删除</el-button>
          </template>
        </el-table-column>
      </el-table>
    </el-card>

    <el-dialog
      v-model="dialogVisible"
      :title="editing ? '编辑资产' : '新增资产'"
      width="640px"
      top="6vh"
    >
      <el-form :model="form" label-width="100px">
        <el-divider content-position="left">基本信息</el-divider>
        <el-form-item label="所属账户" required>
          <el-select v-model="form.account_id" style="width: 100%" :disabled="!!editing">
            <el-option
              v-for="a in store.accounts"
              :key="a.id"
              :label="`${store.memberName(a.owner_member_id)} · ${a.name}`"
              :value="a.id"
            />
          </el-select>
        </el-form-item>
        <el-form-item label="资产类型">
          <el-select v-model="form.asset_type" style="width: 100%" :disabled="!!editing">
            <el-option
              v-for="(label, value) in assetTypeLabels"
              :key="value"
              :label="label"
              :value="value"
            />
          </el-select>
        </el-form-item>
        <el-form-item label="资产名称" required>
          <el-input v-model="form.name" maxlength="100" />
        </el-form-item>
        <el-form-item label="初始金额(元)">
          <el-input v-model="form.opening_balance_yuan" :disabled="!!editing && row?.opening_balance !== row?.current_balance" />
        </el-form-item>
        <el-form-item label="备注">
          <el-input v-model="form.remark" maxlength="500" />
        </el-form-item>

        <template v-if="form.asset_type === 'TERM_DEPOSIT'">
          <el-divider content-position="left">定期存款</el-divider>
          <el-form-item label="年利率(%)">
            <el-input v-model="detail.annual_interest_rate_percent" placeholder="如 1.85" />
          </el-form-item>
          <el-form-item label="起息日" required>
            <el-date-picker v-model="detail.start_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
          <el-form-item label="期限" required>
            <el-input v-model.number="detail.term_value" style="width: 120px" />
            <el-select v-model="detail.term_unit" style="width: 120px; margin-left: 8px">
              <el-option v-for="(label, value) in termUnitLabels" :key="value" :label="label" :value="value" />
            </el-select>
          </el-form-item>
          <el-form-item label="到期日">
            <el-date-picker
              v-model="detail.maturity_date"
              type="date"
              value-format="YYYY-MM-DD"
              style="width: 100%"
              :placeholder="estimatedMaturity || '留空按存期自动计算'"
            />
          </el-form-item>
          <el-form-item v-if="estimatedMaturity" label=" ">
            <span style="color: #909399">预计到期日：{{ estimatedMaturity }}</span>
          </el-form-item>
          <el-form-item label="自动转存">
            <el-switch v-model="detail.auto_rollover" />
          </el-form-item>
        </template>

        <template v-else-if="form.asset_type === 'FUND'">
          <el-divider content-position="left">基金</el-divider>
          <el-form-item label="基金代码">
            <el-input v-model="detail.fund_code" />
          </el-form-item>
          <el-form-item label="基金类型">
            <el-input v-model="detail.fund_type" />
          </el-form-item>
          <el-form-item label="锁定结束">
            <el-date-picker v-model="detail.lock_end_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
        </template>

        <template v-else-if="form.asset_type === 'BOND'">
          <el-divider content-position="left">债券</el-divider>
          <el-form-item label="债券代码">
            <el-input v-model="detail.bond_code" />
          </el-form-item>
          <el-form-item label="年利率(%)">
            <el-input v-model="detail.annual_coupon_rate_percent" />
          </el-form-item>
          <el-form-item label="购买日期">
            <el-date-picker v-model="detail.purchase_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
          <el-form-item label="到期日期">
            <el-date-picker v-model="detail.maturity_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
        </template>

        <template v-else-if="form.asset_type === 'BOND_FUND'">
          <el-divider content-position="left">债券基金</el-divider>
          <el-form-item label="基金代码">
            <el-input v-model="detail.bf_fund_code" />
          </el-form-item>
          <el-form-item label="预期年化(%)">
            <el-input v-model="detail.bf_expected_annual_yield_percent" placeholder="如 3.5" />
          </el-form-item>
          <el-form-item label="购买日期" required>
            <el-date-picker v-model="detail.bf_purchase_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
          <el-form-item label="持有方式" required>
            <el-radio-group v-model="detail.bf_holding_mode">
              <el-radio-button v-for="(label, value) in holdingModeLabels" :key="value" :value="value">
                {{ label }}
              </el-radio-button>
            </el-radio-group>
          </el-form-item>
          <el-form-item label="持有周期(天)" required>
            <el-input v-model.number="detail.bf_holding_period_days" style="width: 160px" />
          </el-form-item>
          <el-form-item label="首次可赎回">
            <el-date-picker
              v-model="detail.bf_first_redeem_date"
              type="date"
              value-format="YYYY-MM-DD"
              style="width: 100%"
              :placeholder="estimatedFirstRedeem || '留空自动计算'"
            />
          </el-form-item>
          <el-form-item v-if="detail.bf_holding_mode === 'ROLLING'" label="下一赎回日">
            <el-date-picker
              v-model="detail.bf_next_redeem_date"
              type="date"
              value-format="YYYY-MM-DD"
              style="width: 100%"
              :placeholder="estimatedFirstRedeem || '留空自动计算'"
            />
          </el-form-item>
          <el-form-item label="最终到期日">
            <el-date-picker v-model="detail.bf_maturity_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
          <el-form-item v-if="estimatedFirstRedeem" label=" ">
            <span style="color: #909399">预计首次可赎回日期：{{ estimatedFirstRedeem }}</span>
          </el-form-item>
        </template>

        <template v-else-if="form.asset_type === 'INSURANCE'">
          <el-divider content-position="left">保险</el-divider>
          <el-form-item label="保单号">
            <el-input v-model="detail.policy_no" />
          </el-form-item>
          <el-form-item label="保险公司">
            <el-input v-model="detail.insurance_company" />
          </el-form-item>
          <el-form-item label="产品名称">
            <el-input v-model="detail.product_name" />
          </el-form-item>
          <el-form-item label="生效日期">
            <el-date-picker v-model="detail.effective_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
          <el-form-item label="年保费(元)">
            <el-input v-model="detail.annual_premium_yuan" />
          </el-form-item>
          <el-form-item label="保额(元)">
            <el-input v-model="detail.insured_amount_yuan" />
          </el-form-item>
        </template>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" :loading="saving" @click="save">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
// 职责：展示/维护资产列表；弹窗表单按 asset_type 切换明细块，负责「元↔分」「百分数↔定点利率」换算后提交。
import { computed, onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Plus } from '@element-plus/icons-vue'

import AmountText from '@/components/AmountText.vue'
import {
  createAsset,
  deleteAsset,
  listAssets,
  updateAsset,
  updateAssetDetail,
  updateAssetStatus,
} from '@/api'
import { useAppStore } from '@/stores/app'
import { assetStatusLabels, assetTypeLabels, holdingModeLabels, termUnitLabels } from '@/utils/labels'
import { percentToScaled, scaledToPercent, toMinor, toYuanInput } from '@/utils/money'
import type { Asset, AssetStatus, AssetType, HoldingMode, TermUnit } from '@/types'

const store = useAppStore()
const assets = ref<Asset[]>([])       // 当前筛选条件下的资产列表
const loading = ref(false)            // 列表加载中
const saving = ref(false)             // 表单提交中
const dialogVisible = ref(false)      // 弹窗显隐
const editing = ref<Asset | null>(null) // 当前编辑对象，null 表示新增
const row = ref<Asset | null>(null)   // 当前编辑行，用于判断初始金额是否可改
const filterMember = ref<number | undefined>(undefined)  // 按成员筛选
const filterAccount = ref<number | undefined>(undefined) // 按账户筛选

// 资产基本信息表单（金额以「元」字符串编辑，提交时再换算为「分」）。
const form = reactive({
  account_id: 0,
  name: '',
  asset_type: 'CASH' as AssetType,
  opening_balance_yuan: '0.00',
  remark: '',
})

// 各类型明细的合并表单：金额以「元」、利率以百分数字符串编辑，仅渲染当前类型所需字段。
const detail = reactive({
  annual_interest_rate_percent: '',
  start_date: '' as string | null,
  maturity_date: '' as string | null,
  term_value: undefined as number | undefined,
  term_unit: 'YEAR' as TermUnit,
  auto_rollover: false,
  fund_code: '',
  fund_type: '',
  lock_end_date: '' as string | null,
  bond_code: '',
  annual_coupon_rate_percent: '',
  purchase_date: '' as string | null,
  bf_fund_code: '',
  bf_expected_annual_yield_percent: '',
  bf_purchase_date: '' as string | null,
  bf_holding_mode: 'MIN_HOLDING' as HoldingMode,
  bf_holding_period_days: undefined as number | undefined,
  bf_first_redeem_date: '' as string | null,
  bf_next_redeem_date: '' as string | null,
  bf_maturity_date: '' as string | null,
  policy_no: '',
  insurance_company: '',
  product_name: '',
  effective_date: '' as string | null,
  annual_premium_yuan: '0.00',
  insured_amount_yuan: '0.00',
})

// 将所有明细字段恢复为默认值，避免编辑不同类型时残留旧数据。
function resetDetail() {
  Object.assign(detail, {
    annual_interest_rate_percent: '',
    start_date: null,
    maturity_date: null,
    term_value: undefined,
    term_unit: 'YEAR',
    auto_rollover: false,
    fund_code: '',
    fund_type: '',
    lock_end_date: null,
    bond_code: '',
    annual_coupon_rate_percent: '',
    purchase_date: null,
    bf_fund_code: '',
    bf_expected_annual_yield_percent: '',
    bf_purchase_date: null,
    bf_holding_mode: 'MIN_HOLDING',
    bf_holding_period_days: undefined,
    bf_first_redeem_date: null,
    bf_next_redeem_date: null,
    bf_maturity_date: null,
    policy_no: '',
    insurance_company: '',
    product_name: '',
    effective_date: null,
    annual_premium_yuan: '0.00',
    insured_amount_yuan: '0.00',
  })
}

// 把 "YYYY-MM-DD" 加上天数，返回同格式日期；与后端 time_util::add_days 算法一致。
function addDays(date: string | null, days: number | undefined): string {
  if (!date || !days || days <= 0) {
    return ''
  }
  const parsed = new Date(`${date}T00:00:00Z`)
  if (Number.isNaN(parsed.getTime())) {
    return ''
  }
  parsed.setUTCDate(parsed.getUTCDate() + days)
  return parsed.toISOString().slice(0, 10)
}

// 按存期推算到期日（与后端 add_term 规则一致：目标月无对应日时取该月最后一天）。
function addTermDate(date: string | null, value: number | undefined, unit: TermUnit): string {
  if (!date || !value || value <= 0) {
    return ''
  }
  const [year, month, day] = date.split('-').map(Number)
  if (!year || !month || !day) {
    return ''
  }
  if (unit === 'DAY') {
    return addDays(date, value)
  }
  const months = unit === 'YEAR' ? value * 12 : value
  const total = year * 12 + (month - 1) + months
  const targetYear = Math.floor(total / 12)
  const targetMonth = (total % 12) + 1
  const lastDay = new Date(Date.UTC(targetYear, targetMonth, 0)).getUTCDate()
  const targetDay = Math.min(day, lastDay)
  const pad = (n: number) => String(n).padStart(2, '0')
  return `${targetYear}-${pad(targetMonth)}-${pad(targetDay)}`
}

// 系统按「购买日期 + 持有周期」预估的首次可赎回日期，作为填写建议展示。
const estimatedFirstRedeem = computed(() =>
  addDays(detail.bf_purchase_date, detail.bf_holding_period_days),
)

// 系统按「起息日 + 存期」预估的定期存款到期日，作为填写建议展示。
const estimatedMaturity = computed(() =>
  addTermDate(detail.start_date, detail.term_value, detail.term_unit),
)

// 债券基金赎回状态中文名（由后端按业务时区推导）。
const bondFundStatusLabels: Record<string, string> = {
  LOCKED: '锁定中',
  REDEEMABLE: '已满足持有期',
  REDEEMABLE_TODAY: '今日可赎回',
  PENDING: '待维护',
  UNKNOWN: '未知',
}

// 定期存款到期状态中文名（由后端按业务时区推导）。
const termDepositStatusLabels: Record<string, string> = {
  ACTIVE: '存续中',
  MATURED: '已到期',
  UNKNOWN: '未知',
}

// 定期存款到期状态文案（仅 TERM_DEPOSIT 行显示），同样使用后端派生字段。
function termDepositInfo(asset: Asset): string {
  const deposit = asset.term_deposit
  if (!deposit || !deposit.status) {
    return '—'
  }
  const label = termDepositStatusLabels[deposit.status] ?? deposit.status
  const date = deposit.maturity_date
  const days = deposit.days_until_maturity
  if (deposit.status === 'ACTIVE' && days !== null && days > 0) {
    return `${label} · 还有 ${days} 天`
  }
  if (deposit.status === 'MATURED') {
    return date ? `已到期（${date}）` : '已到期'
  }
  return date ? `${label}（${date}）` : label
}

// 债券基金的赎回状态文案（仅 BOND_FUND 行显示）。状态与剩余天数均由后端按业务日期给出，
// 前端不再用浏览器日期自行推断，避免前后端「今天」不一致。
function bondFundInfo(asset: Asset): string {
  const bondFund = asset.bond_fund
  if (!bondFund || !bondFund.status) {
    return '—'
  }
  const label = bondFundStatusLabels[bondFund.status] ?? bondFund.status
  const date =
    bondFund.holding_mode === 'ROLLING' ? bondFund.next_redeem_date : bondFund.first_redeem_date
  const days = bondFund.days_until_redeem
  if (bondFund.status === 'LOCKED' && days !== null && days > 0) {
    return `${label} · 还有 ${days} 天`
  }
  return date ? `${label}（${date}）` : label
}

// 按筛选条件拉取资产列表，并同步 store 中的资产缓存（供交易页下拉使用）。
async function load() {
  if (!store.householdId) {
    return
  }
  loading.value = true
  try {
    assets.value = await listAssets(store.householdId, {
      ownerMemberId: filterMember.value,
      accountId: filterAccount.value,
    })
    await store.refreshAssets()
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    loading.value = false
  }
}

// 打开「新增」弹窗，默认账户为第一个、类型为 CASH。
function openCreate() {
  editing.value = null
  row.value = null
  form.account_id = store.accounts[0]?.id ?? 0
  form.name = ''
  form.asset_type = 'CASH'
  form.opening_balance_yuan = '0.00'
  form.remark = ''
  resetDetail()
  dialogVisible.value = true
}

// 打开「编辑」弹窗：基本信息回填，并把「分」金额、「定点利率」分别转成「元」「百分数」展示。
function openEdit(asset: Asset) {
  editing.value = asset
  row.value = asset
  form.account_id = asset.account_id
  form.name = asset.name
  form.asset_type = asset.asset_type
  form.opening_balance_yuan = toYuanInput(asset.opening_balance) // 分 -> 元
  form.remark = asset.remark ?? ''
  resetDetail()
  // 按存在的明细块分别回填，金额/利率做展示口径转换。
  if (asset.term_deposit) {
    detail.annual_interest_rate_percent = scaledToPercent(asset.term_deposit.annual_interest_rate)
    detail.start_date = asset.term_deposit.start_date
    detail.maturity_date = asset.term_deposit.maturity_date
    detail.term_value = asset.term_deposit.term_value ?? undefined
    detail.term_unit = asset.term_deposit.term_unit ?? 'YEAR'
    detail.auto_rollover = asset.term_deposit.auto_rollover
  }
  if (asset.fund) {
    detail.fund_code = asset.fund.fund_code ?? ''
    detail.fund_type = asset.fund.fund_type ?? ''
    detail.lock_end_date = asset.fund.lock_end_date
  }
  if (asset.bond) {
    detail.bond_code = asset.bond.bond_code ?? ''
    detail.annual_coupon_rate_percent = scaledToPercent(asset.bond.annual_coupon_rate)
    detail.purchase_date = asset.bond.purchase_date
    detail.maturity_date = asset.bond.maturity_date
  }
  if (asset.bond_fund) {
    detail.bf_fund_code = asset.bond_fund.fund_code ?? ''
    detail.bf_expected_annual_yield_percent =
      asset.bond_fund.expected_annual_yield_rate !== null
        ? scaledToPercent(asset.bond_fund.expected_annual_yield_rate)
        : ''
    detail.bf_purchase_date = asset.bond_fund.purchase_date
    detail.bf_holding_mode = asset.bond_fund.holding_mode
    detail.bf_holding_period_days = asset.bond_fund.holding_period_days
    detail.bf_first_redeem_date = asset.bond_fund.first_redeem_date
    detail.bf_next_redeem_date = asset.bond_fund.next_redeem_date
    detail.bf_maturity_date = asset.bond_fund.maturity_date
  }
  if (asset.insurance) {
    detail.policy_no = asset.insurance.policy_no ?? ''
    detail.insurance_company = asset.insurance.insurance_company ?? ''
    detail.product_name = asset.insurance.product_name ?? ''
    detail.effective_date = asset.insurance.effective_date
    detail.annual_premium_yuan = toYuanInput(asset.insurance.annual_premium)
    detail.insured_amount_yuan = toYuanInput(asset.insurance.insured_amount)
  }
  dialogVisible.value = true
}

// 根据资产类型把明细表单组装为后端所需 payload：金额转「分」，利率转定点整数；无明细类型返回 null。
function buildDetail(type: AssetType): Record<string, unknown> | null {
  if (type === 'TERM_DEPOSIT') {
    return {
      annual_interest_rate: percentToScaled(detail.annual_interest_rate_percent || '0'),
      start_date: detail.start_date || null,
      maturity_date: detail.maturity_date || null,
      term_value: detail.term_value ?? null,
      term_unit: detail.term_unit,
      auto_rollover: detail.auto_rollover,
    }
  }
  if (type === 'FUND') {
    return {
      fund_code: detail.fund_code || null,
      fund_type: detail.fund_type || null,
      lock_end_date: detail.lock_end_date || null,
    }
  }
  if (type === 'BOND') {
    return {
      bond_code: detail.bond_code || null,
      annual_coupon_rate: percentToScaled(detail.annual_coupon_rate_percent || '0'),
      purchase_date: detail.purchase_date || null,
      maturity_date: detail.maturity_date || null,
    }
  }
  if (type === 'BOND_FUND') {
    return {
      fund_code: detail.bf_fund_code || null,
      expected_annual_yield_rate:
        detail.bf_expected_annual_yield_percent === ''
          ? null
          : percentToScaled(detail.bf_expected_annual_yield_percent),
      purchase_date: detail.bf_purchase_date,
      holding_mode: detail.bf_holding_mode,
      holding_period_days: detail.bf_holding_period_days ?? 0,
      first_redeem_date: detail.bf_first_redeem_date || null,
      next_redeem_date: detail.bf_next_redeem_date || null,
      maturity_date: detail.bf_maturity_date || null,
    }
  }
  if (type === 'INSURANCE') {
    return {
      policy_no: detail.policy_no || null,
      insurance_company: detail.insurance_company || null,
      product_name: detail.product_name || null,
      effective_date: detail.effective_date || null,
      annual_premium: toMinor(detail.annual_premium_yuan),
      insured_amount: toMinor(detail.insured_amount_yuan),
    }
  }
  return null
}

// 资产类型 -> 后端明细块字段名 的映射，用于提交时组装 detail_type 与对应明细键。
const detailKey: Partial<Record<AssetType, string>> = {
  TERM_DEPOSIT: 'term_deposit',
  FUND: 'fund',
  BOND: 'bond',
  BOND_FUND: 'bond_fund',
  INSURANCE: 'insurance',
}

// 校验账户与名称后提交：编辑走「基本信息 + 明细」两个接口，新增则合并为一个 payload。
async function save() {
  if (!form.account_id) {
    ElMessage.warning('请选择账户')
    return
  }
  if (!form.name.trim()) {
    ElMessage.warning('请填写资产名称')
    return
  }
  if (form.asset_type === 'TERM_DEPOSIT' && (!detail.start_date || !detail.term_value)) {
    ElMessage.warning('请填写定期存款的起息日与存期')
    return
  }
  saving.value = true
  try {
    const detailPayload = buildDetail(form.asset_type)
    if (editing.value) {
      await updateAsset(editing.value.id, {
        name: form.name,
        opening_balance: toMinor(form.opening_balance_yuan),
        remark: form.remark,
      })
      if (detailPayload && detailKey[form.asset_type]) {
        await updateAssetDetail(editing.value.id, {
          detail_type: form.asset_type,
          [detailKey[form.asset_type] as string]: detailPayload,
        })
      }
    } else {
      const payload: Record<string, unknown> = {
        account_id: form.account_id,
        name: form.name,
        asset_type: form.asset_type,
        opening_balance: toMinor(form.opening_balance_yuan),
        remark: form.remark,
      }
      if (detailPayload && detailKey[form.asset_type]) {
        payload[detailKey[form.asset_type] as string] = detailPayload
      }
      await createAsset(store.householdId, payload)
    }
    dialogVisible.value = false
    await load()
    ElMessage.success('已保存')
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    saving.value = false
  }
}

// 切换资产启停状态，关闭前二次确认，成功后在当前筛选条件下重新加载。
async function toggleStatus(asset: Asset) {
  const next: AssetStatus = asset.status === 'ACTIVE' ? 'CLOSED' : 'ACTIVE'
  try {
    await ElMessageBox.confirm(
      next === 'CLOSED' ? '关闭后该资产将不再计入统计，且不能新增交易。' : '确认重新启用该资产？',
      '确认',
      { type: 'warning' },
    )
  } catch {
    return
  }
  try {
    await updateAssetStatus(asset.id, next)
    await load()
    ElMessage.success('已更新')
  } catch (error) {
    ElMessage.error((error as Error).message)
  }
}

onMounted(async () => {
  await store.refreshAccounts()
  await load()
})

// 删除资产：级联删除其全部交易与明细，属不可恢复操作，要求手动输入资产名二次确认。
async function remove(asset: Asset) {
  try {
    await ElMessageBox.prompt(
      `删除后该资产及其名下全部交易、明细将被永久删除，且不可恢复。请输入资产名称「${asset.name}」以确认：`,
      '危险操作',
      {
        type: 'warning',
        confirmButtonText: '确认删除',
        cancelButtonText: '取消',
        confirmButtonClass: 'el-button--danger',
        inputValidator: (value) => value === asset.name || '输入的资产名称不匹配',
      },
    )
  } catch {
    return
  }
  try {
    await deleteAsset(asset.id)
    await load()
    ElMessage.success('已删除')
  } catch (error) {
    ElMessage.error((error as Error).message)
  }
}
</script>
