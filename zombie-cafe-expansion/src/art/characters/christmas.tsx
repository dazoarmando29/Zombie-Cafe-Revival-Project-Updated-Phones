import { Art, INK, SKIN, Shadow } from '../kit';

const S = { stroke: INK, strokeLinejoin: 'round' as const, strokeLinecap: 'round' as const };

/* --- 5. Zombie Santa --------------------------------------------- */
export function ZombieSanta() {
  return (
    <Art>
      <Shadow rx={60} />
      {/* legs + boots */}
      <path d="M80 188 l-2 36 q0 11 12 11 h8 q9 0 9 -11 l0 -34 z" fill="#B32330" {...S} strokeWidth={4} />
      <path d="M120 188 l2 36 q0 11 -11 11 h-8 q-9 0 -9 -11 l1 -34 z" fill="#B32330" {...S} strokeWidth={4} />
      <path d="M76 230 q-10 2 -12 10 q-1 8 9 9 h26 q7 0 7 -8 l0 -11 z" fill="#2E2A26" {...S} strokeWidth={4} />
      <path d="M124 230 q10 2 12 10 q1 8 -9 9 h-25 q-7 0 -7 -8 l0 -11 z" fill="#2E2A26" {...S} strokeWidth={4} />
      {/* boot fur */}
      <path d="M76 228 q12 8 26 2 l2 8 q-14 8 -30 -2 z" fill="#F0EAD8" {...S} strokeWidth={3} />
      <path d="M124 228 q-12 8 -26 2 l-2 8 q14 8 30 -2 z" fill="#F0EAD8" {...S} strokeWidth={3} />
      {/* suit belly */}
      <path
        d="M100 106 q-36 0 -44 34 q-6 26 2 44 q2 12 12 12 l10 1 4 -14 8 12 8 -12 8 12 8 -12 6 12 8 -12 6 12 4 -12 8 12 q10 0 12 -12 q8 -18 2 -44 q-8 -34 -44 -34 z"
        fill="#B32330"
        {...S}
        strokeWidth={4}
      />
      {/* suit patches + seams */}
      <path d="M68 152 l16 4 -2 14 -16 -3 z" fill="#8C1620" {...S} strokeWidth={2.5} />
      <path d="M124 138 l10 12 -8 6 -8 -10 z" fill="#8C1620" {...S} strokeWidth={2.5} />
      <path d="M100 112 q-4 40 2 74" stroke="#8C1620" strokeWidth={3} fill="none" />
      {/* belt */}
      <path d="M60 158 q40 14 80 0 l2 14 q-40 14 -82 0 z" fill="#2E2A26" {...S} strokeWidth={3.5} />
      <rect x={88} y={158} width={26} height={22} rx={4} fill="#E8B84B" {...S} strokeWidth={3.5} />
      <rect x={95} y={164} width={12} height={10} rx={2} fill="#2E2A26" stroke={INK} strokeWidth={2.5} />
      {/* arms + sleeves */}
      <path d="M58 122 q-20 12 -24 38 q-2 12 8 15 q10 2 13 -8 l12 -28 z" fill="#B32330" {...S} strokeWidth={4} />
      <path d="M142 122 q20 12 24 38 q2 12 -8 15 q-10 2 -13 -8 l-12 -28 z" fill="#B32330" {...S} strokeWidth={4} />
      {/* cuffs + hands */}
      <path d="M36 164 q12 8 26 1 l2 9 q-14 8 -30 -1 z" fill="#F0EAD8" {...S} strokeWidth={3} />
      <path d="M40 176 q-4 10 2 15 q7 5 12 -2 q4 -6 0 -13 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      <path d="M164 164 q-12 8 -26 1 l-2 9 q14 8 30 -1 z" fill="#F0EAD8" {...S} strokeWidth={3} />
      <path d="M160 176 q4 10 -2 15 q-7 5 -12 -2 q-4 -6 0 -13 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      {/* present sack */}
      <path d="M148 178 q22 -4 26 16 q3 16 -12 22 q-20 6 -28 -8 q-6 -14 14 -30 z" fill="#7C121C" {...S} strokeWidth={4} />
      <path d="M148 182 q-4 -12 8 -16 q10 -3 12 6" stroke={INK} strokeWidth={3} fill="none" />
      <rect x={152} y={170} width={16} height={12} rx={2} fill="#2E7D4F" {...S} strokeWidth={2.5} transform="rotate(-12 160 176)" />
      <rect x={138} y={162} width={14} height={11} rx={2} fill="#A8D8E8" {...S} strokeWidth={2.5} transform="rotate(10 145 168)" />
      <path d="M145 158 l10 8 M150 158 l-10 8" stroke="#E8B84B" strokeWidth={2.5} fill="none" />
      <path d="M142 162 l2 -6 5 2" stroke={INK} strokeWidth={2} fill="none" />
      {/* beard */}
      <path
        d="M100 74 q-30 2 -32 30 q-2 26 12 38 q6 4 10 -2 l4 -12 6 12 6 -12 6 12 4 12 q4 6 10 2 q14 -12 12 -38 q-2 -28 -32 -30 z"
        fill="#F0EAD8"
        {...S}
        strokeWidth={4}
      />
      {/* face */}
      <ellipse cx={100} cy={64} rx={30} ry={27} fill="#A8B896" {...S} strokeWidth={4} />
      {/* droopy eyes */}
      <path d="M82 60 q7 -5 13 1 q-7 5 -13 -1 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <path d="M118 60 q-7 -5 -13 1 q7 5 13 -1 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <circle cx={89} cy={61} r={2.6} fill="#2A1C10" />
      <circle cx={111} cy={61} r={2.6} fill="#2A1C10" />
      <path d="M78 52 q10 -6 18 0 M122 52 q-10 -6 -18 0" stroke="#F0EAD8" strokeWidth={5} fill="none" strokeLinecap="round" />
      {/* nose + rosy cheeks */}
      <ellipse cx={100} cy={72} rx={8} ry={6} fill="#C9846E" {...S} strokeWidth={3} />
      <circle cx={80} cy={72} r={5} fill="#C9846E" opacity={0.6} />
      <circle cx={120} cy={72} r={5} fill="#C9846E" opacity={0.6} />
      {/* santa hat */}
      <path d="M68 46 q2 -30 32 -34 q22 -3 30 16 q22 4 30 18 q4 8 -4 10 q-30 -4 -88 -2 z" fill="#B32330" {...S} strokeWidth={4} />
      <path d="M62 44 q40 -8 94 2 q8 2 6 10 q-2 8 -12 8 q-44 -6 -88 -6 q-10 0 -8 -8 q1 -4 8 -6 z" fill="#F0EAD8" {...S} strokeWidth={4} />
      <circle cx={136} cy={26} r={10} fill="#F0EAD8" {...S} strokeWidth={3.5} />
      <path d="M74 36 l4 -8 4 8 4 -8" stroke="#8C1620" strokeWidth={2.5} fill="none" />
    </Art>
  );
}

/* --- 6. Frozen Elf ----------------------------------------------- */
export function FrozenElf() {
  return (
    <Art>
      <Shadow rx={50} />
      {/* icicle drips behind */}
      <path d="M46 150 l4 12 4 -12 z M150 168 l4 14 4 -14 z" fill="#BFE6F2" stroke={INK} strokeWidth={2.5} />
      {/* legs — striped tights */}
      <path d="M82 186 l-2 40 q0 11 12 11 h6 q9 0 9 -11 l0 -38 z" fill="#2E7D4F" {...S} strokeWidth={4} />
      <path d="M116 186 l2 40 q0 11 -11 11 h-6 q-9 0 -9 -11 l1 -38 z" fill="#2E7D4F" {...S} strokeWidth={4} />
      <path d="M82 200 h24 M82 214 h24 M116 200 h-22 M116 214 h-22" stroke="#A8D8E8" strokeWidth={4} fill="none" />
      {/* curled shoes */}
      <path d="M78 232 q-12 2 -16 10 q-2 8 8 9 q12 1 18 -6 l2 -11 z" fill="#B32330" {...S} strokeWidth={4} />
      <path d="M122 232 q12 2 16 10 q2 8 -8 9 q-12 1 -18 -6 l-2 -11 z" fill="#B32330" {...S} strokeWidth={4} />
      {/* tunic */}
      <path
        d="M100 108 q-28 0 -36 26 l-8 32 q-2 12 8 14 l12 2 4 -18 8 14 6 -14 8 14 6 -14 8 14 4 -14 10 16 q10 -2 8 -14 l-8 -32 q-8 -26 -36 -26 z"
        fill="#2E7D4F"
        {...S}
        strokeWidth={4}
      />
      {/* jagged hem detail */}
      <path d="M64 168 l8 2 6 -4 8 4 8 -4 8 4 8 -4 8 4 6 -4 8 2" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* red trim + belt */}
      <path d="M70 122 q30 14 60 0" stroke="#B32330" strokeWidth={7} fill="none" />
      <path d="M70 122 q30 14 60 0" stroke={INK} strokeWidth={2.2} fill="none" />
      <path d="M66 150 q34 12 68 0 l1 12 q-34 12 -70 0 z" fill="#B32330" {...S} strokeWidth={3} />
      <rect x={90} y={152} width={20} height={16} rx={3} fill="#E8B84B" {...S} strokeWidth={3} />
      {/* frost sparkle on chest */}
      <path d="M100 128 l3 6 6 1 -4 5 1 6 -6 -3 -6 3 1 -6 -4 -5 6 -1 z" fill="#E8F6FC" stroke={INK} strokeWidth={2} />
      {/* arms */}
      <path d="M64 122 q-16 12 -18 34 q-1 11 9 13 q9 1 11 -8 l6 -24 z" fill="#2E7D4F" {...S} strokeWidth={4} />
      <path d="M136 122 q16 12 18 34 q1 11 -9 13 q-9 1 -11 -8 l-6 -24 z" fill="#2E7D4F" {...S} strokeWidth={4} />
      {/* mittens */}
      <path d="M52 166 q-6 8 -2 14 q5 6 12 1 q6 -4 4 -12 z" fill="#B32330" {...S} strokeWidth={3.5} />
      <path d="M148 166 q6 8 2 14 q-5 6 -12 1 q-6 -4 -4 -12 z" fill="#B32330" {...S} strokeWidth={3.5} />
      {/* bitten candy cane */}
      <path
        d="M152 168 q2 -22 -8 -30 q-8 -6 -14 2 q-4 6 2 10 q6 4 10 -2"
        stroke="#F0EAD8"
        strokeWidth={8}
        fill="none"
        strokeLinecap="round"
      />
      <path
        d="M152 168 q2 -22 -8 -30 q-8 -6 -14 2 q-4 6 2 10 q6 4 10 -2"
        stroke="#B32330"
        strokeWidth={8}
        strokeDasharray="5 7"
        fill="none"
        strokeLinecap="round"
      />
      <path d="M128 132 l6 -4 M132 142 l6 -2" stroke={INK} strokeWidth={2.2} fill="none" />
      {/* head + ears */}
      <path d="M70 54 l-18 -8 2 16 z M130 54 l18 -8 -2 16 z" fill={SKIN.blue} {...S} strokeWidth={3.5} />
      <ellipse cx={100} cy={72} rx={33} ry={31} fill={SKIN.blue} {...S} strokeWidth={4} />
      {/* frost cheeks */}
      <circle cx={78} cy={80} r={6} fill="#C9E8F2" opacity={0.8} />
      <circle cx={122} cy={80} r={6} fill="#C9E8F2" opacity={0.8} />
      {/* sleepy eyes */}
      <path d="M80 68 q8 -6 15 1 q-8 5 -15 -1 z" fill="#2A3A4A" stroke={INK} strokeWidth={2.5} />
      <path d="M120 68 q-8 -6 -15 1 q8 5 15 -1 z" fill="#2A3A4A" stroke={INK} strokeWidth={2.5} />
      <path d="M78 60 q9 -4 17 2 M122 60 q-9 -4 -17 2" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* grin with tiny fang */}
      <path d="M88 86 q12 8 22 -2 q-8 12 -18 8 q-4 -2 -4 -6 z" fill="#2A3A4A" stroke={INK} strokeWidth={2.5} />
      <path d="M94 88 l2 4 2 -4" fill="#FFFFFF" stroke={INK} strokeWidth={1.5} />
      {/* dark teal hair with icicles */}
      <path d="M70 48 q4 -22 30 -24 q26 2 30 24 q-8 -10 -18 -12 q-8 6 -16 2 q-10 4 -18 8 q-4 2 -8 2 z" fill="#2A5A66" {...S} strokeWidth={4} />
      <path d="M84 34 l2 8 3 -8 M104 30 l2 9 3 -9 M118 36 l2 7 3 -7" stroke="#BFE6F2" strokeWidth={3} fill="none" />
      {/* elf hat */}
      <path d="M72 42 q0 -32 34 -38 q6 18 -2 28 q22 2 34 12 q-2 8 -12 8 l-46 -2 q-8 0 -8 -8 z" fill="#2E7D4F" {...S} strokeWidth={4} />
      <path d="M132 42 q4 -6 12 -4 q-2 6 -8 8 z" fill="#B32330" {...S} strokeWidth={3} />
      <circle cx={108} cy={8} r={8} fill="#F0EAD8" {...S} strokeWidth={3} />
      <path d="M74 42 q34 -8 62 2" stroke="#B32330" strokeWidth={6} fill="none" />
    </Art>
  );
}

/* --- 7. Gingerbread Zombie --------------------------------------- */
export function Gingerbread() {
  return (
    <Art>
      <Shadow rx={52} />
      {/* legs */}
      <path d="M78 182 l-8 40 q-2 11 10 13 q10 1 13 -9 l5 -32 z" fill="#B97C3E" {...S} strokeWidth={4} />
      <path d="M122 182 l8 40 q2 11 -10 13 q-10 1 -13 -9 l-5 -32 z" fill="#B97C3E" {...S} strokeWidth={4} />
      {/* icing squiggles on legs */}
      <path d="M76 196 q6 4 4 10 q-2 5 -8 2 M124 196 q-6 4 -4 10 q2 5 8 2" stroke="#F0EAD8" strokeWidth={3.5} fill="none" />
      <path d="M72 214 q6 4 3 9 M128 214 q-6 4 -3 9" stroke="#F0EAD8" strokeWidth={3.5} fill="none" />
      {/* body — gingerbread */}
      <path
        d="M100 104 q-30 0 -38 28 q-6 22 -2 42 q2 12 12 12 l12 1 2 -14 8 12 6 -12 8 12 6 -12 6 12 4 -12 8 12 q10 0 12 -12 q4 -20 -2 -42 q-8 -28 -38 -28 z"
        fill="#B97C3E"
        {...S}
        strokeWidth={4}
      />
      {/* cracked frosting outline on body */}
      <path d="M72 118 q28 -12 56 0" stroke="#F0EAD8" strokeWidth={5} fill="none" strokeLinecap="round" />
      <path d="M68 132 q8 6 6 14 M132 132 q-8 6 -6 14" stroke="#F0EAD8" strokeWidth={4.5} fill="none" strokeLinecap="round" />
      <path d="M84 128 l6 4 -2 5 M114 122 l6 4 -3 5" stroke={INK} strokeWidth={2.2} fill="none" />
      {/* candy buttons (some missing = empty sockets) */}
      <circle cx={100} cy={132} r={6} fill="#C0392B" {...S} strokeWidth={2.5} />
      <circle cx={100} cy={152} r={6} fill="#2E7D4F" {...S} strokeWidth={2.5} />
      <circle cx={100} cy={172} r={6} fill="#8A5A2E" {...S} strokeWidth={2.5} strokeDasharray="3 3" />
      <circle cx={100} cy={172} r={6} fill="#8A5A2E" opacity={0.25} />
      {/* arms — one bitten */}
      <path d="M62 116 q-20 10 -24 34 q-2 12 8 15 q10 2 13 -8 l11 -26 z" fill="#B97C3E" {...S} strokeWidth={4} />
      <path d="M138 116 q20 10 24 34 q2 12 -8 15 q-10 2 -13 -8 l-11 -26 z" fill="#B97C3E" {...S} strokeWidth={4} />
      {/* bite marks */}
      <path d="M38 150 q4 -2 6 2 q2 4 -2 6 q-4 2 -6 -2 q-2 -4 2 -6 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <path d="M158 152 q-4 -2 -6 2 q-2 4 2 6 q4 2 6 -2 q2 -4 -2 -6 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      {/* icing cuffs */}
      <path d="M56 138 q10 8 22 1 M144 138 q-10 8 -22 1" stroke="#F0EAD8" strokeWidth={5} fill="none" strokeLinecap="round" />
      {/* head — round cookie */}
      <circle cx={100} cy={70} r={38} fill="#B97C3E" {...S} strokeWidth={4} />
      {/* icing swirl on head */}
      <path d="M76 42 q24 -10 48 2 q-24 -2 -48 -2 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <path d="M82 52 q18 -6 36 2" stroke="#F0EAD8" strokeWidth={4} fill="none" strokeLinecap="round" />
      {/* crack lines */}
      <path d="M112 40 l8 8 -5 6 8 6" stroke={INK} strokeWidth={2.5} fill="none" />
      <path d="M68 82 l6 5 -3 6" stroke={INK} strokeWidth={2.2} fill="none" />
      {/* candy eyes — one missing */}
      <circle cx={85} cy={68} r={7} fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <circle cx={85} cy={68} r={3} fill="#2A1C10" />
      <circle cx={115} cy={68} r={7} fill="#8A5A2E" stroke={INK} strokeWidth={2.5} strokeDasharray="3 2" />
      <path d="M111 64 l8 8 M119 64 l-8 8" stroke={INK} strokeWidth={2} fill="none" />
      {/* stitched smile */}
      <path d="M84 88 q16 10 32 -2" stroke="#F0EAD8" strokeWidth={4} fill="none" strokeLinecap="round" />
      <path d="M90 89 l-1 5 M100 92 l0 5 M110 88 l1 5" stroke={INK} strokeWidth={2} fill="none" />
      {/* holly on head */}
      <path d="M100 30 l6 -8 2 8 8 -4 -4 8 8 0 -8 6 z" fill="#2E7D4F" stroke={INK} strokeWidth={2.5} />
      <circle cx={108} cy={34} r={3.5} fill="#C0392B" stroke={INK} strokeWidth={2} />
      <circle cx={114} cy={30} r={3.5} fill="#C0392B" stroke={INK} strokeWidth={2} />
    </Art>
  );
}

/* --- 8. Reindeer Zombie ------------------------------------------ */
export function ReindeerZombie() {
  return (
    <Art>
      <Shadow rx={56} />
      {/* legs */}
      <path d="M80 186 l-2 40 q0 11 12 11 h6 q9 0 9 -11 l0 -38 z" fill="#8A5A32" {...S} strokeWidth={4} />
      <path d="M118 186 l2 40 q0 11 -11 11 h-6 q-9 0 -9 -11 l1 -38 z" fill="#8A5A32" {...S} strokeWidth={4} />
      {/* hooves */}
      <path d="M78 232 q-8 2 -8 9 q0 6 8 6 h22 q6 0 6 -7 l0 -8 z" fill="#3A2A18" {...S} strokeWidth={4} />
      <path d="M122 232 q8 2 8 9 q0 6 -8 6 h-21 q-6 0 -6 -7 l0 -8 z" fill="#3A2A18" {...S} strokeWidth={4} />
      <path d="M92 238 v8 M108 238 v8" stroke={INK} strokeWidth={2.5} />
      {/* costume body */}
      <path
        d="M100 108 q-32 0 -40 30 q-6 24 0 42 q2 12 12 13 l12 1 4 -16 8 13 6 -13 8 13 6 -13 8 13 4 -13 10 14 q10 -1 12 -13 q6 -18 0 -42 q-8 -30 -40 -30 z"
        fill="#8A5A32"
        {...S}
        strokeWidth={4}
      />
      {/* belly patch */}
      <path d="M100 118 q-18 4 -20 24 q-2 22 20 26 q22 -4 20 -26 q-2 -20 -20 -24 z" fill="#C9A06A" {...S} strokeWidth={3.5} />
      <path d="M86 140 l4 3 -2 4 M112 152 l4 3 -2 4" stroke={INK} strokeWidth={2} fill="none" />
      {/* scarf */}
      <path d="M72 112 q28 14 56 0 l3 13 q-30 15 -62 0 z" fill="#2E7D4F" {...S} strokeWidth={3.5} />
      <path d="M118 124 l10 30 -10 4 -8 -30 z" fill="#B32330" {...S} strokeWidth={3.5} />
      <path d="M118 138 l10 4 M116 150 l10 4" stroke="#2E7D4F" strokeWidth={3} fill="none" />
      {/* arms + hooves */}
      <path d="M60 122 q-18 12 -20 36 q-1 11 9 13 q10 2 12 -8 l8 -26 z" fill="#8A5A32" {...S} strokeWidth={4} />
      <path d="M140 122 q18 12 20 36 q1 11 -9 13 q-10 2 -12 -8 l-8 -26 z" fill="#8A5A32" {...S} strokeWidth={4} />
      <path d="M44 166 q-6 8 -2 15 q6 6 13 1 q5 -4 3 -12 z" fill="#3A2A18" {...S} strokeWidth={3.5} />
      <path d="M156 166 q6 8 2 15 q-6 6 -13 1 q-5 -4 -3 -12 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      <path d="M150 176 l8 2 M150 182 l8 -1" stroke={INK} strokeWidth={2} fill="none" />
      {/* bell */}
      <path d="M100 122 l3 -8" stroke={INK} strokeWidth={3} />
      <path d="M92 124 q8 -12 16 0 q3 8 -8 10 q-11 -2 -8 -10 z" fill="#E8B84B" {...S} strokeWidth={3} />
      <circle cx={100} cy={132} r={2.5} fill={INK} />
      {/* reindeer hood head */}
      <path d="M100 22 q-34 2 -38 38 q-2 30 38 36 q40 -6 38 -36 q-4 -36 -38 -38 z" fill="#8A5A32" {...S} strokeWidth={4} />
      {/* hood opening = zombie face */}
      <ellipse cx={100} cy={72} rx={27} ry={25} fill={SKIN.green} {...S} strokeWidth={3.5} />
      {/* zombie eyes */}
      <path d="M84 66 q7 -5 13 1 q-7 5 -13 -1 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <path d="M116 66 q-7 -5 -13 1 q7 5 13 -1 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <circle cx={91} cy={67} r={2.6} fill="#2A1C10" />
      <circle cx={109} cy={67} r={2.6} fill="#2A1C10" />
      <path d="M82 58 q8 -4 14 1 M118 58 q-8 -4 -14 1" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* stitched grin */}
      <path d="M88 86 q12 8 24 -2" stroke={INK} strokeWidth={3} fill="none" />
      <path d="M93 87 l-1 5 M103 89 l0 5 M113 85 l1 5" stroke={INK} strokeWidth={2} fill="none" />
      {/* antlers — one broken */}
      <g fill="none" strokeLinecap="round">
        <g stroke={INK} strokeWidth={11}>
          <path d="M74 36 q-12 -8 -18 -18 M66 28 l-10 -4 M60 22 l-4 -8 M58 30 l-10 2" />
          <path d="M126 36 q12 -8 18 -18 M134 28 l10 -4" />
        </g>
        <g stroke="#6E4620" strokeWidth={7}>
          <path d="M74 36 q-12 -8 -18 -18 M66 28 l-10 -4 M60 22 l-4 -8 M58 30 l-10 2" />
          <path d="M126 36 q12 -8 18 -18 M134 28 l10 -4" />
        </g>
      </g>
      <path d="M141 17 l4 -6 3 5" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} strokeLinejoin="round" />
      {/* ears */}
      <path d="M62 52 q-12 -2 -14 8 q-1 8 8 10 z" fill="#8A5A32" {...S} strokeWidth={3.5} />
      <path d="M138 52 q12 -2 14 8 q1 8 -8 10 z" fill="#8A5A32" {...S} strokeWidth={3.5} />
      <path d="M64 56 q-6 0 -6 5 q0 4 5 5 z" fill="#C9A06A" stroke="none" />
      <path d="M136 56 q6 0 6 5 q0 4 -5 5 z" fill="#C9A06A" stroke="none" />
      {/* red stitched nose */}
      <ellipse cx={100} cy={78} rx={8} ry={6} fill="#C0392B" {...S} strokeWidth={3} />
      <path d="M94 78 h12" stroke={INK} strokeWidth={2} fill="none" />
      {/* ear tear + stitches */}
      <path d="M132 58 l4 4 -3 3" stroke={INK} strokeWidth={2} fill="none" />
      <path d="M128 62 l3 2 M132 65 l3 2" stroke={INK} strokeWidth={1.8} fill="none" />
    </Art>
  );
}

export const christmasCharacters = [ZombieSanta, FrozenElf, Gingerbread, ReindeerZombie];
