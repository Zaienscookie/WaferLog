import { Router } from 'express'
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { db, uid, now, getNoteTopics, setNoteTopics, noteRowToJson } from './db.js'
import {
  createNote,
  recomputeRelationsFor,
  refreshCandidateTasks,
  getRelationsFor,
  echoCountFor,
} from './service.js'
import { answerQuestion } from './ai.js'

export const api = Router()

const dataDir = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'data')
const audioDir = path.join(dataDir, 'audio')
mkdirSync(audioDir, { recursive: true })

function pcmToWav(pcm, sampleRate, channels, bits) {
  const safeRate = Math.max(1, Number(sampleRate) || 16000)
  const safeChannels = Math.max(1, Number(channels) || 1)
  const safeBits = Math.max(8, Number(bits) || 16)
  const blockAlign = safeChannels * (safeBits / 8)
  const byteRate = safeRate * blockAlign
  const header = Buffer.alloc(44)
  header.write('RIFF', 0)
  header.writeUInt32LE(36 + pcm.length, 4)
  header.write('WAVE', 8)
  header.write('fmt ', 12)
  header.writeUInt32LE(16, 16)
  header.writeUInt16LE(1, 20)
  header.writeUInt16LE(safeChannels, 22)
  header.writeUInt32LE(safeRate, 24)
  header.writeUInt32LE(byteRate, 28)
  header.writeUInt16LE(blockAlign, 32)
  header.writeUInt16LE(safeBits, 34)
  header.write('data', 36)
  header.writeUInt32LE(pcm.length, 40)
  return Buffer.concat([header, pcm])
}

const wrap = (fn) => (req, res) => {
  try {
    fn(req, res)
  } catch (err) {
    console.error(err)
    res.status(500).json({ error: String(err.message || err) })
  }
}

api.get('/health', (req, res) => res.json({ ok: true, ts: now() }))

api.post(
  '/device/audio',
  wrap((req, res) => {
    const body = Buffer.isBuffer(req.body) ? req.body : Buffer.from(req.body || '')
    if (body.length === 0) return res.status(400).json({ error: 'audio body is required' })
    const id = uid()
    const fileName = `${id}.pcm`
    writeFileSync(path.join(audioDir, fileName), body)
    res.status(201).json({
      id,
      size: body.length,
      mime: req.get('Content-Type') || 'audio/pcm',
      url: `/api/device/audio/${id}`,
    })
  })
)

api.get(
  '/device/audio/:id',
  wrap((req, res) => {
    if (!/^[0-9a-f-]+$/i.test(req.params.id)) return res.status(400).json({ error: 'invalid audio id' })
    const file = path.join(audioDir, `${req.params.id}.pcm`)
    if (!existsSync(file)) return res.status(404).json({ error: 'audio not found' })
    const linkedNote = db
      .prepare('SELECT audioSampleRate, audioChannels, audioBits FROM notes WHERE audioId = ? ORDER BY createdAt DESC LIMIT 1')
      .get(req.params.id)
    const wav = pcmToWav(
      readFileSync(file),
      linkedNote?.audioSampleRate,
      linkedNote?.audioChannels,
      linkedNote?.audioBits
    )
    res.type('audio/wav').set('Accept-Ranges', 'bytes').send(wav)
  })
)

api.post(
  '/device/notes',
  wrap((req, res) => {
    const {
      deviceId = 'waferlog-t5ai',
      title,
      inputType = 'handwriting',
      rawContent = '',
      rawText = '',
      audioId = '',
      audioMime = '',
      audioSize = 0,
      audioSampleRate = 0,
      audioChannels = 0,
      audioBits = 0,
    } = req.body || {}
    if (!rawContent && !rawText) return res.status(400).json({ error: 'note content is required' })
    if (audioId && !existsSync(path.join(audioDir, `${audioId}.pcm`))) {
      return res.status(400).json({ error: 'audio not found' })
    }
    const result = createNote({ title, inputType, rawContent, rawText })
    db.prepare(
      `UPDATE notes SET deviceId = ?, audioId = ?, audioMime = ?, audioSize = ?, audioSampleRate = ?,
       audioChannels = ?, audioBits = ? WHERE id = ?`
    ).run(
      deviceId,
      audioId || null,
      audioMime || null,
      Number(audioSize) || 0,
      Number(audioSampleRate) || 0,
      Number(audioChannels) || 0,
      Number(audioBits) || 0,
      result.note.id
    )
    const note = db.prepare('SELECT * FROM notes WHERE id = ?').get(result.note.id)
    res.status(201).json({
      deviceId,
      note: noteRowToJson(note),
      relations: result.relations,
      audioUrl: audioId ? `/api/device/audio/${audioId}` : null,
    })
  })
)

api.get(
  '/device/notes/:id',
  wrap((req, res) => {
    const row = db.prepare('SELECT * FROM notes WHERE id = ? AND deleted = 0').get(req.params.id)
    if (!row) return res.status(404).json({ error: 'note not found' })
    const requestedDevice = String(req.query.deviceId || '')
    if (requestedDevice && row.deviceId && row.deviceId !== requestedDevice) {
      return res.status(403).json({ error: 'note belongs to another device' })
    }
    const tasks = db
      .prepare('SELECT * FROM tasks WHERE noteId = ? ORDER BY createdAt ASC')
      .all(row.id)
    res.json({
      note: noteRowToJson(row),
      relations: getRelationsFor(row.id),
      tasks,
      serverTime: now(),
    })
  })
)

api.get(
  '/device/sync',
  wrap((req, res) => {
    const deviceId = String(req.query.deviceId || 'waferlog-t5ai')
    const since = String(req.query.since || '')
    const limit = Math.min(Math.max(Number(req.query.limit) || 10, 1), 50)
    const rows = db
      .prepare(
        `SELECT * FROM notes
         WHERE deleted = 0 AND deviceId = ? AND updatedAt > ?
         ORDER BY updatedAt ASC LIMIT ?`
      )
      .all(deviceId, since, limit)
    const items = rows.map((row) => ({
      note: noteRowToJson(row),
      relations: getRelationsFor(row.id),
      tasks: db
        .prepare('SELECT * FROM tasks WHERE noteId = ? ORDER BY createdAt ASC')
        .all(row.id),
    }))
    res.json({ deviceId, serverTime: now(), items })
  })
)

// ---------- 档案馆首页 ----------
api.get(
  '/home',
  wrap((req, res) => {
    const latest = db
      .prepare('SELECT * FROM notes WHERE deleted = 0 ORDER BY createdAt DESC LIMIT 1')
      .get()
    const recent = db
      .prepare('SELECT * FROM notes WHERE deleted = 0 ORDER BY createdAt DESC LIMIT 7')
      .all()
      .map((r) => noteRowToJson(r))
    let echo = { count: 0, tickets: [] }
    if (latest) {
      const rels = getRelationsFor(latest.id)
      echo = { count: rels.length, tickets: rels.slice(0, 2) }
    }
    res.json({
      latestNote: latest ? noteRowToJson(latest) : null,
      echo,
      recentNotes: recent.slice(1),
      stats: {
        notes: db.prepare('SELECT COUNT(*) c FROM notes WHERE deleted = 0').get().c,
        topics: db.prepare('SELECT COUNT(*) c FROM topics').get().c,
        relations: db.prepare('SELECT COUNT(*) c FROM relations').get().c,
      },
    })
  })
)

// ---------- 记录 ----------
api.get(
  '/notes',
  wrap((req, res) => {
    const { q = '', type = '', topic = '', hasEcho = '', deleted = '' } = req.query
    const wantDeleted = deleted === '1'
    let rows = db
      .prepare('SELECT * FROM notes WHERE deleted = ? ORDER BY createdAt DESC')
      .all(wantDeleted ? 1 : 0)
    if (type) rows = rows.filter((r) => r.inputType === type)
    if (topic) rows = rows.filter((r) => getNoteTopics(r.id).some((t) => t.id === topic))
    if (q) {
      const needle = String(q).toLowerCase()
      rows = rows.filter(
        (r) =>
          r.title.toLowerCase().includes(needle) ||
          r.rawText.toLowerCase().includes(needle)
      )
    }
    if (hasEcho === '1') rows = rows.filter((r) => echoCountFor(r.id) > 0)
    res.json(
      rows.map((r) => ({ ...noteRowToJson(r), echoCount: echoCountFor(r.id) }))
    )
  })
)

api.post(
  '/notes',
  wrap((req, res) => {
    const { title, inputType, rawContent, rawText, topicIds } = req.body || {}
    if (!rawText && !rawContent) {
      return res.status(400).json({ error: 'rawText 或 rawContent 至少提供一个' })
    }
    const result = createNote({ title, inputType, rawContent, rawText, topicIds })
    res.status(201).json(result)
  })
)

api.get(
  '/notes/:id',
  wrap((req, res) => {
    const row = db.prepare('SELECT * FROM notes WHERE id = ?').get(req.params.id)
    if (!row) return res.status(404).json({ error: '记录不存在' })
    const relations = getRelationsFor(row.id)
    const tasks = db
      .prepare('SELECT * FROM tasks WHERE noteId = ? ORDER BY createdAt ASC')
      .all(row.id)
    res.json({ note: noteRowToJson(row), relations, tasks })
  })
)

api.patch(
  '/notes/:id',
  wrap((req, res) => {
    const row = db.prepare('SELECT * FROM notes WHERE id = ?').get(req.params.id)
    if (!row) return res.status(404).json({ error: '记录不存在' })
    const { title, rawText, rawContent, topicIds } = req.body || {}
    const nextTitle = title !== undefined ? title : row.title
    const nextText = rawText !== undefined ? rawText : row.rawText
    const nextContent = rawContent !== undefined ? rawContent : row.rawContent
    db.prepare('UPDATE notes SET title = ?, rawText = ?, rawContent = ?, updatedAt = ? WHERE id = ?').run(
      nextTitle,
      nextText,
      nextContent,
      now(),
      row.id
    )
    if (topicIds !== undefined) setNoteTopics(row.id, topicIds)
    if (rawText !== undefined && rawText !== row.rawText) {
      refreshCandidateTasks(row.id, nextText, row.createdAt)
    }
    if (rawText !== undefined || topicIds !== undefined) recomputeRelationsFor(row.id)
    const updated = db.prepare('SELECT * FROM notes WHERE id = ?').get(row.id)
    res.json({ note: noteRowToJson(updated), relations: getRelationsFor(row.id) })
  })
)

api.delete(
  '/notes/:id',
  wrap((req, res) => {
    db.prepare('UPDATE notes SET deleted = 1, updatedAt = ? WHERE id = ?').run(now(), req.params.id)
    res.json({ ok: true })
  })
)

api.post(
  '/notes/:id/restore',
  wrap((req, res) => {
    db.prepare('UPDATE notes SET deleted = 0, updatedAt = ? WHERE id = ?').run(now(), req.params.id)
    res.json({ ok: true })
  })
)

api.delete(
  '/notes/:id/permanent',
  wrap((req, res) => {
    const id = req.params.id
    db.prepare('DELETE FROM notes WHERE id = ?').run(id)
    db.prepare('DELETE FROM note_topics WHERE noteId = ?').run(id)
    db.prepare('DELETE FROM relations WHERE fromNoteId = ? OR toNoteId = ?').run(id, id)
    db.prepare('DELETE FROM tasks WHERE noteId = ?').run(id)
    res.json({ ok: true })
  })
)

// ---------- 主题 ----------
api.get(
  '/topics',
  wrap((req, res) => {
    const topics = db.prepare('SELECT * FROM topics ORDER BY rowid ASC').all()
    res.json(
      topics.map((t) => {
        const notes = db
          .prepare(
            `SELECT n.id, n.title, n.createdAt, n.inputType FROM notes n
             JOIN note_topics nt ON nt.noteId = n.id
             WHERE nt.topicId = ? AND n.deleted = 0 ORDER BY n.createdAt DESC`
          )
          .all(t.id)
        return {
          ...t,
          noteCount: notes.length,
          echoCount: notes.reduce((acc, n) => acc + echoCountFor(n.id), 0),
          notes: notes.slice(0, 8),
        }
      })
    )
  })
)

api.post(
  '/topics',
  wrap((req, res) => {
    const { name, description = '' } = req.body || {}
    if (!name) return res.status(400).json({ error: 'name 必填' })
    const id = uid()
    db.prepare('INSERT INTO topics (id, name, description) VALUES (?, ?, ?)').run(id, name, description)
    res.status(201).json({ id, name, description })
  })
)

api.patch(
  '/topics/:id',
  wrap((req, res) => {
    const { name, description } = req.body || {}
    const t = db.prepare('SELECT * FROM topics WHERE id = ?').get(req.params.id)
    if (!t) return res.status(404).json({ error: '主题不存在' })
    db.prepare('UPDATE topics SET name = ?, description = ? WHERE id = ?').run(
      name !== undefined ? name : t.name,
      description !== undefined ? description : t.description,
      t.id
    )
    res.json(db.prepare('SELECT * FROM topics WHERE id = ?').get(t.id))
  })
)

api.post(
  '/topics/:id/merge',
  wrap((req, res) => {
    const { intoId } = req.body || {}
    if (!intoId) return res.status(400).json({ error: 'intoId 必填' })
    const fromId = req.params.id
    const rows = db.prepare('SELECT noteId FROM note_topics WHERE topicId = ?').all(fromId)
    const ins = db.prepare('INSERT OR IGNORE INTO note_topics (noteId, topicId) VALUES (?, ?)')
    for (const r of rows) ins.run(r.noteId, intoId)
    db.prepare('DELETE FROM note_topics WHERE topicId = ?').run(fromId)
    db.prepare('DELETE FROM topics WHERE id = ?').run(fromId)
    res.json({ ok: true })
  })
)

// ---------- 记忆线 ----------
api.get(
  '/notes/:id/timeline',
  wrap((req, res) => {
    const center = db.prepare('SELECT * FROM notes WHERE id = ?').get(req.params.id)
    if (!center) return res.status(404).json({ error: '记录不存在' })
    const rels = getRelationsFor(center.id).slice(0, 5)
    const nodes = rels
      .map((rel) => {
        const n = db.prepare('SELECT * FROM notes WHERE id = ?').get(rel.otherNote.id)
        return {
          note: noteRowToJson(n),
          reason: rel.reason,
          score: rel.score,
        }
      })
      .sort((a, b) => new Date(a.note.createdAt) - new Date(b.note.createdAt))
    res.json({ center: noteRowToJson(center), nodes })
  })
)

// ---------- 待办 ----------
api.get(
  '/tasks',
  wrap((req, res) => {
    const { status = '', month = '' } = req.query
    let rows = db
      .prepare(
        `SELECT tk.*, n.title AS noteTitle FROM tasks tk
         JOIN notes n ON n.id = tk.noteId WHERE n.deleted = 0 ORDER BY tk.createdAt DESC`
      )
      .all()
    if (status) rows = rows.filter((r) => r.status === status)
    if (month) rows = rows.filter((r) => (r.dueDate || '').startsWith(month))
    res.json(rows)
  })
)

api.patch(
  '/tasks/:id',
  wrap((req, res) => {
    const t = db.prepare('SELECT * FROM tasks WHERE id = ?').get(req.params.id)
    if (!t) return res.status(404).json({ error: '待办不存在' })
    const { status, dueDate, title } = req.body || {}
    db.prepare('UPDATE tasks SET status = ?, dueDate = ?, title = ? WHERE id = ?').run(
      status !== undefined ? status : t.status,
      dueDate !== undefined ? dueDate : t.dueDate,
      title !== undefined ? title : t.title,
      t.id
    )
    res.json(db.prepare('SELECT * FROM tasks WHERE id = ?').get(t.id))
  })
)

// ---------- 时间册 ----------
api.get(
  '/calendar',
  wrap((req, res) => {
    const month = String(req.query.month || now().slice(0, 7))
    const notes = db
      .prepare('SELECT id, title, createdAt, inputType FROM notes WHERE deleted = 0')
      .all()
      .filter((n) => n.createdAt.startsWith(month))
      .map((n) => ({
        ...n,
        day: n.createdAt.slice(0, 10),
        echoCount: echoCountFor(n.id),
      }))
    const tasks = db
      .prepare(
        `SELECT tk.*, n.title AS noteTitle FROM tasks tk JOIN notes n ON n.id = tk.noteId
         WHERE tk.dueDate LIKE ? AND n.deleted = 0`
      )
      .all(`${month}%`)
    const candidates = db
      .prepare(
        `SELECT tk.*, n.title AS noteTitle FROM tasks tk JOIN notes n ON n.id = tk.noteId
         WHERE tk.status = 'candidate' AND n.deleted = 0 ORDER BY tk.createdAt DESC`
      )
      .all()
    res.json({ month, notes, tasks, candidates })
  })
)

// ---------- 知识问答 ----------
api.post(
  '/ask',
  wrap((req, res) => {
    const { question, scope } = req.body || {}
    if (!question || !String(question).trim()) {
      return res.status(400).json({ error: 'question 必填' })
    }
    let rows = db.prepare('SELECT * FROM notes WHERE deleted = 0').all()
    if (scope?.type === 'topic' && scope.id) {
      rows = rows.filter((r) => getNoteTopics(r.id).some((t) => t.id === scope.id))
    } else if (scope?.type === 'note' && scope.id) {
      rows = rows.filter((r) => r.id === scope.id)
    }
    const candidates = rows.map((r) => ({
      id: r.id,
      title: r.title,
      createdAt: r.createdAt,
      rawText: r.rawText,
      topics: getNoteTopics(r.id),
    }))
    res.json(answerQuestion(String(question), candidates))
  })
)

// ---------- 导出 ----------
api.get(
  '/export',
  wrap((req, res) => {
    res.json({
      exportedAt: now(),
      notes: db.prepare('SELECT * FROM notes').all().map((r) => noteRowToJson(r)),
      topics: db.prepare('SELECT * FROM topics').all(),
      relations: db.prepare('SELECT * FROM relations').all(),
      tasks: db.prepare('SELECT * FROM tasks').all(),
    })
  })
)
