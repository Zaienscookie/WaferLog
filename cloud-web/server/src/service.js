import { db, uid, now, getNoteTopics, setNoteTopics, noteRowToJson } from './db.js'
import { scorePair, extractTasks, RELATION_THRESHOLD } from './ai.js'

export function recomputeRelationsFor(noteId) {
  const note = db.prepare('SELECT * FROM notes WHERE id = ?').get(noteId)
  if (!note) return []
  db.prepare('DELETE FROM relations WHERE fromNoteId = ? OR toNoteId = ?').run(noteId, noteId)
  const others = db
    .prepare('SELECT * FROM notes WHERE id != ? AND deleted = 0')
    .all(noteId)
  const topicsA = getNoteTopics(noteId)
  const found = []
  const ins = db.prepare(
    'INSERT INTO relations (id, fromNoteId, toNoteId, reason, score, createdAt) VALUES (?, ?, ?, ?, ?, ?)'
  )
  for (const other of others) {
    const topicsB = getNoteTopics(other.id)
    const { score, reason } = scorePair(note, topicsA, other, topicsB)
    if (score >= RELATION_THRESHOLD) {
      const newer = new Date(note.createdAt) >= new Date(other.createdAt) ? note : other
      const older = newer.id === note.id ? other : note
      const rel = {
        id: uid(),
        fromNoteId: newer.id,
        toNoteId: older.id,
        reason,
        score: Math.round(score * 1000) / 1000,
        createdAt: now(),
      }
      ins.run(rel.id, rel.fromNoteId, rel.toNoteId, rel.reason, rel.score, rel.createdAt)
      found.push(rel)
    }
  }
  return found.sort((a, b) => b.score - a.score).slice(0, 5)
}

export function refreshCandidateTasks(noteId, rawText, createdAt) {
  db.prepare(`DELETE FROM tasks WHERE noteId = ? AND status = 'candidate'`).run(noteId)
  const candidates = extractTasks(rawText, createdAt ? new Date(createdAt) : new Date())
  const ins = db.prepare(
    'INSERT INTO tasks (id, noteId, title, dueDate, status, createdAt) VALUES (?, ?, ?, ?, ?, ?)'
  )
  for (const t of candidates) {
    ins.run(uid(), noteId, t.title, t.dueDate, 'candidate', now())
  }
  return candidates
}

export function createNote({ title = '', inputType = 'text', rawContent = '', rawText = '', createdAt, topicIds = [] }) {
  const id = uid()
  const ts = createdAt || now()
  const derivedTitle =
    title ||
    (rawText ? rawText.split('\n')[0].replace(/[#\s]/g, '').slice(0, 18) : '') ||
    '无标题纸页'
  db.prepare(
    `INSERT INTO notes (id, title, createdAt, updatedAt, inputType, rawContent, rawText, deleted)
     VALUES (?, ?, ?, ?, ?, ?, ?, 0)`
  ).run(id, derivedTitle, ts, ts, inputType, rawContent, rawText)
  if (topicIds.length > 0) setNoteTopics(id, topicIds)
  refreshCandidateTasks(id, rawText, ts)
  const relations = recomputeRelationsFor(id)
  const row = db.prepare('SELECT * FROM notes WHERE id = ?').get(id)
  return { note: noteRowToJson(row), relations }
}

export function getRelationsFor(noteId) {
  const rows = db
    .prepare(
      `SELECT * FROM relations WHERE (fromNoteId = ? OR toNoteId = ?) ORDER BY score DESC`
    )
    .all(noteId, noteId)
  return rows.map((rel) => {
    const otherId = rel.fromNoteId === noteId ? rel.toNoteId : rel.fromNoteId
    const other = db.prepare('SELECT id, title, createdAt, inputType FROM notes WHERE id = ?').get(otherId)
    return { ...rel, otherNote: other }
  })
}

export function echoCountFor(noteId) {
  const row = db
    .prepare(
      `SELECT COUNT(*) AS c FROM relations WHERE fromNoteId = ? OR toNoteId = ?`
    )
    .get(noteId, noteId)
  return row ? row.c : 0
}
