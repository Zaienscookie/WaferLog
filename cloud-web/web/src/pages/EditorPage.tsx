import { useCallback, useEffect, useRef, useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { getStroke } from 'perfect-freehand'
import { api } from '../api'
import type { InputType, Stroke } from '../types'
import { getSvgPathFromStroke } from '../components/PaperPage'
import { Icon } from '../components/Icon'

const VB_W = 1000
const VB_H = 1400

const INKS = [
  { name: '墨', value: '#3a3128' },
  { name: '琥珀', value: '#b06c2a' },
  { name: '灰', value: '#8d8271' },
]
const SIZES = [
  { name: '细', value: 4 },
  { name: '中', value: 6.5 },
  { name: '粗', value: 10 },
]

type SaveState = 'idle' | 'dirty' | 'saving' | 'saved'

interface Ripple {
  id: number
  x: number
  y: number
}

export function EditorPage() {
  const navigate = useNavigate()
  const [mode, setMode] = useState<InputType>('handwriting')
  const [title, setTitle] = useState('')
  const titleTouched = useRef(false)
  const [strokes, setStrokes] = useState<Stroke[]>([])
  const [active, setActive] = useState<Stroke | null>(null)
  const [text, setText] = useState('')
  const [handText, setHandText] = useState('')
  const [ink, setInk] = useState(INKS[0].value)
  const [size, setSize] = useState(SIZES[1].value)
  const [erasing, setErasing] = useState(false)
  const [noteId, setNoteId] = useState<string | null>(null)
  const [saveState, setSaveState] = useState<SaveState>('idle')
  const [toast, setToast] = useState<{ echoCount: number } | null>(null)
  const [ripples, setRipples] = useState<Ripple[]>([])
  const svgRef = useRef<SVGSVGElement>(null)
  const paperRef = useRef<HTMLDivElement>(null)
  const saveTimer = useRef<number | null>(null)
  const rippleId = useRef(0)

  // ---------- 语音 ----------
  const SR =
    typeof window !== 'undefined'
      ? ((window as unknown as Record<string, unknown>).SpeechRecognition ||
          (window as unknown as Record<string, unknown>).webkitSpeechRecognition)
      : null
  const [listening, setListening] = useState(false)
  const recogRef = useRef<{ stop: () => void } | null>(null)

  const toggleVoice = () => {
    if (!SR) return
    if (listening) {
      recogRef.current?.stop()
      setListening(false)
      return
    }
    const R = SR as new () => {
      lang: string
      continuous: boolean
      interimResults: boolean
      onresult: (e: { results: ArrayLike<{ isFinal: boolean; 0: { transcript: string } }> }) => void
      onend: () => void
      start: () => void
      stop: () => void
    }
    const recog = new R()
    recog.lang = 'zh-CN'
    recog.continuous = true
    recog.interimResults = true
    recog.onresult = (e) => {
      let final = ''
      for (let i = 0; i < e.results.length; i++) {
        if (e.results[i].isFinal) final += e.results[i][0].transcript
      }
      if (final) setText((t) => (t ? `${t}${final}` : final))
    }
    recog.onend = () => setListening(false)
    recogRef.current = recog
    recog.start()
    setListening(true)
  }

  // ---------- 保存 ----------
  const isEmpty = useCallback(() => {
    if (mode === 'handwriting') return strokes.length === 0 && !handText.trim()
    return !text.trim()
  }, [mode, strokes, handText, text])

  const save = useCallback(async () => {
    if (isEmpty()) return
    setSaveState('saving')
    try {
      const payload =
        mode === 'handwriting'
          ? {
              inputType: 'handwriting' as const,
              rawContent: JSON.stringify({ viewBox: [VB_W, VB_H], strokes }),
              rawText: handText,
              title: title || undefined,
            }
          : {
              inputType: mode,
              rawContent: text,
              rawText: text,
              title: title || undefined,
            }
      if (!noteId) {
        const res = await api.createNote(payload)
        setNoteId(res.note.id)
        if (!titleTouched.current) setTitle(res.note.title)
        setToast({ echoCount: res.relations.length })
        window.setTimeout(() => setToast(null), 6000)
      } else {
        await api.updateNote(noteId, payload)
      }
      setSaveState('saved')
    } catch {
      setSaveState('dirty')
    }
  }, [isEmpty, mode, strokes, handText, text, title, noteId])

  const scheduleSave = useCallback(() => {
    if (isEmpty()) return
    setSaveState('dirty')
    if (saveTimer.current) window.clearTimeout(saveTimer.current)
    saveTimer.current = window.setTimeout(() => {
      void save()
    }, 900)
  }, [isEmpty, save])

  useEffect(() => {
    return () => {
      if (saveTimer.current) window.clearTimeout(saveTimer.current)
    }
  }, [])

  // 文本变化自动保存
  const onTextChange = (v: string) => {
    setText(v)
    scheduleSave()
  }
  const onHandTextChange = (v: string) => {
    setHandText(v)
    scheduleSave()
  }

  // ---------- 手写 ----------
  const toViewBox = (e: React.PointerEvent): [number, number, number] => {
    const svg = svgRef.current!
    const pt = svg.createSVGPoint()
    pt.x = e.clientX
    pt.y = e.clientY
    const ctm = svg.getScreenCTM()
    const p = ctm ? pt.matrixTransform(ctm.inverse()) : pt
    return [p.x, p.y, e.pressure > 0 ? e.pressure : 0.5]
  }

  const spawnRipple = (clientX: number, clientY: number) => {
    const paper = paperRef.current
    if (!paper) return
    const b = paper.getBoundingClientRect()
    const id = ++rippleId.current
    setRipples((r) => [...r, { id, x: clientX - b.left, y: clientY - b.top }])
    window.setTimeout(() => setRipples((r) => r.filter((x) => x.id !== id)), 600)
  }

  const eraseAt = (pt: [number, number, number]) => {
    setStrokes((prev) => {
      const idx = prev.findIndex((s) =>
        s.points.some((p) => Math.hypot(p[0] - pt[0], p[1] - pt[1]) < 26 + s.size)
      )
      if (idx === -1) return prev
      const next = [...prev.slice(0, idx), ...prev.slice(idx + 1)]
      return next
    })
    scheduleSave()
  }

  const onPointerDown = (e: React.PointerEvent) => {
    if (mode !== 'handwriting') return
    e.preventDefault()
    ;(e.target as Element).setPointerCapture?.(e.pointerId)
    const pt = toViewBox(e)
    if (erasing) {
      eraseAt(pt)
      return
    }
    setActive({ points: [pt], color: ink, size })
  }

  const onPointerMove = (e: React.PointerEvent) => {
    if (!active) return
    const pt = toViewBox(e)
    setActive((a) => {
      if (!a) return a
      const last = a.points[a.points.length - 1]
      if (Math.hypot(pt[0] - last[0], pt[1] - last[1]) < 1.5) return a
      return { ...a, points: [...a.points, pt] }
    })
  }

  const onPointerUp = (e: React.PointerEvent) => {
    if (!active) return
    if (active.points.length === 1) {
      // 单点也落成一笔（圆点）
      const [x, y, p] = active.points[0]
      active.points.push([x + 0.1, y + 0.1, p])
    }
    setStrokes((s) => [...s, active])
    setActive(null)
    spawnRipple(e.clientX, e.clientY)
    scheduleSave()
  }

  const undo = () => {
    setStrokes((s) => s.slice(0, -1))
    scheduleSave()
  }

  const goBack = async () => {
    if (saveTimer.current) window.clearTimeout(saveTimer.current)
    await save()
    navigate('/')
  }

  const strokePath = (s: Stroke) => {
    const outline = getStroke(s.points, {
      size: s.size * 2.2,
      thinning: 0.55,
      smoothing: 0.6,
      streamline: 0.4,
      last: true,
    })
    return getSvgPathFromStroke(outline)
  }

  const paperStyle = localStorage.getItem('waferlog.paperStyle') || 'ruled'

  return (
    <div className="editor-overlay">
      <div className="editor-head">
        <button className="btn btn-ghost" onClick={() => void goBack()} aria-label="收起">
          <Icon name="left" width={18} height={18} />
        </button>
        <input
          className="editor-title-input"
          placeholder="无标题纸页"
          value={title}
          onChange={(e) => {
            titleTouched.current = true
            setTitle(e.target.value)
            scheduleSave()
          }}
        />
        <div className="editor-modes" role="tablist">
          {(
            [
              ['handwriting', '手写', 'hand'],
              ['text', '文字', 'type'],
              ['voice', '语音', 'mic'],
            ] as const
          ).map(([m, label, icon]) => (
            <button
              key={m}
              role="tab"
              className={`mode-tab${mode === m ? ' active' : ''}`}
              disabled={!!noteId && mode !== m}
              title={noteId && mode !== m ? '本页已按当前方式保存' : undefined}
              onClick={() => setMode(m)}
            >
              <Icon name={icon} width={14} height={14} style={{ verticalAlign: '-3px', marginRight: 4 }} />
              {label}
            </button>
          ))}
        </div>
        <span className={`save-state${saveState === 'saved' ? ' saved' : ''}`}>
          {saveState === 'saving' && '保存中…'}
          {saveState === 'saved' && (
            <>
              <Icon name="check" width={13} height={13} /> 已自动保存
            </>
          )}
          {saveState === 'dirty' && '待保存'}
          {saveState === 'idle' && '落笔即存'}
        </span>
      </div>

      <div className="editor-stage">
        <div
          ref={paperRef}
          className={`editor-paper${paperStyle === 'ruled' ? ' paper-ruled' : ''}${paperStyle === 'grid' ? ' paper-grid' : ''}`}
        >
          {mode === 'handwriting' ? (
            <svg
              ref={svgRef}
              viewBox={`0 0 ${VB_W} ${VB_H}`}
              preserveAspectRatio="xMidYMid meet"
              onPointerDown={onPointerDown}
              onPointerMove={onPointerMove}
              onPointerUp={onPointerUp}
              onPointerCancel={() => setActive(null)}
            >
              {strokes.map((s, i) => (
                <path key={i} d={strokePath(s)} fill={s.color} />
              ))}
              {active && <path d={strokePath(active)} fill={active.color} />}
            </svg>
          ) : (
            <textarea
              className="editor-textarea"
              placeholder={mode === 'voice' ? '点开右下麦克风开始说，或直接输入…' : '随手写，不用整理…'}
              value={text}
              onChange={(e) => onTextChange(e.target.value)}
              autoFocus
            />
          )}

          {mode === 'handwriting' && (
            <input
              className="input"
              style={{
                position: 'absolute',
                left: 14,
                bottom: 14,
                width: 'min(46%, 340px)',
                zIndex: 6,
                fontSize: 13,
                minHeight: 38,
                opacity: 0.92,
              }}
              placeholder="补一句这页写了什么（用于检索与回响）"
              value={handText}
              onChange={(e) => onHandTextChange(e.target.value)}
            />
          )}

          <div className="editor-dock">
            {mode === 'handwriting' ? (
              <>
                {INKS.map((c) => (
                  <button
                    key={c.value}
                    className={`dock-dot${ink === c.value && !erasing ? ' active' : ''}`}
                    style={{ background: c.value }}
                    title={c.name}
                    onClick={() => {
                      setInk(c.value)
                      setErasing(false)
                    }}
                  />
                ))}
                <span style={{ width: 1, height: 20, background: 'var(--line)' }} />
                {SIZES.map((s) => (
                  <button
                    key={s.value}
                    className={`dock-btn${size === s.value ? ' active' : ''}`}
                    title={s.name}
                    onClick={() => setSize(s.value)}
                  >
                    <span
                      style={{
                        width: 4 + s.value,
                        height: 4 + s.value,
                        borderRadius: '50%',
                        background: 'currentColor',
                      }}
                    />
                  </button>
                ))}
                <button
                  className={`dock-btn${erasing ? ' active' : ''}`}
                  title="橡皮"
                  onClick={() => setErasing((v) => !v)}
                >
                  <Icon name="eraser" />
                </button>
                <button className="dock-btn" title="撤销一笔" onClick={undo}>
                  <Icon name="undo" />
                </button>
              </>
            ) : mode === 'voice' ? (
              <button
                className={`dock-btn${listening ? ' active' : ''}`}
                title={SR ? (listening ? '停止' : '开始语音') : '此浏览器不支持语音转写'}
                onClick={toggleVoice}
                disabled={!SR}
                style={listening ? { color: 'var(--danger)' } : undefined}
              >
                <Icon name="mic" />
              </button>
            ) : null}
          </div>

          {ripples.map((r) => (
            <span key={r.id} className="ripple" style={{ left: r.x, top: r.y }} />
          ))}
        </div>

        {toast && (
          <div className="echo-toast">
            <Icon name="sparkle" width={15} height={15} style={{ color: 'var(--echo)' }} />
            {toast.echoCount > 0 ? `已保存，触发 ${toast.echoCount} 条回响` : '已保存'}
            <Link to="/">去档案馆看看 →</Link>
          </div>
        )}
      </div>
    </div>
  )
}
