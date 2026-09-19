<template>
  <div>
    <div class="toolbar">
      <el-button type="success" :icon="Plus" @click="openDialog('income')">记收入</el-button>
      <el-button type="warning" :icon="Minus" @click="openDialog('expense')">记支出</el-button>
      <el-button type="primary" :icon="Sort" @click="openDialog('transfer')">转账</el-button>
      <el-button :icon="Edit" @click="openDialog('adjustment')">余额调整</el-button>
    </div>

    <div class="toolbar">
      <el-select v-model="filterMember" clearable placeholder="按成员" style="width: 150px" @change="reload">
        <el-option v-for="m in store.members" :key="m.id" :label="m.name" :value="m.id" />
      </el-select>
      <el-select v-model="filterAsset" clearable filterable placeholder="按资产" style="width: 220px" @change="reload">
        <el-option
          v-for="a in store.assets"
          :key="a.id"
          :label="`${store.memberName(a.owner_member_id)} · ${a.name}`"
          :value="a.id"
        />
      </el-select>
      <el-select v-model="filterType" clearable placeholder="按类型" style="width: 140px" @change="reload">
        <el-option v-for="(label, value) in transactionTypeLabels" :key="value" :label="label" :value="value" />
      </el-select>
      <div class="spacer" />
    </div>

    <el-card shadow="never">
      <el-table :data="transactions" v-loading="loading">
        <el-table-column prop="transaction_time" label="时间" width="170" />
        <el-table-column label="成员" width="110">
          <template #default="{ row }">{{ store.memberName(row.owner_member_id) }}</template>
        </el-table-column>
        <el-table-column label="资产" width="160">
          <template #default="{ row }">{{ store.assetName(row.asset_id) }}</template>
        </el-table-column>
        <el-table-column label="类型" width="90">
          <template #default="{ row }">
            <el-tag size="small" :type="tagType(row.type)">
              {{ transactionTypeLabels[row.type as TransactionType] }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="category" label="分类" width="110" />
        <el-table-column label="金额" width="150" align="right">
          <template #default="{ row }">
            <AmountText :value="signedAmount(row)" />
          </template>
        </el-table-column>
        <el-table-column label="余额" width="150" align="right">
          <template #default="{ row }">
            <span v-if="row.balance_after !== null" class="amount">{{ formatMoney(row.balance_after) }}</span>
          </template>
        </el-table-column>
        <el-table-column prop="remark" label="备注" show-overflow-tooltip />
      </el-table>
      <div class="pager">
        <el-pagination
          layout="total, prev, pager, next"
          :total="total"
          :page-size="pageSize"
          :current-page="page"
          @current-change="onPage"
        />
      </div>
    </el-card>

    <el-dialog v-model="dialogVisible" :title="dialogTitles[kind]" width="480px">
      <el-form label-width="90px">
        <el-form-item v-if="kind === 'transfer'" label="转出资产" required>
          <el-select v-model="form.from_asset_id" filterable style="width: 100%">
            <el-option v-for="a in store.assets" :key="a.id" :label="assetLabel(a.id)" :value="a.id" />
          </el-select>
        </el-form-item>
        <el-form-item v-if="kind === 'transfer'" label="转入资产" required>
          <el-select v-model="form.to_asset_id" filterable style="width: 100%">
            <el-option v-for="a in store.assets" :key="a.id" :label="assetLabel(a.id)" :value="a.id" />
          </el-select>
        </el-form-item>
        <el-form-item v-else label="资产" required>
          <el-select v-model="form.asset_id" filterable style="width: 100%">
            <el-option v-for="a in store.assets" :key="a.id" :label="assetLabel(a.id)" :value="a.id" />
          </el-select>
        </el-form-item>
        <el-form-item v-if="kind === 'income' || kind === 'expense'" label="分类">
          <el-select v-model="form.category" clearable style="width: 100%">
            <el-option v-for="c in categories" :key="c" :label="c" :value="c" />
          </el-select>
        </el-form-item>
        <el-form-item :label="kind === 'adjustment' ? '调整金额(元)' : '金额(元)'" required>
          <el-input v-model="form.amount_yuan" placeholder="如 100.00，调整可为负数" />
        </el-form-item>
        <el-form-item label="时间">
          <el-date-picker
            v-model="form.transaction_time"
            type="datetime"
            value-format="YYYY-MM-DD HH:mm:ss"
            placeholder="默认当前时间"
            style="width: 100%"
          />
        </el-form-item>
        <el-form-item label="备注">
          <el-input v-model="form.remark" maxlength="500" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" :loading="saving" @click="submit">提交</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { Edit, Minus, Plus, Sort } from '@element-plus/icons-vue'

import AmountText from '@/components/AmountText.vue'
import {
  listTransactions,
  recordAdjustment,
  recordExpense,
  recordIncome,
  transfer,
} from '@/api'
import { useAppStore } from '@/stores/app'
import { transactionTypeLabels } from '@/utils/labels'
import { formatMoney, toMinor } from '@/utils/money'
import type { Transaction, TransactionType } from '@/types'

type DialogKind = 'income' | 'expense' | 'transfer' | 'adjustment'

const store = useAppStore()
const transactions = ref<Transaction[]>([])
const loading = ref(false)
const saving = ref(false)
const total = ref(0)
const page = ref(1)
const pageSize = 20

const filterMember = ref<number | undefined>(undefined)
const filterAsset = ref<number | undefined>(undefined)
const filterType = ref<string | undefined>(undefined)

const dialogVisible = ref(false)
const kind = ref<DialogKind>('income')

const dialogTitles: Record<DialogKind, string> = {
  income: '记录收入',
  expense: '记录支出',
  transfer: '账户转账',
  adjustment: '余额调整',
}

const form = reactive({
  asset_id: 0,
  from_asset_id: 0,
  to_asset_id: 0,
  category: '',
  amount_yuan: '',
  transaction_time: '',
  remark: '',
})

const categories = computed(() =>
  kind.value === 'income' ? store.incomeCategories : store.expenseCategories,
)

function assetLabel(id: number): string {
  const asset = store.assets.find((item) => item.id === id)
  if (!asset) {
    return `#${id}`
  }
  return `${store.memberName(asset.owner_member_id)} · ${store.accountName(asset.account_id)} · ${asset.name}`
}

function signedAmount(transaction: Transaction): number {
  switch (transaction.type) {
    case 'EXPENSE':
    case 'TRANSFER_OUT':
      return -transaction.amount
    case 'ADJUSTMENT':
      return transaction.amount
    default:
      return transaction.amount
  }
}

function tagType(type: TransactionType): 'success' | 'warning' | 'primary' | 'info' {
  switch (type) {
    case 'INCOME':
    case 'TRANSFER_IN':
      return 'success'
    case 'EXPENSE':
    case 'TRANSFER_OUT':
      return 'warning'
    case 'ADJUSTMENT':
      return 'info'
  }
}

async function load() {
  if (!store.householdId) {
    return
  }
  loading.value = true
  try {
    const result = await listTransactions(store.householdId, {
      ownerMemberId: filterMember.value,
      assetId: filterAsset.value,
      type: filterType.value,
      limit: pageSize,
      offset: (page.value - 1) * pageSize,
    })
    transactions.value = result.items
    total.value = result.total
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    loading.value = false
  }
}

function reload() {
  page.value = 1
  void load()
}

function onPage(next: number) {
  page.value = next
  void load()
}

function openDialog(next: DialogKind) {
  kind.value = next
  form.asset_id = store.assets[0]?.id ?? 0
  form.from_asset_id = store.assets[0]?.id ?? 0
  form.to_asset_id = store.assets[1]?.id ?? 0
  form.category = ''
  form.amount_yuan = ''
  form.transaction_time = ''
  form.remark = ''
  dialogVisible.value = true
}

async function submit() {
  const amount = toMinor(form.amount_yuan)
  if (kind.value !== 'adjustment' && amount <= 0) {
    ElMessage.warning('请输入正确的金额')
    return
  }
  if (kind.value === 'adjustment' && amount === 0) {
    ElMessage.warning('调整金额不能为 0')
    return
  }
  saving.value = true
  try {
    if (kind.value === 'income') {
      await recordIncome(store.householdId, {
        asset_id: form.asset_id,
        category: form.category || null,
        amount,
        transaction_time: form.transaction_time,
        remark: form.remark,
      })
    } else if (kind.value === 'expense') {
      await recordExpense(store.householdId, {
        asset_id: form.asset_id,
        category: form.category || null,
        amount,
        transaction_time: form.transaction_time,
        remark: form.remark,
      })
    } else if (kind.value === 'adjustment') {
      await recordAdjustment(store.householdId, {
        asset_id: form.asset_id,
        amount,
        transaction_time: form.transaction_time,
        remark: form.remark,
      })
    } else {
      if (form.from_asset_id === form.to_asset_id) {
        ElMessage.warning('转出与转入资产不能相同')
        return
      }
      await transfer(store.householdId, {
        from_asset_id: form.from_asset_id,
        to_asset_id: form.to_asset_id,
        amount,
        transaction_time: form.transaction_time,
        remark: form.remark,
      })
    }
    dialogVisible.value = false
    await Promise.all([load(), store.refreshAssets()])
    ElMessage.success('已提交')
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    saving.value = false
  }
}

onMounted(async () => {
  await store.refreshAssets()
  await load()
})
</script>

<style scoped>
.pager {
  margin-top: 12px;
  display: flex;
  justify-content: flex-end;
}
</style>
