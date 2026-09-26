<!-- 应用根组件：左侧导航菜单 + 顶部标题/家庭名，主体区域渲染路由页面。 -->
<template>
  <el-container class="app-shell">
    <el-aside width="216px" class="aside">
      <div class="brand">
        <span class="brand-name">财迹</span>
        <span class="brand-sub">wealth-trace</span>
      </div>
      <el-menu :default-active="activeMenu" router class="menu">
        <el-menu-item index="/dashboard">
          <el-icon><DataAnalysis /></el-icon>
          <span>家庭总览</span>
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
        <el-menu-item index="/statistics">
          <el-icon><TrendCharts /></el-icon>
          <span>收支统计</span>
        </el-menu-item>
        <el-menu-item index="/data-backup">
          <el-icon><FolderOpened /></el-icon>
          <span>数据备份</span>
        </el-menu-item>
      </el-menu>
    </el-aside>

    <el-container>
      <el-header class="header">
        <div class="title">{{ pageTitle }}</div>
        <div class="household">{{ householdName }}</div>
      </el-header>
      <el-main>
        <div class="page">
          <router-view v-if="store.ready" />
          <el-skeleton v-else :rows="8" animated />
        </div>
      </el-main>
    </el-container>
  </el-container>
</template>

<script setup lang="ts">
import { computed, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { ElMessage } from 'element-plus'

import { useAppStore } from '@/stores/app'

const store = useAppStore()
const route = useRoute()

const activeMenu = computed(() => route.path) // 当前高亮菜单由路由路径决定
const pageTitle = computed(() => (route.meta.title as string) ?? '财迹')
const householdName = computed(() => store.household?.name ?? '')

// 首次挂载时初始化全局数据（元数据、家庭、成员等），失败给出提示。
onMounted(async () => {
  try {
    await store.initialize()
  } catch (error) {
    ElMessage.error((error as Error).message)
  }
})
</script>

<style scoped>
.app-shell {
  height: 100%;
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
}
</style>
