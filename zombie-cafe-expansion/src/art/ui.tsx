import type { ReactElement } from 'react';

const w = (el: ReactElement) => el;

export function PumpkinIcon({ size = 28 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 32 32" xmlns="http://www.w3.org/2000/svg">
      <path d="M16 8 q3 -5 7 -3 q-1 3 1 4" stroke="#5E7A2E" strokeWidth="2.5" fill="none" strokeLinecap="round" />
      <ellipse cx={16} cy={19} rx={11} ry={9.5} fill="#E87828" stroke="#2A1C10" strokeWidth={2} />
      <path d="M10 13 q-2 6 0 12 M16 10 q-2 8 0 18 M22 13 q2 6 0 12" stroke="#C45E1C" strokeWidth="1.8" fill="none" />
      <path d="M10 17 l4 -2 0 4 z M22 17 l-4 -2 0 4 z" fill="#2A1C10" />
      <path d="M11 22 q5 5 10 0 q-3 4 -5 3 q-2 -1 -3 1 q-1 1 -2 -4 z" fill="#2A1C10" />
    </svg>,
  );
}

export function SnowflakeIcon({ size = 28 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 32 32" xmlns="http://www.w3.org/2000/svg">
      <g stroke="#EAF6FC" strokeWidth="2.6" strokeLinecap="round">
        <path d="M16 3 v26 M5 9 l22 14 M27 9 l-22 14" />
        <path d="M16 8 l-4 -3 M16 8 l4 -3 M16 24 l-4 3 M16 24 l4 3" />
        <path d="M9 12 l-4 1 M9 12 l-1 -4 M23 20 l4 -1 M23 20 l1 4 M9 20 l-4 -1 M9 20 l-1 4 M23 12 l4 1 M23 12 l1 -4" />
      </g>
    </svg>,
  );
}

export function EggIcon({ size = 28 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 32 32" xmlns="http://www.w3.org/2000/svg">
      <path d="M16 3 q9 4 9 16 q0 8 -9 10 q-9 -2 -9 -10 q0 -12 9 -16 z" fill="#F6E58B" stroke="#2A1C10" strokeWidth={2} />
      <path d="M8 15 q8 -3 16 1" stroke="#F5B7C6" strokeWidth="2.5" fill="none" />
      <path d="M8 21 q8 -3 16 1" stroke="#A9D6F0" strokeWidth="2.5" fill="none" />
      <path d="M9 11 l3 -2 3 2 3 -2 3 2" stroke="#C9B6E4" strokeWidth="1.8" fill="none" />
    </svg>,
  );
}

export function GhostIcon({ size = 28 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 32 32" xmlns="http://www.w3.org/2000/svg">
      <path d="M16 3 q10 0 10 12 l0 12 q-3 3 -5 0 q-2 3 -5 0 q-2 3 -5 0 q-2 3 -5 0 l0 -12 q0 -12 10 -12 z" fill="#F4F1E6" stroke="#2A1C10" strokeWidth={2} />
      <circle cx={12} cy={13} r={2} fill="#2A1C10" />
      <circle cx={20} cy={13} r={2} fill="#2A1C10" />
      <path d="M13 19 q3 3 6 0" stroke="#2A1C10" strokeWidth={2} fill="none" strokeLinecap="round" />
    </svg>,
  );
}

export function StarIcon({ size = 22 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
      <path d="M12 2 l3 6 7 1 -5 5 1 7 -6 -3 -6 3 1 -7 -5 -5 7 -1 z" fill="#F2B632" stroke="#2A1C10" strokeWidth={1.8} strokeLinejoin="round" />
    </svg>,
  );
}

export function BrainIcon({ size = 20 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
      <path d="M12 4 q-7 -1 -8 6 q-3 4 1 7 q-1 6 7 5 q8 1 7 -5 q4 -3 1 -7 q-1 -7 -8 -6 z" fill="#E8A0B4" stroke="#2A1C10" strokeWidth={1.8} />
      <path d="M12 5 v16 M8 8 q2 2 0 4 q-2 2 0 4 M16 8 q-2 2 0 4 q2 2 0 4" stroke="#2A1C10" strokeWidth={1.5} fill="none" />
    </svg>,
  );
}

export function CoinIcon({ size = 18 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 20 20" xmlns="http://www.w3.org/2000/svg">
      <circle cx={10} cy={10} r={8} fill="#F2C14E" stroke="#2A1C10" strokeWidth={1.8} />
      <circle cx={10} cy={10} r={4.6} fill="none" stroke="#A8720E" strokeWidth={1.5} />
    </svg>,
  );
}

export function ChefHatIcon({ size = 30 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 32 32" xmlns="http://www.w3.org/2000/svg">
      <path d="M10 20 q-6 0 -6 -6 q0 -6 6 -6 q1 -6 7 -6 q6 0 7 6 q6 0 6 6 q0 6 -6 6 z" fill="#FDF8F2" stroke="#2A1C10" strokeWidth={2} strokeLinejoin="round" />
      <path d="M10 20 v5 q6 3 12 0 v-5" fill="#EDE4D2" stroke="#2A1C10" strokeWidth={2} />
      <path d="M11 20 v5 M16 21 v5 M21 20 v5" stroke="#2A1C10" strokeWidth={1.5} />
    </svg>,
  );
}

export function LockIcon({ size = 18 }: { size?: number }) {
  return w(
    <svg width={size} height={size} viewBox="0 0 20 20" xmlns="http://www.w3.org/2000/svg">
      <rect x={4} y={9} width={12} height={9} rx={2} fill="#B9A88F" stroke="#2A1C10" strokeWidth={1.8} />
      <path d="M7 9 v-3 q0 -4 3 -4 q3 0 3 4 v3" stroke="#2A1C10" strokeWidth={1.8} fill="none" />
    </svg>,
  );
}
