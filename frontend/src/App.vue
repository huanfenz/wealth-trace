<!-- 应用根组件：登录等公共页直接裸渲染；其余页面套左侧导航 + 顶部标题/家庭名外壳。 -->
<template>
  <router-view v-if="isPublicPage" />

  <el-container v-else class="app-shell">
    <el-aside width="216px" class="aside">
      <div class="brand">
        <span class="brand-name">财迹</span>
        <span class="brand-sub">wealth-trace</span>
      </div>
      <el-menu :default-active="activeMenu" router class="menu">
        <el-menu-item index="/dashboard">
          <el-icon><DataAnalysis /></el-icon>
          <span>资产总览</span>
        </el-menu-item>
        <el-menu-item index="/statistics">
          <el-icon><TrendCharts /></el-icon>
          <span>收支统计</span>
        </el-menu-item>
        <el-menu-item index="/members">
          <el-icon><User /></el-icon>
          <span>成员管理</span>
        </el-menu-item>
        <el-menu-item index="/accounts">
          <el-icon><CreditCard /></el-icon>
          <span>账户管理</span>
        </el-menu-item>
        <el-menu-item index="/assets">
          <el-icon><Wallet /></el-icon>
          <span>资产管理</span>
        </el-menu-item>
        <el-menu-item index="/transactions">
          <el-icon><Tickets /></el-icon>
          <span>收支与转账</span>
        </el-menu-item>
        <el-menu-item index="/categories">
          <el-icon><CollectionTag /></el-icon>
          <span>分类管理</span>
        </el-menu-item>
        <el-menu-item index="/investments">
          <el-icon><Clock /></el-icon>
          <span>定投管理</span>
        </el-menu-item>
        <el-menu-item index="/data-backup">
          <el-icon><FolderOpened /></el-icon>
          <span>数据备份</span>
        </el-menu-item>
      </el-menu>
    </el-aside>

    <el-container class="main-shell">
      <el-header class="header">
        <div class="header-leading">
          <el-button class="mobile-menu-button" text circle aria-label="打开导航菜单" @click="menuVisible = true">
            <el-icon><Menu /></el-icon>
          </el-button>
          <div class="title">{{ pageTitle }}</div>
        </div>
        <div class="header-trailing">
          <div class="household">{{ householdName }}</div>
          <el-button
            v-if="loggedIn"
            class="logout-button"
            text
            :loading="loggingOut"
            aria-label="登出"
            @click="handleLogout"
          >
            <el-icon><SwitchButton /></el-icon>
            <span class="logout-username">{{ sessionUsername }}</span>
          </el-button>
        </div>
      </el-header>
      <el-main>
        <div class="page">
          <router-view v-if="store.ready" />
          <el-skeleton v-else :rows="8" animated />
        </div>
      </el-main>
    </el-container>
  </el-container>

  <el-drawer v-model="menuVisible" title="财迹 · 家庭资产管理" direction="ltr" size="min(82vw, 300px)" class="mobile-nav-drawer">
    <el-menu :default-active="activeMenu" router class="menu mobile-menu" @select="menuVisible = false">
      <el-menu-item index="/dashboard"><el-icon><DataAnalysis /></el-icon><span>资产总览</span></el-menu-item>
      <el-menu-item index="/statistics"><el-icon><TrendCharts /></el-icon><span>收支统计</span></el-menu-item>
      <el-menu-item index="/members"><el-icon><User /></el-icon><span>成员管理</span></el-menu-item>
      <el-menu-item index="/accounts"><el-icon><CreditCard /></el-icon><span>账户管理</span></el-menu-item>
      <el-menu-item index="/assets"><el-icon><Wallet /></el-icon><span>资产管理</span></el-menu-item>
      <el-menu-item index="/transactions"><el-icon><Tickets /></el-icon><span>收支与转账</span></el-menu-item>
      <el-menu-item index="/categories"><el-icon><CollectionTag /></el-icon><span>分类管理</span></el-menu-item>
      <el-menu-item index="/investments"><el-icon><Clock /></el-icon><span>定投管理</span></el-menu-item>
      <el-menu-item index="/data-backup"><el-icon><FolderOpened /></el-icon><span>数据备份</span></el-menu-item>
    </el-menu>
  </el-drawer>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'
import { Menu } from '@element-plus/icons-vue'

import { getSessionUsername, hasSession, logout } from '@/api/auth'
import { useAppStore } from '@/stores/app'

const store = useAppStore()
const route = useRoute()
const router = useRouter()
const menuVisible = ref(false)

const activeMenu = computed(() => route.path) // 当前高亮菜单由路由路径决定
const pageTitle = computed(() => (route.meta.title as string) ?? '财迹')
const householdName = computed(() => store.household?.name ?? '')
// 登录页等无外壳公共页：meta.public 标记。
const isPublicPage = computed(() => route.meta.public === true)
const loggedIn = computed(() => hasSession())
const sessionUsername = computed(() => getSessionUsername())
const loggingOut = ref(false)

// 首次挂载时初始化全局数据（元数据、家庭、成员等），失败给出提示。
onMounted(async () => {
  try {
    await store.initialize()
  } catch (error) {
    ElMessage.error((error as Error).message)
  }
})

async function handleLogout() {
  if (loggingOut.value) return
  loggingOut.value = true
  try {
    await logout()
    await router.push('/login')
  } finally {
    loggingOut.value = false
  }
}
</script>

<style scoped>
.app-shell {
  min-height: 100%;
}

.aside {
  background: #ffffff;
  border-right: 1px solid #ebeef5;
  display: flex;
  flex-direction: column;
}

.brand {
  padding: 20px 20px 12px;
  display: flex;
  flex-direction: column;
}

.brand-name {
  font-size: 22px;
  font-weight: 700;
  color: var(--wt-brand);
}

.brand-sub {
  font-size: 12px;
  color: #909399;
  letter-spacing: 1px;
}

.menu {
  border-right: none;
}

.header {
  background: #ffffff;
  border-bottom: 1px solid #ebeef5;
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.header-leading { display: flex; align-items: center; min-width: 0; gap: 8px; }
.header-trailing { display: flex; align-items: center; gap: 10px; min-width: 0; }
.logout-button { color: #909399; padding: 4px 8px; }
.logout-username { font-size: 13px; }
.mobile-menu-button { display: none; flex: none; font-size: 20px; }

.header .title {
  font-size: 16px;
  font-weight: 600;
}

.header .household {
  color: #909399;
  font-size: 13px;
}

.el-main {
  background: var(--wt-bg);
  min-width: 0;
}

@media (max-width: 767px) {
  .app-shell { min-height: 100dvh; height: auto; }
  .aside { display: none; }
  .main-shell { min-width: 0; width: 100%; }
  .header { height: 56px; padding: 0 14px; }
  .mobile-menu-button { display: inline-flex; }
  .header .title { font-size: 16px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .header .household { max-width: 35vw; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 12px; }
  .el-main { padding: 12px; }
  .mobile-menu { border-right: none; }
}
</style>
