// 应用入口：创建 Vue 实例并注册 Pinia、路由、Element Plus 及全局图标，最后挂载到 #app。
import { createApp } from 'vue'
import { createPinia } from 'pinia'
import ElementPlus from 'element-plus'
import zhCn from 'element-plus/es/locale/lang/zh-cn'
import 'element-plus/dist/index.css'
import * as ElementPlusIconsVue from '@element-plus/icons-vue'
import 'dayjs/locale/zh-cn'
import type { Component } from 'vue'

import App from './App.vue'
import router from './router'
import { setUnauthorizedHandler } from './api/http'
import './styles.css'

// 任意 API 返回 401（会话过期/被吊销）时清除本地状态并回到登录页；
// 登录相关接口的 401 由 http.ts 自行排除，不会触发跳转。
setUnauthorizedHandler(() => {
  router.push('/login')
})

const app = createApp(App)

// 将 Element Plus 的全部图标注册为全局组件，模板中可直接使用 <DataAnalysis /> 等。
for (const [name, component] of Object.entries(ElementPlusIconsVue)) {
  app.component(name, component as Component)
}

app.use(createPinia())
app.use(router)
app.use(ElementPlus, { locale: zhCn })
app.mount('#app')
