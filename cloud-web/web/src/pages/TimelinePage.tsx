import { useEffect, useState } from 'react'
import { Link, useSearchParams } from 'react-router-dom'
import { api, fmtDate } from '../api'
import type { Note, TimelinePayload } from '../types'
import { Loading, EmptyHint } from '../components/bits'
import { Icon } from '../components/Icon'

export function TimelinePage() {
  const [params, setParams] = useSearchParams()
  const [notes, setNotes] = useState<Note[]>([])
  const [data, setData] = useState<TimelinePayload | null>(null)
  const [error, setError] = useState('')
  const noteId = params.get('note') || ''

  useEffect(() => {
    api
      .notes()
      .then((list) => {
        setNotes(list)
        if (!noteId && list.length > 0) {
          setParams({ note: list[0].id }, { replace: true })
        }
      })
      .catch((e) => setError(String(e.message || e)))
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  useEffect(() => {
    if (!noteId) return
    setData(null)
    api
      .timeline(noteId)
      .then(setData)
      .catch((e) => setError(String(e.message || e)))
  }, [noteId])

  return (
    <div className="page">
      <header className="page-head">
        <h1 className="page-title">
          记忆线
          <span className="sub">一个想法是怎样逐步形成的</span>
        </h1>
        <select
          className="input"
          style={{ width: 'min(280px, 100%)' }}
          value={noteId}
          onChange={(e) => setParams({ note: e.target.value })}
          aria-label="选择一页记录"
        >
          {notes.map((n) => (
            <option key={n.id} value={n.id}>
              {n.title}
            </option>
          ))}
        </select>
      </header>

      {error && <EmptyHint>{error}</EmptyHint>}
      {!data && !error && <Loading />}
      {data && (
        <>
          {data.nodes.length === 0 ? (
            <EmptyHint>
              这页还没有强关联的过去。记忆线只呈现高质量关联，等多写几页再来看看。
            </EmptyHint>
          ) : (
            <div className="timeline-wrap">
              <div className="timeline-spine" />
              {data.nodes.map((node) => (
                <div key={node.note.id} className="timeline-node">
                  <span className="timeline-reason">{node.reason}</span>
                  <Link to={`/notes/${node.note.id}`} className="ticket" style={{ display: 'block' }}>
                    <div className="ticket-date">{fmtDate(node.note.createdAt)}</div>
                    <div className="ticket-title">{node.note.title}</div>
                    <div className="ticket-reason" style={{ borderTop: 'none', paddingTop: 0 }}>
                      {node.note.rawText.split('\n').find((s) => s.trim().length >= 4)?.slice(0, 60) || '（手写原稿）'}
                      {node.note.rawText.length > 60 ? '…' : ''}
                    </div>
                  </Link>
                </div>
              ))}
              <div className="timeline-node now">
                <span className="timeline-reason" style={{ background: 'var(--film-2)' }}>
                  NOW · 今天
                </span>
                <Link
                  to={`/notes/${data.center.id}`}
                  className="ticket"
                  style={{ display: 'block', borderColor: 'var(--amber)' }}
                >
                  <div className="ticket-date">{fmtDate(data.center.createdAt)}</div>
                  <div className="ticket-title">{data.center.title}</div>
                  <div className="ticket-reason" style={{ borderTop: 'none', paddingTop: 0 }}>
                    沿着这条线，想法走到了这里。
                  </div>
                </Link>
              </div>
            </div>
          )}
          <p className="muted" style={{ fontSize: 12, textAlign: 'center', marginTop: 18 }}>
            <Icon name="sparkle" width={12} height={12} style={{ verticalAlign: '-2px' }} /> 仅呈现
            3–5 个最强关联节点，每个节点都能回到原稿
          </p>
        </>
      )}
    </div>
  )
}
