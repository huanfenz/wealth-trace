<template>
  <div class="backup-page">
    <el-card shadow="never">
      <template #header>
        <div class="card-title">导出数据</div>
      </template>
      <p>下载当前家庭的完整数据库备份，可用于保存或迁移到另一台设备。</p>
      <el-button type="primary" :loading="exporting" @click="downloadBackup">
        下载数据库备份
      </el-button>
    </el-card>

    <el-card shadow="never">
      <template #header>
        <div class="card-title">导入数据</div>
      </template>
      <el-alert
        title="导入会整体替换当前数据库。操作前系统会自动保留一份当前数据备份。"
        type="warning"
        :closable="false"
        show-icon
      />
      <p>选择财迹导出的 .db 文件，最大 100 MiB。较早版本的备份会自动升级。</p>
      <input
        ref="fileInput"
        class="file-input"
        type="file"
        accept=".db,application/vnd.sqlite3,application/x-sqlite3"
        @change="onFileSelected"
      />
      <el-button :loading="importing" @click="fileInput?.click()">选择备份文件</el-button>
      <span v-if="selectedFile" class="file-name">{{ selectedFile.name }}</span>
      <el-button
        v-if="selectedFile"
        type="danger"
        :loading="importing"
        @click="restoreBackup"
      >
        导入并替换
      </el-button>
    </el-card>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { exportDatabase, importDatabase } from '@/api'

const maxImportBytes = 100 * 1024 * 1024
const exporting = ref(false)
const importing = ref(false)
const selectedFile = ref<File | null>(null)
const fileInput = ref<HTMLInputElement>()

function onFileSelected(event: Event) {
  const input = event.target as HTMLInputElement
  const file = input.files?.[0] ?? null
  if (file && file.size > maxImportBytes) {
    ElMessage.error('数据库文件不能超过 100 MiB')
    input.value = ''
    selectedFile.value = null
    return
  }
  selectedFile.value = file
}

async function downloadBackup() {
  exporting.value = true
  try {
    const blob = await exportDatabase()
    const url = URL.createObjectURL(blob)
    const link = document.createElement('a')
    link.href = url
    link.download = `wealth-trace-${new Date().toISOString().slice(0, 10)}.db`
    link.click()
    URL.revokeObjectURL(url)
    ElMessage.success('数据库备份已下载')
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    exporting.value = false
  }
}

async function restoreBackup() {
  if (!selectedFile.value) return
  try {
    await ElMessageBox.confirm(
      `导入“${selectedFile.value.name}”会替换当前数据库中的全部数据，是否继续？`,
      '确认导入数据库',
      { type: 'warning', confirmButtonText: '导入并替换', cancelButtonText: '取消' },
    )
  } catch {
    return
  }

  importing.value = true
  try {
    await importDatabase(selectedFile.value)
    ElMessage.success('数据库导入成功，正在刷新页面')
    window.setTimeout(() => window.location.reload(), 500)
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    importing.value = false
  }
}
</script>

<style scoped>
.backup-page {
  display: grid;
  gap: 16px;
  max-width: 900px;
}

.card-title {
  font-weight: 600;
}

.backup-page p {
  margin: 0 0 16px;
  color: #606266;
}

.file-input {
  display: none;
}

.file-name {
  display: inline-block;
  margin: 0 12px;
  color: #606266;
}
</style>
