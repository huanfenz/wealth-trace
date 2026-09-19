<!-- 账户管理页：按成员筛选账户列表，并通过弹窗新增/编辑账户（属主、类型、机构等）。 -->
<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" :icon="Plus" @click="openCreate">新增账户</el-button>
      <el-select v-model="filterMember" clearable placeholder="按成员筛选" style="width: 180px" @change="load">
        <el-option v-for="m in store.members" :key="m.id" :label="m.name" :value="m.id" />
      </el-select>
      <div class="spacer" />
    </div>

    <el-card shadow="never">
      <el-table :data="accounts" v-loading="loading">
        <el-table-column prop="name" label="账户" />
        <el-table-column label="类型" width="100">
          <template #default="{ row }">{{ accountTypeLabels[row.type as AccountType] }}</template>
        </el-table-column>
        <el-table-column label="属主" width="120">
          <template #default="{ row }">{{ store.memberName(row.owner_member_id) }}</template>
        </el-table-column>
        <el-table-column prop="institution_name" label="机构" />
        <el-table-column label="资产数" width="90" align="right">
          <template #default="{ row }">{{ row.asset_count }}</template>
        </el-table-column>
        <el-table-column label="余额" width="150" align="right">
          <template #default="{ row }"><AmountText :value="row.balance" /></template>
        </el-table-column>
        <el-table-column label="状态" width="90">
          <template #default="{ row }">
            <el-tag :type="row.enabled ? 'success' : 'info'" size="small">
              {{ row.enabled ? '启用' : '停用' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="90" align="right">
          <template #default="{ row }">
            <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
          </template>
        </el-table-column>
      </el-table>
    </el-card>

    <el-dialog v-model="dialogVisible" :title="editing ? '编辑账户' : '新增账户'" width="480px">
      <el-form :model="form" label-width="80px">
        <el-form-item label="属主" required>
          <el-select v-model="form.owner_member_id" style="width: 100%">
            <el-option v-for="m in store.members" :key="m.id" :label="m.name" :value="m.id" />
          </el-select>
        </el-form-item>
        <el-form-item label="账户名" required>
          <el-input v-model="form.name" maxlength="100" placeholder="如 工商银行工资卡" />
        </el-form-item>
        <el-form-item label="类型">
          <el-select v-model="form.type" style="width: 100%">
            <el-option
              v-for="(label, value) in accountTypeLabels"
              :key="value"
              :label="label"
              :value="value"
            />
          </el-select>
        </el-form-item>
        <el-form-item label="机构">
          <el-input v-model="form.institution_name" maxlength="100" />
        </el-form-item>
        <el-form-item label="脱敏账号">
          <el-input v-model="form.account_no_masked" maxlength="64" placeholder="如 ****1234" />
        </el-form-item>
        <el-form-item label="备注">
          <el-input v-model="form.remark" maxlength="500" />
        </el-form-item>
        <el-form-item label="启用">
          <el-switch v-model="form.enabled" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" :loading="saving" @click="save">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
// 职责：展示/维护账户列表；支持按成员筛选，弹窗新增或编辑账户，保存后同步刷新 store 缓存。
import { onMounted, reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { Plus } from '@element-plus/icons-vue'

import AmountText from '@/components/AmountText.vue'
import { createAccount, listAccounts, updateAccount } from '@/api'
import { useAppStore } from '@/stores/app'
import { accountTypeLabels } from '@/utils/labels'
import type { Account, AccountType } from '@/types'

const store = useAppStore()
const accounts = ref<Account[]>([])       // 当前筛选条件下的账户列表
const loading = ref(false)                // 列表加载中
const saving = ref(false)                 // 表单提交中
const dialogVisible = ref(false)          // 弹窗显隐
const editing = ref<Account | null>(null) // 当前编辑对象，null 表示新增
const filterMember = ref<number | undefined>(undefined) // 按成员筛选

// 弹窗表单数据（余额等统计字段不在此维护）。
const form = reactive({
  owner_member_id: 0,
  name: '',
  type: 'BANK' as AccountType,
  institution_name: '',
  account_no_masked: '',
  remark: '',
  enabled: true,
})

// 按筛选条件拉取账户列表，并同步 store 中的账户缓存（供其他页面下拉使用）。
async function load() {
  if (!store.householdId) {
    return
  }
  loading.value = true
  try {
    accounts.value = await listAccounts(store.householdId, filterMember.value)
    await store.refreshAccounts()
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    loading.value = false
  }
}

// 打开「新增」弹窗，默认属主为第一位成员。
function openCreate() {
  editing.value = null
  form.owner_member_id = store.members[0]?.id ?? 0
  form.name = ''
  form.type = 'BANK'
  form.institution_name = ''
  form.account_no_masked = ''
  form.remark = ''
  form.enabled = true
  dialogVisible.value = true
}

// 打开「编辑」弹窗并回填选中账户数据（可空字段以空串兜底）。
function openEdit(account: Account) {
  editing.value = account
  form.owner_member_id = account.owner_member_id
  form.name = account.name
  form.type = account.type
  form.institution_name = account.institution_name ?? ''
  form.account_no_masked = account.account_no_masked ?? ''
  form.remark = account.remark ?? ''
  form.enabled = account.enabled
  dialogVisible.value = true
}

// 校验属主与账户名后，按编辑/新增分支提交，成功后重新加载列表。
async function save() {
  if (!form.owner_member_id) {
    ElMessage.warning('请选择属主')
    return
  }
  if (!form.name.trim()) {
    ElMessage.warning('请填写账户名')
    return
  }
  saving.value = true
  try {
    const payload = { ...form }
    if (editing.value) {
      await updateAccount(editing.value.id, payload)
    } else {
      await createAccount(store.householdId, payload)
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

onMounted(load)
</script>
