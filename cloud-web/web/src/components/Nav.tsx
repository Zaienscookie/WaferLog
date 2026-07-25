import { NavLink, Link } from 'react-router-dom'
import { Icon } from './Icon'
import type { IconName } from './Icon'

interface NavItem {
  to: string
  icon: IconName
  label: string
  end?: boolean
}

const ITEMS: NavItem[] = [
  { to: '/', icon: 'archive', label: '档案馆', end: true },
  { to: '/notes', icon: 'notes', label: '所有记录' },
  { to: '/topics', icon: 'topics', label: '主题盒' },
  { to: '/timeline', icon: 'timeline', label: '记忆线' },
  { to: '/calendar', icon: 'calendar', label: '时间册' },
  { to: '/ask', icon: 'ask', label: '知识问答' },
]

const MOBILE_ITEMS: (NavItem | null)[] = [
  { to: '/', icon: 'archive', label: '档案馆', end: true },
  { to: '/notes', icon: 'notes', label: '记录' },
  null, // 中间是落笔按钮
  { to: '/calendar', icon: 'calendar', label: '时间册' },
  { to: '/ask', icon: 'ask', label: '问答' },
]

export function Rail() {
  return (
    <nav className="rail" aria-label="主导航">
      <Link to="/" className="rail-logo" title="WaferLog 硅笺">
        笺
      </Link>
      <Link to="/new" className="rail-new" title="落笔（新建记录）">
        <Icon name="pen" />
      </Link>
      {ITEMS.map((it) => (
        <NavLink
          key={it.to}
          to={it.to}
          end={it.end}
          className={({ isActive }) => `rail-item${isActive ? ' active' : ''}`}
        >
          <Icon name={it.icon} />
          {it.label}
        </NavLink>
      ))}
      <div className="rail-spacer" />
      <NavLink to="/settings" className={({ isActive }) => `rail-item${isActive ? ' active' : ''}`}>
        <Icon name="settings" />
        设置
      </NavLink>
    </nav>
  )
}

export function TopBar() {
  return (
    <div className="topbar">
      <Link to="/" className="topbar-logo">
        <span className="seal">笺</span>
        硅笺
      </Link>
      <Link to="/settings" className="btn btn-ghost" aria-label="设置" style={{ minHeight: 36, padding: '4px 10px' }}>
        <Icon name="settings" width={20} height={20} />
      </Link>
    </div>
  )
}

export function BottomNav() {
  return (
    <nav className="bottomnav" aria-label="主导航">
      {MOBILE_ITEMS.map((it) =>
        it === null ? (
          <Link key="new" to="/new" className="bottomnav-new" aria-label="落笔（新建记录）">
            <Icon name="pen" />
          </Link>
        ) : (
          <NavLink
            key={it.to}
            to={it.to}
            end={it.end}
            className={({ isActive }) => `bottomnav-item${isActive ? ' active' : ''}`}
          >
            <Icon name={it.icon} />
            {it.label}
          </NavLink>
        )
      )}
    </nav>
  )
}
