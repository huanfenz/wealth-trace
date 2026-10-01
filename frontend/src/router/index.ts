// 路由配置：定义各页面路径与懒加载组件，meta.title 用于顶部标题栏展示；
// 全局前置守卫在后端启用鉴权且本地无会话时重定向到 /login。
import { createRouter, createWebHistory } from 'vue-router'

import { getAuthStatus, hasSession } from '@/api/auth'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', redirect: '/dashboard' }, // 根路径默认跳转总览
    {
      path: '/login',
      name: 'login',
      component: () => import('@/views/LoginView.vue'),
      meta: { title: '登录', public: true }, // 登录页不套应用外壳
    },
    {
      path: '/dashboard',
      name: 'dashboard',
      component: () => import('@/views/DashboardView.vue'),
      meta: { title: '资产总览' },
    },
    {
      path: '/statistics',
      name: 'statistics',
      component: () => import('@/views/StatisticsView.vue'),
      meta: { title: '收支统计' },
    },
    {
      path: '/members',
      name: 'members',
      component: () => import('@/views/MembersView.vue'),
      meta: { title: '成员管理' },
    },
    {
      path: '/accounts',
      name: 'accounts',
      component: () => import('@/views/AccountsView.vue'),
      meta: { title: '账户管理' },
    },
    {
      path: '/assets',
      name: 'assets',
      component: () => import('@/views/AssetsView.vue'),
      meta: { title: '资产管理' },
    },
    {
      path: '/transactions',
      name: 'transactions',
      component: () => import('@/views/TransactionsView.vue'),
      meta: { title: '收支与转账' },
    },
    {
      path: '/categories',
      name: 'categories',
      component: () => import('@/views/CategoriesView.vue'),
      meta: { title: '收支分类' },
    },
    {
      path: '/investments',
      name: 'investments',
      component: () => import('@/views/RecurringInvestmentsView.vue'),
      meta: { title: '定投管理' },
    },
    {
      path: '/data-backup',
      name: 'data-backup',
      component: () => import('@/views/DatabaseBackupView.vue'),
      meta: { title: '数据备份' },
    },
  ],
})

// 鉴权是否启用只需向后端查询一次，结果进程内缓存。
let authEnabledCache: boolean | null = null
let authEnabledCheck: Promise<boolean> | null = null

async function isAuthEnabled(): Promise<boolean> {
  if (authEnabledCache !== null) return authEnabledCache
  authEnabledCheck ??= getAuthStatus()
    .then((status) => {
      authEnabledCache = status.enabled
      return status.enabled
    })
    .catch(() => {
      // 后端暂不可达时放行导航；若鉴权确实开启，后续请求的 401 拦截会把
      // 用户送到登录页，行为最终收敛一致。
      authEnabledCache = false
      return false
    })
  return authEnabledCheck
}

router.beforeEach(async (to) => {
  if (to.path === '/login' || hasSession()) {
    return true
  }
  if (!(await isAuthEnabled())) {
    return true
  }
  return { path: '/login', query: to.fullPath === '/' ? {} : { redirect: to.fullPath } }
})

export default router
