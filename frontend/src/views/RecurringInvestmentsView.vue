<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" :icon="Plus" @click="openCreate()">新增定投</el-button>
      <el-button :icon="Refresh" @click="load">刷新</el-button>
      <el-button :disabled="selectedPlans.length === 0" @click="bulkPause">批量暂停</el-button>
      <el-button type="success" :disabled="selectedPlans.length === 0" @click="bulkExecute">批量执行</el-button>
      <el-button type="danger" :disabled="selectedPlans.length === 0" @click="bulkRemove">批量删除</el-button>
      <span v-if="selectedPlans.length" class="selection-count">已选 {{ selectedPlans.length }} 项</span>
    </div>
    <el-card shadow="never">
      <el-table :data="plans" v-loading="loading" @selection-change="selectedPlans = $event">
        <el-table-column type="selection" width="48" :selectable="planSelectable" />
        <el-table-column label="股票基金" min-width="160"><template #default="{ row }">{{ assetName(row.target_asset_id) }}</template></el-table-column>
        <el-table-column label="付款资产" min-width="160"><template #default="{ row }">{{ assetName(row.source_asset_id) }}</template></el-table-column>
        <el-table-column label="定投金额" width="130" align="right"><template #default="{ row }">{{ formatMoney(row.amount) }}</template></el-table-column>
        <el-table-column label="周期" width="135"><template #default="{ row }">{{ cadence(row) }}</template></el-table-column>
        <el-table-column prop="next_due_date" label="下次执行" width="125" />
        <el-table-column label="状态" width="95"><template #default="{ row }"><el-tag :type="row.status === 'ACTIVE' ? 'success' : row.status === 'PAUSED' ? 'warning' : 'info'">{{ statusLabel(row.status) }}</el-tag></template></el-table-column>
        <el-table-column label="操作" width="250" fixed="right">
          <template #default="{ row }">
            <el-button link type="primary" @click="showHistory(row)">执行记录</el-button>
            <template v-if="row.status !== 'DELETED'">
              <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
              <el-button link @click="toggleStatus(row)">{{ row.status === 'ACTIVE' ? '暂停' : '恢复' }}</el-button>
              <el-button link type="danger" @click="remove(row)">删除</el-button>
            </template>
          </template>
        </el-table-column>
      </el-table>
      <el-empty v-if="!loading && plans.length === 0" description="还没有定投计划" />
    </el-card>

    <el-dialog v-model="dialogVisible" :title="editing ? '编辑定投计划' : '新增定投计划'" width="540px">
      <el-form label-width="110px">
        <el-form-item label="股票基金" required>
          <el-select v-model="form.target_asset_id" filterable style="width:100%" @change="targetChanged">
            <el-option v-for="asset in targetAssets" :key="asset.id" :value="asset.id" :label="assetLabel(asset)" />
          </el-select>
        </el-form-item>
        <el-form-item label="付款资产" required>
          <el-select v-model="form.source_asset_id" filterable style="width:100%">
            <el-option v-for="asset in sourceAssets" :key="asset.id" :value="asset.id" :label="assetLabel(asset)" />
          </el-select>
        </el-form-item>
        <el-form-item label="每次金额(元)" required><el-input v-model="form.amount_yuan" placeholder="如 500.00" /></el-form-item>
        <el-form-item label="定投周期" required>
          <el-select v-model="form.frequency" style="width:100%">
            <el-option label="每天" value="DAILY" /><el-option label="每周" value="WEEKLY" />
            <el-option label="每两周" value="BIWEEKLY" /><el-option label="每月" value="MONTHLY" />
          </el-select>
        </el-form-item>
        <el-form-item v-if="form.frequency === 'WEEKLY' || form.frequency === 'BIWEEKLY'" label="星期" required>
          <el-select v-model="form.weekday" style="width:100%">
            <el-option v-for="(label, index) in weekdays" :key="index" :label="label" :value="index + 1" />
          </el-select>
        </el-form-item>
        <el-form-item v-if="form.frequency === 'MONTHLY'" label="每月几号" required>
          <el-select v-model="form.month_day" style="width:100%"><el-option v-for="day in 28" :key="day" :label="`${day} 日`" :value="day" /></el-select>
        </el-form-item>
        <el-form-item label="起始日期" required>
          <el-date-picker v-model="form.start_date" type="date" value-format="YYYY-MM-DD" :disabled-date="pastDate" style="width:100%" />
        </el-form-item>
      </el-form>
      <template #footer><el-button @click="dialogVisible = false">取消</el-button><el-button type="primary" :loading="saving" @click="submit">保存</el-button></template>
    </el-dialog>

    <el-dialog v-model="historyVisible" :title="`${historyPlan ? assetName(historyPlan.target_asset_id) : ''} · 执行记录`" width="760px">
      <el-table :data="executions" v-loading="historyLoading" max-height="480">
        <el-table-column prop="scheduled_date" label="预定日期" width="125" />
        <el-table-column label="金额" width="130" align="right"><template #default="{ row }">{{ formatMoney(row.amount) }}</template></el-table-column>
        <el-table-column label="付款资产" min-width="140"><template #default="{ row }">{{ assetName(row.source_asset_id) }}</template></el-table-column>
        <el-table-column label="结果" width="100"><template #default="{ row }"><el-tag :type="executionTag(row.status)">{{ executionLabel(row.status) }}</el-tag></template></el-table-column>
        <el-table-column prop="failure_reason" label="说明" min-width="150" show-overflow-tooltip />
        <el-table-column label="操作" width="90"><template #default="{ row }"><el-button v-if="row.status === 'FAILED' && historyPlan?.status !== 'DELETED'" link type="primary" @click="retry(row.id)">重试</el-button></template></el-table-column>
      </el-table>
      <el-empty v-if="!historyLoading && executions.length === 0" description="暂无执行记录" />
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref } from 'vue'
import { useRoute } from 'vue-router'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Plus, Refresh } from '@element-plus/icons-vue'
import { createInvestmentPlan, deleteInvestmentPlan, executeInvestmentPlan, listInvestmentExecutions, listInvestmentPlans, retryInvestmentExecution, setInvestmentPlanStatus, updateInvestmentPlan } from '@/api'
import { useAppStore } from '@/stores/app'
import { formatMoney, toMinor } from '@/utils/money'
import type { Asset, RecurringInvestmentExecution, RecurringInvestmentPlan } from '@/types'

const store = useAppStore()
const route = useRoute()
const plans = ref<RecurringInvestmentPlan[]>([])
const selectedPlans = ref<RecurringInvestmentPlan[]>([])
const executions = ref<RecurringInvestmentExecution[]>([])
const loading = ref(false)
const saving = ref(false)
const historyLoading = ref(false)
const dialogVisible = ref(false)
const historyVisible = ref(false)
const editing = ref<number | null>(null)
const historyPlan = ref<RecurringInvestmentPlan | null>(null)
const weekdays = ['周一', '周二', '周三', '周四', '周五', '周六', '周日']
const form = reactive({ target_asset_id: 0, source_asset_id: 0, amount_yuan: '', frequency: 'DAILY', weekday: 1, month_day: 1, start_date: today() })
const targetAssets = computed(() => store.assets.filter((a) => a.asset_type === 'STOCK_FUND' && a.status === 'ACTIVE'))
const selectedTarget = computed(() => store.assets.find((a) => a.id === form.target_asset_id))
const sourceAssets = computed(() => store.assets.filter((a) => a.status === 'ACTIVE' && a.asset_type !== 'LIABILITY' && a.id !== form.target_asset_id && a.owner_member_id === selectedTarget.value?.owner_member_id))

function today() { const d = new Date(); return `${d.getFullYear()}-${String(d.getMonth()+1).padStart(2,'0')}-${String(d.getDate()).padStart(2,'0')}` }
function pastDate(date: Date) { const d=new Date(date.getFullYear(),date.getMonth(),date.getDate()); return d < new Date(`${today()}T00:00:00`) }
function assetName(id: number | null) { return store.assets.find((a) => a.id === id)?.name ?? (id === null ? '资产已删除' : `#${id}`) }
function assetLabel(a: Asset) { return `${store.memberName(a.owner_member_id)} · ${store.accountName(a.account_id)} · ${a.name}` }
function statusLabel(status: string) { return status === 'ACTIVE' ? '运行中' : status === 'PAUSED' ? '已暂停' : '已删除' }
function planSelectable(plan: RecurringInvestmentPlan) { return plan.status !== 'DELETED' }
function cadence(p: RecurringInvestmentPlan) {
  if (p.frequency === 'DAILY') return '每天'
  if (p.frequency === 'WEEKLY') return `每周${weekdays[(p.weekday ?? 1)-1]}`
  if (p.frequency === 'BIWEEKLY') return `每两周${weekdays[(p.weekday ?? 1)-1]}`
  return `每月${p.month_day}日`
}
function executionLabel(status: string) { return status === 'SUCCESS' ? '成功' : status === 'FAILED' ? '失败' : '已撤销' }
function executionTag(status: string): 'success'|'danger'|'info' { return status === 'SUCCESS' ? 'success' : status === 'FAILED' ? 'danger' : 'info' }
function resetForm() { form.target_asset_id=targetAssets.value[0]?.id ?? 0; form.source_asset_id=0; form.amount_yuan=''; form.frequency='DAILY'; form.weekday=1; form.month_day=1; form.start_date=today() }
function openCreate(targetAssetId?: number) {
  editing.value=null
  resetForm()
  if (targetAssetId && targetAssets.value.some((asset) => asset.id === targetAssetId)) {
    form.target_asset_id=targetAssetId
  }
  targetChanged()
  dialogVisible.value=true
}
function openEdit(p: RecurringInvestmentPlan) {
  if (p.target_asset_id === null || p.source_asset_id === null) { ElMessage.warning('关联资产已删除，无法编辑此计划'); return }
  editing.value=p.id; form.target_asset_id=p.target_asset_id; form.source_asset_id=p.source_asset_id
  form.amount_yuan=(p.amount/100).toFixed(2); form.frequency=p.frequency; form.weekday=p.weekday ?? 1
  form.month_day=p.month_day ?? 1; form.start_date=p.start_date; dialogVisible.value=true
}
function targetChanged() { if (!sourceAssets.value.some((a) => a.id === form.source_asset_id)) form.source_asset_id=sourceAssets.value[0]?.id ?? 0 }
function requestBody() {
  const amount=toMinor(form.amount_yuan)
  if (amount <= 0) throw new Error('请输入大于 0 的定投金额')
  if (!form.target_asset_id || !form.source_asset_id) throw new Error('请选择股票基金和付款资产')
  return { target_asset_id:form.target_asset_id, source_asset_id:form.source_asset_id, amount, frequency:form.frequency,
    weekday:form.frequency==='WEEKLY'||form.frequency==='BIWEEKLY' ? form.weekday : null,
    month_day:form.frequency==='MONTHLY' ? form.month_day : null, start_date:form.start_date }
}
async function load() { if (!store.householdId) return; loading.value=true; try { plans.value=await listInvestmentPlans(store.householdId) } catch(e) { ElMessage.error((e as Error).message) } finally { loading.value=false } }
async function submit() { let body:Record<string,unknown>; try { body=requestBody() } catch(e) { ElMessage.warning((e as Error).message); return }
  saving.value=true; try { if(editing.value===null) await createInvestmentPlan(store.householdId,body); else await updateInvestmentPlan(editing.value,body)
    dialogVisible.value=false; await Promise.all([load(),store.refreshAssets()]); ElMessage.success('定投计划已保存')
  } catch(e) { ElMessage.error((e as Error).message) } finally { saving.value=false } }
async function toggleStatus(p:RecurringInvestmentPlan) { try { await setInvestmentPlanStatus(p.id,p.status==='ACTIVE'?'PAUSED':'ACTIVE'); await load() } catch(e) { ElMessage.error((e as Error).message) } }
async function remove(p:RecurringInvestmentPlan) { try { await ElMessageBox.confirm('删除后停止后续定投，已生成的交易和执行记录会保留。','删除定投计划',{type:'warning'}) } catch { return }
  try { await deleteInvestmentPlan(p.id); await load(); ElMessage.success('计划已删除') } catch(e) { ElMessage.error((e as Error).message) } }
async function bulkPause() {
  const targets = selectedPlans.value.filter((plan) => plan.status === 'ACTIVE')
  if (!targets.length) { ElMessage.warning('所选计划均已暂停'); return }
  try { await ElMessageBox.confirm(`确认暂停选中的 ${targets.length} 个定投计划？`, '批量暂停定投', { type: 'warning' }) } catch { return }
  const results = await Promise.allSettled(targets.map((plan) => setInvestmentPlanStatus(plan.id, 'PAUSED')))
  await load()
  const failed = results.filter((result) => result.status === 'rejected').length
  ElMessage[failed ? 'warning' : 'success'](`批量暂停完成：成功 ${targets.length - failed} 个，失败 ${failed} 个`)
}
async function bulkExecute() {
  const targets = selectedPlans.value.filter((plan) => plan.status !== 'DELETED')
  if (!targets.length) return
  try {
    await ElMessageBox.confirm(
      `将立即执行选中的 ${targets.length} 个定投计划，并生成今天的转账流水。每个计划每天只能执行一次；余额不足等情况会记录为执行失败。确认继续？`,
      '批量执行定投',
      { type: 'warning', confirmButtonText: '立即执行', cancelButtonText: '取消' },
    )
  } catch { return }
  const results = await Promise.allSettled(targets.map((plan) => executeInvestmentPlan(plan.id)))
  await Promise.all([load(), store.refreshAssets()])
  const succeeded = results.filter((result) => result.status === 'fulfilled' && result.value.status === 'SUCCESS').length
  const failed = targets.length - succeeded
  const message = `批量执行完成：成功 ${succeeded} 个，失败 ${failed} 个，可在各计划的执行记录中查看详情`
  if (failed) ElMessage.warning(message)
  else ElMessage.success(message)
}
async function bulkRemove() {
  const targets = selectedPlans.value.filter((plan) => plan.status !== 'DELETED')
  if (!targets.length) return
  try { await ElMessageBox.confirm(`删除后将停止这 ${targets.length} 个计划的后续定投，已生成的交易和执行记录会保留。确认删除？`, '批量删除定投计划', { type: 'warning', confirmButtonText: '确认删除', confirmButtonClass: 'el-button--danger' }) } catch { return }
  const results = await Promise.allSettled(targets.map((plan) => deleteInvestmentPlan(plan.id)))
  await load()
  const failed = results.filter((result) => result.status === 'rejected').length
  ElMessage[failed ? 'warning' : 'success'](`批量删除完成：成功 ${targets.length - failed} 个，失败 ${failed} 个`)
}
async function showHistory(p:RecurringInvestmentPlan) { historyPlan.value=p; historyVisible.value=true; historyLoading.value=true
  try { executions.value=await listInvestmentExecutions(p.id) } catch(e) { ElMessage.error((e as Error).message) } finally { historyLoading.value=false } }
async function retry(id:number) { try { await retryInvestmentExecution(id); await Promise.all([showHistory(historyPlan.value!),load(),store.refreshAssets()]); ElMessage.success('定投已重试') } catch(e) { ElMessage.error((e as Error).message) } }
onMounted(async()=>{
  await store.refreshAssets()
  await load()
  const requestedAssetId = Number(route.query.target_asset_id)
  if (Number.isSafeInteger(requestedAssetId) && requestedAssetId > 0 &&
      targetAssets.value.some((asset) => asset.id === requestedAssetId)) {
    openCreate(requestedAssetId)
  }
})
</script>

<style scoped>
.toolbar { margin-bottom: 14px; }
.selection-count { color: #909399; font-size: 13px; }
</style>
