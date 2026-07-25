import { useEffect, useLayoutEffect, useRef, useState } from 'react'
import { Link } from 'react-router-dom'
import { api, fmtDateTime } from '../api'
import type { HomePayload } from '../types'
import { PaperPage } from '../components/PaperPage'
import { Loading, EmptyHint } from '../components/bits'
import { Icon } from '../components/Icon'

interface EchoLine {
  d: string
  x: number
  y: number
  len: number
  key: string
}

export function ArchivePage() {
  const [data, setData] = useState<HomePayload | null>(null)
  const [error, setError] = useState('')

  useEffect(() => {
    api.home().then(setData).catch((e) => setError(String(e.message || e)))
  }, [])

  if (error) return <EmptyHint>连接不到本地档案库：{error}。请确认 server 已启动。</EmptyHint>
  if (!data) return <Loading />

  const { latestNote, echo, recentNotes, stats } = data

  return (
    <div className="page">
      <header className="page-head">
        <h1 className="page-title">
          档案馆
          <span className="sub">随手一笔，久久有回响 · 共 {stats.notes} 页记录</span>
        </h1>
        <Link to="/new" className="btn btn-primary">
          <Icon name="pen" width={16} height={16} />
          落笔
        </Link>
      </header>

      {!latestNote ? (
        <EmptyHint>
          档案馆还是空的。写下第一页，它会在这里等待与未来的你重逢。
          <div style={{ marginTop: 12 }}>
            <Link to="/new" className="btn btn-primary">
              写下第一页
            </Link>
          </div>
        </EmptyHint>
      ) : (
        <div className="archive-grid">
          <section className="latest-stage">
            <h2 className="section-title">刚刚落笔 · {fmtDateTime(latestNote.createdAt)}</h2>
            <PaperPage note={latestNote} meta className="hoverable">
              <div style={{ marginTop: 14 }}>
                <Link to={`/notes/${latestNote.id}`} className="btn latest-open">
                  打开这页
                  <Icon name="right" width={14} height={14} />
                </Link>
              </div>
            </PaperPage>
          </section>

          <section>
            <h2 className="section-title">记忆回路</h2>
            <FilmStage noteId={latestNote.id} echo={echo} />
          </section>
        </div>
      )}

      {recentNotes.length > 0 && (
        <section className="recent-strip">
          <h2 className="section-title">
            还在生长的纸页
            <Link to="/notes" className="muted" style={{ fontSize: 13, marginLeft: 'auto' }}>
              全部记录 →
            </Link>
          </h2>
          <div className="recent-scroll">
            {recentNotes.map((n) => (
              <Link key={n.id} to={`/notes/${n.id}`} className="mini-note">
                <PaperPage note={n} clamp meta />
              </Link>
            ))}
          </div>
        </section>
      )}
    </div>
  )
}

function FilmStage({ noteId, echo }: { noteId: string; echo: HomePayload['echo'] }) {
  const layoutRef = useRef<HTMLDivElement>(null)
  const coreRef = useRef<HTMLDivElement>(null)
  const ticketRefs = useRef<(HTMLAnchorElement | null)[]>([])
  const [lines, setLines] = useState<EchoLine[]>([])

  useLayoutEffect(() => {
    const layout = layoutRef.current
    if (!layout) return
    const measure = () => {
      const core = coreRef.current
      if (!core || layout.clientWidth < 520 || echo.tickets.length === 0) {
        setLines([])
        return
      }
      const lb = layout.getBoundingClientRect()
      const cb = core.getBoundingClientRect()
      const cx = cb.left - lb.left + cb.width / 2
      const cy = cb.top - lb.top + cb.height
      const next: EchoLine[] = []
      ticketRefs.current.slice(0, echo.tickets.length).forEach((el, i) => {
        if (!el) return
        const tb = el.getBoundingClientRect()
        const tx = tb.left - lb.left + tb.width / 2
        const ty = tb.top - lb.top + 6
        const mx = (cx + tx) / 2
        const my = cy + (ty - cy) * 0.15
        const dist = Math.hypot(tx - cx, ty - cy)
        next.push({
          d: `M ${cx.toFixed(1)} ${cy.toFixed(1)} Q ${mx.toFixed(1)} ${my.toFixed(1)} ${tx.toFixed(1)} ${ty.toFixed(1)}`,
          x: tx,
          y: ty,
          len: dist * 1.2,
          key: `${noteId}-${i}`,
        })
      })
      setLines(next)
    }
    const raf = requestAnimationFrame(measure)
    const ro = new ResizeObserver(measure)
    ro.observe(layout)
    return () => {
      cancelAnimationFrame(raf)
      ro.disconnect()
    }
  }, [noteId, echo.tickets.length])

  return (
    <div className="film film-stage">
      <svg className="echo-lines" aria-hidden>
        {lines.map((l) => (
          <g key={l.key}>
            <path d={l.d} style={{ ['--len' as string]: `${l.len.toFixed(0)}` }} />
            <circle cx={l.x} cy={l.y} r={3.4} />
          </g>
        ))}
      </svg>
      <div className="film-layout" ref={layoutRef}>
        <div className="film-core" ref={coreRef}>
          {echo.count > 0 ? (
            <>
              <div className="echo-count">{echo.count}</div>
              <div className="echo-count-label">条回响正在形成</div>
            </>
          ) : (
            <div className="echo-count-label" style={{ padding: '18px 0' }}>
              这页正在等待新的连接
            </div>
          )}
        </div>
        {echo.tickets.map((rel, i) => (
          <Link
            key={rel.id}
            to={`/notes/${rel.otherNote.id}`}
            className="ticket film-ticket"
            ref={(el) => {
              ticketRefs.current[i] = el
            }}
          >
            <div className="ticket-date">{fmtDateTime(rel.otherNote.createdAt)}</div>
            <div className="ticket-title">{rel.otherNote.title}</div>
            <div className="ticket-reason">
              <span className="why">回响依据</span> · {rel.reason}
            </div>
          </Link>
        ))}
        {echo.count > echo.tickets.length && (
          <Link to={`/notes/${noteId}`} className="btn btn-ghost film-more">
            查看全部 {echo.count} 条回响 →
          </Link>
        )}
      </div>
    </div>
  )
}
