import { db, uid } from './db.js'
import { createNote } from './service.js'

const TOPICS = [
  ['产品设计', '关于硅笺本身与低干扰设备的构想'],
  ['学习方法', '如何更有效地记录、记忆与回看'],
  ['随手灵感', '来不及分类的一闪念'],
  ['知识管理', '记录、整理与关联的长期讨论'],
  ['项目管理', '原型、评审与截止'],
  ['未整理', '还没有归宿的纸页'],
]

// 手写草图（viewBox 0 0 1000 1400）：设备外框 + 波浪 + 箭头，抽象涂鸦即可信
function sketchStrokes() {
  const ink = '#3a3128'
  const amber = '#b06c2a'
  const strokes = []
  const rect = []
  const x0 = 300
  const y0 = 420
  const x1 = 700
  const y1 = 900
  for (let i = 0; i <= 24; i++) rect.push([x0 + ((x1 - x0) * i) / 24, y0, 0.6])
  for (let i = 0; i <= 28; i++) rect.push([x1, y0 + ((y1 - y0) * i) / 28, 0.6])
  for (let i = 0; i <= 24; i++) rect.push([x1 - ((x1 - x0) * i) / 24, y1, 0.6])
  for (let i = 0; i <= 28; i++) rect.push([x0, y1 - ((y1 - y0) * i) / 28, 0.5])
  strokes.push({ points: rect, color: ink, size: 7 })
  const wave = []
  for (let i = 0; i <= 60; i++) {
    const x = 340 + (320 * i) / 60
    wave.push([x, 620 + Math.sin(i / 5) * 40, 0.5 + 0.3 * Math.abs(Math.sin(i / 5))])
  }
  strokes.push({ points: wave, color: amber, size: 5 })
  const arrow = []
  for (let i = 0; i <= 20; i++) arrow.push([360 + (280 * i) / 20, 1000, 0.6])
  strokes.push({ points: arrow, color: ink, size: 6 })
  strokes.push({
    points: [
      [640, 1000, 0.6],
      [600, 960, 0.5],
      [606, 1002, 0.4],
      [600, 1040, 0.5],
    ],
    color: ink,
    size: 6,
  })
  const underline = []
  for (let i = 0; i <= 30; i++) underline.push([320 + (360 * i) / 30, 1120 + Math.sin(i / 3) * 4, 0.5])
  strokes.push({ points: underline, color: amber, size: 4 })
  return strokes
}

function isoDaysAgo(days, hour = 9) {
  const d = new Date()
  d.setDate(d.getDate() - days)
  d.setHours(hour, 12, 0, 0)
  return d.toISOString()
}

function isoDaysAhead(days, hour = 18) {
  const d = new Date()
  d.setDate(d.getDate() + days)
  d.setHours(hour, 0, 0, 0)
  const y = d.getFullYear()
  const m = String(d.getMonth() + 1).padStart(2, '0')
  const day = String(d.getDate()).padStart(2, '0')
  return `${y}-${m}-${day}`
}

const NOTES = [
  {
    title: '通知为什么会打断思考？',
    inputType: 'text',
    createdAt: isoDaysAgo(52),
    topics: ['产品设计'],
    rawText:
      '通知为什么会打断思考？\n每次手机亮起，注意力就被拉走一次。真正的专注需要低干扰的环境：没有红点，没有弹窗，只有纸和笔。\n设备的职责应该是安静，而不是争夺注意力。',
  },
  {
    title: '低干扰输入的三种形态',
    inputType: 'text',
    createdAt: isoDaysAgo(45),
    topics: ['随手灵感', '产品设计'],
    rawText:
      '低干扰输入的三种形态：\n一、纸笔，零通知，但难检索；二、打字，快但容易被打断；三、手写设备加自动保存，既有纸的安静，又有数字的检索。\n第三种值得做成原型。',
  },
  {
    title: '手写比打字更容易记住',
    inputType: 'voice',
    createdAt: isoDaysAgo(41, 22),
    topics: ['学习方法'],
    rawText:
      '语音转写：今天读到一篇研究，说手写比打字更容易形成记忆。因为书写速度慢，大脑被迫提炼重点。记录不只是存档，也是思考本身。',
  },
  {
    title: '知识管理工具为什么越整理越乱',
    inputType: 'text',
    createdAt: isoDaysAgo(36),
    topics: ['知识管理'],
    rawText:
      '知识管理工具为什么越整理越乱？因为分类和文件夹是别人的结构，不是我的思路。记录的时候不该被要求先整理。先写下来，让关联自己长出来。',
  },
  {
    title: '周会纪要',
    inputType: 'text',
    createdAt: isoDaysAgo(32, 16),
    topics: ['项目管理'],
    rawText:
      '周会纪要：\n原型第一版要在周五前完成首页原型。需要准备演示脚本。\n下周三约小李评审交互细节。\n别忘了整理出测试用的十二页示例笔记。',
  },
  {
    title: '想要一个开机即写的设备',
    inputType: 'handwriting',
    createdAt: isoDaysAgo(27, 8),
    topics: ['随手灵感', '产品设计'],
    rawText:
      '想要一个开机即写的设备：没有首页，没有菜单，打开就是一张空白纸。写完自动保存，自动同步。低干扰不是功能，是默认状态。',
    sketch: true,
  },
  {
    title: '图书馆自习观察',
    inputType: 'text',
    createdAt: isoDaysAgo(22, 15),
    topics: ['学习方法'],
    rawText:
      '在图书馆观察了一下午：最专注的人桌上没有手机。环境比意志力可靠。把通知关掉，专注的时间立刻变长。低干扰的空间值得被设计出来。',
  },
  {
    title: '想法不写下来就会消失',
    inputType: 'voice',
    createdAt: isoDaysAgo(17, 23),
    topics: ['知识管理', '随手灵感'],
    rawText:
      '语音记录：睡前想到一个点子，早上就忘了大半。灵感不写下来就会消失。记录的门槛必须低到一只手就能完成，回看时再让笔记彼此呼应。',
  },
  {
    title: '给这个产品起个名字',
    inputType: 'text',
    createdAt: isoDaysAgo(13, 21),
    topics: ['产品设计'],
    rawText:
      '给产品起名字：硅笺。硅是芯片，笺是纸笺。随手一笔，久久有回响。名字的寓意是：写下的每一笔，都会被记忆回应。',
  },
  {
    title: '录音笔记的坏处',
    inputType: 'text',
    createdAt: isoDaysAgo(9, 14),
    topics: ['学习方法'],
    rawText:
      '录音笔记的问题：回听成本太高，一小时录音要一小时人生。语音必须变成可检索的文字，才有回看的价值。转写质量比录音时长重要。',
  },
  {
    title: '月末复盘',
    inputType: 'text',
    createdAt: isoDaysAgo(5, 20),
    topics: ['项目管理'],
    rawText:
      '月末复盘：原型跑通了记录到同步的链路。下周需要完成日历视图，月底之前把问答的来源跳转做好。焦虑主要来自截止，不来自工作量。',
  },
  {
    title: '为什么会反复担心做不完',
    inputType: 'text',
    createdAt: isoDaysAgo(3, 7),
    topics: ['未整理'],
    rawText:
      '反复担心项目做不完。把担心写下来之后，发现它只是在重复：截止、评审、演示。写下来本身就是一种整理。',
  },
]

export function seedIfEmpty() {
  const c = db.prepare('SELECT COUNT(*) AS c FROM notes').get().c
  if (c > 0) return false
  seed()
  return true
}

export function seed() {
  const topicIds = {}
  const insTopic = db.prepare('INSERT INTO topics (id, name, description) VALUES (?, ?, ?)')
  for (const [name, description] of TOPICS) {
    const id = uid()
    topicIds[name] = id
    insTopic.run(id, name, description)
  }

  for (const n of NOTES) {
    const rawContent = n.sketch
      ? JSON.stringify({ viewBox: [1000, 1400], strokes: sketchStrokes() })
      : n.rawText
    createNote({
      title: n.title,
      inputType: n.inputType,
      rawContent,
      rawText: n.rawText,
      createdAt: n.createdAt,
      topicIds: n.topics.map((t) => topicIds[t]),
    })
  }

  // 演示用：确认两条待办，让时间册里立刻有「已确认」标记
  const tasks = db.prepare('SELECT * FROM tasks ORDER BY createdAt ASC').all()
  if (tasks.length >= 2) {
    db.prepare(`UPDATE tasks SET status = 'confirmed', dueDate = ? WHERE id = ?`).run(
      isoDaysAhead(0),
      tasks[0].id
    )
    db.prepare(`UPDATE tasks SET status = 'confirmed', dueDate = ? WHERE id = ?`).run(
      isoDaysAhead(3),
      tasks[1].id
    )
  }
}

export function wipe() {
  db.exec(`
    DELETE FROM note_topics;
    DELETE FROM relations;
    DELETE FROM tasks;
    DELETE FROM notes;
    DELETE FROM topics;
  `)
}

if (process.argv[1] && process.argv[1].endsWith('seed.js')) {
  if (process.argv.includes('--force')) wipe()
  const did = seedIfEmpty()
  const counts = {
    notes: db.prepare('SELECT COUNT(*) c FROM notes').get().c,
    topics: db.prepare('SELECT COUNT(*) c FROM topics').get().c,
    relations: db.prepare('SELECT COUNT(*) c FROM relations').get().c,
    tasks: db.prepare('SELECT COUNT(*) c FROM tasks').get().c,
  }
  console.log(did ? '已注入示例数据' : '已有数据，跳过（用 --force 重建）', counts)
}
