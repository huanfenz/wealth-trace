// 应用入口：创建 Vue 实例并注册 Pinia、路由、Element Plus 及全局图标，最后挂载到 #app。
import { createApp } from 'vue'
import { createPinia } from 'pinia'
import ElementPlus from 'element-plus'
import 'element-plus/dist/index.css'
import * as ElementPlusIconsVue from '@element-plus/icons-vue'
import type { Component } from 'vue'

import App from './App.vue'
import router from './router'
import './styles.css'

const app = createApp(App)

// 将 Element Plus 的全部图标注册为全局组件，模板中可直接使用 <DataAnalysis /> 等。
for (const [name, component] of Object.entries(ElementPlusIconsVue)) {
  app.component(name, component as Component)
}

app.use(createPinia())
app.use(router)
app.use(ElementPlus)
app.mount('#app')
