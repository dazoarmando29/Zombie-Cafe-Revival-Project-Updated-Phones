import type { ReactElement } from 'react';
import { INK } from './kit';

const S = { stroke: INK, strokeLinejoin: 'round' as const, strokeLinecap: 'round' as const };

function Item({ children, w = 120, h = 100 }: { children: ReactElement | ReactElement[]; w?: number; h?: number }) {
  return (
    <svg viewBox={`0 0 ${w} ${h}`} width={w} height={h} xmlns="http://www.w3.org/2000/svg">
      {children}
    </svg>
  );
}

const coinShadow = (cx = 60, cy = 92) => <ellipse cx={cx} cy={cy} rx={38} ry={6} fill="rgba(20,10,4,0.25)" />;

/* ================= HALLOWEEN FURNITURE ================= */
const HauntedTable = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M22 44 q38 -10 76 0 l-4 8 q-34 -8 -68 0 z" fill="#6C3A96" {...S} strokeWidth={3} />
      <path d="M30 52 l-4 36 M90 52 l4 36" stroke={INK} strokeWidth={5} fill="none" />
      <path d="M24 88 h12 M84 88 h12" stroke={INK} strokeWidth={4} fill="none" />
      <path d="M28 52 q32 -8 64 0" stroke="#9B6BC4" strokeWidth={3} fill="none" />
      <circle cx={60} cy={38} r={7} fill="#9BE23B" {...S} strokeWidth={2.5} />
      <path d="M56 38 q4 -6 8 0 q-4 5 -8 0 z" fill="#2A1C10" />
      <path d="M40 42 q6 -6 12 -2 M68 40 q6 -5 12 0" stroke="#4A2470" strokeWidth={2.5} fill="none" />
    </g>
  </Item>
);

const PumpkinChair = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M38 46 q22 -30 44 0 q4 30 -22 36 q-26 -6 -22 -36 z" fill="#E87828" {...S} strokeWidth={3} />
      <path d="M52 30 q8 -10 16 0" stroke="#5E7A2E" strokeWidth={5} fill="none" />
      <path d="M50 52 l8 -5 1 9 z M70 52 l-8 -5 -1 9 z" fill="#2A1C10" stroke={INK} strokeWidth={2} />
      <path d="M52 66 q8 8 16 0 q-8 6 -16 0 z" fill="#2A1C10" stroke={INK} strokeWidth={2} />
      <path d="M44 80 l-4 12 M76 80 l4 12" stroke={INK} strokeWidth={5} fill="none" />
    </g>
  </Item>
);

const CoffinCouch = () => (
  <Item>
    <g>
      {coinShadow(60, 92)}
      <path d="M60 12 l32 18 4 46 -36 16 -36 -16 4 -46 z" fill="#4A2470" {...S} strokeWidth={3} />
      <path d="M60 22 l22 12 3 34 -25 12 -25 -12 3 -34 z" fill="#6C3A96" stroke={INK} strokeWidth={2.5} />
      <path d="M60 34 l0 34 M48 44 l24 8" stroke="#2A1C10" strokeWidth={2.5} fill="none" />
      <path d="M30 62 l-8 12 M90 62 l8 12" stroke={INK} strokeWidth={5} fill="none" />
      <circle cx={60} cy={20} r={4} fill="#E8B84B" stroke={INK} strokeWidth={2} />
    </g>
  </Item>
);

const SpookyStove = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M28 30 h64 v52 q0 6 -6 6 h-52 q-6 0 -6 -6 z" fill="#3A3448" {...S} strokeWidth={3} />
      <rect x={36} y={40} width={20} height={16} rx={3} fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <rect x={64} y={40} width={20} height={16} rx={3} fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <path d="M40 52 q6 -8 12 0 q-6 6 -12 0 z" fill="#9BE23B" />
      <path d="M68 52 q6 -8 12 0 q-6 6 -12 0 z" fill="#9BE23B" />
      <rect x={36} y={64} width={48} height={8} rx={3} fill="#4A4458" stroke={INK} strokeWidth={2.5} />
      <path d="M44 30 v-8 M76 30 v-8" stroke={INK} strokeWidth={4} fill="none" />
      <path d="M40 22 h8 M72 22 h8" stroke={INK} strokeWidth={3} fill="none" />
    </g>
  </Item>
);

const GhostChandelier = () => (
  <Item>
    <g>
      <path d="M60 8 v14" stroke={INK} strokeWidth={3} />
      <path d="M30 22 q30 -12 60 0 q4 4 -2 6 l-56 0 q-6 -2 -2 -6 z" fill="#8A7454" {...S} strokeWidth={3} />
      <path d="M44 28 q-4 22 2 30 q6 6 10 -2 l2 -28 z" fill="#F4F1E6" fillOpacity={0.92} {...S} strokeWidth={2.5} />
      <path d="M60 28 q-2 26 4 32 q6 5 9 -3 l1 -29 z" fill="#F4F1E6" fillOpacity={0.92} {...S} strokeWidth={2.5} />
      <path d="M76 28 q4 22 -2 30 q-6 6 -10 -2 l-2 -28 z" fill="#F4F1E6" fillOpacity={0.92} {...S} strokeWidth={2.5} />
      <circle cx={46} cy={40} r={2} fill={INK} />
      <circle cx={62} cy={42} r={2} fill={INK} />
      <circle cx={74} cy={40} r={2} fill={INK} />
      <path d="M34 26 l-4 -12 M86 26 l4 -12" stroke={INK} strokeWidth={3} fill="none" />
      <path d="M28 14 h10 M82 14 h10" stroke="#F2B632" strokeWidth={6} fill="none" strokeLinecap="round" />
      <path d="M28 14 h10 M82 14 h10" stroke={INK} strokeWidth={2} fill="none" />
      <circle cx={33} cy={10} r={3} fill="#FFEFB2" stroke={INK} strokeWidth={2} />
      <circle cx={87} cy={10} r={3} fill="#FFEFB2" stroke={INK} strokeWidth={2} />
    </g>
  </Item>
);

const HauntedWallpaper = () => (
  <Item>
    <g>
      <rect x={14} y={10} width={92} height={76} rx={4} fill="#2B1B4A" {...S} strokeWidth={3} />
      <path d="M28 22 q8 -8 14 0 q-7 8 -14 0 z M78 22 q8 -8 14 0 q-7 8 -14 0 z" fill="#F4801E" stroke={INK} strokeWidth={2} />
      <circle cx={35} cy={21} r={2} fill="#2A1C10" />
      <circle cx={85} cy={21} r={2} fill="#2A1C10" />
      <path d="M28 42 q8 -8 14 0 q-7 8 -14 0 z M78 42 q8 -8 14 0 q-7 8 -14 0 z" fill="#F4801E" stroke={INK} strokeWidth={2} />
      <path d="M20 10 q12 14 4 30 M100 10 q-12 14 -4 30 M20 86 q12 -14 4 -30 M100 86 q-12 -14 -4 -30" stroke="#7C3AA6" strokeWidth={2.5} fill="none" />
      <path d="M46 58 q8 8 14 0 q-7 8 -14 0 z M64 58 q8 8 14 0 q-7 8 -14 0 z" fill="#9BE23B" stroke={INK} strokeWidth={2} />
      <path d="M14 64 q22 -8 46 0 q24 -8 46 0" stroke="#4A2470" strokeWidth={2.5} fill="none" />
    </g>
  </Item>
);

/* ================= CHRISTMAS FURNITURE ================= */
const HolidayTable = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M20 46 q40 -12 80 0 l-4 10 q-36 -10 -72 0 z" fill="#B32330" {...S} strokeWidth={3} />
      <path d="M20 52 q40 -10 80 0" stroke="#E8B84B" strokeWidth={3} fill="none" />
      <path d="M32 56 l-4 34 M88 56 l4 34" stroke={INK} strokeWidth={5} fill="none" />
      <path d="M26 90 h12 M82 90 h12" stroke={INK} strokeWidth={4} fill="none" />
      <rect x={50} y={28} width={20} height={16} rx={2} fill="#2E7D4F" {...S} strokeWidth={2.5} />
      <path d="M50 34 h20 M60 28 v16" stroke="#E8B84B" strokeWidth={2.5} fill="none" />
    </g>
  </Item>
);

const SnowChair = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M36 38 q24 -16 48 0 l0 44 q-24 10 -48 0 z" fill="#F0F6FA" {...S} strokeWidth={3} />
      <path d="M36 38 q24 -16 48 0" stroke="#C9E8F2" strokeWidth={4} fill="none" />
      <path d="M42 50 q18 -8 36 0" stroke="#C9E8F2" strokeWidth={3} fill="none" />
      <path d="M44 82 l-4 10 M76 82 l4 10" stroke={INK} strokeWidth={5} fill="none" />
      <path d="M84 44 l8 -6 M86 54 l10 -2" stroke="#BFE6F2" strokeWidth={3} fill="none" />
      <circle cx={60} cy={62} r={5} fill="#C9E8F2" stroke={INK} strokeWidth={2} />
    </g>
  </Item>
);

const ChristmasTree = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M52 82 h16 l2 10 h-20 z" fill="#6E4620" {...S} strokeWidth={3} />
      <path d="M60 8 l24 32 -12 0 16 26 -20 0 16 24 -48 0 16 -24 -20 0 16 -26 -12 0 z" fill="#1E6B3A" {...S} strokeWidth={3} />
      <circle cx={52} cy={54} r={4} fill="#C0392B" stroke={INK} strokeWidth={2} />
      <circle cx={70} cy={62} r={4} fill="#E8B84B" stroke={INK} strokeWidth={2} />
      <circle cx={58} cy={74} r={4} fill="#A8D8E8" stroke={INK} strokeWidth={2} />
      <circle cx={72} cy={40} r={3.5} fill="#C0392B" stroke={INK} strokeWidth={2} />
      <path d="M60 2 l2 6 6 1 -5 4 2 6 -5 -3 -5 3 2 -6 -5 -4 6 -1 z" fill="#E8B84B" {...S} strokeWidth={2} />
    </g>
  </Item>
);

const PresentStack = () => (
  <Item>
    <g>
      {coinShadow()}
      <rect x={22} y={56} width={40} height={32} rx={3} fill="#B32330" {...S} strokeWidth={3} />
      <rect x={40} y={56} width={6} height={32} fill="#E8B84B" stroke={INK} strokeWidth={2} />
      <rect x={22} y={68} width={40} height={6} fill="#E8B84B" stroke={INK} strokeWidth={2} />
      <rect x={56} y={40} width={34} height={26} rx={3} fill="#2E7D4F" {...S} strokeWidth={3} />
      <rect x={70} y={40} width={6} height={26} fill="#F0EAD8" stroke={INK} strokeWidth={2} />
      <rect x={56} y={48} width={34} height={6} fill="#F0EAD8" stroke={INK} strokeWidth={2} />
      <rect x={40} y={20} width={28} height={22} rx={3} fill="#2A1C10" {...S} strokeWidth={3} />
      <path d="M54 20 v22 M40 30 h28" stroke="#9BE23B" strokeWidth={3} fill="none" />
      <circle cx={54} cy={28} r={3} fill="#9BE23B" stroke={INK} strokeWidth={1.5} />
    </g>
  </Item>
);

const FestiveStove = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M28 30 h64 v52 q0 6 -6 6 h-52 q-6 0 -6 -6 z" fill="#B32330" {...S} strokeWidth={3} />
      <path d="M28 30 h64" stroke="#F0EAD8" strokeWidth={5} fill="none" />
      <rect x={36} y={42} width={20} height={16} rx={3} fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <rect x={64} y={42} width={20} height={16} rx={3} fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <path d="M40 54 q6 -7 12 0 q-6 5 -12 0 z" fill="#FF9D4A" />
      <path d="M68 54 q6 -7 12 0 q-6 5 -12 0 z" fill="#FF9D4A" />
      <rect x={36} y={66} width={48} height={8} rx={3} fill="#8C1620" stroke={INK} strokeWidth={2.5} />
      <path d="M46 30 q0 -10 8 -12 q10 2 8 12" stroke="#F0EAD8" strokeWidth={4} fill="none" />
      <path d="M50 18 q2 -8 10 -8 q10 2 10 10 q-10 -4 -20 -2 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
    </g>
  </Item>
);

const SnowyWallpaper = () => (
  <Item>
    <g>
      <rect x={14} y={10} width={92} height={76} rx={4} fill="#A8D8E8" {...S} strokeWidth={3} />
      <path d="M14 58 q23 -14 46 0 q23 -14 46 0 v28 h-92 z" fill="#F0F6FA" stroke={INK} strokeWidth={2.5} />
      <path d="M30 30 v22 M30 36 l-6 -6 M30 36 l6 -6 M30 44 l-6 -6 M30 44 l6 -6" stroke="#F0F6FA" strokeWidth={2.5} fill="none" />
      <path d="M84 24 v22 M84 30 l-6 -6 M84 30 l6 -6 M84 38 l-6 -6 M84 38 l6 -6" stroke="#F0F6FA" strokeWidth={2.5} fill="none" />
      <circle cx={58} cy={28} r={5} fill="#F0F6FA" stroke={INK} strokeWidth={2} />
      <circle cx={58} cy={22} r={3} fill="#F0F6FA" stroke={INK} strokeWidth={2} />
    </g>
  </Item>
);

/* ================= EASTER FURNITURE ================= */
const EggTable = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M60 18 q26 6 28 34 q2 26 -28 32 q-30 -6 -28 -32 q2 -28 28 -34 z" fill="#F6E58B" {...S} strokeWidth={3} />
      <path d="M36 52 q24 -8 48 2" stroke="#F5B7C6" strokeWidth={5} fill="none" />
      <path d="M34 64 q26 -8 52 2" stroke="#A9D6F0" strokeWidth={5} fill="none" />
      <path d="M40 40 l6 -5 6 5 6 -5 6 5 6 -5 6 5" stroke="#C9B6E4" strokeWidth={2.5} fill="none" />
      <path d="M60 84 v10" stroke={INK} strokeWidth={5} />
      <path d="M46 94 h28" stroke={INK} strokeWidth={5} fill="none" />
    </g>
  </Item>
);

const BunnyChair = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M40 42 q20 -12 40 0 l2 40 q-22 10 -44 0 z" fill="#E8C8D4" {...S} strokeWidth={3} />
      <path d="M52 40 q-6 -28 2 -30 q8 -1 8 26 z" fill="#E8C8D4" {...S} strokeWidth={3} />
      <path d="M68 40 q6 -28 -2 -30 q-8 -1 -8 26 z" fill="#E8C8D4" {...S} strokeWidth={3} />
      <path d="M55 24 q-2 -12 2 -13 M65 24 q2 -12 -2 -13" stroke="#F5B7C6" strokeWidth={3} fill="none" />
      <circle cx={60} cy={78} r={7} fill="#FDF8F2" {...S} strokeWidth={2.5} />
      <path d="M48 82 l-3 10 M72 82 l3 10" stroke={INK} strokeWidth={4} fill="none" />
    </g>
  </Item>
);

const EasterBasket = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M30 48 q30 -10 60 0 l-6 34 q-24 8 -48 0 z" fill="#C9954A" {...S} strokeWidth={3} />
      <path d="M32 58 q28 -8 56 0 M34 68 q26 -8 52 0" stroke="#8A5A2E" strokeWidth={2.5} fill="none" />
      <path d="M36 48 q24 -30 48 0" stroke="#8A5A2E" strokeWidth={4} fill="none" />
      <ellipse cx={48} cy={44} rx={9} ry={11} fill="#A9D6F0" {...S} strokeWidth={2.5} transform="rotate(-14 48 44)" />
      <ellipse cx={72} cy={44} rx={9} ry={11} fill="#F5B7C6" {...S} strokeWidth={2.5} transform="rotate(12 72 44)" />
      <ellipse cx={60} cy={38} rx={9} ry={11} fill="#A8E6C9" {...S} strokeWidth={2.5} />
      <path d="M52 36 q8 -4 16 2" stroke="#F6E58B" strokeWidth={3} fill="none" />
      <path d="M40 42 q8 -4 14 1" stroke="#F6E58B" strokeWidth={3} fill="none" />
    </g>
  </Item>
);

const SpringFountain = () => (
  <Item>
    <g>
      {coinShadow()}
      <ellipse cx={60} cy={82} rx={38} ry={8} fill="#C9B6E4" {...S} strokeWidth={3} />
      <path d="M34 76 q26 -8 52 0 l-4 8 q-22 -6 -44 0 z" fill="#A9D6F0" {...S} strokeWidth={3} />
      <path d="M52 74 l2 -30 M68 74 l-2 -30" stroke="#C9B6E4" strokeWidth={5} fill="none" />
      <ellipse cx={60} cy={42} rx={16} ry={6} fill="#A9D6F0" {...S} strokeWidth={3} />
      <path d="M60 36 q-8 -8 -14 -12 M60 36 q0 -10 0 -14 M60 36 q8 -8 14 -12" stroke="#7FB8D8" strokeWidth={3} fill="none" />
      <ellipse cx={60} cy={22} rx={7} ry={9} fill="#F5B7C6" {...S} strokeWidth={2.5} />
      <ellipse cx={44} cy={70} rx={7} ry={5} fill="#6B3F1E" stroke={INK} strokeWidth={2} />
      <ellipse cx={76} cy={70} rx={7} ry={5} fill="#6B3F1E" stroke={INK} strokeWidth={2} />
    </g>
  </Item>
);

const EggStove = () => (
  <Item>
    <g>
      {coinShadow()}
      <path d="M60 14 q26 8 28 38 q2 28 -28 36 q-30 -8 -28 -36 q2 -30 28 -38 z" fill="#F0EAD8" {...S} strokeWidth={3} />
      <rect x={40} y={44} width={18} height={14} rx={3} fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <rect x={64} y={44} width={18} height={14} rx={3} fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <path d="M44 56 q5 -6 10 0 q-5 5 -10 0 z" fill="#FF9D4A" />
      <path d="M68 56 q5 -6 10 0 q-5 5 -10 0 z" fill="#FF9D4A" />
      <path d="M44 68 q16 6 32 0" stroke="#C9B6E4" strokeWidth={3} fill="none" />
      <path d="M48 28 q6 -6 12 -2 M64 22 q6 -4 10 0" stroke="#F5B7C6" strokeWidth={3} fill="none" />
    </g>
  </Item>
);

const SpringWallpaper = () => (
  <Item>
    <g>
      <rect x={14} y={10} width={92} height={76} rx={4} fill="#A9D6F0" {...S} strokeWidth={3} />
      <path d="M14 52 q22 -16 44 -2 q22 -12 48 2 v34 h-92 z" fill="#A8E6C9" stroke={INK} strokeWidth={2.5} />
      <path d="M14 68 q24 -12 48 0 q22 -10 44 0 v18 h-92 z" fill="#B8E8A8" stroke={INK} strokeWidth={2} />
      <circle cx={36} cy={40} r={4} fill="#F5B7C6" stroke={INK} strokeWidth={2} />
      <circle cx={80} cy={34} r={4} fill="#F6E58B" stroke={INK} strokeWidth={2} />
      <ellipse cx={58} cy={60} rx={5} ry={7} fill="#C9B6E4" stroke={INK} strokeWidth={2} />
      <ellipse cx={88} cy={58} rx={5} ry={7} fill="#F5B7C6" stroke={INK} strokeWidth={2} />
      <path d="M28 56 q0 -8 4 -10 q4 4 2 10" fill="#7FB878" stroke={INK} strokeWidth={2} />
    </g>
  </Item>
);

/* ================= RECIPES ================= */
const WitchBrewStew = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={36} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M30 52 q30 -8 60 0 l-4 34 q-26 8 -52 0 z" fill="#3A3448" {...S} strokeWidth={3} />
      <path d="M28 50 q32 -10 64 0 q3 6 -4 7 l-56 0 q-7 -1 -4 -7 z" fill="#4A4458" {...S} strokeWidth={3} />
      <ellipse cx={60} cy={52} rx={30} ry={7} fill="#9BE23B" {...S} strokeWidth={2.5} />
      <circle cx={48} cy={50} r={4} fill="#7FBF2E" />
      <circle cx={70} cy={53} r={5} fill="#7FBF2E" />
      <circle cx={58} cy={49} r={3.5} fill="#B8F26A" />
      <path d="M44 38 q2 -8 -2 -12 M60 34 q0 -10 2 -14 M76 38 q-2 -8 2 -12" stroke="#C9E8B0" strokeWidth={3} fill="none" />
      <circle cx={68} cy={51} r={5} fill="#FDF8F2" {...S} strokeWidth={2} />
      <circle cx={68} cy={51} r={2} fill="#2A1C10" />
      <path d="M26 62 l-8 4 M28 72 l-8 6" stroke={INK} strokeWidth={3} fill="none" />
    </g>
  </Item>
);

const FingerFries = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={34} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M34 62 q26 -8 52 0 l-6 28 q-20 6 -40 0 z" fill="#C0392B" {...S} strokeWidth={3} />
      <path d="M34 62 q26 -8 52 0" stroke="#F0EAD8" strokeWidth={3} fill="none" />
      <path d="M46 60 l-2 -30 q0 -6 5 -6 q5 0 5 6 l0 30 z" fill="#E8C876" {...S} strokeWidth={2.5} />
      <path d="M60 58 l0 -34 q0 -6 5 -6 q5 0 5 6 l0 34 z" fill="#E8C876" {...S} strokeWidth={2.5} />
      <path d="M74 60 l2 -28 q1 -6 6 -6 q5 0 5 6 l-2 28 z" fill="#E8C876" {...S} strokeWidth={2.5} />
      <path d="M44 24 h6 v8 h-6 z" fill="#C0392B" stroke={INK} strokeWidth={1.8} />
      <path d="M60 18 h6 v8 h-6 z" fill="#C0392B" stroke={INK} strokeWidth={1.8} />
      <path d="M80 26 h6 v8 h-6 z" fill="#C0392B" stroke={INK} strokeWidth={1.8} />
      <path d="M42 30 l6 1 M62 26 l6 1 M78 32 l6 1" stroke={INK} strokeWidth={1.5} fill="none" />
    </g>
  </Item>
);

const EyeballPunch = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={28} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M38 34 l6 56 q1 8 8 8 h16 q7 0 8 -8 l6 -56 z" fill="#B8E8A8" fillOpacity={0.8} {...S} strokeWidth={3} />
      <path d="M42 44 l5 46 M78 44 l-5 46" stroke="#D8F2C8" strokeWidth={3} fill="none" />
      <circle cx={52} cy={56} r={8} fill="#FDF8F2" {...S} strokeWidth={2.5} />
      <circle cx={52} cy={56} r={3.4} fill="#7C3AA6" stroke={INK} strokeWidth={1.8} />
      <circle cx={70} cy={72} r={8} fill="#FDF8F2" {...S} strokeWidth={2.5} />
      <circle cx={70} cy={72} r={3.4} fill="#C0392B" stroke={INK} strokeWidth={1.8} />
      <circle cx={58} cy={82} r={6} fill="#FDF8F2" {...S} strokeWidth={2} />
      <circle cx={58} cy={82} r={2.6} fill="#2E7D4F" stroke={INK} strokeWidth={1.5} />
      <path d="M74 30 l8 -12" stroke="#C0392B" strokeWidth={4} fill="none" />
      <path d="M82 18 q6 -2 8 4" stroke="#2E7D4F" strokeWidth={3} fill="none" />
    </g>
  </Item>
);

const GraveyardCake = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={38} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M26 62 q34 -8 68 0 l-4 28 q-30 8 -60 0 z" fill="#6B3F1E" {...S} strokeWidth={3} />
      <path d="M26 62 q34 -8 68 0 q2 5 -4 5 l-60 0 q-6 0 -4 -5 z" fill="#4A2C12" {...S} strokeWidth={3} />
      <ellipse cx={60} cy={62} rx={32} ry={6} fill="#3A2410" stroke={INK} strokeWidth={2.5} />
      <path d="M42 58 l2 -22 q0 -5 6 -5 q6 0 6 5 l0 22 z" fill="#B9A88F" {...S} strokeWidth={2.5} />
      <path d="M46 40 h12 M52 34 v10" stroke={INK} strokeWidth={1.8} fill="none" />
      <path d="M70 58 v-14 q0 -5 5 -5 q5 0 5 5 v14 z" fill="#B9A88F" {...S} strokeWidth={2.5} />
      <path d="M72 46 h8" stroke={INK} strokeWidth={1.8} fill="none" />
      <path d="M52 88 q4 -14 14 -12 q8 2 6 12 q10 -2 8 6 q-16 6 -28 6 z" fill="#F0EAD8" {...S} strokeWidth={2.5} />
      <path d="M60 78 l4 -8 4 8" fill="#F0EAD8" stroke={INK} strokeWidth={2} />
    </g>
  </Item>
);

const SnowmanStew = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={34} ry={6} fill="rgba(20,10,4,0.25)" />
      <circle cx={60} cy={78} r={20} fill="#F0F6FA" {...S} strokeWidth={3} />
      <circle cx={60} cy={52} r={15} fill="#F0F6FA" {...S} strokeWidth={3} />
      <path d="M60 37 v-8" stroke="#E87828" strokeWidth={5} fill="none" />
      <path d="M60 29 l10 -2 q-2 6 -8 6 z" fill="#E87828" stroke={INK} strokeWidth={2} />
      <circle cx={55} cy={50} r={2.2} fill={INK} />
      <circle cx={65} cy={50} r={2.2} fill={INK} />
      <path d="M54 57 q6 5 12 0" stroke={INK} strokeWidth={2} fill="none" />
      <circle cx={60} cy={72} r={2} fill={INK} />
      <circle cx={60} cy={82} r={2} fill={INK} />
      <ellipse cx={60} cy={34} rx={22} ry={6} fill="#B8E8D8" {...S} strokeWidth={2.5} />
      <path d="M44 34 q4 -6 8 -1 M62 31 q5 -5 9 -1" stroke="#8ACBB8" strokeWidth={2.5} fill="none" />
      <path d="M40 30 q16 -8 40 -2" stroke={INK} strokeWidth={3} fill="none" />
    </g>
  </Item>
);

const GingerGhoulCookies = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={38} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M40 82 l-8 -20 q-2 -6 3 -7 l8 -2 q-2 -8 4 -10 q6 -1 6 6 l0 10 q8 0 10 4 l2 20 q1 6 -4 7 l-14 2 q-6 1 -7 -10 z" fill="#B97C3E" {...S} strokeWidth={3} />
      <circle cx={37} cy={60} r={2.5} fill="#F0EAD8" stroke={INK} strokeWidth={1.5} />
      <path d="M32 68 q5 4 9 1" stroke="#F0EAD8" strokeWidth={2} fill="none" />
      <path d="M74 84 l6 -22 q2 -6 -3 -7 l-8 -1 q3 -8 -3 -11 q-6 -1 -6 6 l-1 10 q-8 1 -9 5 l-3 20 q-1 6 4 7 l14 1 q6 1 7 -8 z" fill="#B97C3E" {...S} strokeWidth={3} />
      <circle cx={74} cy={58} r={2.5} fill="#C0392B" stroke={INK} strokeWidth={1.5} />
      <path d="M78 68 q-5 4 -9 1" stroke="#F0EAD8" strokeWidth={2} fill="none" />
      <circle cx={60} cy={40} r={3} fill="#2E7D4F" stroke={INK} strokeWidth={1.5} />
    </g>
  </Item>
);

const CandyCaneCranium = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={30} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M60 20 q-22 2 -22 26 q0 12 6 18 l0 12 q0 8 8 8 h16 q8 0 8 -8 l0 -12 q6 -6 6 -18 q0 -24 -22 -26 z" fill="#FDF8F2" {...S} strokeWidth={3} />
      <path d="M52 22 l-8 6 8 4 -8 6 8 4 -7 6 8 4 -6 6 M68 22 l8 6 -8 4 8 6 -8 4 7 6 -8 4 6 6" stroke="#C0392B" strokeWidth={3} fill="none" />
      <circle cx={50} cy={52} r={6} fill="#2A1C10" stroke={INK} strokeWidth={2} />
      <circle cx={70} cy={52} r={6} fill="#2A1C10" stroke={INK} strokeWidth={2} />
      <path d="M56 62 l-2 8 6 0 z M64 62 l2 8 -6 0 z" fill="#2A1C10" stroke={INK} strokeWidth={2} />
      <path d="M48 78 v8 M56 78 v10 M64 78 v10 M72 78 v8" stroke={INK} strokeWidth={2.2} fill="none" />
    </g>
  </Item>
);

const YuleSlimeLog = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={38} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M28 74 q4 -26 32 -26 q28 0 32 26 l-2 12 q-30 8 -60 0 z" fill="#6B3F1E" {...S} strokeWidth={3} />
      <path d="M32 62 q28 -10 56 0" stroke="#8A5A32" strokeWidth={4} fill="none" />
      <path d="M36 68 q4 10 2 18 M60 64 q3 12 1 22 M84 68 q-4 10 -2 18" stroke="#9BE23B" strokeWidth={4} fill="none" strokeLinecap="round" />
      <circle cx={48} cy={52} r={6} fill="#F0EAD8" {...S} strokeWidth={2.5} />
      <path d="M44 52 h8" stroke={INK} strokeWidth={1.5} fill="none" />
      <circle cx={72} cy={50} r={5} fill="#F0EAD8" {...S} strokeWidth={2.5} />
      <path d="M68 50 h8" stroke={INK} strokeWidth={1.5} fill="none" />
      <path d="M52 46 l0 -8 M68 44 l0 -8" stroke="#F0EAD8" strokeWidth={3} fill="none" />
      <path d="M26 82 q34 -8 68 0" stroke="#F0EAD8" strokeWidth={3} fill="none" strokeDasharray="3 4" />
    </g>
  </Item>
);

const RottenEggOmelette = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={36} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M24 84 q36 -12 72 0 q2 8 -8 9 l-56 0 q-10 -1 -8 -9 z" fill="#E8E2D2" {...S} strokeWidth={3} />
      <path d="M32 74 q28 -16 56 0 q2 10 -12 11 l-32 0 q-14 -1 -12 -11 z" fill="#E8E2C2" {...S} strokeWidth={3} />
      <path d="M40 70 q20 -10 40 0 q2 8 -10 8 l-20 0 q-12 0 -10 -8 z" fill="#B8C96E" {...S} strokeWidth={2.5} />
      <path d="M48 66 q6 -4 12 -1 M62 68 q6 -3 10 1" stroke="#9BAF52" strokeWidth={2.5} fill="none" />
      <path d="M44 62 l2 -8 M60 60 l0 -9 M74 62 l-2 -8" stroke="#2E7D4F" strokeWidth={3} fill="none" />
      <circle cx={52} cy={72} r={2.5} fill="#C0392B" stroke={INK} strokeWidth={1.5} />
      <circle cx={68} cy={73} r={2.5} fill="#F6E58B" stroke={INK} strokeWidth={1.5} />
    </g>
  </Item>
);

const BunnyTailTruffles = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={36} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M26 88 q34 -10 68 0 q2 6 -6 7 l-56 0 q-8 -1 -6 -7 z" fill="#C9954A" {...S} strokeWidth={3} />
      <circle cx={42} cy={68} r={13} fill="#4A2C12" {...S} strokeWidth={3} />
      <circle cx={76} cy={70} r={13} fill="#4A2C12" {...S} strokeWidth={3} />
      <circle cx={58} cy={56} r={13} fill="#4A2C12" {...S} strokeWidth={3} />
      <circle cx={42} cy={68} r={5} fill="#FDF8F2" stroke={INK} strokeWidth={2} />
      <circle cx={76} cy={70} r={5} fill="#FDF8F2" stroke={INK} strokeWidth={2} />
      <circle cx={58} cy={56} r={5} fill="#FDF8F2" stroke={INK} strokeWidth={2} />
      <path d="M40 66 q2 -3 4 -1 M74 68 q2 -3 4 -1 M56 54 q2 -3 4 -1" stroke="#D4C8B8" strokeWidth={1.5} fill="none" />
      <path d="M36 62 l-2 -4 M64 50 l-1 -4 M82 64 l2 -4" stroke="#FDF8F2" strokeWidth={2} fill="none" />
    </g>
  </Item>
);

const JellyBeanStew = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={34} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M32 52 q28 -8 56 0 l-4 36 q-24 8 -48 0 z" fill="#6C3A96" {...S} strokeWidth={3} />
      <path d="M30 50 q30 -10 60 0 q3 6 -4 7 l-52 0 q-7 -1 -4 -7 z" fill="#7C3AA6" {...S} strokeWidth={3} />
      <ellipse cx={60} cy={53} rx={26} ry={6} fill="#C9788E" stroke={INK} strokeWidth={2.5} />
      <ellipse cx={50} cy={51} rx={5} ry={3.5} fill="#F6E58B" stroke={INK} strokeWidth={1.8} />
      <ellipse cx={62} cy={54} rx={5} ry={3.5} fill="#9BE23B" stroke={INK} strokeWidth={1.8} />
      <ellipse cx={72} cy={50} rx={5} ry={3.5} fill="#A9D6F0" stroke={INK} strokeWidth={1.8} />
      <ellipse cx={56} cy={47} rx={4} ry={3} fill="#F5B7C6" stroke={INK} strokeWidth={1.5} />
      <path d="M46 40 q2 -8 -2 -12 M62 36 q0 -10 2 -14" stroke="#E8C8D4" strokeWidth={3} fill="none" />
    </g>
  </Item>
);

const HotCrossBrains = () => (
  <Item w={120} h={110}>
    <g>
      <ellipse cx={60} cy={98} rx={36} ry={6} fill="rgba(20,10,4,0.25)" />
      <path d="M30 84 q30 -14 60 0 q2 8 -8 9 l-44 0 q-10 -1 -8 -9 z" fill="#B97C3E" {...S} strokeWidth={3} />
      <path d="M36 78 q24 -12 48 0 q1 6 -8 6 l-32 0 q-9 0 -8 -6 z" fill="#E8A0B4" {...S} strokeWidth={3} />
      <path d="M44 74 q4 -6 8 -2 q4 -6 8 -1 q4 -5 8 0 q4 -5 8 1 q3 -4 6 1" stroke="#D4788E" strokeWidth={2.5} fill="none" />
      <path d="M60 62 v18 M51 68 h18" stroke="#FDF8F2" strokeWidth={4} fill="none" />
      <path d="M38 66 l-4 -6 M82 66 l4 -6" stroke="#8A5A2E" strokeWidth={3} fill="none" />
      <circle cx={48} cy={82} r={1.6} fill={INK} />
      <circle cx={72} cy={82} r={1.6} fill={INK} />
    </g>
  </Item>
);

export const itemArt: Record<string, () => ReactElement> = {
  'haunted-table': HauntedTable,
  'pumpkin-chair': PumpkinChair,
  'coffin-couch': CoffinCouch,
  'spooky-stove': SpookyStove,
  'ghost-chandelier': GhostChandelier,
  'haunted-wallpaper': HauntedWallpaper,
  'holiday-table': HolidayTable,
  'snow-chair': SnowChair,
  'christmas-tree': ChristmasTree,
  'present-stack': PresentStack,
  'festive-stove': FestiveStove,
  'snowy-wallpaper': SnowyWallpaper,
  'egg-table': EggTable,
  'bunny-chair': BunnyChair,
  'easter-basket': EasterBasket,
  'spring-fountain': SpringFountain,
  'egg-stove': EggStove,
  'spring-wallpaper': SpringWallpaper,
  'witch-brew-stew': WitchBrewStew,
  'severed-finger-fries': FingerFries,
  'eyeball-punch': EyeballPunch,
  'graveyard-cake': GraveyardCake,
  'snowman-stew': SnowmanStew,
  'ginger-ghoul-cookies': GingerGhoulCookies,
  'candy-cane-cranium': CandyCaneCranium,
  'yule-slime-log': YuleSlimeLog,
  'rotten-egg-omelette': RottenEggOmelette,
  'bunny-tail-truffles': BunnyTailTruffles,
  'jelly-bean-stew': JellyBeanStew,
  'hot-cross-brains': HotCrossBrains,
};
