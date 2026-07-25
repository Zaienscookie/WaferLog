import { useEffect, useState } from 'react'
import { Link, useNavigate, useParams, useSearchParams } from 'react-router-dom'
import { api, fmtDateTime, todayISO, INPUT_TYPE_LABEL } from '../api'
import type { NoteDetail, TaskItem, TopicWithNotes } from '../types'
import { PaperPage } from '../components/PaperPage'
import { Loading, EmptyHint, Confirm } from '../components/bits'
import { Icon } from '../components/Icon'

type Tab = 'text' | 'relations' | 'tasks'

export function NoteDetailPage() {
  const { id = '' } = useParams()
  const navigate = useNavigate()
  const [detail, setDetail] = useState<NoteDetail | null>(null)
  const [error, setError] = useState('')
  const [params] = useSearchParams()
  const tabParam = params.get('tab')
  const [tab, setTab] = useState<Tab>(tabParam === 'relations' || tabParam === 'tasks' ? tabParam : 'text')
  const [allTopics, setAllTopics] = useState<TopicWithNotes[]>([])
  const [textDraft, setTextDraft] = useState('')
  const [textSavedTip, setTextSavedTip] = useState('')
  const [pendingDelete, setPendingDelete] = useState(false)

  const load = () =>
    api
      .note(id)
      .then((d) => {
        setDetail(d)
        setTextDraft(d.note.rawText)
      })
      .catch((e) => setError(String(e.message || e)))

  useEffect(() => {
    setDetail(null)
    void load()
    api.topics().then(setAllTopics).catch(() => undefined)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [id])

  if (error) return <EmptyHint>{error}</EmptyHint>
  if (!detail) return <Loading />
  const { note, relations, tasks } = detail

  const candidates = tasks.filter((t) => t.status === 'candidate' || t.status === 'later')
  const confirmed = tasks.filter((t) => t.status === 'confirmed')
  const remainingTopics = allTopics.filter((t) => !note.topics.some((x) => x.id === t.id))

  const saveTitle = async (v: string) => {
    if (v.trim() && v !== note.title) {
      const res = await api.updateNote(note.id, { title: v.trim() })
      setDetail((d) => (d ? { ...d, note: res.note } : d))
    }
  }

  const saveTextLayer = async () => {
    const res = await api.updateNote(note.id, { rawText: textDraft })
    setDetail((d) => (d ? { ...d, note: res.note, relations: res.relations } : d))
    setTextSavedTip(`文字层已保存，回响重新显影（${res.relations.length} 条）`)
    window.setTimeout(() => setTextSavedTip(''), 4000)
  }

  const setTopics = async (topicIds: string[]) => {
    const res = await api.updateNote(note.id, { topicIds })
    setDetail((d) => (d ? { ...d, note: res.note, relations: res.relations } : d))
  }

  const patchTask = async (taskId: string, body: Parameters<typeof api.updateTask>[1]) => {
    await api.updateTask(taskId, body)
    await load()
  }

  return (
    <div className="page">
      <header className="page-head">
        <Link to="/notes" className="muted" style={{ fontSize: 14 }}>
          ← 所有记录
        </Link>
        <div style={{ display: 'flex', gap: 8 }}>
          <Link to={`/ask?note=${note.id}`} className="btn">
            <Icon name="ask" width={15} height={15} />
            就此页提问
          </Link>
          <Link to={`/timeline?note=${note.id}`} className="btn">
            <Icon name="timeline" width={15} height={15} />
            记忆线
          </Link>
          <button className="btn btn-ghost btn-danger" onClick={() => setPendingDelete(true)}>
            <Icon name="trash" width={15} height={15} />
          </button>
        </div>
      </header>

      <div className="detail-grid">
        <section>
          <input
            className="detail-title hand"
            defaultValue={note.title}
            key={note.id + note.title}
            onBlur={(e) => void saveTitle(e.target.value)}
            aria-label="标题"
          />
          <div className="detail-sub">
            <span>{fmtDateTime(note.createdAt)}</span>
            <span>·</span>
            <span>{INPUT_TYPE_LABEL[note.inputType]}</span>
            <span>·</span>
            <span className="topic-row">
              {note.topics.map((t) => (
                <span key={t.id} className="chip active">
                  {t.name}
                  <button
                    aria-label={`移出主题 ${t.name}`}
                    style={{ display: 'inline-flex', color: 'inherit' }}
                    onClick={() => void setTopics(note.topics.filter((x) => x.id !== t.id).map((x) => x.id))}
                  >
                    <Icon name="x" width={11} height={11} />
                  </button>
                </span>
              ))}
              {remainingTopics.length > 0 && (
                <select
                  className="input"
                  style={{ width: 'auto', minHeight: 30, padding: '2px 8px', fontSize: 12 }}
                  value=""
                  onChange={(e) => {
                    if (e.target.value) void setTopics([...note.topics.map((t) => t.id), e.target.value])
                  }}
                >
                  <option value="">+ 主题</option>
                  {remainingTopics.map((t) => (
                    <option key={t.id} value={t.id}>
                      {t.name}
                    </option>
                  ))}
                </select>
              )}
            </span>
          </div>
          <div style={{ marginTop: 16 }}>
            <PaperPage note={note} meta={false} className="detail-paper" />
          </div>
        </section>

        <aside>
          <div className="membrane-tabs" role="tablist">
            <button className={`membrane-tab${tab === 'text' ? ' active' : ''}`} onClick={() => setTab('text')}>
              文字层
            </button>
            <button className={`membrane-tab${tab === 'relations' ? ' active' : ''}`} onClick={() => setTab('relations')}>
              关联 {relations.length > 0 && `· ${relations.length}`}
            </button>
            <button className={`membrane-tab${tab === 'tasks' ? ' active' : ''}`} onClick={() => setTab('tasks')}>
              待办 {candidates.length > 0 && `· ${candidates.length}`}
            </button>
          </div>
          <div className="membrane-panel">
            {tab === 'text' && (
              <div>
                <p className="muted" style={{ fontSize: 13, lineHeight: 1.8, marginBottom: 10 }}>
                  AI 只读取文字层做检索与关联，不会改动原稿。
                </p>
                <textarea
                  className="input"
                  rows={9}
                  value={textDraft}
                  onChange={(e) => setTextDraft(e.target.value)}
                  placeholder="这页的文字内容（OCR 或手动转写）"
                />
                <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginTop: 10 }}>
                  <button className="btn btn-primary" onClick={() => void saveTextLayer()}>
                    保存文字层
                  </button>
                  {textSavedTip && (
                    <span className="muted" style={{ fontSize: 12 }}>
                      {textSavedTip}
                    </span>
                  )}
                </div>
              </div>
            )}

            {tab === 'relations' && (
              <div className="relation-list">
                {relations.length === 0 && (
                  <EmptyHint>暂无强关联。等多写几页，或在文字层补充内容后再看。</EmptyHint>
                )}
                {relations.map((rel) => (
                  <Link key={rel.id} to={`/notes/${rel.otherNote.id}`} className="ticket">
                    <div className="ticket-date">{fmtDateTime(rel.otherNote.createdAt)}</div>
                    <div className="ticket-title">{rel.otherNote.title}</div>
                    <div className="ticket-reason">
                      <span className="why">回响依据</span> · {rel.reason}
                    </div>
                  </Link>
                ))}
              </div>
            )}

            {tab === 'tasks' && (
              <div>
                {tasks.length === 0 && (
                  <EmptyHint>这页没有识别出行动项。写下「需要…」「明天…」试试。</EmptyHint>
                )}
                {candidates.map((t) => (
                  <TaskCandidateRow key={t.id} task={t} onPatch={patchTask} />
                ))}
                {confirmed.length > 0 && (
                  <>
                    <h4 className="section-title" style={{ marginTop: 16 }}>
                      已确认
                    </h4>
                    {confirmed.map((t) => (
                      <div key={t.id} className="task-row confirmed">
                        <Icon name="check" width={15} height={15} style={{ color: 'var(--ok)', flexShrink: 0 }} />
                        <span className="grow">
                          {t.title}
                          <span className="muted" style={{ marginLeft: 8, fontSize: 12 }}>
                            {t.dueDate || '未定日期'}
                          </span>
                        </span>
                        <button className="btn btn-ghost" style={{ minHeight: 32, padding: '4px 10px', fontSize: 12 }} onClick={() => void patchTask(t.id, { status: 'candidate' })}>
                          撤回
                        </button>
                      </div>
                    ))}
                  </>
                )}
                <p className="muted" style={{ fontSize: 12, marginTop: 14, lineHeight: 1.8 }}>
                  待办只有在你确认后才会进入时间册。
                </p>
              </div>
            )}
          </div>
        </aside>
      </div>

      {pendingDelete && (
        <Confirm
          title="移入回收站？"
          danger
          confirmText="移入回收站"
          onCancel={() => setPendingDelete(false)}
          onConfirm={async () => {
            await api.deleteNote(note.id)
            navigate('/notes')
          }}
        >
          《{note.title}》会移入回收站，可在设置中恢复。
        </Confirm>
      )}
    </div>
  )
}

function TaskCandidateRow({
  task,
  onPatch,
}: {
  task: TaskItem
  onPatch: (id: string, body: Parameters<typeof api.updateTask>[1]) => Promise<void>
}) {
  const [date, setDate] = useState(task.dueDate || todayISO())
  return (
    <div className="task-row">
      <span className="grow">{task.title}</span>
      <input
        type="date"
        className="date-input"
        value={date}
        onChange={(e) => setDate(e.target.value)}
        aria-label="截止日期"
      />
      <button
        className="btn btn-primary"
        style={{ minHeight: 34, padding: '4px 12px', fontSize: 12 }}
        onClick={() => void onPatch(task.id, { status: 'confirmed', dueDate: date })}
      >
        加入时间册
      </button>
      <button
        className="btn btn-ghost"
        style={{ minHeight: 34, padding: '4px 10px', fontSize: 12 }}
        title="稍后处理"
        onClick={() => void onPatch(task.id, { status: 'later' })}
      >
        稍后
      </button>
      <button
        className="btn btn-ghost"
        style={{ minHeight: 34, padding: '4px 10px', fontSize: 12 }}
        title="忽略"
        onClick={() => void onPatch(task.id, { status: 'dismissed' })}
      >
        <Icon name="x" width={13} height={13} />
      </button>
    </div>
  )
}
