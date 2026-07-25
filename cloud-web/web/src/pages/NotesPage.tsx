import { useEffect, useMemo, useState } from 'react'
import { Link } from 'react-router-dom'
import { api } from '../api'
import type { Note, TopicWithNotes } from '../types'
import { PaperPage } from '../components/PaperPage'
import { Loading, EmptyHint, Confirm } from '../components/bits'
import { Icon } from '../components/Icon'

const TYPE_FILTERS = [
  ['', '全部'],
  ['handwriting', '手写'],
  ['text', '文字'],
  ['voice', '语音'],
] as const

export function NotesPage() {
  const [notes, setNotes] = useState<Note[] | null>(null)
  const [topics, setTopics] = useState<TopicWithNotes[]>([])
  const [q, setQ] = useState('')
  const [type, setType] = useState('')
  const [topic, setTopic] = useState('')
  const [hasEcho, setHasEcho] = useState(false)
  const [pendingDelete, setPendingDelete] = useState<Note | null>(null)
  const [error, setError] = useState('')

  const load = () => {
    api
      .notes({ q, type, topic, hasEcho: hasEcho ? '1' : '' })
      .then(setNotes)
      .catch((e) => setError(String(e.message || e)))
  }

  useEffect(() => {
    api.topics().then(setTopics).catch(() => undefined)
  }, [])

  useEffect(() => {
    const t = window.setTimeout(load, q ? 250 : 0)
    return () => window.clearTimeout(t)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [q, type, topic, hasEcho])

  const groups = useMemo(() => {
    if (!notes) return []
    const map = new Map<string, Note[]>()
    for (const n of notes) {
      const d = new Date(n.createdAt)
      const key = `${d.getFullYear()} 年 ${d.getMonth() + 1} 月`
      if (!map.has(key)) map.set(key, [])
      map.get(key)!.push(n)
    }
    return [...map.entries()]
  }, [notes])

  return (
    <div className="page">
      <header className="page-head">
        <h1 className="page-title">
          所有记录
          <span className="sub">原始手写页是主角，AI 整理结果只是附着层</span>
        </h1>
        <Link to="/new" className="btn btn-primary">
          <Icon name="pen" width={16} height={16} />
          落笔
        </Link>
      </header>

      <div className="notes-toolbar">
        <input
          className="input"
          placeholder="搜索标题或文字层…"
          value={q}
          onChange={(e) => setQ(e.target.value)}
        />
        {TYPE_FILTERS.map(([v, label]) => (
          <button key={v} className={`chip${type === v ? ' active' : ''}`} onClick={() => setType(v)}>
            {label}
          </button>
        ))}
        <select className="input" style={{ width: 'auto', minHeight: 36, padding: '4px 10px' }} value={topic} onChange={(e) => setTopic(e.target.value)}>
          <option value="">全部主题</option>
          {topics.map((t) => (
            <option key={t.id} value={t.id}>
              {t.name}
            </option>
          ))}
        </select>
        <button className={`chip${hasEcho ? ' active' : ''}`} onClick={() => setHasEcho((v) => !v)}>
          <Icon name="sparkle" width={12} height={12} /> 有回响
        </button>
      </div>

      {error && <EmptyHint>{error}</EmptyHint>}
      {!notes && !error && <Loading />}
      {notes && notes.length === 0 && (
        <EmptyHint>没有找到符合条件的纸页。换个关键词，或者去写下新的一页。</EmptyHint>
      )}

      {groups.map(([month, list]) => (
        <section key={month}>
          <h2 className="month-head">{month}</h2>
          <div className="notes-grid">
            {list.map((n) => (
              <div key={n.id} className="note-card" style={{ position: 'relative' }}>
                <Link to={`/notes/${n.id}`}>
                  <PaperPage note={n} clamp meta className="hoverable" />
                </Link>
                <button
                  className="card-x"
                  title="移入回收站"
                  aria-label={`删除《${n.title}》`}
                  onClick={() => setPendingDelete(n)}
                >
                  <Icon name="x" width={14} height={14} />
                </button>
              </div>
            ))}
          </div>
        </section>
      ))}

      {pendingDelete && (
        <Confirm
          title="移入回收站？"
          danger
          confirmText="移入回收站"
          onCancel={() => setPendingDelete(null)}
          onConfirm={async () => {
            await api.deleteNote(pendingDelete.id)
            setPendingDelete(null)
            load()
          }}
        >
          《{pendingDelete.title}》会移入回收站，可在设置中恢复。
        </Confirm>
      )}
    </div>
  )
}
