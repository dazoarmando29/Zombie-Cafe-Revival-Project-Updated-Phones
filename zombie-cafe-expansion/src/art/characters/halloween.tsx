import { Art, INK, SKIN, Shadow } from '../kit';

/* ============================================================
   HALLOWEEN ZOMBIES — hand-built SVG characters.
   Style: thick ink outlines, big heads, flat muted fills,
   classic 2011 cartoon mobile-game look.
   ============================================================ */

const S = { stroke: INK, strokeLinejoin: 'round' as const, strokeLinecap: 'round' as const };

/* --- 1. Pumpkin Head Zombie -------------------------------------- */
export function PumpkinHead() {
  return (
    <Art>
      <Shadow />
      {/* legs */}
      <path d="M78 186 l4 44 q0 10 12 10 h8 q10 0 10-10 l-2-38 z" fill="#4A4458" {...S} strokeWidth={4} />
      <path d="M112 182 l0 46 q0 10 11 10 h9 q10 0 9-11 l-6-42 z" fill="#4A4458" {...S} strokeWidth={4} />
      {/* shoes */}
      <path d="M74 236 q-10 2 -12 10 q-1 8 9 9 h26 q8 0 8-8 v-11 z" fill="#2E2A38" {...S} strokeWidth={4} />
      <path d="M118 238 q-2 10 7 11 h25 q10-1 8-9 q-2-8 -12-10 l-10 -2 z" fill="#2E2A38" {...S} strokeWidth={4} />
      {/* coat */}
      <path
        d="M100 108 q-34 2 -40 34 l-8 34 q-2 12 8 14 l10 2 4 -14 6 12 8 -12 6 12 8 -12 6 12 4 -12 8 10 q10 -2 8 -14 l-8 -36 q-6 -30 -40 -30 z"
        fill="#4A4458"
        {...S}
        strokeWidth={4}
      />
      {/* coat details */}
      <path d="M100 112 l-12 72" stroke={INK} strokeWidth={3} fill="none" />
      <path d="M76 138 h20 v18 h-22 z" fill="#3A3448" {...S} strokeWidth={3} />
      <path d="M86 158 l6 4 M92 154 l8 5" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* arms */}
      <path d="M62 128 q-18 12 -20 36 q-2 12 8 16 q10 3 13 -7 l10 -26 z" fill="#4A4458" {...S} strokeWidth={4} />
      <path d="M138 128 q18 12 20 36 q2 12 -8 16 q-10 3 -13 -7 l-10 -26 z" fill="#4A4458" {...S} strokeWidth={4} />
      {/* hands */}
      <path d="M45 176 q-6 8 -2 14 q4 7 12 3 q7 -4 6 -12 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      <path d="M155 176 q6 8 2 14 q-4 7 -12 3 q-7 -4 -6 -12 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      {/* lantern in right hand */}
      <path d="M152 168 l4 -12" stroke={INK} strokeWidth={3} fill="none" />
      <path d="M142 142 h22 l-3 22 h-16 z" fill="#F2B632" {...S} strokeWidth={3.5} />
      <path d="M140 142 h26 M145 132 q8 -8 16 0" stroke={INK} strokeWidth={3} fill="none" />
      <rect x={148} y={146} width={10} height={12} rx={3} fill="#FFF3B8" {...S} strokeWidth={2.5} />
      {/* pumpkin head */}
      <path d="M100 22 q10 -12 20 -6 q-4 8 2 12" stroke="#5E7A2E" strokeWidth={5} fill="none" strokeLinecap="round" />
      <path d="M96 30 q-2 -12 6 -14 q10 -2 12 8 l2 10 z" fill="#5E7A2E" {...S} strokeWidth={3.5} />
      <ellipse cx={100} cy={72} rx={48} ry={40} fill="#E87828" {...S} strokeWidth={4} />
      <path d="M72 42 q-8 28 0 58 M100 32 q-8 36 0 80 M128 42 q8 28 0 58" stroke="#C45E1C" strokeWidth={3.5} fill="none" strokeLinecap="round" />
      {/* carved face — glow */}
      <path d="M72 62 l18 -10 2 20 z" fill="#2A1C10" {...S} strokeWidth={3} />
      <path d="M128 62 l-18 -10 -2 20 z" fill="#2A1C10" {...S} strokeWidth={3} />
      <path d="M78 62 l9 -4 1 8 z" fill="#CFFF4A" />
      <path d="M122 62 l-9 -4 -1 8 z" fill="#CFFF4A" />
      <path
        d="M68 82 q32 26 64 -2 q3 12 -4 15 q-28 13 -56 0 q-7 -3 -4 -13 z"
        fill="#CFFF4A"
        {...S}
        strokeWidth={3}
      />
      <path d="M78 84 l6 10 6 -8 z M100 85 l6 10 6 -8 z" fill="#E87828" stroke={INK} strokeWidth={2.5} strokeLinejoin="round" />
      <path d="M80 99 l6 -8 6 7 z M104 100 l6 -9 5 8 z" fill="#E87828" stroke={INK} strokeWidth={2.5} strokeLinejoin="round" />
    </Art>
  );
}

/* --- 2. Haunted Bride -------------------------------------------- */
export function HauntedBride() {
  return (
    <Art>
      <Shadow rx={52} />
      {/* veil behind */}
      <path
        d="M100 18 q-42 6 -46 52 q-4 44 8 84 l16 -8 q-10 -42 -6 -72 q4 -28 28 -38 z"
        fill="#F4F1E6"
        fillOpacity={0.85}
        {...S}
        strokeWidth={4}
      />
      <path
        d="M100 18 q42 6 46 52 q4 44 -8 84 l-16 -8 q10 -42 6 -72 q-4 -28 -28 -38 z"
        fill="#F4F1E6"
        fillOpacity={0.85}
        {...S}
        strokeWidth={4}
      />
      {/* dress */}
      <path
        d="M100 112 q-26 2 -32 24 l-22 76 q-3 12 8 14 q10 2 14 -6 l8 -16 6 16 8 -14 8 14 8 -16 8 14 q4 8 13 6 q11 -2 8 -14 l-22 -76 q-6 -22 -32 -24 z"
        fill="#F0EDE2"
        {...S}
        strokeWidth={4}
      />
      {/* dress stains & tears */}
      <path d="M84 168 l10 6 M86 174 l9 5 M88 180 l8 4" stroke="#B9A898" strokeWidth={3} fill="none" />
      <path d="M112 148 l8 12 -6 2 4 12" stroke={INK} strokeWidth={2.5} fill="none" />
      <path d="M70 128 q6 -4 10 2" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* arms */}
      <path d="M70 122 q-18 14 -18 34 q0 12 10 14 q9 1 11 -9 l6 -24 z" fill="#E4DDCE" {...S} strokeWidth={4} />
      <path d="M130 122 q18 14 18 34 q0 12 -10 14 q-9 1 -11 -9 l-6 -24 z" fill="#E4DDCE" {...S} strokeWidth={4} />
      {/* hands */}
      <path d="M58 168 q-5 9 0 15 q5 6 12 1 q6 -5 3 -13 z" fill={SKIN.pale} {...S} strokeWidth={3.5} />
      <path d="M142 168 q5 9 0 15 q-5 6 -12 1 q-6 -5 -3 -13 z" fill={SKIN.pale} {...S} strokeWidth={3.5} />
      {/* wilted bouquet in hand */}
      <path d="M56 172 l-4 -10 M56 162 l-7 -5 M56 162 l1 -8 M56 162 l8 -3" stroke="#4A3423" strokeWidth={2.5} fill="none" />
      <circle cx={53} cy={152} r={5} fill="#6E2436" {...S} strokeWidth={2.5} />
      <circle cx={63} cy={156} r={4} fill="#5A1A2C" {...S} strokeWidth={2.5} />
      <circle cx={52} cy={159} r={4} fill="#7C2A40" {...S} strokeWidth={2.5} />
      {/* hair */}
      <path
        d="M100 26 q-34 2 -38 38 q-2 22 4 34 l10 -6 q-6 -20 -2 -34 q4 -18 26 -22 z"
        fill="#3A2A38"
        {...S}
        strokeWidth={4}
      />
      <path
        d="M100 26 q34 2 38 38 q2 22 -4 34 l-10 -6 q6 -20 2 -34 q-4 -18 -26 -22 z"
        fill="#3A2A38"
        {...S}
        strokeWidth={4}
      />
      {/* face */}
      <ellipse cx={100} cy={72} rx={33} ry={32} fill={SKIN.pale} {...S} strokeWidth={4} />
      {/* dark makeup eyes */}
      <path d="M80 66 q7 -6 15 -1 q-7 8 -15 1 z" fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <path d="M120 66 q-7 -6 -15 -1 q7 8 15 1 z" fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <circle cx={88} cy={66} r={3} fill="#9BE23B" />
      <circle cx={112} cy={66} r={3} fill="#9BE23B" />
      <path d="M76 58 q8 -8 18 -2 M124 58 q-8 -8 -18 -2" stroke="#3A2A38" strokeWidth={3} fill="none" />
      {/* dark lips + fang */}
      <path d="M92 86 q8 7 16 0 q-8 10 -16 0 z" fill="#6E2436" stroke={INK} strokeWidth={2.5} />
      <path d="M97 88 l2 5 2 -5" fill="#F0EDE2" stroke={INK} strokeWidth={1.6} />
      {/* cheek cracks */}
      <path d="M74 78 l5 3 -2 4" stroke={INK} strokeWidth={2} fill="none" />
    </Art>
  );
}

/* --- 3. Scarecrow Zombie ----------------------------------------- */
export function Scarecrow() {
  return (
    <Art>
      <Shadow rx={58} />
      {/* legs — patched jeans */}
      <path d="M80 182 l-2 42 q0 12 13 12 h6 q10 0 10 -11 l0 -38 z" fill="#5E6E86" {...S} strokeWidth={4} />
      <path d="M118 182 l2 42 q0 12 -12 12 h-7 q-10 0 -9 -11 l2 -38 z" fill="#5E6E86" {...S} strokeWidth={4} />
      <path d="M86 206 l14 2 -2 12 -14 -2 z" fill="#4A5A70" {...S} strokeWidth={2.5} />
      <path d="M76 232 q-8 4 -8 10 q0 6 9 6 h22 q6 0 6 -7 v-9 z" fill="#3A2E22" {...S} strokeWidth={4} />
      <path d="M124 232 q8 4 8 10 q0 6 -9 6 h-21 q-6 0 -6 -7 v-9 z" fill="#3A2E22" {...S} strokeWidth={4} />
      {/* plaid shirt torso */}
      <path
        d="M100 112 q-30 0 -38 26 l-8 32 q-2 12 8 14 l12 2 4 -16 8 12 6 -12 8 12 6 -12 8 12 4 -12 10 12 q10 -2 8 -14 l-8 -32 q-8 -26 -38 -26 z"
        fill="#B4622E"
        {...S}
        strokeWidth={4}
      />
      {/* plaid pattern */}
      <path d="M74 130 h52 M72 148 h56 M78 166 h44 M88 118 v70 M110 118 v70" stroke="#8A4520" strokeWidth={2.5} fill="none" opacity={0.8} />
      {/* rope belt */}
      <path d="M68 156 q32 12 64 0" stroke="#D9B26A" strokeWidth={7} fill="none" />
      <path d="M68 156 q32 12 64 0" stroke={INK} strokeWidth={2.4} fill="none" />
      <circle cx={100} cy={161} r={5} fill="#D9B26A" {...S} strokeWidth={2.5} />
      {/* torn sleeve with straw */}
      <path d="M62 124 q-18 10 -22 32 l-2 8 q12 6 20 -2 l12 -22 z" fill="#B4622E" {...S} strokeWidth={4} />
      <path d="M138 124 q18 10 22 32 l2 8 q-12 6 -20 -2 l-12 -22 z" fill="#B4622E" {...S} strokeWidth={4} />
      <g stroke="#E8C876" strokeWidth={3} strokeLinecap="round">
        <path d="M42 156 l-8 4 M40 162 l-8 1 M44 150 l-6 -5 M158 156 l8 4 M160 162 l8 1 M156 150 l6 -5" fill="none" />
      </g>
      {/* hands at sleeve ends */}
      <path d="M40 152 q-7 7 -4 14 q4 6 11 2 q6 -4 4 -11 z" fill="#C89B5A" {...S} strokeWidth={3.5} />
      <path d="M160 152 q7 7 4 14 q-4 6 -11 2 q-6 -4 -4 -11 z" fill="#C89B5A" {...S} strokeWidth={3.5} />
      {/* crow on shoulder */}
      <path d="M142 118 q-2 -14 10 -18 q10 -3 14 6 q3 8 -4 12 l2 8 -8 -2 q-8 2 -14 -6 z" fill="#241C24" {...S} strokeWidth={3} />
      <path d="M156 106 l8 -2 -6 6 z" fill="#E8862E" stroke={INK} strokeWidth={2} />
      <circle cx={152} cy={103} r={1.8} fill="#F2E8D2" />
      {/* burlap head */}
      <path d="M100 28 q-32 4 -34 38 q-2 30 34 34 q36 -4 34 -34 q-2 -34 -34 -38 z" fill="#C89B5A" {...S} strokeWidth={4} />
      <path d="M76 52 q6 -8 14 -2 M124 52 q-6 -8 -14 -2" stroke={INK} strokeWidth={3} fill="none" />
      {/* stitched X eyes */}
      <path d="M78 62 l14 12 M92 62 l-14 12 M108 62 l14 12 M122 62 l-14 12" stroke={INK} strokeWidth={3.5} fill="none" />
      {/* stitched mouth */}
      <path d="M82 84 q18 8 36 -2" stroke={INK} strokeWidth={3.5} fill="none" />
      <path d="M88 83 l-1 7 M98 86 l-1 7 M108 85 l-1 7 M118 81 l-1 7" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* straw tufts from collar */}
      <g stroke="#E8C876" strokeWidth={3} strokeLinecap="round">
        <path d="M84 104 l-4 10 M94 108 l0 10 M106 108 l1 10 M116 104 l5 10" fill="none" />
      </g>
      {/* damaged scarecrow hat */}
      <path d="M58 40 q-4 -22 24 -30 q16 -5 22 6 q22 2 30 12 q4 6 -4 8 l-72 8 z" fill="#8A5E2E" {...S} strokeWidth={4} />
      <path d="M52 42 q30 -12 96 -6 q6 1 4 8 q-2 8 -12 8 l-78 -2 q-12 -1 -10 -8 z" fill="#6E4620" {...S} strokeWidth={4} />
      <path d="M78 24 l6 8 -8 2" fill="#E8C876" stroke={INK} strokeWidth={2.5} />
      <path d="M118 36 q8 -4 12 2" stroke="#E8C876" strokeWidth={3} fill="none" />
    </Art>
  );
}

/* --- 4. Vampire Chef --------------------------------------------- */
export function VampireChef() {
  return (
    <Art>
      <Shadow rx={58} />
      {/* cape behind */}
      <path
        d="M100 104 q-44 4 -52 44 l-12 56 q-2 12 10 12 l14 -2 4 -24 12 18 12 -18 12 18 12 -18 12 18 4 24 14 2 q12 0 10 -12 l-12 -56 q-8 -40 -52 -44 z"
        fill="#2E2242"
        {...S}
        strokeWidth={4}
      />
      <path d="M62 120 q-8 28 -10 56 l20 -6 q4 -26 12 -44 z" fill="#4A2A66" stroke={INK} strokeWidth={3} />
      <path d="M138 120 q8 28 10 56 l-20 -6 q-4 -26 -12 -44 z" fill="#4A2A66" stroke={INK} strokeWidth={3} />
      {/* legs */}
      <path d="M84 190 l-2 38 q0 11 12 11 h6 q9 0 9 -11 l0 -36 z" fill="#2E2242" {...S} strokeWidth={4} />
      <path d="M118 190 l2 38 q0 11 -11 11 h-7 q-9 0 -9 -11 l1 -36 z" fill="#2E2242" {...S} strokeWidth={4} />
      <path d="M78 236 q-9 3 -9 10 q0 6 9 6 h24 q6 0 6 -7 l0 -9 z" fill="#1C1428" {...S} strokeWidth={4} />
      <path d="M122 236 q9 3 9 10 q0 6 -9 6 h-23 q-6 0 -6 -7 l0 -9 z" fill="#1C1428" {...S} strokeWidth={4} />
      {/* chef jacket */}
      <path
        d="M100 106 q-30 0 -38 28 l-6 30 q-2 12 9 13 l11 1 4 -16 8 12 6 -12 8 12 6 -12 8 12 4 -12 10 13 q11 -1 9 -13 l-6 -30 q-8 -28 -38 -28 z"
        fill="#EFE9DC"
        {...S}
        strokeWidth={4}
      />
      {/* jacket buttons + folds */}
      <circle cx={100} cy={130} r={3} fill="#B9A88F" stroke={INK} strokeWidth={2} />
      <circle cx={100} cy={146} r={3} fill="#B9A88F" stroke={INK} strokeWidth={2} />
      <circle cx={100} cy={162} r={3} fill="#B9A88F" stroke={INK} strokeWidth={2} />
      <path d="M100 110 v18 M88 118 q6 8 0 16 M112 118 q-6 8 0 16" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* red neckerchief */}
      <path d="M84 106 q16 12 32 0 q2 12 -16 18 q-18 -6 -16 -18 z" fill="#B32330" {...S} strokeWidth={3.5} />
      {/* bloody apron */}
      <path
        d="M80 128 h40 v46 q0 10 -10 10 h-20 q-10 0 -10 -10 z"
        fill="#D9CFC0"
        {...S}
        strokeWidth={3.5}
      />
      <path d="M88 140 q6 2 8 -3 q3 -5 -2 -6 q-6 -1 -7 4 q-1 3 1 5 z" fill="#8E1B2E" stroke="none" />
      <path d="M106 152 q5 3 7 -2 q2 -5 -3 -6 q-5 0 -6 4 q-1 3 2 4 z" fill="#8E1B2E" stroke="none" />
      <path d="M92 164 q4 6 10 2 M104 172 q3 4 -1 6" stroke="#8E1B2E" strokeWidth={3.5} fill="none" strokeLinecap="round" />
      {/* arms */}
      <path d="M62 122 q-16 14 -16 34 q0 12 10 13 q9 1 11 -9 l4 -24 z" fill="#EFE9DC" {...S} strokeWidth={4} />
      <path d="M138 122 q16 14 16 34 q0 12 -10 13 q-9 1 -11 -9 l-4 -24 z" fill="#EFE9DC" {...S} strokeWidth={4} />
      {/* hands */}
      <path d="M56 166 q-5 9 0 15 q6 6 12 0 q6 -5 3 -13 z" fill="#E8D8C8" {...S} strokeWidth={3.5} />
      <path d="M144 166 q5 9 0 15 q-6 6 -12 0 q-6 -5 -3 -13 z" fill="#E8D8C8" {...S} strokeWidth={3.5} />
      {/* ladle */}
      <path d="M146 170 l16 -26" stroke={INK} strokeWidth={5} fill="none" />
      <ellipse cx={166} cy={138} rx={9} ry={7} fill="#B9A88F" {...S} strokeWidth={3} transform="rotate(-20 166 138)" />
      {/* hair + face */}
      <path d="M100 22 q-32 4 -34 36 l0 12 q4 -22 12 -28 q-2 12 4 16 q4 -16 18 -18 q14 2 18 18 q6 -4 4 -16 q8 6 12 28 l0 -12 q-2 -32 -34 -36 z" fill="#1C1428" {...S} strokeWidth={4} />
      <ellipse cx={100} cy={72} rx={31} ry={30} fill="#E8D8C8" {...S} strokeWidth={4} />
      {/* vampire brows + red eyes */}
      <path d="M78 58 l16 6 M122 58 l-16 6" stroke={INK} strokeWidth={3.5} fill="none" />
      <path d="M80 68 q7 -5 13 0 q-7 6 -13 0 z" fill="#C0392B" stroke={INK} strokeWidth={2.5} />
      <path d="M120 68 q-7 -5 -13 0 q7 6 13 0 z" fill="#C0392B" stroke={INK} strokeWidth={2.5} />
      <circle cx={87} cy={68} r={2.2} fill="#FFE9D2" />
      <circle cx={113} cy={68} r={2.2} fill="#FFE9D2" />
      {/* smirk + fangs */}
      <path d="M88 84 q10 8 20 -2 q-6 12 -14 10 q-6 -1 -6 -8 z" fill="#B9786E" stroke={INK} strokeWidth={2.5} />
      <path d="M92 86 l2 6 3 -5 M104 86 l2 5 3 -6" fill="#FFFFFF" stroke={INK} strokeWidth={1.8} />
      {/* widow's peak shading */}
      <path d="M84 52 q16 10 32 0 q-6 -14 -16 -14 q-10 0 -16 14 z" fill="#1C1428" stroke="none" />
      {/* tilted chef toque */}
      <g transform="rotate(-7 100 26)">
        <path
          d="M72 30 q-12 -2 -13 -13 q-1 -11 10 -13 q1 -9 12 -10 q10 -1 13 7 q5 -5 12 -2 q9 4 7 13 q1 10 -9 13 z"
          fill="#FDF8F2"
          {...S}
          strokeWidth={4}
        />
        <path d="M70 28 q30 -8 60 0 l-2 15 q-28 8 -56 0 z" fill="#EDE4D2" {...S} strokeWidth={4} />
        <path d="M82 32 v10 M100 34 v10 M118 32 v10" stroke={INK} strokeWidth={2.2} fill="none" />
      </g>
    </Art>
  );
}

export const halloweenCharacters = [PumpkinHead, HauntedBride, Scarecrow, VampireChef];
