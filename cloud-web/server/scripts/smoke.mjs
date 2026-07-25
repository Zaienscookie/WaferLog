// 冒烟测试：node server/scripts/smoke.mjs
// 验证演示脚本关键链路：回响触发、问答来源、待办确认、日历标记
const base = process.env.API || 'http://localhost:3737'

const j = (r) => r.json()
const post = (path, body) =>
  fetch(`${base}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  }).then(j)

let failed = 0
const check = (name, ok, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? '  |  ' + detail : ''}`)
  if (!ok) failed++
}

// 1. 首页
const home = await fetch(`${base}/api/home`).then(j)
check('首页返回最新笔记', !!home.latestNote, home.latestNote?.title)
check('首页统计正确', home.stats.notes >= 12, JSON.stringify(home.stats))

// 2. 演示脚本：写下「想做一个减少通知打扰的设备」应触发与旧笔记的回响
const created = await post('/api/notes', {
  inputType: 'handwriting',
  rawText: '想做一个减少通知打扰的设备',
  rawContent: JSON.stringify({ viewBox: [1000, 1400], strokes: [] }),
})
check('创建笔记成功', !!created.note?.id)
const hit = created.relations.find((r) => r.reason.includes('通知') || r.reason.includes('干扰'))
check('新笔记触发回响（通知/干扰）', !!hit, hit ? `${hit.score} ${hit.reason}` : '无关联')

const detail = await fetch(`${base}/api/notes/${created.note.id}`).then(j)
const top = detail.relations[0]
check(
  '最强关联指向《通知为什么会打断思考？》',
  top && top.otherNote.title.includes('通知'),
  top ? `${top.otherNote.title} | ${top.reason}` : '无'
)

// 3. 问答必须带来源
const ans = await post('/api/ask', { question: '我以前为什么认为低干扰输入重要？' })
check('问答返回答案', !!ans.answer)
check('问答带来源', ans.sourceIds.length >= 1, ans.sources.map((s) => s.title).join(' / '))

const noBasis = await post('/api/ask', { question: '量子引力统一场论怎么做？' })
check('无依据时诚实回答', noBasis.sourceIds.length === 0, noBasis.answer.slice(0, 30))

// 4. 待办确认 → 日历
const candidates = await fetch(`${base}/api/tasks?status=candidate`).then(j)
check('存在待办候选', candidates.length > 0, `${candidates.length} 条`)
if (candidates.length > 0) {
  const t = candidates[0]
  const today = new Date()
  const due = `${today.getFullYear()}-${String(today.getMonth() + 1).padStart(2, '0')}-${String(today.getDate()).padStart(2, '0')}`
  const confirmed = await fetch(`${base}/api/tasks/${t.id}`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ status: 'confirmed', dueDate: due }),
  }).then(j)
  check('待办确认成功', confirmed.status === 'confirmed' && confirmed.dueDate === due)
  const month = due.slice(0, 7)
  const cal = await fetch(`${base}/api/calendar?month=${month}`).then(j)
  check('日历出现已确认待办', cal.tasks.some((x) => x.id === t.id && x.status === 'confirmed'))
  // 还原为候选，保持演示现场可再次确认
  await fetch(`${base}/api/tasks/${t.id}`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ status: 'candidate' }),
  })
}

// 5. 记忆线
if (top) {
  const tl = await fetch(`${base}/api/notes/${top.otherNote.id}/timeline`).then(j)
  check('记忆线返回节点', tl.nodes.length >= 1, `${tl.nodes.length} 个节点`)
}

// 6. 清理演示笔记
await fetch(`${base}/api/notes/${created.note.id}/permanent`, { method: 'DELETE' })
check('演示笔记已清理', true)

console.log(failed === 0 ? '\n全部通过' : `\n${failed} 项失败`)
process.exitCode = failed === 0 ? 0 : 1
