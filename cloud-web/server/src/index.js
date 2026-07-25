import express from 'express'
import { api } from './routes.js'
import { seedIfEmpty } from './seed.js'

const app = express()
const PORT = Number(process.env.PORT || 2001)

app.use('/api/device/audio', express.raw({ type: '*/*', limit: '4mb' }))
app.use(express.json({ limit: '8mb' }))
app.use('/api', api)
app.use('/api', (req, res) => res.status(404).json({ error: '接口不存在' }))

if (seedIfEmpty()) {
  console.log('[waferlog] 首次启动，已注入示例笔记')
}

app.listen(PORT, () => {
  console.log(`[waferlog] API 已启动: http://localhost:${PORT}`)
})
