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
        <el-table-column label="状态" width="90">
          <template #default="{ row }">
            <el-tag :type="row.status === 'ACTIVE' ? 'success' : 'info'" size="small">
              {{ assetStatusLabels[row.status as AssetStatus] }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="150" align="right">
          <template #default="{ row }">
            <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
            <el-button
              link
              :type="row.status === 'ACTIVE' ? 'danger' : 'success'"
              @click="toggleStatus(row)"
            >
              {{ row.status === 'ACTIVE' ? '关闭' : '启用' }}
            </el-button>
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
        <el-form-item label="资产名称" required>
          <el-input v-model="form.name" maxlength="100" />
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
        <el-form-item label="初始金额(元)">
          <el-input v-model="form.opening_balance_yuan" :disabled="!!editing && row?.opening_balance !== row?.current_balance" />
        </el-form-item>
        <el-form-item label="备注">
          <el-input v-model="form.remark" maxlength="500" />
        </el-form-item>

        <template v-if="form.asset_type === 'TERM_DEPOSIT'">
          <el-divider content-position="left">定期存款</el-divider>
          <el-form-item label="本金(元)">
            <el-input v-model="detail.principal_yuan" />
          </el-form-item>
          <el-form-item label="年利率(%)">
            <el-input v-model="detail.annual_interest_rate_percent" placeholder="如 1.85" />
          </el-form-item>
          <el-form-item label="起息日">
            <el-date-picker v-model="detail.start_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
          <el-form-item label="到期日">
            <el-date-picker v-model="detail.maturity_date" type="date" value-format="YYYY-MM-DD" style="width: 100%" />
          </el-form-item>
          <el-form-item label="期限">
            <el-input v-model.number="detail.term_value" style="width: 120px" />
            <el-select v-model="detail.term_unit" style="width: 120px; margin-left: 8px">
              <el-option v-for="(label, value) in termUnitLabels" :key="value" :label="label" :value="value" />
            </el-select>
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
          <el-form-item label="基金名称">
            <el-input v-model="detail.fund_name" />
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
          <el-form-item label="债券名称">
            <el-input v-model="detail.bond_name" />
          </el-form-item>
          <el-form-item label="本金(元)">
            <el-input v-model="detail.bond_principal_yuan" />
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
import { onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Plus } from '@element-plus/icons-vue'

import AmountText from '@/components/AmountText.vue'
import {
  createAsset,
  listAssets,
  updateAsset,
  updateAssetDetail,
  updateAssetStatus,
} from '@/api'
import { useAppStore } from '@/stores/app'
import { assetStatusLabels, assetTypeLabels, termUnitLabels } from '@/utils/labels'
import { percentToScaled, scaledToPercent, toMinor, toYuanInput } from '@/utils/money'
import type { Asset, AssetStatus, AssetType, TermUnit } from '@/types'

const store = useAppStore()
const assets = ref<Asset[]>([])
const loading = ref(false)
const saving = ref(false)
const dialogVisible = ref(false)
const editing = ref<Asset | null>(null)
const row = ref<Asset | null>(null)
const filterMember = ref<number | undefined>(undefined)
const filterAccount = ref<number | undefined>(undefined)

const form = reactive({
  account_id: 0,
  name: '',
  asset_type: 'CASH' as AssetType,
  opening_balance_yuan: '0.00',
  remark: '',
})

const detail = reactive({
  principal_yuan: '0.00',
  annual_interest_rate_percent: '',
  start_date: '' as string | null,
  maturity_date: '' as string | null,
  term_value: undefined as number | undefined,
  term_unit: 'YEAR' as TermUnit,
  auto_rollover: false,
  fund_code: '',
  fund_name: '',
  fund_type: '',
  lock_end_date: '' as string | null,
  bond_code: '',
  bond_name: '',
  bond_principal_yuan: '0.00',
  annual_coupon_rate_percent: '',
  purchase_date: '' as string | null,
  policy_no: '',
  insurance_company: '',
  product_name: '',
  effective_date: '' as string | null,
  annual_premium_yuan: '0.00',
  insured_amount_yuan: '0.00',
})

function resetDetail() {
  Object.assign(detail, {
    principal_yuan: '0.00',
    annual_interest_rate_percent: '',
    start_date: null,
    maturity_date: null,
    term_value: undefined,
    term_unit: 'YEAR',
    auto_rollover: false,
    fund_code: '',
    fund_name: '',
    fund_type: '',
    lock_end_date: null,
    bond_code: '',
    bond_name: '',
    bond_principal_yuan: '0.00',
    annual_coupon_rate_percent: '',
    purchase_date: null,
    policy_no: '',
    insurance_company: '',
    product_name: '',
    effective_date: null,
    annual_premium_yuan: '0.00',
    insured_amount_yuan: '0.00',
  })
}

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

function openEdit(asset: Asset) {
  editing.value = asset
  row.value = asset
  form.account_id = asset.account_id
  form.name = asset.name
  form.asset_type = asset.asset_type
  form.opening_balance_yuan = toYuanInput(asset.opening_balance)
  form.remark = asset.remark ?? ''
  resetDetail()
  if (asset.term_deposit) {
    detail.principal_yuan = toYuanInput(asset.term_deposit.principal)
    detail.annual_interest_rate_percent = scaledToPercent(asset.term_deposit.annual_interest_rate)
    detail.start_date = asset.term_deposit.start_date
    detail.maturity_date = asset.term_deposit.maturity_date
    detail.term_value = asset.term_deposit.term_value ?? undefined
    detail.term_unit = asset.term_deposit.term_unit ?? 'YEAR'
    detail.auto_rollover = asset.term_deposit.auto_rollover
  }
  if (asset.fund) {
    detail.fund_code = asset.fund.fund_code ?? ''
    detail.fund_name = asset.fund.fund_name ?? ''
    detail.fund_type = asset.fund.fund_type ?? ''
    detail.lock_end_date = asset.fund.lock_end_date
  }
  if (asset.bond) {
    detail.bond_code = asset.bond.bond_code ?? ''
    detail.bond_name = asset.bond.bond_name ?? ''
    detail.bond_principal_yuan = toYuanInput(asset.bond.principal)
    detail.annual_coupon_rate_percent = scaledToPercent(asset.bond.annual_coupon_rate)
    detail.purchase_date = asset.bond.purchase_date
    detail.maturity_date = asset.bond.maturity_date
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

function buildDetail(type: AssetType): Record<string, unknown> | null {
  if (type === 'TERM_DEPOSIT') {
    return {
      principal: toMinor(detail.principal_yuan),
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
      fund_name: detail.fund_name || null,
      fund_type: detail.fund_type || null,
      lock_end_date: detail.lock_end_date || null,
    }
  }
  if (type === 'BOND') {
    return {
      bond_code: detail.bond_code || null,
      bond_name: detail.bond_name || null,
      principal: toMinor(detail.bond_principal_yuan),
      annual_coupon_rate: percentToScaled(detail.annual_coupon_rate_percent || '0'),
      purchase_date: detail.purchase_date || null,
      maturity_date: detail.maturity_date || null,
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

const detailKey: Partial<Record<AssetType, string>> = {
  TERM_DEPOSIT: 'term_deposit',
  FUND: 'fund',
  BOND: 'bond',
  INSURANCE: 'insurance',
}

async function save() {
  if (!form.account_id) {
    ElMessage.warning('请选择账户')
    return
  }
  if (!form.name.trim()) {
    ElMessage.warning('请填写资产名称')
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
</script>
