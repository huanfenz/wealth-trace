<!-- 登录/初始化页：未建号时引导创建管理员账号，已建号时登录；附修改密码入口。 -->
<template>
  <div class="login-page">
    <el-card class="login-card">
      <div class="brand">
        <span class="brand-name">财迹</span>
        <span class="brand-sub">wealth-trace · 家庭资产管理</span>
      </div>

      <template v-if="loading">
        <el-skeleton :rows="3" animated />
      </template>

      <template v-else-if="!status.enabled">
        <el-result icon="info" title="鉴权未启用" sub-title="后端运行在免鉴权模式（本地开发），无需登录。">
          <template #extra>
            <el-button type="primary" @click="enterApp">进入系统</el-button>
          </template>
        </el-result>
      </template>

      <el-form v-else label-position="top" @submit.prevent="submit">
        <h2 class="mode-title">{{ needsSetup ? '创建管理员账号' : '登录' }}</h2>
        <p v-if="needsSetup" class="mode-hint">首次使用，请先设置管理员用户名与密码。</p>

        <el-form-item label="用户名">
          <el-input v-model="username" placeholder="用户名" autocomplete="username" :disabled="submitting" />
        </el-form-item>
        <el-form-item label="密码">
          <el-input
            v-model="password"
            type="password"
            show-password
            :placeholder="needsSetup ? '至少 8 位' : '密码'"
            :autocomplete="needsSetup ? 'new-password' : 'current-password'"
            :disabled="submitting"
            @keyup.enter="submit"
          />
        </el-form-item>
        <el-form-item v-if="needsSetup" label="确认密码">
          <el-input
            v-model="confirmPassword"
            type="password"
            show-password
            placeholder="再输入一次密码"
            autocomplete="new-password"
            :disabled="submitting"
            @keyup.enter="submit"
          />
        </el-form-item>

        <el-button class="submit" type="primary" native-type="submit" :loading="submitting">
          {{ needsSetup ? '创建账号并登录' : '登录' }}
        </el-button>

        <div v-if="!needsSetup" class="aux-actions">
          <el-button link type="primary" @click="passwordDialogVisible = true">修改密码</el-button>
        </div>
      </el-form>
    </el-card>

    <el-dialog v-model="passwordDialogVisible" title="修改密码" width="min(92vw, 380px)">
      <el-form label-position="top" @submit.prevent="submitChangePassword">
        <el-form-item label="旧密码">
          <el-input v-model="changeForm.oldPassword" type="password" show-password autocomplete="current-password" />
        </el-form-item>
        <el-form-item label="新密码（至少 8 位）">
          <el-input v-model="changeForm.newPassword" type="password" show-password autocomplete="new-password" />
        </el-form-item>
        <el-form-item label="确认新密码">
          <el-input v-model="changeForm.confirmPassword" type="password" show-password autocomplete="new-password" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="passwordDialogVisible = false">取消</el-button>
        <el-button type="primary" :loading="changing" @click="submitChangePassword">确认修改</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { onMounted, reactive, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'

import {
  changePassword,
  getAuthStatus,
  login,
  setupAccount,
  type AuthStatus,
} from '@/api/auth'

const route = useRoute()
const router = useRouter()

const loading = ref(true)
const status = ref<AuthStatus>({ enabled: true, initialized: false })
const needsSetup = ref(false)

const username = ref('')
const password = ref('')
const confirmPassword = ref('')
const submitting = ref(false)

const passwordDialogVisible = ref(false)
const changing = ref(false)
const changeForm = reactive({ oldPassword: '', newPassword: '', confirmPassword: '' })

onMounted(async () => {
  try {
    status.value = await getAuthStatus()
    needsSetup.value = status.value.enabled && !status.value.initialized
  } catch (error) {
    // 后端不可达：展示错误提示，待恢复后手动重试。
    ElMessage.error((error as Error).message)
  } finally {
    loading.value = false
  }
})

function enterApp() {
  router.push(redirectTarget())
}

function redirectTarget(): string {
  const target = route.query.redirect
  return typeof target === 'string' && target.startsWith('/') ? target : '/dashboard'
}

async function submit() {
  if (submitting.value) return
  if (!username.value.trim()) {
    ElMessage.warning('请输入用户名')
    return
  }
  if (password.value.length < 8) {
    ElMessage.warning('密码至少 8 位')
    return
  }
  if (needsSetup.value && password.value !== confirmPassword.value) {
    ElMessage.warning('两次输入的密码不一致')
    return
  }
  submitting.value = true
  try {
    if (needsSetup.value) {
      await setupAccount(username.value.trim(), password.value)
    } else {
      await login(username.value.trim(), password.value)
    }
    await router.push(redirectTarget())
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    submitting.value = false
  }
}

async function submitChangePassword() {
  if (changing.value) return
  if (!changeForm.oldPassword || changeForm.newPassword.length < 8) {
    ElMessage.warning('请填写旧密码，新密码至少 8 位')
    return
  }
  if (changeForm.newPassword !== changeForm.confirmPassword) {
    ElMessage.warning('两次输入的新密码不一致')
    return
  }
  changing.value = true
  try {
    await changePassword(changeForm.oldPassword, changeForm.newPassword)
    ElMessage.success('密码已修改，全部会话已吊销，请用新密码重新登录')
    passwordDialogVisible.value = false
    changeForm.oldPassword = ''
    changeForm.newPassword = ''
    changeForm.confirmPassword = ''
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    changing.value = false
  }
}
</script>

<style scoped>
.login-page {
  min-height: 100dvh;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--wt-bg);
  padding: 16px;
}

.login-card {
  width: min(92vw, 380px);
}

.brand {
  display: flex;
  flex-direction: column;
  align-items: center;
  margin-bottom: 8px;
}

.brand-name {
  font-size: 26px;
  font-weight: 700;
  color: var(--wt-brand);
}

.brand-sub {
  font-size: 12px;
  color: #909399;
  margin-top: 4px;
  letter-spacing: 1px;
}

.mode-title {
  margin: 8px 0 4px;
  font-size: 16px;
  text-align: center;
}

.mode-hint {
  margin: 0 0 8px;
  font-size: 13px;
  color: #909399;
  text-align: center;
}

.submit {
  width: 100%;
}

.aux-actions {
  margin-top: 12px;
  text-align: center;
}
</style>
