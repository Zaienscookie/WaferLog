import { useEffect, useMemo, useState } from 'react'
import { Link } from 'react-router-dom'
import { api, todayISO } from '../api'
import type { CalendarPayload, TaskItem } from '../types'
import { Loading, EmptyHint } from '../components/bits'
import { Icon } from '../components/Icon'

function monthOf(date: Date) {
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}`
}

function shiftMonth(month: string, delta: number) {
  const [y, m] = month.split('-').map(Number)
  return monthOf(new Date(y, m - 1 + delta, 1))
}

function pseudoWeather(city: string, day: string) {
  let h = 0
  const s = `${city}|${day}`
  for (let i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) % 997
  const conds = ['晴', '多云', '阴', '小雨']
  return { cond: conds[h % conds.length], temp: 18 + (h % 14) }
}

export function CalendarPage() {
  const [month, setMonth] = useState(monthOf(new Date()))
  const [data, setData] = useState<CalendarPayload | null>(null)
  const [error, setError] = useState('')
  const [selected, setSelected] = useState(todayISO())

  const load = () =>
    api
      .calendar(month)
      .then(setData)
      .catch((e) => setError(String(e.message || e)))

  useEffect(() => {
    setData(null)
    void load()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [month])

  const city = localStorage.getItem('waferlog.city') || '杭州'

  const cells = useMemo(() => {
    const [y, m] = month.split('-').map(Number)
    const daysInMonth = new Date(y, m, 0).getDate()
    const offset = (new Date(y, m - 1, 1).getDay() + 6) % 7 // 周一开头
    const prevDays = new Date(y, m - 1, 0).getDate()
    const list: { day: string; num: number; inMonth: boolean }[] = []
    for (let i = offset - 1; i >= 0; i--) {
      list.push({ day: `${shiftMonth(month, -1)}-${String(prevDays - i).padStart(2, '0')}`, num: prevDays - i, inMonth: false })
    }
    for (let d = 1; d <= daysInMonth; d++) {
      list.push({ day: `${month}-${String(d).padStart(2, '0')}`, num: d, inMonth: true })
    }
    while (list.length % 7 !== 0) {
      const n = list.length - offset - daysInMonth + 1
      list.push({ day: `${shiftMonth(month, 1)}-${String(n).padStart(2, '0')}`, num: n, inMonth: false })
    }
    return list
  }, [month])

  if (error) return <EmptyHint>{error}</EmptyHint>

  const notesByDay = new Map<string, CalendarPayload['notes']>()
  const tasksByDay = new Map<string, TaskItem[]>()
  if (data) {
    for (const n of data.notes) {
      if (!notesByDay.has(n.day)) notesByDay.set(n.day, [])
      notesByDay.get(n.day)!.push(n)
    }
    for (const t of data.tasks) {
      if (!t.dueDate) continue
      if (!tasksByDay.has(t.dueDate)) tasksByDay.set(t.dueDate, [])
      tasksByDay.get(t.dueDate)!.push(t)
    }
  }
  const today = todayISO()
  const weather = pseudoWeather(city, selected)
  const selNotes = notesByDay.get(selected) || []
  const selTasks = (tasksByDay.get(selected) || []).filter((t) => t.status === 'confirmed')

  return (
    <div className="page">
      <header className="page-head">
        <h1 className="page-title">
          时间册
          <span className="sub">哪天写了什么，哪些行动要发生</span>
        </h1>
      </header>

      <div className="calendar-grid-wrap">
        <section>
          <div className="cal-head">
            <button className="btn btn-ghost" onClick={() => setMonth(shiftMonth(month, -1))} aria-label="上个月">
              <Icon name="left" width={16} height={16} />
            </button>
            <span className="cal-month">
              {month.split('-')[0]} 年 {Number(month.split('-')[1])} 月
            </span>
            <button className="btn btn-ghost" onClick={() => setMonth(shiftMonth(month, 1))} aria-label="下个月">
              <Icon name="right" width={16} height={16} />
            </button>
          </div>
          <div className="cal-week">
            {['一', '二', '三', '四', '五', '六', '日'].map((d) => (
              <span key={d}>{d}</span>
            ))}
          </div>
          {!data ? (
            <Loading />
          ) : (
            <div className="cal-grid">
              {cells.map((c) => {
                const ns = notesByDay.get(c.day) || []
                const ts = (tasksByDay.get(c.day) || []).filter((t) => t.status === 'confirmed')
                const hasEcho = ns.some((n) => n.echoCount > 0)
                return (
                  <button
                    key={c.day}
                    className={`cal-day${c.inMonth ? ' in-month' : ' dim'}${c.day === today ? ' today' : ''}${c.day === selected ? ' selected' : ''}`}
                    onClick={() => setSelected(c.day)}
                  >
                    <span>{c.num}</span>
                    <span className="cal-marks">
                      {ts.length > 0 && <span className="mark-task" title={`${ts.length} 条待办`} />}
                      {hasEcho && <span className="mark-echo">✦</span>}
                      {ns.length > 0 && <span className="mark-note" title={`${ns.length} 页记录`} />}
                    </span>
                  </button>
                )
              })}
            </div>
          )}
          <div className="cal-legend">
            <span>
              <span className="mark-task" /> 已确认待办
            </span>
            <span>
              <span className="mark-echo">✦</span> 有回响
            </span>
            <span>
              <span className="mark-note" /> 当天有记录
            </span>
          </div>

          <div className="day-detail">
            <h2 className="section-title">
              {Number(selected.split('-')[1])} 月 {Number(selected.split('-')[2])} 日
            </h2>
            {selTasks.length === 0 && selNotes.length === 0 && (
              <p className="muted" style={{ fontSize: 13 }}>
                这一天还没有待办或记录。
              </p>
            )}
            {selTasks.map((t) => (
              <div key={t.id} className="task-row confirmed">
                <Icon name="check" width={14} height={14} style={{ color: 'var(--ok)', flexShrink: 0 }} />
                <span className="grow">{t.title}</span>
                <span className="muted" style={{ fontSize: 12 }}>
                  来自《{t.noteTitle}》
                </span>
              </div>
            ))}
            <div style={{ display: 'grid', gap: 8, marginTop: selTasks.length > 0 ? 10 : 0 }}>
              {selNotes.map((n) => (
                <Link key={n.id} to={`/notes/${n.id}`} className="box-paper-link">
                  {n.title}
                  {n.echoCount > 0 && <span className="echo-badge" style={{ fontSize: 11 }}>✦ {n.echoCount}</span>}
                </Link>
              ))}
            </div>
          </div>
        </section>

        <aside className="candidate-panel">
          <div className="weather-card">
            <Icon name="sun" width={26} height={26} style={{ color: 'var(--amber)' }} />
            <div>
              <div>
                {city} · {weather.cond}
                <span className="temp" style={{ marginLeft: 10 }}>
                  {weather.temp}°
                </span>
              </div>
              <div className="muted" style={{ fontSize: 11 }}>
                本地天气 · 演示数据，可在设置修改城市
              </div>
            </div>
          </div>

          <h2 className="section-title">待确认 · {data?.candidates.length ?? 0}</h2>
          {!data ? (
            <Loading />
          ) : data.candidates.length === 0 ? (
            <EmptyHint>没有待确认的待办。AI 只给候选，确认权在你。</EmptyHint>
          ) : (
            data.candidates.map((t) => <CandidateRow key={t.id} task={t} onDone={load} />)
          )}
        </aside>
      </div>
    </div>
  )
}

function CandidateRow({ task, onDone }: { task: TaskItem; onDone: () => void }) {
  const [date, setDate] = useState(task.dueDate || todayISO())
  const patch = async (body: Parameters<typeof api.updateTask>[1]) => {
    await api.updateTask(task.id, body)
    onDone()
  }
  return (
    <div className="task-row" style={{ flexWrap: 'wrap' }}>
      <span className="grow" style={{ flexBasis: '100%' }}>
        {task.title}
        <span className="muted" style={{ display: 'block', fontSize: 11, marginTop: 2 }}>
          来自《{task.noteTitle}》
        </span>
      </span>
      <input type="date" className="date-input" value={date} onChange={(e) => setDate(e.target.value)} aria-label="截止日期" />
      <button className="btn btn-primary" style={{ minHeight: 34, padding: '4px 12px', fontSize: 12 }} onClick={() => void patch({ status: 'confirmed', dueDate: date })}>
        一键加入
      </button>
      <button className="btn btn-ghost" style={{ minHeight: 34, padding: '4px 10px', fontSize: 12 }} onClick={() => void patch({ status: 'later' })}>
        稍后
      </button>
      <button className="btn btn-ghost" style={{ minHeight: 34, padding: '4px 10px', fontSize: 12 }} title="忽略" onClick={() => void patch({ status: 'dismissed' })}>
        <Icon name="x" width={13} height={13} />
      </button>
    </div>
  )
}
