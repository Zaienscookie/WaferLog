import { useEffect, useState } from 'react'
import { Link, useSearchParams } from 'react-router-dom'
import { api, fmtDate } from '../api'
import type { AskResult, Note, TopicWithNotes } from '../types'
import { Loading, EmptyHint } from '../components/bits'
import { Icon } from '../components/Icon'

const EXAMPLES = [
  '我以前为什么认为低干扰输入重要？',
  '和产品设计有关的想法有哪些还没有变成行动？',
  '我在这个项目上反复提到的担忧是什么？',
]

type Scope = { type: 'all' | 'topic' | 'note'; id?: string }

export function AskPage() {
  const [params] = useSearchParams()
  const initialNote = params.get('note')
  const [question, setQuestion] = useState('')
  const [scope, setScope] = useState<Scope>(initialNote ? { type: 'note', id: initialNote } : { type: 'all' })
  const [topics, setTopics] = useState<TopicWithNotes[]>([])
  const [notes, setNotes] = useState<Note[]>([])
  const [result, setResult] = useState<AskResult | null>(null)
  const [asking, setAsking] = useState(false)
  const [error, setError] = useState('')

  useEffect(() => {
    api.topics().then(setTopics).catch(() => undefined)
    api.notes().then(setNotes).catch(() => undefined)
    // 支持 /ask?q=… 直接提问（可分享的提问链接）
    const q = params.get('q')
    if (q) {
      setQuestion(q)
      void submit(q)
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  const submit = async (q?: string) => {
    const questionText = (q ?? question).trim()
    if (!questionText || asking) return
    setAsking(true)
    setResult(null)
    setError('')
    try {
      const res = await api.ask(questionText, scope)
      setResult(res)
    } catch (e) {
      setError(String((e as Error).message || e))
    } finally {
      setAsking(false)
    }
  }

  const scopeNote = scope.type === 'note' ? notes.find((n) => n.id === scope.id) : null

  return (
    <div className="page">
      <header className="page-head">
        <h1 className="page-title">
          知识问答
          <span className="sub">只面向「我的档案」的提问，每个回答都能回到来源</span>
        </h1>
      </header>

      <div className="ask-stage">
        <div className="ask-box">
          <textarea
            className="input hand"
            placeholder="问一问过去的记录…"
            value={question}
            onChange={(e) => setQuestion(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault()
                void submit()
              }
            }}
          />
          <button className="btn btn-primary" style={{ minHeight: 52 }} disabled={!question.trim() || asking} onClick={() => void submit()}>
            <Icon name="ask" width={16} height={16} />
            提问
          </button>
        </div>

        <div className="ask-scope">
          <button className={`chip${scope.type === 'all' ? ' active' : ''}`} onClick={() => setScope({ type: 'all' })}>
            全部档案
          </button>
          {topics.map((t) => (
            <button
              key={t.id}
              className={`chip${scope.type === 'topic' && scope.id === t.id ? ' active' : ''}`}
              onClick={() => setScope({ type: 'topic', id: t.id })}
            >
              {t.name}
            </button>
          ))}
          <select
            className="input"
            style={{ width: 'auto', minHeight: 30, padding: '2px 8px', fontSize: 12 }}
            value={scope.type === 'note' ? scope.id : ''}
            onChange={(e) => {
              if (e.target.value) setScope({ type: 'note', id: e.target.value })
              else setScope({ type: 'all' })
            }}
            aria-label="限定到某一页"
          >
            <option value="">限定某一页…</option>
            {notes.map((n) => (
              <option key={n.id} value={n.id}>
                {n.title}
              </option>
            ))}
          </select>
          {scopeNote && (
            <span className="chip active">
              仅《{scopeNote.title}》
              <button style={{ display: 'inline-flex', color: 'inherit' }} onClick={() => setScope({ type: 'all' })} aria-label="取消限定">
                <Icon name="x" width={11} height={11} />
              </button>
            </span>
          )}
        </div>

        {error && <EmptyHint>{error}</EmptyHint>}
        {asking && <Loading text="正在档案里显影" />}

        {!result && !asking && !error && (
          <div className="ask-examples">
            {EXAMPLES.map((q) => (
              <button
                key={q}
                className="ask-example"
                onClick={() => {
                  setQuestion(q)
                  void submit(q)
                }}
              >
                {q}
              </button>
            ))}
          </div>
        )}

        {result && !asking && (
          <div className="answer-sheet">
            <div className="paper">
              <div className="paper-body answer-text">{result.answer}</div>
            </div>
            {result.sources.length > 0 ? (
              <div className="source-cards">
                {result.sources.map((s) => (
                  <Link key={s.noteId} to={`/notes/${s.noteId}`} className="ticket source-card">
                    <div className="ticket-date">{fmtDate(s.createdAt)} · 来源</div>
                    <div className="ticket-title">{s.title}</div>
                    <p className="quote">「{s.excerpt}」</p>
                    <div className="source-kws">
                      {s.shared.map((w) => (
                        <span key={w} className="chip" style={{ minHeight: 24, fontSize: 11, padding: '2px 8px' }}>
                          {w}
                        </span>
                      ))}
                    </div>
                  </Link>
                ))}
              </div>
            ) : (
              <p className="muted" style={{ fontSize: 13, marginTop: 12, lineHeight: 1.8 }}>
                硅笺不编造没有来源的回答。先多记录几页，答案会自己长出来。
              </p>
            )}
          </div>
        )}
      </div>
    </div>
  )
}
