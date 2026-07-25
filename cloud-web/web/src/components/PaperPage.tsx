import { getStroke } from 'perfect-freehand'
import type { Note, StrokeDoc } from '../types'
import { fmtDate, INPUT_TYPE_LABEL } from '../api'
import { Icon } from './Icon'

export function parseStrokeDoc(rawContent: string): StrokeDoc | null {
  try {
    const doc = JSON.parse(rawContent) as StrokeDoc
    if (doc && Array.isArray(doc.strokes) && Array.isArray(doc.viewBox)) return doc
  } catch {
    /* 不是笔画文档 */
  }
  return null
}

export function getSvgPathFromStroke(points: number[][]): string {
  const len = points.length
  if (len < 2) return ''
  let d = `M ${points[0][0].toFixed(1)} ${points[0][1].toFixed(1)} Q`
  for (let i = 0; i < len; i++) {
    const a = points[i]
    const b = points[(i + 1) % len]
    d += ` ${a[0].toFixed(1)} ${a[1].toFixed(1)} ${((a[0] + b[0]) / 2).toFixed(1)} ${((a[1] + b[1]) / 2).toFixed(1)}`
  }
  return `${d} Z`
}

export function Strokes({
  doc,
  inkColor,
  className,
}: {
  doc: StrokeDoc
  inkColor?: string
  className?: string
}) {
  const [w, h] = doc.viewBox
  return (
    <svg className={className ?? 'paper-strokes'} viewBox={`0 0 ${w} ${h}`} preserveAspectRatio="xMidYMid meet">
      {doc.strokes.map((s, i) => {
        const outline = getStroke(s.points, {
          size: s.size * 2.2,
          thinning: 0.55,
          smoothing: 0.6,
          streamline: 0.4,
          last: true,
        })
        const d = getSvgPathFromStroke(outline)
        return d ? <path key={i} d={d} fill={s.color || inkColor || 'currentColor'} /> : null
      })}
    </svg>
  )
}

function tiltOf(id: string) {
  let hash = 0
  for (let i = 0; i < id.length; i++) hash = (hash * 31 + id.charCodeAt(i)) % 997
  return ((hash % 30) / 10 - 1.5).toFixed(2)
}

export function PaperContent({ note, clamp = false }: { note: Note; clamp?: boolean }) {
  const doc = parseStrokeDoc(note.rawContent)
  if (doc && doc.strokes.length > 0) {
    return <Strokes doc={doc} inkColor="var(--ink)" />
  }
  const text = note.rawText || '（空白纸页）'
  return <div className={`paper-text${clamp ? ' clamp' : ''}`}>{text}</div>
}

export function PaperPage({
  note,
  clamp = false,
  meta = true,
  ruled,
  className = '',
  children,
}: {
  note: Note
  clamp?: boolean
  meta?: boolean
  ruled?: boolean
  className?: string
  children?: React.ReactNode
}) {
  const paperStyle = (localStorage.getItem('waferlog.paperStyle') || 'ruled') as string
  const useRuled = ruled !== undefined ? ruled : paperStyle === 'ruled'
  const grid = paperStyle === 'grid'
  return (
    <div
      className={`paper${useRuled ? ' paper-ruled' : ''}${grid ? ' paper-grid' : ''} ${className}`}
      style={{ ['--tilt' as string]: `${tiltOf(note.id)}deg` }}
    >
      <div className="paper-body">
        <PaperContent note={note} clamp={clamp} />
        {children}
      </div>
      {meta && (
        <div className="paper-meta">
          <span>{fmtDate(note.createdAt)}</span>
          <span>·</span>
          <span>{INPUT_TYPE_LABEL[note.inputType]}</span>
          <span className="spacer" />
          {(note.echoCount ?? 0) > 0 && (
            <span className="echo-badge" title="回响数量">
              <Icon name="sparkle" />
              {note.echoCount}
            </span>
          )}
        </div>
      )}
    </div>
  )
}
