import { useEffect, useState } from 'react'
import { api, fmtDateTime } from '../api'
import type { Note } from '../types'
import { EmptyHint, Confirm } from '../components/bits'
import { Icon } from '../components/Icon'

function usePref(key: string, fallback: string) {
  const [value, setValue] = useState(() => localStorage.getItem(key) || fallback)
  const set = (v: string) => {
    localStorage.setItem(key, v)
    setValue(v)
  }
  return [value, set] as const
}

export function SettingsPage() {
  const [theme, setTheme] = usePref('waferlog.theme', 'light')
  const [paperStyle, setPaperStyle] = usePref('waferlog.paperStyle', 'ruled')
  const [city, setCity] = usePref('waferlog.city', '杭州')
  const [trash, setTrash] = useState<Note[]>([])
  const [error, setError] = useState('')
  const [destroying, setDestroying] = useState<Note | null>(null)

  const loadTrash = () =>
    api
      .notes({ deleted: '1' })
      .then(setTrash)
      .catch((e) => setError(String(e.message || e)))

  useEffect(() => {
    void loadTrash()
  }, [])

  useEffect(() => {
    document.documentElement.dataset.theme = theme
  }, [theme])

  const exportData = async () => {
    const res = await fetch('/api/export')
    const blob = await res.blob()
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `waferlog-export-${new Date().toISOString().slice(0, 10)}.json`
    a.click()
    URL.revokeObjectURL(url)
  }

  return (
    <div className="page">
      <header className="page-head">
        <h1 className="page-title">
          设置
          <span className="sub">硬件、同步与体验</span>
        </h1>
      </header>

      {error && <EmptyHint>{error}</EmptyHint>}

      <div className="settings-grid">
        <section className="settings-card">
          <h3>
            <span className="device-dot" />
            T5 设备
          </h3>
          <div className="setting-row">
            <span>连接状态</span>
            <span className="muted">已连接 · 演示</span>
          </div>
          <div className="setting-row">
            <span>Wi-Fi</span>
            <span className="muted">waferlog-home</span>
          </div>
          <div className="setting-row">
            <span>电量</span>
            <span className="muted">82%</span>
          </div>
          <div className="setting-row">
            <span>上次同步</span>
            <span className="muted">
              <Icon name="sync" width={12} height={12} style={{ verticalAlign: '-2px' }} /> 刚刚
            </span>
          </div>
        </section>

        <section className="settings-card">
          <h3>体验偏好</h3>
          <div className="setting-row">
            <span>默认纸张</span>
            <span className="seg">
              {(
                [
                  ['ruled', '横线'],
                  ['grid', '方格'],
                  ['blank', '空白'],
                ] as const
              ).map(([v, label]) => (
                <button key={v} className={paperStyle === v ? 'active' : ''} onClick={() => setPaperStyle(v)}>
                  {label}
                </button>
              ))}
            </span>
          </div>
          <div className="setting-row">
            <span>外观</span>
            <span className="seg">
              {(
                [
                  ['light', '纸白'],
                  ['dark', '墨夜'],
                ] as const
              ).map(([v, label]) => (
                <button key={v} className={theme === v ? 'active' : ''} onClick={() => setTheme(v)}>
                  {label}
                </button>
              ))}
            </span>
          </div>
          <div className="setting-row">
            <span>天气城市</span>
            <input
              className="input"
              style={{ width: 120, minHeight: 34, padding: '4px 10px' }}
              value={city}
              onChange={(e) => setCity(e.target.value)}
            />
          </div>
          <div className="setting-row">
            <span>语言</span>
            <span className="muted">中文</span>
          </div>
        </section>

        <section className="settings-card">
          <h3>回收站 · {trash.length}</h3>
          {trash.length === 0 && <p className="muted" style={{ fontSize: 13 }}>回收站是空的。</p>}
          {trash.map((n) => (
            <div key={n.id} className="trash-row">
              <span className="grow">
                {n.title}
                <span className="muted" style={{ marginLeft: 8, fontSize: 11 }}>
                  {fmtDateTime(n.createdAt)}
                </span>
              </span>
              <button
                className="btn"
                style={{ minHeight: 30, padding: '2px 10px', fontSize: 12 }}
                onClick={async () => {
                  await api.restoreNote(n.id)
                  await loadTrash()
                }}
              >
                <Icon name="restore" width={12} height={12} /> 恢复
              </button>
              <button className="btn btn-ghost btn-danger" style={{ minHeight: 30, padding: '2px 8px' }} onClick={() => setDestroying(n)}>
                <Icon name="trash" width={13} height={13} />
              </button>
            </div>
          ))}
        </section>

        <section className="settings-card">
          <h3>数据</h3>
          <div className="setting-row">
            <span>导出全部档案</span>
            <button className="btn" onClick={() => void exportData()}>
              <Icon name="download" width={14} height={14} /> JSON
            </button>
          </div>
          <div className="setting-row">
            <span>存储</span>
            <span className="muted">本地 SQLite · 云端同步演示</span>
          </div>
        </section>

        <section className="settings-card">
          <h3>关于</h3>
          <p className="hand" style={{ fontSize: 18, lineHeight: 1.8 }}>
            随手一笔，久久有回响。
          </p>
          <p className="muted" style={{ fontSize: 12, marginTop: 8, lineHeight: 1.8 }}>
            WaferLog 硅笺 · Adventure X 黑客松原型
            <br />
            原稿优先 · AI 不打断书写 · 一切结论可溯源
          </p>
        </section>
      </div>

      {destroying && (
        <Confirm
          title="彻底删除？"
          danger
          confirmText="彻底删除"
          onCancel={() => setDestroying(null)}
          onConfirm={async () => {
            await api.destroyNote(destroying.id)
            setDestroying(null)
            await loadTrash()
          }}
        >
          《{destroying.title}》将无法找回，原稿与关联都会移除。
        </Confirm>
      )}
    </div>
  )
}
