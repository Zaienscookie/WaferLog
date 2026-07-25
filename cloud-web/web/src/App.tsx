import { useEffect } from 'react'
import { Routes, Route, useLocation } from 'react-router-dom'
import { Rail, TopBar, BottomNav } from './components/Nav'
import { ArchivePage } from './pages/ArchivePage'
import { NotesPage } from './pages/NotesPage'
import { NoteDetailPage } from './pages/NoteDetailPage'
import { EditorPage } from './pages/EditorPage'
import { TopicsPage } from './pages/TopicsPage'
import { TimelinePage } from './pages/TimelinePage'
import { CalendarPage } from './pages/CalendarPage'
import { AskPage } from './pages/AskPage'
import { SettingsPage } from './pages/SettingsPage'

export default function App() {
  const location = useLocation()
  const isEditor = location.pathname === '/new'

  useEffect(() => {
    const theme = localStorage.getItem('waferlog.theme') || 'light'
    document.documentElement.dataset.theme = theme
  }, [])

  useEffect(() => {
    window.scrollTo(0, 0)
  }, [location.pathname])

  if (isEditor) {
    return (
      <>
        <div className="app-bg" />
        <EditorPage />
      </>
    )
  }

  return (
    <>
      <div className="app-bg" />
      <div className="shell">
        <Rail />
        <main className="main">
          <TopBar />
          <Routes>
            <Route path="/" element={<ArchivePage />} />
            <Route path="/notes" element={<NotesPage />} />
            <Route path="/notes/:id" element={<NoteDetailPage />} />
            <Route path="/topics" element={<TopicsPage />} />
            <Route path="/timeline" element={<TimelinePage />} />
            <Route path="/calendar" element={<CalendarPage />} />
            <Route path="/ask" element={<AskPage />} />
            <Route path="/settings" element={<SettingsPage />} />
          </Routes>
        </main>
      </div>
      <BottomNav />
    </>
  )
}
