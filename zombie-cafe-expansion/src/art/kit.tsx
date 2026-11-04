import type { CSSProperties, ReactNode } from 'react';

export const INK = '#2A1C10';

export const SKIN = {
  green: '#A8C970',
  greenDark: '#7FA653',
  gray: '#B9C2B0',
  pale: '#D9E4D4',
  blue: '#A9C8D8',
  choc: '#6B3F1E',
};

export function Art({
  children,
  vb = '0 0 200 260',
  style,
  className,
}: {
  children: ReactNode;
  vb?: string;
  style?: CSSProperties;
  className?: string;
}) {
  const [, , w, h] = vb.split(' ').map(Number);
  return (
    <svg
      viewBox={vb}
      width={w}
      height={h}
      xmlns="http://www.w3.org/2000/svg"
      className={className}
      style={style}
    >
      {children}
    </svg>
  );
}

export function Shadow({ cx = 100, cy = 248, rx = 56 }: { cx?: number; cy?: number; rx?: number }) {
  return <ellipse cx={cx} cy={cy} rx={rx} ry={9} fill="rgba(20,10,4,0.28)" />;
}
