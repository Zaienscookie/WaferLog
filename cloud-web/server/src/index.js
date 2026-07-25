import express from 'express'
import path from 'node:path'
import { existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { api } from './routes.js'
import { seedIfEmpty } from './seed.js'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const app = express()
const PORT = Number(process.env.PORT || 2001)
const COMPAT_PORT = Number(process.env.COMPAT_PORT || 0)

app.use('/api/device/audio', express.raw({ type: '*/*', limit: '4mb' }))
app.use(express.json({ limit: '8mb' }))
app.use('/api', api)
app.use('/api', (req, res) => res.status(404).json({ error: '接口不存在' }))

const distDir = path.join(__dirname, '..', '..', 'web', 'dist')
if (existsSync(distDir)) {
  app.use(express.static(distDir))
  app.get('*', (req, res) => res.sendFile(path.join(distDir, 'index.html')))
}

if (seedIfEmpty()) {
  console.log('[waferlog] 首次启动，已注入示例笔记')
}

const listen = (port, label = '') => {
  app.listen(port, () => {
    console.log(`[waferlog] Web/API 已启动: http://localhost:${port}${label}`)
  })
}

listen(PORT)
if (COMPAT_PORT > 0 && COMPAT_PORT !== PORT) {
  listen(COMPAT_PORT, '（板端兼容入口）')
}
