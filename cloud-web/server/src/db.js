import { DatabaseSync } from 'node:sqlite'
import { mkdirSync } from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import crypto from 'node:crypto'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const dataDir = path.join(__dirname, '..', 'data')
mkdirSync(dataDir, { recursive: true })

export const db = new DatabaseSync(path.join(dataDir, 'waferlog.db'))

db.exec(`
PRAGMA journal_mode = WAL;

CREATE TABLE IF NOT EXISTS notes (
  id TEXT PRIMARY KEY,
  title TEXT NOT NULL DEFAULT '',
  createdAt TEXT NOT NULL,
  updatedAt TEXT NOT NULL,
  inputType TEXT NOT NULL DEFAULT 'text',
  rawContent TEXT NOT NULL DEFAULT '',
  rawText TEXT NOT NULL DEFAULT '',
  deleted INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS topics (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS note_topics (
  noteId TEXT NOT NULL,
  topicId TEXT NOT NULL,
  PRIMARY KEY (noteId, topicId)
);

CREATE TABLE IF NOT EXISTS relations (
  id TEXT PRIMARY KEY,
  fromNoteId TEXT NOT NULL,
  toNoteId TEXT NOT NULL,
  reason TEXT NOT NULL DEFAULT '',
  score REAL NOT NULL DEFAULT 0,
  createdAt TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS tasks (
  id TEXT PRIMARY KEY,
  noteId TEXT NOT NULL,
  title TEXT NOT NULL,
  dueDate TEXT,
  status TEXT NOT NULL DEFAULT 'candidate',
  createdAt TEXT NOT NULL
);
`)

export const uid = () => crypto.randomUUID()
export const now = () => new Date().toISOString()

export function getNoteTopics(noteId) {
  return db
    .prepare(
      `SELECT t.id, t.name, t.description FROM topics t
       JOIN note_topics nt ON nt.topicId = t.id WHERE nt.noteId = ?`
    )
    .all(noteId)
}

export function setNoteTopics(noteId, topicIds) {
  db.prepare('DELETE FROM note_topics WHERE noteId = ?').run(noteId)
  const ins = db.prepare('INSERT OR IGNORE INTO note_topics (noteId, topicId) VALUES (?, ?)')
  for (const tid of topicIds || []) ins.run(noteId, tid)
}

export function noteRowToJson(row, { withTopics = true } = {}) {
  if (!row) return null
  const note = { ...row, deleted: !!row.deleted }
  if (withTopics) note.topics = getNoteTopics(row.id)
  return note
}
