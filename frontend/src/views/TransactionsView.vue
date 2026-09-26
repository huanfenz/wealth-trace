<!-- 收支与转账页：分页展示交易流水，并支持记收入/支出、资产间转账与余额调整。 -->
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
      <el-button type="danger" :disabled="selectedTransactions.length === 0" @click="bulkRemove">批量删除</el-button>
      <span v-if="selectedTransactions.length" class="selection-count">已选 {{ selectedTransactions.length }} 项</span>
    </div>

    <el-card shadow="never">
      <el-table :data="transactions" v-loading="loading" @selection-change="selectedTransactions = $event">
        <el-table-column type="selection" width="48" />
        <el-table-column prop="transaction_time" label="时间" width="170" />
        <el-table-column label="成员" width="110">
          <template #default="{ row }">{{ store.memberName(row.owner_member_id) }}</template>
        </el-table-column>
        <el-table-column label="资产" width="160">
          <template #default="{ row }">{{ store.assetName(row.asset_id) }}</template>
        </el-table-column>
        <el-table-column label="类型" width="110">
          <template #default="{ row }">
            <el-tag
              size="small"
              :type="row.transfer_group_id !== null ? 'primary' : tagType(row.type)"
            >
              {{ typeLabel(row) }}
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
        <el-table-column label="操作" width="80" align="right">
          <template #default="{ row }">
            <el-button link type="danger" @click="remove(row)">删除</el-button>
          </template>
        </el-table-column>
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
          <el-select v-model="form.category_id" clearable style="width: 100%">
            <el-option v-for="c in categories" :key="c.id" :label="c.name" :value="c.id" />
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
// 职责：展示/新增交易流水；同一弹窗按 kind 复用为收入、支出、转账、调整四种表单并分派到对应接口。
import { computed, onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Edit, Minus, Plus, Sort } from '@element-plus/icons-vue'

import AmountText from '@/components/AmountText.vue'
import {
  deleteTransaction,
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

type DialogKind = 'income' | 'expense' | 'transfer' | 'adjustment' // 弹窗形态

const store = useAppStore()
const transactions = ref<Transaction[]>([]) // 当前页流水
const selectedTransactions = ref<Transaction[]>([])
const loading = ref(false)                  // 列表加载中
const saving = ref(false)                   // 表单提交中
const total = ref(0)                        // 总条数
const page = ref(1)                         // 当前页码
const pageSize = 20                         // 每页条数

const filterMember = ref<number | undefined>(undefined) // 按成员筛选
const filterAsset = ref<number | undefined>(undefined)  // 按资产筛选
const filterType = ref<string | undefined>(undefined)   // 按交易类型筛选

const dialogVisible = ref(false)      // 弹窗显隐
const kind = ref<DialogKind>('income') // 当前弹窗形态

// 各形态对应的弹窗标题。
const dialogTitles: Record<DialogKind, string> = {
  income: '记录收入',
  expense: '记录支出',
  transfer: '账户转账',
  adjustment: '余额调整',
}

// 弹窗表单：金额以「元」字符串编辑，提交时用 toMinor 转为「分」。
const form = reactive({
  asset_id: 0,
  from_asset_id: 0,
  to_asset_id: 0,
  category_id: null as number | null,
  amount_yuan: '',
  transaction_time: '',
  remark: '',
})

// 分类下拉选项随弹窗形态切换：收入用收入分类，否则用支出分类。
const categories = computed(() =>
  kind.value === 'income' ? store.incomeCategories : store.expenseCategories,
)

// 组装资产下拉的完整标签：成员 · 账户 · 资产名。
function assetLabel(id: number): string {
  const asset = store.assets.find((item) => item.id === id)
  if (!asset) {
    return `#${id}`
  }
  return `${store.memberName(asset.owner_member_id)} · ${store.accountName(asset.account_id)} · ${asset.name}`
}

// 将金额按交易方向转为带符号数值：支出/转出取负，收入/转入/调整保持原值。
function signedAmount(transaction: Transaction): number {
  switch (transaction.type) {
    case 'EXPENSE':
    case 'TRANSFER_OUT':
    case 'ASSET_PURCHASE':
      return -transaction.amount
    case 'ADJUSTMENT':
      return transaction.amount
    default:
      return transaction.amount
  }
}

// 交易类型对应的标签配色。
function tagType(type: TransactionType): 'success' | 'warning' | 'primary' | 'info' {
  switch (type) {
    case 'INCOME':
    case 'TRANSFER_IN':
      return 'success'
    case 'EXPENSE':
    case 'TRANSFER_OUT':
    case 'ASSET_PURCHASE':
      return 'warning'
    case 'ADJUSTMENT':
      return 'info'
  }
}

// 类型文案：转账流水（有 transfer_group_id）统一标注为「转账」并附方向，
// 便于在收支列表里一眼识别出这是一次转账而非普通收支。
function typeLabel(transaction: Transaction): string {
  if (transaction.transfer_group_id !== null) {
    return transaction.type === 'TRANSFER_OUT' ? '转账·转出' : '转账·转入'
  }
  return transactionTypeLabels[transaction.type]
}

// 按筛选条件与分页参数查询流水，offset 由当前页码换算。
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

// 筛选条件变化时回到第一页再查询。
function reload() {
  page.value = 1
  void load()
}

// 翻页查询。
function onPage(next: number) {
  page.value = next
  void load()
}

// 打开指定形态的弹窗并重置表单默认值。
function openDialog(next: DialogKind) {
  kind.value = next
  form.asset_id = store.assets[0]?.id ?? 0
  form.from_asset_id = store.assets[0]?.id ?? 0
  form.to_asset_id = store.assets[1]?.id ?? 0
  form.category_id = null
  form.amount_yuan = ''
  form.transaction_time = ''
  form.remark = ''
  dialogVisible.value = true
}

// 校验金额（非调整须为正、调整不得为 0），按形态调用对应接口，成功后刷新列表与资产缓存。
async function submit() {
  const amount = toMinor(form.amount_yuan) // 元 -> 分
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
        category_id: form.category_id,
        amount,
        transaction_time: form.transaction_time,
        remark: form.remark,
      })
    } else if (kind.value === 'expense') {
      await recordExpense(store.householdId, {
        asset_id: form.asset_id,
        category_id: form.category_id,
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

// 删除流水：删除后后端会回滚资产余额；若为转账则成对删除两条，删除前明确提示。
async function remove(transaction: Transaction) {
  const isTransfer = transaction.transfer_group_id !== null
  const detail = isTransfer
    ? '该记录属于一次转账，删除后将同时删除与之配对的两条转账记录，并回滚两端资产余额。'
    : '删除后将回滚该资产余额。'
  try {
    await ElMessageBox.confirm(`${detail}此操作不可恢复，确认删除？`, '确认删除', {
      type: 'warning',
      confirmButtonText: '确认删除',
      cancelButtonText: '取消',
      confirmButtonClass: 'el-button--danger',
    })
  } catch {
    return
  }
  try {
    await deleteTransaction(transaction.id)
    await Promise.all([load(), store.refreshAssets()])
    ElMessage.success('已删除')
  } catch (error) {
    ElMessage.error((error as Error).message)
  }
}

async function bulkRemove() {
  const targets = selectedTransactions.value
  if (!targets.length) return
  try {
    await ElMessageBox.confirm(
      `确认删除选中的 ${targets.length} 条流水？删除转账记录时会同时删除配对流水，并回滚相关资产余额。此操作不可恢复。`,
      '批量删除流水',
      { type: 'warning', confirmButtonText: '确认删除', cancelButtonText: '取消', confirmButtonClass: 'el-button--danger' },
    )
  } catch { return }
  const seenGroups = new Set<number>()
  const operations = targets.filter((transaction) => {
    if (transaction.transfer_group_id === null) return true
    if (seenGroups.has(transaction.transfer_group_id)) return false
    seenGroups.add(transaction.transfer_group_id)
    return true
  })
  const results = await Promise.allSettled(operations.map((transaction) => deleteTransaction(transaction.id)))
  await Promise.all([load(), store.refreshAssets()])
  const failed = results.filter((result) => result.status === 'rejected').length
  ElMessage[failed ? 'warning' : 'success'](`批量删除完成：成功 ${operations.length - failed} 项，失败 ${failed} 项`)
}
</script>

<style scoped>
.pager {
  margin-top: 12px;
  display: flex;
  justify-content: flex-end;
}
.selection-count { color: #909399; font-size: 13px; }
</style>
