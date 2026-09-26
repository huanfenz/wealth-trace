<!-- 收支与转账页：分页展示交易流水，并支持记收入/支出、资产间转账与余额调整。 -->
<template>
  <div>
    <div class="toolbar">
      <el-button type="warning" :icon="Minus" @click="openDialog('expense')">记支出</el-button>
      <el-button type="success" :icon="Plus" @click="openDialog('income')">记收入</el-button>
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
        <el-table-column prop="transaction_time" label="时间" width="150" />
        <el-table-column label="成员" width="110">
          <template #default="{ row }">{{ store.memberName(row.owner_member_id) }}</template>
        </el-table-column>
        <el-table-column label="交易" min-width="220">
          <template #default="{ row }">
            <strong>{{ row.title }}</strong>
            <div class="muted">{{ row.subtitle }}</div>
          </template>
        </el-table-column>
        <el-table-column label="类型" width="54" align="center">
          <template #default="{ row }">
            <el-tag size="small" :type="tagType(row.type)">
              {{ typeLabel(row) }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="金额" width="150" align="center">
          <template #default="{ row }">
            <span class="amount" :class="row.direction === 'OUT' ? 'negative' : row.direction === 'IN' ? 'positive' : ''">
              {{ amountText(row) }}
            </span>
          </template>
        </el-table-column>
        <el-table-column prop="remark" label="备注" min-width="240" show-overflow-tooltip />
        <el-table-column label="操作" width="110" align="right">
          <template #default="{ row }">
            <el-button v-if="row.type === 'INCOME' || row.type === 'EXPENSE'" link type="primary" @click="openCategoryEditor(row)">编辑</el-button>
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

    <el-dialog v-model="editDialogVisible" title="编辑交易分类" width="420px" :close-on-click-modal="false">
      <el-form label-width="70px">
        <el-form-item label="分类">
          <el-select v-model="editCategoryId" clearable :value-on-clear="null" filterable placeholder="未分类" style="width: 100%">
            <el-option
              v-if="unavailableCategory"
              :label="unavailableCategory.label"
              :value="unavailableCategory.id"
              disabled
            />
            <el-option v-for="category in editCategories" :key="category.id" :label="category.name" :value="category.id" />
          </el-select>
        </el-form-item>
      </el-form>
      <p v-if="editingTransaction?.category_id === null && editingTransaction.category" class="edit-hint">
        原分类“{{ editingTransaction.category }}”来自旧记录，请选择现有分类或清空。
      </p>
      <p v-if="unavailableCategory" class="edit-hint">原分类已停用，请选择可用分类或清空。</p>
      <template #footer>
        <el-button :disabled="editSaving" @click="editDialogVisible = false">取消</el-button>
        <el-button type="primary" :loading="editSaving" :disabled="!canSaveCategory" @click="saveCategory">保存</el-button>
      </template>
    </el-dialog>

    <el-dialog
      v-model="deleteDialogVisible"
      title="删除交易记录"
      width="480px"
      :close-on-click-modal="false"
      :close-on-press-escape="!deleting"
      :show-close="!deleting"
    >
      <p>将删除{{ deleteTargets.length === 1 ? '这条' : `选中的 ${deleteTargets.length} 条` }}交易记录。</p>
      <p v-if="deleteHasTransfer">选中记录包含转账；每笔转账作为一个整体删除。</p>
      <el-radio-group v-model="deleteMode" class="delete-options">
        <el-radio value="records">仅删除交易记录（资产余额保持不变）</el-radio>
        <el-radio value="rollback">删除交易记录并回滚资产余额</el-radio>
      </el-radio-group>
      <p class="delete-hint">仅删除记录后，流水合计可能与资产余额不一致。删除后无法恢复。</p>
      <template #footer>
        <el-button :disabled="deleting" @click="deleteDialogVisible = false">取消</el-button>
        <el-button type="danger" :loading="deleting" :disabled="!deleteMode" @click="confirmDelete">确认删除</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
// 职责：展示和新增完整业务交易；收入、支出、转账与调整使用对应业务接口。
import { computed, onMounted, reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { Edit, Minus, Plus, Sort } from '@element-plus/icons-vue'

import {
  deleteTransaction,
  listAssetTransactions,
  listTransactions,
  recordAdjustment,
  recordExpense,
  recordIncome,
  transfer,
  updateTransactionCategory,
} from '@/api'
import { useAppStore } from '@/stores/app'
import { transactionTypeLabels } from '@/utils/labels'
import { formatMoney, toMinor } from '@/utils/money'
import type { Transaction, TransactionType } from '@/types'

type DialogKind = 'income' | 'expense' | 'transfer' | 'adjustment' // 弹窗形态
type DeleteMode = 'records' | 'rollback'

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
const deleteDialogVisible = ref(false)
const deleteTargets = ref<Transaction[]>([])
const deleteMode = ref<DeleteMode | null>(null)
const deleting = ref(false)
const deleteHasTransfer = computed(() => deleteTargets.value.some((item) => item.type === 'TRANSFER'))
const editDialogVisible = ref(false)
const editingTransaction = ref<Transaction | null>(null)
const editCategoryId = ref<number | null>(null)
const editSaving = ref(false)
const editCategories = computed(() =>
  editingTransaction.value?.type === 'INCOME' ? store.incomeCategories : store.expenseCategories,
)
const unavailableCategory = computed(() => {
  const transaction = editingTransaction.value
  if (transaction?.category_id === null || !transaction) return null
  if (editCategories.value.some((category) => category.id === transaction.category_id)) return null
  return { id: transaction.category_id, label: `${transaction.category ?? `#${transaction.category_id}`}（已停用）` }
})
const canSaveCategory = computed(() =>
  !!editingTransaction.value && !editSaving.value &&
  (editCategoryId.value === null || editCategories.value.some((category) => category.id === editCategoryId.value)),
)

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
function amountText(transaction: Transaction): string {
  const amount = formatMoney(transaction.amount)
  if (transaction.direction === 'IN') return `+ ${amount}`
  if (transaction.direction === 'OUT') return `- ${amount}`
  return amount
}

// 交易类型对应的标签配色。
function tagType(type: TransactionType): 'success' | 'warning' | 'primary' | 'info' {
  switch (type) {
    case 'INCOME':
    case 'TRANSFER':
    case 'INVESTMENT':
      return 'success'
    case 'EXPENSE':
      return 'warning'
    case 'ADJUSTMENT':
      return 'info'
  }
}

function typeLabel(transaction: Transaction): string {
  return transactionTypeLabels[transaction.type]
}

// 按筛选条件与分页参数查询流水，offset 由当前页码换算。
async function load() {
  if (!store.householdId) {
    return
  }
  loading.value = true
  try {
    const query = {
      ownerMemberId: filterMember.value,
      assetId: filterAsset.value,
      type: filterType.value,
      limit: pageSize,
      offset: (page.value - 1) * pageSize,
    }
    const result = filterAsset.value
      ? await listAssetTransactions(filterAsset.value, store.householdId, query)
      : await listTransactions(store.householdId, query)
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

function openCategoryEditor(transaction: Transaction) {
  editingTransaction.value = transaction
  editCategoryId.value = transaction.category_id
  editDialogVisible.value = true
}

async function saveCategory() {
  if (!canSaveCategory.value || !editingTransaction.value) return
  editSaving.value = true
  try {
    await updateTransactionCategory(editingTransaction.value.id, editCategoryId.value)
    editDialogVisible.value = false
    await load()
    ElMessage.success('分类已更新')
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    editSaving.value = false
  }
}

function remove(transaction: Transaction) {
  deleteTargets.value = [transaction]
  deleteMode.value = null
  deleteDialogVisible.value = true
}

function bulkRemove() {
  if (!selectedTransactions.value.length) return
  deleteTargets.value = [...selectedTransactions.value]
  deleteMode.value = null
  deleteDialogVisible.value = true
}

async function confirmDelete() {
  if (!deleteMode.value || deleting.value) return
  const operations = deleteTargets.value
  deleting.value = true
  try {
    const results = await Promise.allSettled(
      operations.map((transaction) => deleteTransaction(transaction.id, deleteMode.value === 'rollback')),
    )
    await Promise.all([load(), store.refreshAssets()])
    if (transactions.value.length === 0 && page.value > 1) {
      page.value = Math.min(page.value - 1, Math.max(1, Math.ceil(total.value / pageSize)))
      await load()
    }
    const failures = results.filter((result) => result.status === 'rejected')
    if (operations.length === 1 && failures.length) {
      ElMessage.error((failures[0] as PromiseRejectedResult).reason.message)
      return
    }
    deleteDialogVisible.value = false
    if (operations.length === 1) {
      ElMessage.success('已删除')
    } else {
      ElMessage[failures.length ? 'warning' : 'success'](
        `批量删除完成：成功 ${operations.length - failures.length} 项，失败 ${failures.length} 项`,
      )
    }
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    deleting.value = false
  }
}
</script>

<style scoped>
.pager {
  margin-top: 12px;
  display: flex;
  justify-content: flex-end;
}
.selection-count { color: #909399; font-size: 13px; }
.delete-options { display: flex; flex-direction: column; align-items: flex-start; gap: 8px; margin: 8px 0; }
.delete-hint { color: #909399; font-size: 13px; }
.edit-hint { color: #909399; font-size: 13px; }
</style>
