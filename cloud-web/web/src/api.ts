import type {
  AskResult,
  CalendarPayload,
  HomePayload,
  InputType,
  Note,
  NoteDetail,
  Relation,
  TaskItem,
  TaskStatus,
  TimelinePayload,
  Topic,
  TopicWithNotes,
} from './types'

async function req<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(path, {
    headers: { 'Content-Type': 'application/json' },
    ...init,
  })
  if (!res.ok) {
    const body = await res.json().catch(() => ({}))
    throw new Error((body as { error?: string }).error || `请求失败 ${res.status}`)
  }
  return res.json() as Promise<T>
}

const qs = (params: Record<string, string | undefined>) => {
  const p = new URLSearchParams()
  for (const [k, v] of Object.entries(params)) if (v) p.set(k, v)
  const s = p.toString()
  return s ? `?${s}` : ''
}

export const api = {
  home: () => req<HomePayload>('/api/home'),

  notes: (params: { q?: string; type?: string; topic?: string; hasEcho?: string; deleted?: string } = {}) =>
    req<Note[]>(`/api/notes${qs(params)}`),

  createNote: (body: {
    title?: string
    inputType: InputType
    rawContent: string
    rawText?: string
    topicIds?: string[]
  }) => req<{ note: Note; relations: Relation[] }>('/api/notes', { method: 'POST', body: JSON.stringify(body) }),

  note: (id: string) => req<NoteDetail>(`/api/notes/${id}`),

  updateNote: (
    id: string,
    body: { title?: string; rawText?: string; rawContent?: string; topicIds?: string[] }
  ) => req<{ note: Note; relations: Relation[] }>(`/api/notes/${id}`, { method: 'PATCH', body: JSON.stringify(body) }),

  deleteNote: (id: string) => req<{ ok: boolean }>(`/api/notes/${id}`, { method: 'DELETE' }),
  restoreNote: (id: string) => req<{ ok: boolean }>(`/api/notes/${id}/restore`, { method: 'POST' }),
  destroyNote: (id: string) => req<{ ok: boolean }>(`/api/notes/${id}/permanent`, { method: 'DELETE' }),

  topics: () => req<TopicWithNotes[]>('/api/topics'),
  createTopic: (name: string, description = '') =>
    req<Topic>('/api/topics', { method: 'POST', body: JSON.stringify({ name, description }) }),
  updateTopic: (id: string, body: { name?: string; description?: string }) =>
    req<Topic>(`/api/topics/${id}`, { method: 'PATCH', body: JSON.stringify(body) }),
  mergeTopic: (id: string, intoId: string) =>
    req<{ ok: boolean }>(`/api/topics/${id}/merge`, { method: 'POST', body: JSON.stringify({ intoId }) }),

  timeline: (noteId: string) => req<TimelinePayload>(`/api/notes/${noteId}/timeline`),

  tasks: (params: { status?: TaskStatus; month?: string } = {}) =>
    req<TaskItem[]>(`/api/tasks${qs(params as Record<string, string>)}`),
  updateTask: (id: string, body: { status?: TaskStatus; dueDate?: string | null; title?: string }) =>
    req<TaskItem>(`/api/tasks/${id}`, { method: 'PATCH', body: JSON.stringify(body) }),

  calendar: (month: string) => req<CalendarPayload>(`/api/calendar?month=${month}`),

  ask: (question: string, scope?: { type: 'all' | 'topic' | 'note'; id?: string }) =>
    req<AskResult>('/api/ask', { method: 'POST', body: JSON.stringify({ question, scope }) }),
}

export function fmtDate(iso: string) {
  const d = new Date(iso)
  return `${d.getMonth() + 1}月${d.getDate()}日`
}

export function fmtDateTime(iso: string) {
  const d = new Date(iso)
  const hh = String(d.getHours()).padStart(2, '0')
  const mm = String(d.getMinutes()).padStart(2, '0')
  return `${d.getMonth() + 1}月${d.getDate()}日 ${hh}:${mm}`
}

export function todayISO() {
  const d = new Date()
  const m = String(d.getMonth() + 1).padStart(2, '0')
  const day = String(d.getDate()).padStart(2, '0')
  return `${d.getFullYear()}-${m}-${day}`
}

export const INPUT_TYPE_LABEL: Record<InputType, string> = {
  handwriting: '手写',
  text: '文字',
  voice: '语音',
}
