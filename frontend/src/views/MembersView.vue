<!-- 成员管理页：列出家庭成员，可通过弹窗新增或编辑（姓名、角色、状态）。 -->
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
// 职责：展示/维护成员列表；弹窗表单用于新增或编辑，保存后刷新 store 中的成员缓存。
import { reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { Plus } from '@element-plus/icons-vue'

import { createMember, updateMember } from '@/api'
import { useAppStore } from '@/stores/app'
import { memberRoleLabels, memberStatusLabels } from '@/utils/labels'
import type { Member, MemberRole, MemberStatus } from '@/types'

const store = useAppStore()
const loading = ref(false)        // 列表加载中
const saving = ref(false)         // 表单提交中
const dialogVisible = ref(false)  // 弹窗显隐
const editing = ref<Member | null>(null) // 当前编辑对象，null 表示新增

// 弹窗表单数据。
const form = reactive({
  name: '',
  role: 'MEMBER' as MemberRole,
  status: 'ACTIVE' as MemberStatus,
})

// 打开「新增」弹窗并重置表单为默认值。
function openCreate() {
  editing.value = null
  form.name = ''
  form.role = 'MEMBER'
  form.status = 'ACTIVE'
  dialogVisible.value = true
}

// 打开「编辑」弹窗并回填选中成员的数据。
function openEdit(member: Member) {
  editing.value = member
  form.name = member.name
  form.role = member.role
  form.status = member.status
  dialogVisible.value = true
}

// 校验后按编辑/新增分支提交，成功后关闭弹窗并刷新成员列表。
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
