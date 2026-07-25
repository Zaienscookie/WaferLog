// 规则 AI 层：无外部 API 时的可运行兜底。
// 关联 = 关键词重叠 + 主题重合 + 时间距离；问答 = 抽取式且必带来源；待办 = 动作词 + 日期解析。
// 若配置了 OPENAI_COMPAT 环境变量，可在此层之上替换为 LLM 实现，接口保持不变。

const STOP_BIGRAMS = new Set([
  '我们', '你们', '他们', '一个', '一些', '可以', '就是', '不是', '没有', '什么',
  '为什么', '怎么', '这样', '那样', '因为', '所以', '但是', '而且', '或者', '如果',
  '已经', '还是', '只是', '觉得', '知道', '时候', '东西', '事情', '问题', '这个',
  '那个', '这些', '那些', '自己', '现在', '今天', '明天', '昨天', '一下', '一直',
  '真的', '可能', '应该', '需要', '不会', '不要', '不能', '还有', '过来', '起来',
  '为什', '么会', '么回', '回事', '之一', '怎么', '怎样', '是否', '前后', '左右',
])

// 领域词表：优先作为「人能读懂」的关联证据展示
const LEXICON = [
  '通知', '打扰', '打断', '专注', '低干扰', '干扰', '输入', '手写', '打字', '语音',
  '笔记', '记录', '原型', '日历', '待办', '主题', '关联', '记忆', '灵感', '设计',
  '回响', '整理', '分类', '文件夹', '复盘', '迭代', '截止', '焦虑', '设备', '纸张',
  '书写', '同步', '搜索', '遗忘', '回看', '观察', '环境', '手机', '转写', '草图',
  '自动保存', '空白', '开会', '复盘', '测试', '上线', '命名', '检索', '来源',
]

const CJK = /[\u4e00-\u9fff]/

export function tokenize(text) {
  if (!text) return new Map()
  const counts = new Map()
  const add = (w, wgt = 1) => {
    if (!w || STOP_BIGRAMS.has(w)) return
    counts.set(w, (counts.get(w) || 0) + wgt)
  }
  // 领域词表命中（权重高）
  for (const word of LEXICON) {
    let idx = 0
    let hits = 0
    while ((idx = text.indexOf(word, idx)) !== -1) {
      hits++
      idx += word.length
    }
    if (hits > 0) add(word, hits * 2)
  }
  // 中文 bigram
  const chars = text.replace(/[^\u4e00-\u9fff]/g, ' ')
  for (const seg of chars.split(/\s+/)) {
    for (let i = 0; i < seg.length - 1; i++) {
      const bg = seg.slice(i, i + 2)
      if (CJK.test(bg[0]) && CJK.test(bg[1])) add(bg, 1)
    }
  }
  // 英文/数字词
  const latin = text.toLowerCase().match(/[a-z][a-z0-9-]{2,}/g) || []
  for (const w of latin) {
    if (!['the', 'and', 'for', 'with', 'this', 'that'].includes(w)) add(w, 1)
  }
  return counts
}

function topKeywords(counts, n = 6) {
  return [...counts.entries()]
    .sort((a, b) => b[1] - a[1])
    .slice(0, n)
    .map(([w]) => w)
}

const LEXICON_SET = new Set(LEXICON)
const NOISE_CHARS = new Set(
  '的了和是就都也在我有你他她它个之与及或被把让向从到着过呢吗吧啊呀嘛么不只很还又再最更太真假'.split('')
)

function isReadableKeyword(w) {
  if (LEXICON_SET.has(w)) return true
  if (w.length <= 2 && [...w].some((c) => NOISE_CHARS.has(c))) return false
  return true
}

function sharedKeywords(a, b, n = 3) {
  const shared = []
  for (const [w, wa] of a) {
    if (b.has(w) && isReadableKeyword(w)) shared.push([w, wa + b.get(w)])
  }
  return shared
    .sort((x, y) => {
      const lx = LEXICON_SET.has(x[0]) ? 1 : 0
      const ly = LEXICON_SET.has(y[0]) ? 1 : 0
      if (lx !== ly) return ly - lx
      return y[1] - x[1]
    })
    // 去掉非词表的「碎片词」：
    // 1. 残缺子串，如「低干」是「低干扰」的一部分
    // 2. 跨词拼接，如「扰输」的每个字都能在其他共享词里找到（干扰+输入）
    .filter(([w], _, arr) => {
      if (LEXICON_SET.has(w)) return true
      const others = arr.map(([x]) => x).filter((x) => x !== w)
      if (others.some((x) => x.length > w.length && x.includes(w))) return false
      return ![...w].every((c) => others.some((x) => x.includes(c)))
    })
    .slice(0, n)
    .map(([w]) => w)
}

function keywordSimilarity(a, b) {
  // 加权余弦：对「短新笔记 vs 长旧笔记」的长度差不敏感
  if (a.size === 0 || b.size === 0) return 0
  let dot = 0
  let na = 0
  let nb = 0
  for (const [, w] of a) na += w * w
  for (const [k, w] of b) {
    nb += w * w
    if (a.has(k)) dot += w * a.get(k)
  }
  return dot === 0 ? 0 : dot / (Math.sqrt(na) * Math.sqrt(nb))
}

function daysBetween(a, b) {
  return Math.abs(new Date(a).getTime() - new Date(b).getTime()) / 86400000
}

export const RELATION_THRESHOLD = 0.15

export function scorePair(noteA, topicsA, noteB, topicsB) {
  const ka = tokenize(`${noteA.title}\n${noteA.rawText}`)
  const kb = tokenize(`${noteB.title}\n${noteB.rawText}`)
  const kwSim = keywordSimilarity(ka, kb)
  const shared = sharedKeywords(ka, kb, 3)
  const sharedTopics = topicsA.filter((t) => topicsB.some((x) => x.id === t.id))
  const topicSim = sharedTopics.length > 0 ? 1 : 0
  const days = daysBetween(noteA.createdAt, noteB.createdAt)
  const timeFactor = 1 / (1 + days / 120)
  const score = Math.min(1, 0.72 * kwSim + 0.16 * topicSim + 0.12 * timeFactor * kwSim)
  if (shared.length < 2 && !(sharedTopics.length > 0 && shared.length >= 1)) {
    return { score: 0, reason: '' }
  }
  const parts = []
  if (sharedTopics.length > 0) parts.push(`同属「${sharedTopics.map((t) => t.name).join('、')}」`)
  if (shared.length > 0) parts.push(`都提到${shared.map((w) => `「${w}」`).join('、')}`)
  if (days >= 1) parts.push(`相隔 ${Math.round(days)} 天`)
  return { score, reason: parts.join('，'), shared }
}

export function answerQuestion(question, candidates) {
  // candidates: [{ id, title, createdAt, rawText, topics }]
  const qk = tokenize(question)
  const scored = candidates
    .map((note) => {
      const nk = tokenize(`${note.title}\n${note.rawText}`)
      const sim = keywordSimilarity(qk, nk)
      const shared = sharedKeywords(qk, nk, 4)
      return { note, sim, shared }
    })
    .filter((x) => x.sim > 0.01 && x.shared.length > 0)
    .sort((a, b) => b.sim - a.sim)
    .slice(0, 3)

  if (scored.length === 0) {
    return {
      answer: '档案中没有足够依据回答这个问题。换一组关键词，或先多记录几页再来问。',
      sources: [],
      sourceIds: [],
    }
  }

  const fmtDate = (iso) => {
    const d = new Date(iso)
    return `${d.getMonth() + 1}月${d.getDate()}日`
  }
  const pickExcerpt = (note, shared) => {
    const stripTail = (s) => s.replace(/[：:，,。？?！!…\s]+$/, '')
    const titleCore = stripTail(note.title || '')
    const all = note.rawText
      .split(/[。!?\n；;]/)
      .map((s) => s.trim())
      .filter((s) => s.length >= 4)
    // 首行通常是标题本身，引用时跳过它，优先摘真正有内容的句子
    const isTitleLine = (s) => {
      const core = stripTail(s)
      return titleCore && (core === titleCore || titleCore.startsWith(core))
    }
    const body = all.filter((s) => !isTitleLine(s))
    const sentences = body.length > 0 ? body : all
    let best = ''
    let bestHits = -1
    for (const s of sentences) {
      const hits = shared.reduce((acc, w) => acc + (s.includes(w) ? 1 : 0), 0)
      if (hits > bestHits) {
        bestHits = hits
        best = s
      }
    }
    return best.length > 80 ? `${best.slice(0, 80)}…` : best
  }

  const sources = scored.map(({ note, shared }) => ({
    noteId: note.id,
    title: note.title || '无标题',
    createdAt: note.createdAt,
    excerpt: pickExcerpt(note, shared),
    shared,
  }))

  const lines = sources.map(
    (s) => `你在 ${fmtDate(s.createdAt)} 的《${s.title}》里写过：「${s.excerpt}」`
  )
  const common = scored[0].shared.filter((w) => scored.every((x) => x.shared.includes(w)))
  const tail =
    sources.length > 1 && common.length > 0
      ? `这几页记录共同围绕${common.map((w) => `「${w}」`).join('、')}展开。`
      : ''
  return {
    answer: `在你的档案里找到 ${sources.length} 条相关记录。${lines.join('；')}。${tail}`,
    sources,
    sourceIds: sources.map((s) => s.noteId),
  }
}

const ACTION_RE =
  /(要做|需要|记得|别忘了|待办|todo|完成|去做|行动|约|提醒|截止|deadline|之前|准备好|发给|交给|整理出|约个|安排)/i

function parseDueDate(sentence, base = new Date()) {
  const d = new Date(base)
  const iso = (dt) => {
    const y = dt.getFullYear()
    const m = String(dt.getMonth() + 1).padStart(2, '0')
    const day = String(dt.getDate()).padStart(2, '0')
    return `${y}-${m}-${day}`
  }
  if (/今天/.test(sentence)) return iso(d)
  if (/明天/.test(sentence)) return iso(new Date(d.getTime() + 86400000))
  if (/后天/.test(sentence)) return iso(new Date(d.getTime() + 2 * 86400000))
  const weekMap = { 一: 1, 二: 2, 三: 3, 四: 4, 五: 5, 六: 6, 日: 0, 天: 0 }
  let m = sentence.match(/下?周([一二三四五六日天])/)
  if (m) {
    const target = weekMap[m[1]]
    const next = /下周/.test(sentence)
    const cur = d.getDay()
    let delta = (target - cur + 7) % 7
    if (delta === 0) delta = 7
    if (next) delta += 7
    return iso(new Date(d.getTime() + delta * 86400000))
  }
  m = sentence.match(/(\d{1,2})月(\d{1,2})[日号]/)
  if (m) {
    const dt = new Date(d.getFullYear(), Number(m[1]) - 1, Number(m[2]))
    if (dt.getTime() < d.getTime()) dt.setFullYear(dt.getFullYear() + 1)
    return iso(dt)
  }
  if (/月底/.test(sentence)) return iso(new Date(d.getFullYear(), d.getMonth() + 1, 0))
  return null
}

export function extractTasks(rawText, base = new Date()) {
  if (!rawText) return []
  const sentences = rawText
    .split(/(?<=[。!?\n；;])|\n/)
    .map((s) => s.trim())
    .filter((s) => s.length >= 4 && s.length <= 120)
  const seen = new Set()
  const tasks = []
  for (const s of sentences) {
    if (!ACTION_RE.test(s)) continue
    const title = s.replace(/[。!?\n；;]+$/, '').slice(0, 48)
    if (seen.has(title)) continue
    seen.add(title)
    tasks.push({ title, dueDate: parseDueDate(s, base) })
    if (tasks.length >= 3) break
  }
  return tasks
}
