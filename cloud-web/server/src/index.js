import express from 'express'
import path from 'node:path'
import fs from 'node:fs'
import { fileURLToPath } from 'node:url'
import { api } from './routes.js'
import { seedIfEmpty } from './seed.js'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const app = express()
const PORT = process.env.PORT || 3737

app.use(express.json({ limit: '8mb' }))
app.use('/api', api)
app.use('/api', (req, res) => res.status(404).json({ error: '接口不存在' }))

// 生产模式：直接托管前端构建产物
const distDir = path.join(__dirname, '..', '..', 'web', 'dist')
if (fs.existsSync(distDir)) {
  app.use(express.static(distDir))
  app.get('*', (req, res) => res.sendFile(path.join(distDir, 'index.html')))
}

if (seedIfEmpty()) {
  console.log('[waferlog] 首次启动，已注入示例笔记')
}

app.listen(PORT, () => {
  console.log(`[waferlog] API 已启动: http://localhost:${PORT}`)
})
