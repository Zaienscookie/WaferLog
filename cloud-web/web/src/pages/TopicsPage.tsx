import { useEffect, useState } from 'react'
import { Link } from 'react-router-dom'
import { api, fmtDate } from '../api'
import type { TopicWithNotes } from '../types'
import { Loading, EmptyHint, Modal, Confirm } from '../components/bits'
import { Icon } from '../components/Icon'

export function TopicsPage() {
  const [topics, setTopics] = useState<TopicWithNotes[] | null>(null)
  const [openId, setOpenId] = useState<string | null>(null)
  const [error, setError] = useState('')
  const [creating, setCreating] = useState(false)
  const [renaming, setRenaming] = useState<TopicWithNotes | null>(null)
  const [merging, setMerging] = useState<TopicWithNotes | null>(null)

  const load = () =>
    api
      .topics()
      .then(setTopics)
      .catch((e) => setError(String(e.message || e)))

  useEffect(() => {
    void load()
  }, [])

  if (error) return <EmptyHint>{error}</EmptyHint>
  if (!topics) return <Loading />

  const removeFromTopic = async (noteId: string, topicId: string) => {
    const detail = await api.note(noteId)
    await api.updateNote(noteId, {
      topicIds: detail.note.topics.filter((t) => t.id !== topicId).map((t) => t.id),
    })
    await load()
  }

  return (
    <div className="page">
      <header className="page-head">
        <h1 className="page-title">
          主题盒
          <span className="sub">零散记录自己长成主题，不用在记录时分类</span>
        </h1>
        <button className="btn btn-primary" onClick={() => setCreating(true)}>
          <Icon name="plus" width={15} height={15} />
          新主题盒
        </button>
      </header>

      {topics.length === 0 && <EmptyHint>还没有主题。写下几页记录后，主题会在这里聚合成盒。</EmptyHint>}

      <div className="topics-grid">
        {topics.map((t) => (
          <section key={t.id} className="box">
            <div className="box-lid" onClick={() => setOpenId(openId === t.id ? null : t.id)}>
              <span className="box-name">{t.name}</span>
              <span className="box-count">
                {t.noteCount} 页{t.echoCount > 0 ? ` · ✦ ${t.echoCount}` : ''}
              </span>
              <span className="muted" style={{ marginLeft: 'auto' }}>
                <Icon name={openId === t.id ? 'down' : 'right'} width={14} height={14} />
              </span>
            </div>
            {t.description && (
              <p className="muted" style={{ fontSize: 12, marginTop: 6, lineHeight: 1.7 }}>
                {t.description}
              </p>
            )}
            {openId === t.id && (
              <>
                <div className="box-papers">
                  {t.notes.length === 0 && <span className="muted" style={{ fontSize: 13 }}>盒子里还没有纸页。</span>}
                  {t.notes.map((n) => (
                    <span key={n.id} className="box-paper-link">
                      <Link to={`/notes/${n.id}`} style={{ flex: 1, minWidth: 0 }}>
                        {n.title}
                        <span className="muted" style={{ marginLeft: 8, fontSize: 11 }}>
                          {fmtDate(n.createdAt)}
                        </span>
                      </Link>
                      <button
                        className="btn btn-ghost"
                        style={{ minHeight: 28, padding: '2px 8px', fontSize: 11 }}
                        title="移出此主题"
                        onClick={() => void removeFromTopic(n.id, t.id)}
                      >
                        移出
                      </button>
                    </span>
                  ))}
                </div>
                <div className="box-actions">
                  <button className="btn" onClick={() => setRenaming(t)}>
                    改名 / 描述
                  </button>
                  <button className="btn" onClick={() => setMerging(t)}>
                    合并到…
                  </button>
                </div>
              </>
            )}
          </section>
        ))}
      </div>

      {creating && (
        <TopicForm
          title="新主题盒"
          onCancel={() => setCreating(false)}
          onSubmit={async (name, description) => {
            await api.createTopic(name, description)
            setCreating(false)
            await load()
          }}
        />
      )}
      {renaming && (
        <TopicForm
          title={`编辑「${renaming.name}」`}
          initialName={renaming.name}
          initialDesc={renaming.description}
          onCancel={() => setRenaming(null)}
          onSubmit={async (name, description) => {
            await api.updateTopic(renaming.id, { name, description })
            setRenaming(null)
            await load()
          }}
        />
      )}
      {merging && (
        <MergeForm
          topic={merging}
          others={topics.filter((x) => x.id !== merging.id)}
          onCancel={() => setMerging(null)}
          onDone={async () => {
            setMerging(null)
            await load()
          }}
        />
      )}
    </div>
  )
}

function TopicForm({
  title,
  initialName = '',
  initialDesc = '',
  onSubmit,
  onCancel,
}: {
  title: string
  initialName?: string
  initialDesc?: string
  onSubmit: (name: string, desc: string) => Promise<void>
  onCancel: () => void
}) {
  const [name, setName] = useState(initialName)
  const [desc, setDesc] = useState(initialDesc)
  return (
    <Modal
      title={title}
      onClose={onCancel}
      actions={
        <>
          <button className="btn" onClick={onCancel}>
            取消
          </button>
          <button className="btn btn-primary" disabled={!name.trim()} onClick={() => void onSubmit(name.trim(), desc.trim())}>
            保存
          </button>
        </>
      }
    >
      <span style={{ display: 'grid', gap: 10 }}>
        <input className="input" placeholder="主题名" value={name} onChange={(e) => setName(e.target.value)} autoFocus />
        <input className="input" placeholder="一句话描述（可空）" value={desc} onChange={(e) => setDesc(e.target.value)} />
      </span>
    </Modal>
  )
}

function MergeForm({
  topic,
  others,
  onCancel,
  onDone,
}: {
  topic: TopicWithNotes
  others: TopicWithNotes[]
  onCancel: () => void
  onDone: () => Promise<void>
}) {
  const [intoId, setIntoId] = useState('')
  const [confirming, setConfirming] = useState(false)
  if (confirming) {
    const target = others.find((t) => t.id === intoId)
    return (
      <Confirm
        title="合并主题盒？"
        confirmText="合并"
        onCancel={onCancel}
        onConfirm={async () => {
          await api.mergeTopic(topic.id, intoId)
          await onDone()
        }}
      >
        「{topic.name}」里的 {topic.noteCount} 页记录会并入「{target?.name}」，「{topic.name}」将被移除。
      </Confirm>
    )
  }
  return (
    <Modal
      title={`把「${topic.name}」合并到…`}
      onClose={onCancel}
      actions={
        <>
          <button className="btn" onClick={onCancel}>
            取消
          </button>
          <button className="btn btn-primary" disabled={!intoId} onClick={() => setConfirming(true)}>
            下一步
          </button>
        </>
      }
    >
      <select className="input" value={intoId} onChange={(e) => setIntoId(e.target.value)}>
        <option value="">选择目标主题</option>
        {others.map((t) => (
          <option key={t.id} value={t.id}>
            {t.name}（{t.noteCount} 页）
          </option>
        ))}
      </select>
    </Modal>
  )
}
