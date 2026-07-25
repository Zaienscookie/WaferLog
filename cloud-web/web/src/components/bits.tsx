import type { ReactNode } from 'react'

export function Loading({ text = '显影中' }: { text?: string }) {
  return (
    <div className="loading-line">
      <span className="loading-dot" />
      {text}…
    </div>
  )
}

export function EmptyHint({ children }: { children: ReactNode }) {
  return <div className="empty-hint">{children}</div>
}

export function Modal({
  title,
  children,
  actions,
  onClose,
}: {
  title: string
  children: ReactNode
  actions: ReactNode
  onClose: () => void
}) {
  return (
    <div className="modal-mask" onClick={onClose} role="dialog" aria-modal>
      <div className="modal-card" onClick={(e) => e.stopPropagation()}>
        <h3>{title}</h3>
        <p>{children}</p>
        <div className="modal-actions">{actions}</div>
      </div>
    </div>
  )
}

export function Confirm({
  title,
  children,
  confirmText = '确认',
  danger = false,
  onConfirm,
  onCancel,
}: {
  title: string
  children: ReactNode
  confirmText?: string
  danger?: boolean
  onConfirm: () => void
  onCancel: () => void
}) {
  return (
    <Modal
      title={title}
      onClose={onCancel}
      actions={
        <>
          <button className="btn" onClick={onCancel}>
            再想想
          </button>
          <button
            className={`btn ${danger ? 'btn-danger' : 'btn-primary'}`}
            onClick={onConfirm}
            autoFocus
          >
            {confirmText}
          </button>
        </>
      }
    >
      {children}
    </Modal>
  )
}
