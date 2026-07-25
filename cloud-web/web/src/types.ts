export interface Topic {
  id: string
  name: string
  description: string
}

export type InputType = 'handwriting' | 'text' | 'voice'

export interface Note {
  id: string
  title: string
  createdAt: string
  updatedAt: string
  inputType: InputType
  rawContent: string
  rawText: string
  deleted: boolean
  topics: Topic[]
  echoCount?: number
}

export interface OtherNote {
  id: string
  title: string
  createdAt: string
  inputType: InputType
}

export interface Relation {
  id: string
  fromNoteId: string
  toNoteId: string
  reason: string
  score: number
  createdAt: string
  otherNote: OtherNote
}

export type TaskStatus = 'candidate' | 'confirmed' | 'dismissed' | 'later'

export interface TaskItem {
  id: string
  noteId: string
  title: string
  dueDate: string | null
  status: TaskStatus
  createdAt: string
  noteTitle?: string
}

export interface HomePayload {
  latestNote: Note | null
  echo: { count: number; tickets: Relation[] }
  recentNotes: Note[]
  stats: { notes: number; topics: number; relations: number }
}

export interface Stroke {
  points: [number, number, number][]
  color: string
  size: number
}

export interface StrokeDoc {
  viewBox: [number, number]
  strokes: Stroke[]
}

export interface NoteDetail {
  note: Note
  relations: Relation[]
  tasks: TaskItem[]
}

export interface AskSource {
  noteId: string
  title: string
  createdAt: string
  excerpt: string
  shared: string[]
}

export interface AskResult {
  answer: string
  sources: AskSource[]
  sourceIds: string[]
}

export interface TopicWithNotes extends Topic {
  noteCount: number
  echoCount: number
  notes: { id: string; title: string; createdAt: string; inputType: InputType }[]
}

export interface CalendarNote {
  id: string
  title: string
  createdAt: string
  inputType: InputType
  day: string
  echoCount: number
}

export interface CalendarPayload {
  month: string
  notes: CalendarNote[]
  tasks: TaskItem[]
  candidates: TaskItem[]
}

export interface TimelinePayload {
  center: Note
  nodes: { note: Note; reason: string; score: number }[]
}
