<!-- 修改密码对话框：要求已登录（/api/auth/password 需要有效会话），
     成功后后端吊销全部会话，清空本地令牌并回到登录页。 -->
<template>
  <el-dialog
    :model-value="modelValue"
    title="修改密码"
    width="min(92vw, 380px)"
    @update:model-value="emit('update:modelValue', $event)"
  >
    <el-form label-position="top" @submit.prevent="submitChangePassword">
      <el-form-item label="旧密码">
        <el-input v-model="form.oldPassword" type="password" show-password autocomplete="current-password" />
      </el-form-item>
      <el-form-item label="新密码（至少 8 位）">
        <el-input v-model="form.newPassword" type="password" show-password autocomplete="new-password" />
      </el-form-item>
      <el-form-item label="确认新密码">
        <el-input v-model="form.confirmPassword" type="password" show-password autocomplete="new-password" />
      </el-form-item>
    </el-form>
    <template #footer>
      <el-button @click="close">取消</el-button>
      <el-button type="primary" :loading="changing" @click="submitChangePassword">确认修改</el-button>
    </template>
  </el-dialog>
</template>

<script setup lang="ts">
import { reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'

import { changePassword, logout } from '@/api/auth'

defineProps<{ modelValue: boolean }>()
const emit = defineEmits<{ (event: 'update:modelValue', value: boolean): void }>()

const router = useRouter()
const changing = ref(false)
const form = reactive({ oldPassword: '', newPassword: '', confirmPassword: '' })

function close() {
  emit('update:modelValue', false)
}

async function submitChangePassword() {
  if (changing.value) return
  if (!form.oldPassword || form.newPassword.length < 8) {
    ElMessage.warning('请填写旧密码，新密码至少 8 位')
    return
  }
  if (form.newPassword !== form.confirmPassword) {
    ElMessage.warning('两次输入的新密码不一致')
    return
  }
  changing.value = true
  try {
    await changePassword(form.oldPassword, form.newPassword)
    close()
    // 后端已吊销全部会话（含当前）；logout 无条件清空本地令牌并忽略远端错误。
    await logout()
    ElMessage.success('密码已修改，请用新密码重新登录')
    await router.push('/login')
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    changing.value = false
  }
}
</script>
