import type { SVGProps } from 'react'

const PATHS: Record<string, string> = {
  pen: 'M12 20h9 M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4Z',
  archive: 'M4 8V6a1 1 0 0 1 1-1h14a1 1 0 0 1 1 1v2 M4 8h16v11a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1Z M10 12h4',
  notes: 'M7 3h8l4 4v13a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1Z M14 3v5h5 M9 13h6 M9 17h4',
  topics: 'M3 9l9-5 9 5-9 5Z M3 9v6l9 5 9-5V9 M12 14v6',
  timeline: 'M6 3v18 M6 6h9 M6 12h12 M6 18h7 M17 4.5a1.5 1.5 0 1 0 0 .01 M20 10.5a1.5 1.5 0 1 0 0 .01 M15 16.5a1.5 1.5 0 1 0 0 .01',
  calendar: 'M5 5h14a1 1 0 0 1 1 1v13a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1Z M8 3v4 M16 3v4 M4 10h16 M8 15h2 M14 15h2',
  ask: 'M12 4a8 8 0 0 1 8 8 8 8 0 0 1-8 8c-1.4 0-2.8-.35-4-1l-4 1 1.2-3.4A8 8 0 0 1 12 4Z M9.5 10a2.5 2.5 0 0 1 4.9.7c0 1.6-2.1 2-2.1 3.3 M12.2 17h.01',
  settings: 'M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z M19 12a7 7 0 0 0-.14-1.4l2-1.55-2-3.46-2.36.95a7 7 0 0 0-2.42-1.4L13.7 2.6h-3.4l-.38 2.54a7 7 0 0 0-2.42 1.4l-2.36-.95-2 3.46 2 1.55A7 7 0 0 0 5 12c0 .48.05.94.14 1.4l-2 1.55 2 3.46 2.36-.95c.72.6 1.53 1.07 2.42 1.4l.38 2.54h3.4l.38-2.54a7 7 0 0 0 2.42-1.4l2.36.95 2-3.46-2-1.55c.09-.46.14-.92.14-1.4Z',
  search: 'M11 5a6 6 0 1 0 0 12 6 6 0 0 0 0-12Z M20 20l-4.2-4.2',
  trash: 'M4 7h16 M9 7V5a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2 M6.5 7l.8 12a1 1 0 0 0 1 .9h7.4a1 1 0 0 0 1-.9l.8-12 M10 11v6 M14 11v6',
  undo: 'M8 5 4 9l4 4 M4 9h10a6 6 0 0 1 0 12h-3',
  eraser: 'M7 21h10 M5.5 12.5 13 5a2 2 0 0 1 2.8 0l3.2 3.2a2 2 0 0 1 0 2.8l-6.5 6.5H8Z M9 8.5l6.5 6.5',
  check: 'M4.5 12.5 10 18 19.5 6.5',
  x: 'M6 6l12 12 M18 6 6 18',
  left: 'M14.5 5.5 8 12l6.5 6.5',
  right: 'M9.5 5.5 16 12l-6.5 6.5',
  down: 'M5.5 9.5 12 16l6.5-6.5',
  sparkle: 'M12 3l1.9 5.6L19.5 10l-5.6 1.9L12 17.5l-1.9-5.6L4.5 10l5.6-1.4Z M19 16l.8 2.2L22 19l-2.2.8L19 22l-.8-2.2L16 19l2.2-.8Z',
  mic: 'M12 3a3 3 0 0 1 3 3v6a3 3 0 0 1-6 0V6a3 3 0 0 1 3-3Z M6 11a6 6 0 0 0 12 0 M12 17v4',
  type: 'M5 6V4h14v2 M12 4v16 M9 20h6',
  plus: 'M12 5v14 M5 12h14',
  sun: 'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8Z M12 2.5V5 M12 19v2.5 M2.5 12H5 M19 12h2.5 M5 5l1.8 1.8 M17.2 17.2 19 19 M19 5l-1.8 1.8 M6.8 17.2 5 19',
  sync: 'M20 12a8 8 0 0 1-14.5 4.5 M4 12a8 8 0 0 1 14.5-4.5 M18.5 3v4.5H14 M5.5 21v-4.5H10',
  download: 'M12 4v11 M7 11l5 5 5-5 M5 20h14',
  restore: 'M4 10a8 8 0 1 1 2 6 M4 10V5 M4 10h5',
  hand: 'M9 11V5.5a1.5 1.5 0 0 1 3 0V11m0-5.5v-1a1.5 1.5 0 0 1 3 0V11m0-4.5a1.5 1.5 0 0 1 3 0V12m0-3a1.5 1.5 0 0 1 3 0v6a7 7 0 0 1-7 7h-1.6a6 6 0 0 1-4.7-2.3L4.4 15a1.6 1.6 0 0 1 2.4-2.1L9 15.5Z',
}

export type IconName = keyof typeof PATHS

export function Icon({ name, ...rest }: { name: IconName } & SVGProps<SVGSVGElement>) {
  return (
    <svg
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.7}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
      {...rest}
    >
      <path d={PATHS[name]} />
    </svg>
  )
}
