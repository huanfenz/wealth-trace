<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" :icon="Plus" @click="openCreate">新增成员</el-button>
      <div class="spacer" />
    </div>

    <el-card shadow="never">
      <el-table :data="store.members" v-loading="loading">
        <el-table-column prop="name" label="姓名" />
        <el-table-column label="角色">
          <template #default="{ row }">
            <el-tag :type="row.role === 'OWNER' ? 'success' : 'info'" size="small">
              {{ memberRoleLabels[row.role as MemberRole] }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="状态">
          <template #default="{ row }">
            <el-tag :type="row.status === 'ACTIVE' ? 'success' : 'info'" size="small">
              {{ memberStatusLabels[row.status as MemberStatus] }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="120" align="right">
          <template #default="{ row }">
            <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
          </template>
        </el-table-column>
      </el-table>
    </el-card>

    <el-dialog v-model="dialogVisible" :title="editing ? '编辑成员' : '新增成员'" width="420px">
      <el-form :model="form" label-width="72px">
        <el-form-item label="姓名" required>
          <el-input v-model="form.name" maxlength="100" placeholder="成员姓名" />
        </el-form-item>
        <el-form-item label="角色">
          <el-select v-model="form.role" style="width: 100%">
            <el-option label="户主" value="OWNER" />
            <el-option label="成员" value="MEMBER" />
          </el-select>
        </el-form-item>
        <el-form-item label="状态">
          <el-select v-model="form.status" style="width: 100%">
            <el-option label="正常" value="ACTIVE" />
            <el-option label="停用" value="INACTIVE" />
          </el-select>
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
import { reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { Plus } from '@element-plus/icons-vue'

import { createMember, updateMember } from '@/api'
import { useAppStore } from '@/stores/app'
import { memberRoleLabels, memberStatusLabels } from '@/utils/labels'
import type { Member, MemberRole, MemberStatus } from '@/types'

const store = useAppStore()
const loading = ref(false)
const saving = ref(false)
const dialogVisible = ref(false)
const editing = ref<Member | null>(null)

const form = reactive({
  name: '',
  role: 'MEMBER' as MemberRole,
  status: 'ACTIVE' as MemberStatus,
})

function openCreate() {
  editing.value = null
  form.name = ''
  form.role = 'MEMBER'
  form.status = 'ACTIVE'
  dialogVisible.value = true
}

function openEdit(member: Member) {
  editing.value = member
  form.name = member.name
  form.role = member.role
  form.status = member.status
  dialogVisible.value = true
}

async function save() {
  if (!form.name.trim()) {
    ElMessage.warning('请填写姓名')
    return
  }
  saving.value = true
  try {
    if (editing.value) {
      await updateMember(editing.value.id, { ...form })
    } else {
      await createMember(store.householdId, { ...form })
    }
    dialogVisible.value = false
    await store.refreshMembers()
    ElMessage.success('已保存')
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    saving.value = false
  }
}
</script>
