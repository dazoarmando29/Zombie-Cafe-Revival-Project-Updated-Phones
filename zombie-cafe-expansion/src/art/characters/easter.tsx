import { Art, INK, SKIN, Shadow } from '../kit';

const S = { stroke: INK, strokeLinejoin: 'round' as const, strokeLinecap: 'round' as const };

/* --- 9. Rotten Easter Bunny -------------------------------------- */
export function RottenBunny() {
  return (
    <Art>
      <Shadow rx={52} />
      {/* legs — torn bunny suit */}
      <path d="M80 184 l-2 40 q0 11 12 11 h6 q9 0 9 -11 l0 -38 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      <path d="M118 184 l2 40 q0 11 -11 11 h-6 q-9 0 -9 -11 l1 -38 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      {/* big bunny feet */}
      <path d="M74 232 q-12 2 -14 10 q-1 8 9 9 h28 q7 0 7 -8 l0 -11 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      <path d="M126 232 q12 2 14 10 q1 8 -9 9 h-27 q-7 0 -7 -8 l0 -11 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      <path d="M70 240 q6 4 12 1 M120 241 q6 3 12 -1" stroke={INK} strokeWidth={2.2} fill="none" />
      {/* suit body */}
      <path
        d="M100 108 q-30 0 -38 28 q-6 24 -2 42 q2 12 12 13 l12 1 4 -16 8 13 6 -13 8 13 6 -13 8 13 4 -13 10 14 q10 -1 12 -13 q4 -18 -2 -42 q-8 -28 -38 -28 z"
        fill="#E8C8D4"
        {...S}
        strokeWidth={4}
      />
      {/* torn patch + stitches */}
      <path d="M70 140 l14 4 -2 12 -14 -3 z" fill="#D4A8BC" {...S} strokeWidth={2.5} />
      <path d="M72 146 l12 3 M72 152 l12 3" stroke={INK} strokeWidth={1.8} fill="none" />
      {/* suit center seam + stitches */}
      <path d="M100 118 v54" stroke="#D4A8BC" strokeWidth={3} fill="none" />
      <path d="M100 132 l-3 5 M100 146 l-3 5 M100 160 l-3 5" stroke={INK} strokeWidth={2} fill="none" />
      {/* torn seam with stuffing */}
      <path d="M112 156 q6 -3 9 2 q2 6 -4 7 q-6 1 -7 -4 q0 -3 2 -5 z" fill="#FDF8F2" {...S} strokeWidth={2.5} />
      <path d="M114 160 q4 -2 5 1" stroke="#D4C8B8" strokeWidth={1.8} fill="none" />
      {/* arms */}
      <path d="M62 120 q-18 12 -20 36 q-1 11 9 13 q10 2 12 -8 l8 -26 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      <path d="M138 120 q18 12 20 36 q1 11 -9 13 q-10 2 -12 -8 l-8 -26 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      {/* zombie hands */}
      <path d="M46 164 q-6 8 -2 15 q6 6 13 1 q5 -4 3 -12 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      <path d="M154 164 q6 8 2 15 q-6 6 -13 1 q-5 -4 -3 -12 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      {/* bent carrot */}
      <path d="M150 178 q10 -10 22 -8 q6 2 2 8 q-6 10 -18 12 q-8 1 -6 -6 z" fill="#E87828" {...S} strokeWidth={3} />
      <path d="M156 176 l6 3 M162 172 l5 3" stroke="#C45E1C" strokeWidth={2} fill="none" />
      <path d="M172 166 l6 -8 M176 168 l8 -4 M174 172 l8 0" stroke="#2E7D4F" strokeWidth={3} fill="none" />
      {/* cotton tail peeking at hip */}
      <circle cx={142} cy={184} r={10} fill="#FDF8F2" {...S} strokeWidth={3} />
      <path d="M137 181 q4 -3 6 1 M140 189 q4 -2 6 1" stroke="#D4C8B8" strokeWidth={1.8} fill="none" />
      {/* hood head */}
      <path d="M100 28 q-32 2 -35 36 q-2 28 35 34 q37 -6 35 -34 q-3 -34 -35 -36 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      {/* zombie face in hood opening */}
      <ellipse cx={100} cy={72} rx={26} ry={24} fill={SKIN.green} {...S} strokeWidth={3.5} />
      {/* droopy eyes */}
      <path d="M85 68 q7 -6 13 0 q-7 6 -13 0 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <path d="M115 68 q-7 -6 -13 0 q7 6 13 0 z" fill="#F0EAD8" stroke={INK} strokeWidth={2.5} />
      <circle cx={91} cy={69} r={2.4} fill="#2A1C10" />
      <circle cx={109} cy={69} r={2.4} fill="#2A1C10" />
      <path d="M83 61 q8 -5 15 1 M117 61 q-8 -5 -15 1" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* buck teeth grin */}
      <path d="M90 84 q10 8 20 -1 q-8 12 -16 8 q-4 -2 -4 -7 z" fill="#2A3A22" stroke={INK} strokeWidth={2.5} />
      <path d="M96 87 h4 v6 h-4 z M102 87 h4 v6 h-4 z" fill="#F0EAD8" stroke={INK} strokeWidth={1.8} />
      {/* bunny ears — one torn */}
      <path d="M78 34 q-8 -30 4 -32 q10 -1 12 26 l-2 12 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      <path d="M122 34 q10 -28 20 -22 q6 6 -4 24 l-6 8 z" fill="#E8C8D4" {...S} strokeWidth={4} />
      <path d="M84 24 q-2 -14 2 -16 q4 -1 6 14 z" fill="#F5B7C6" stroke="none" />
      <path d="M130 26 q4 -12 8 -10 q3 4 -2 12 z" fill="#F5B7C6" stroke="none" />
      {/* torn ear stuffing */}
      <path d="M132 30 q6 -4 10 0 q2 4 -2 6 q-6 2 -8 -2 z" fill="#FDF8F2" {...S} strokeWidth={2.5} />
      <path d="M136 32 l4 2 M138 37 l4 -1" stroke={INK} strokeWidth={1.8} fill="none" />
    </Art>
  );
}

/* --- 10. Egghead Zombie ------------------------------------------ */
export function Egghead() {
  return (
    <Art>
      <Shadow rx={48} cy={250} />
      {/* small stubby legs */}
      <path d="M84 196 l-2 34 q0 10 11 10 h6 q8 0 8 -10 l0 -32 z" fill="#5E6E86" {...S} strokeWidth={4} />
      <path d="M116 196 l2 34 q0 10 -10 10 h-6 q-8 0 -8 -10 l1 -32 z" fill="#5E6E86" {...S} strokeWidth={4} />
      <path d="M80 234 q-8 2 -8 8 q0 6 8 6 h20 q6 0 6 -7 l0 -7 z" fill="#3A2E22" {...S} strokeWidth={4} />
      <path d="M120 234 q8 2 8 8 q0 6 -8 6 h-19 q-6 0 -6 -7 l0 -7 z" fill="#3A2E22" {...S} strokeWidth={4} />
      {/* pastel shirt */}
      <path
        d="M100 132 q-24 0 -30 22 q-4 18 0 32 q2 10 10 11 l10 1 2 -12 6 10 6 -10 6 10 2 -12 10 11 q8 -1 10 -11 q4 -14 0 -32 q-6 -22 -30 -22 z"
        fill="#A8E6C9"
        {...S}
        strokeWidth={4}
      />
      {/* shirt buttons + bow tie */}
      <circle cx={100} cy={158} r={3} fill="#F6E58B" stroke={INK} strokeWidth={2} />
      <circle cx={100} cy={174} r={3} fill="#F6E58B" stroke={INK} strokeWidth={2} />
      <path d="M92 138 l8 5 -8 5 z M108 138 l-8 5 8 5 z" fill="#F5B7C6" {...S} strokeWidth={2.5} />
      <circle cx={100} cy={143} r={3.5} fill="#E8A0B4" stroke={INK} strokeWidth={2} />
      {/* arms */}
      <path d="M70 140 q-14 10 -15 28 q-1 9 7 11 q8 1 10 -7 l5 -20 z" fill="#A8E6C9" {...S} strokeWidth={4} />
      <path d="M130 140 q14 10 15 28 q1 9 -7 11 q-8 1 -10 -7 l-5 -20 z" fill="#A8E6C9" {...S} strokeWidth={4} />
      {/* hands */}
      <path d="M58 176 q-5 8 0 13 q6 5 11 -1 q4 -5 2 -11 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      <path d="M142 176 q5 8 0 13 q-6 5 -11 -1 q-4 -5 -2 -11 z" fill={SKIN.green} {...S} strokeWidth={3.5} />
      {/* paintbrush */}
      <path d="M144 180 l20 -22" stroke="#8A5A2E" strokeWidth={5} fill="none" />
      <path d="M164 158 l8 -8 5 5 -8 8 z" fill="#B9A88F" stroke={INK} strokeWidth={2.5} />
      <path d="M170 152 q6 -8 12 -4 q3 6 -4 9 q-6 2 -8 -5 z" fill="#F5B7C6" stroke={INK} strokeWidth={2.5} />
      <path d="M178 160 q1 5 -2 7" stroke="#C9788E" strokeWidth={2.5} fill="none" />
      {/* EGG HEAD */}
      <path
        d="M100 18 q-34 8 -36 52 q-2 40 36 46 q38 -6 36 -46 q-2 -44 -36 -52 z"
        fill="#F6E58B"
        {...S}
        strokeWidth={4}
      />
      {/* pastel stripes */}
      <path d="M68 48 q32 -8 64 2" stroke="#F5B7C6" strokeWidth={6} fill="none" />
      <path d="M66 66 q34 -8 68 2" stroke="#A9D6F0" strokeWidth={6} fill="none" />
      <path d="M70 84 q30 -8 60 2" stroke="#A8E6C9" strokeWidth={6} fill="none" />
      {/* zigzag trim */}
      <path d="M67 56 l6 -5 6 5 6 -5 6 5 6 -5 6 5 6 -5 6 5 6 -5 6 5" stroke="#C9B6E4" strokeWidth={3} fill="none" />
      {/* crack lines */}
      <path d="M112 22 l8 10 -6 8 8 8 -4 8" stroke={INK} strokeWidth={3} fill="none" />
      <path d="M72 92 l8 -6 2 8 8 -4" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* chip missing — zombie face peeking */}
      <path d="M64 72 q-8 6 -4 14 q2 6 8 6 l2 -10 z" fill={SKIN.green} {...S} strokeWidth={3} />
      <circle cx={62} cy={82} r={2} fill="#2A1C10" />
      {/* egg face — sleepy eyes + smile */}
      <path d="M82 62 q8 -6 15 0 q-8 6 -15 0 z" fill="#FDF8F2" stroke={INK} strokeWidth={2.5} />
      <path d="M118 62 q-8 -6 -15 0 q8 6 15 0 z" fill="#FDF8F2" stroke={INK} strokeWidth={2.5} />
      <circle cx={89} cy={63} r={2.6} fill="#2A1C10" />
      <circle cx={111} cy={63} r={2.6} fill="#2A1C10" />
      <path d="M80 55 q9 -4 17 2 M120 55 q-9 -4 -17 2" stroke={INK} strokeWidth={2.5} fill="none" />
      <path d="M90 78 q10 7 20 -1 q-8 10 -16 7 q-4 -2 -4 -6 z" fill="#2A3A22" stroke={INK} strokeWidth={2.5} />
      <path d="M96 80 l1 4 M104 80 l1 4" stroke="#FDF8F2" strokeWidth={2} fill="none" />
      {/* shell fragment cap */}
      <path d="M82 22 q10 -12 26 -10 q12 2 12 10 q0 4 -6 5 q-8 -8 -18 -6 q-9 1 -14 1 z" fill="#F6E58B" {...S} strokeWidth={3} />
      <path d="M92 14 l4 6 5 -5 3 6" stroke={INK} strokeWidth={2.2} fill="none" />
    </Art>
  );
}

/* --- 11. Zombie Chick -------------------------------------------- */
export function ZombieChick() {
  return (
    <Art>
      <Shadow cx={100} cy={250} rx={44} />
      {/* cracked eggshell on head (hat) — sits on top of the head */}
      <g transform="translate(0 52)">
        <path d="M62 52 q-6 -10 2 -14 q8 -3 12 6 z" fill="#F0EAD8" {...S} strokeWidth={3} />
        <path
          d="M100 42 q-32 4 -34 30 q10 6 14 -2 q8 6 12 -2 q8 8 14 -2 q8 8 14 -2 q6 6 12 -2 q4 8 12 2 q-2 -26 -44 -22 z"
          fill="#F0EAD8"
          {...S}
          strokeWidth={4}
        />
        <path d="M80 56 q20 -8 40 0" stroke="#D4C8B8" strokeWidth={3} fill="none" />
        <path d="M92 44 q-4 -14 8 -18 q12 2 8 16 z" fill="#F2C94C" {...S} strokeWidth={3.5} />
      </g>
      <path d="M84 216 l-8 18 q-2 6 4 7 h14 q5 0 5 -6 l-2 -17 z" fill="#E8862E" {...S} strokeWidth={3.5} />
      <path d="M116 216 l8 18 q2 6 -4 7 h-13 q-5 0 -5 -6 l2 -17 z" fill="#E8862E" {...S} strokeWidth={3.5} />
      <path d="M78 238 l-6 6 M84 240 l-2 7 M122 238 l6 6 M116 240 l2 7" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* round chick body */}
      <ellipse cx={100} cy={158} rx={52} ry={48} fill="#F2C94C" {...S} strokeWidth={4} />
      {/* belly fluff shading */}
      <path d="M72 178 q28 16 56 0 q-6 22 -28 26 q-22 -4 -28 -26 z" fill="#E8B23C" stroke="none" />
      {/* stitches on belly */}
      <path d="M84 168 l10 8" stroke={INK} strokeWidth={3} fill="none" />
      <path d="M87 165 l3 6 M91 168 l3 6 M95 171 l3 6" stroke={INK} strokeWidth={2.2} fill="none" />
      {/* droopy wing — bandaged */}
      <path d="M52 132 q-20 8 -22 30 q-1 12 10 14 q10 1 12 -9 l6 -24 z" fill="#F2C94C" {...S} strokeWidth={4} />
      <path d="M148 132 q20 8 22 30 q1 12 -10 14 q-10 1 -12 -9 l-6 -24 z" fill="#F2C94C" {...S} strokeWidth={4} />
      <path d="M142 152 q10 8 20 1 l3 10 q-12 8 -24 0 z" fill="#FDF8F2" {...S} strokeWidth={3} />
      <path d="M146 156 l16 1 M147 163 l15 1" stroke="#D4C8B8" strokeWidth={2} fill="none" />
      {/* wing feather lines */}
      <path d="M44 152 q6 6 6 14 M52 148 q4 8 4 16" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* face — big cartoon eyes, one stitched */}
      <circle cx={84} cy={122} r={13} fill="#FDF8F2" {...S} strokeWidth={3.5} />
      <circle cx={118} cy={122} r={13} fill="#FDF8F2" {...S} strokeWidth={3.5} />
      <circle cx={88} cy={124} r={5.5} fill="#2A1C10" />
      <circle cx={86} cy={122} r={2} fill="#FDF8F2" />
      <circle cx={115} cy={124} r={5.5} fill="#2A1C10" />
      <circle cx={113} cy={122} r={2} fill="#FDF8F2" />
      {/* stitched eyebrow */}
      <path d="M110 108 l14 -2" stroke={INK} strokeWidth={2.5} fill="none" />
      <path d="M112 104 l2 4 M118 103 l2 4 M124 103 l1 4" stroke={INK} strokeWidth={1.8} fill="none" />
      <path d="M72 108 q10 -6 18 0" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* crooked beak */}
      <path d="M92 138 l8 -4 12 2 -6 10 q-8 4 -14 -2 z" fill="#E8862E" {...S} strokeWidth={3.5} />
      <path d="M96 140 l10 1" stroke={INK} strokeWidth={2} fill="none" />
      {/* greenish zombie tinge patches */}
      <path d="M64 168 q8 6 6 14 q-6 2 -9 -4 q-2 -6 3 -10 z" fill="#B8C96E" opacity={0.7} stroke="none" />
      <path d="M128 178 q8 2 8 10 q-8 4 -11 -2 q-2 -6 3 -8 z" fill="#B8C96E" opacity={0.7} stroke="none" />
      {/* tiny painted egg in wing */}
      <ellipse cx={42} cy={172} rx={10} ry={13} fill="#C9B6E4" {...S} strokeWidth={3} />
      <path d="M34 170 q8 -4 16 1" stroke="#F5B7C6" strokeWidth={3} fill="none" />
      <path d="M34 176 q8 -4 16 1" stroke="#A8E6C9" strokeWidth={3} fill="none" />
    </Art>
  );
}

/* --- 12. Chocolate Bunny Zombie ---------------------------------- */
export function ChocolateBunny() {
  return (
    <Art>
      <Shadow rx={50} />
      {/* foil wrapper base */}
      <path d="M62 206 q38 16 76 0 l8 26 q-46 18 -92 0 z" fill="#E8B84B" {...S} strokeWidth={4} />
      <path d="M70 216 q30 12 60 0 M74 228 q26 10 52 0" stroke="#B98A1E" strokeWidth={3} fill="none" />
      <path d="M62 210 l-8 -10 12 2 M138 210 l8 -10 -12 2" fill="#C9954A" stroke={INK} strokeWidth={2.5} />
      {/* legs — chocolate */}
      <path d="M80 182 l-4 32 q-1 10 10 11 h8 q8 0 8 -10 l-1 -30 z" fill="#6B3F1E" {...S} strokeWidth={4} />
      <path d="M120 182 l4 32 q1 10 -9 11 h-8 q-8 0 -8 -10 l2 -30 z" fill="#6B3F1E" {...S} strokeWidth={4} />
      <path d="M84 194 q6 3 4 9 M118 196 q-6 3 -4 9" stroke="#8A5A32" strokeWidth={3} fill="none" />
      {/* body */}
      <path
        d="M100 106 q-30 0 -36 28 q-5 22 -1 42 q2 11 11 12 l11 1 3 -13 7 11 6 -11 7 11 5 -11 7 11 3 -12 9 12 q9 -1 11 -12 q4 -20 -1 -42 q-6 -28 -36 -28 z"
        fill="#6B3F1E"
        {...S}
        strokeWidth={4}
      />
      {/* chocolate sheen + cracks */}
      <path d="M72 118 q26 -10 52 0" stroke="#8A5A32" strokeWidth={5} fill="none" strokeLinecap="round" />
      <path d="M112 132 l8 8 -5 7 7 7" stroke={INK} strokeWidth={2.8} fill="none" />
      <path d="M74 152 l7 5 -3 7" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* melt drips */}
      <path d="M70 128 q4 10 0 18 q-6 -8 -4 -16 z" fill="#8A5A32" stroke={INK} strokeWidth={2.5} />
      <path d="M130 136 q5 12 1 22 q-7 -10 -5 -20 z" fill="#8A5A32" stroke={INK} strokeWidth={2.5} />
      <path d="M100 168 q4 12 1 22 q-6 -10 -4 -20 z" fill="#8A5A32" stroke={INK} strokeWidth={2.5} />
      {/* green goo dripping from crack */}
      <path d="M112 136 q5 3 4 9 q0 6 -4 7 q-5 -1 -4 -7 q1 -6 4 -9 z" fill="#9BE23B" {...S} strokeWidth={2.5} />
      <path d="M112 152 q1 5 -1 8" stroke="#9BE23B" strokeWidth={3} fill="none" strokeLinecap="round" />
      {/* chest foil remnant */}
      <path d="M88 128 l13 3 -1 6 -5 3 -2 -4 -6 1 z" fill="#C9954A" {...S} strokeWidth={2.5} />
      {/* arms */}
      <path d="M64 118 q-18 10 -20 32 q-1 11 9 13 q9 1 11 -8 l7 -24 z" fill="#6B3F1E" {...S} strokeWidth={4} />
      <path d="M136 118 q18 10 20 32 q1 11 -9 13 q-9 1 -11 -8 l-7 -24 z" fill="#6B3F1E" {...S} strokeWidth={4} />
      {/* paw pads */}
      <path d="M50 158 q-4 8 1 13 q6 5 11 -1 q4 -5 1 -11 z" fill="#8A5A32" {...S} strokeWidth={3.5} />
      <path d="M150 158 q4 8 -1 13 q-6 5 -11 -1 q-4 -5 -1 -11 z" fill="#8A5A32" {...S} strokeWidth={3.5} />
      {/* head */}
      <ellipse cx={100} cy={68} rx={36} ry={33} fill="#6B3F1E" {...S} strokeWidth={4} />
      {/* chocolate shine */}
      <path d="M78 46 q20 -10 38 -2" stroke="#8A5A32" strokeWidth={5} fill="none" strokeLinecap="round" />
      {/* candy eyes — one red candy */}
      <circle cx={86} cy={64} r={7} fill="#2A1C10" stroke={INK} strokeWidth={2.5} />
      <circle cx={84} cy={62} r={2.4} fill="#FDF8F2" />
      <circle cx={114} cy={64} r={7} fill="#C0392B" {...S} strokeWidth={2.5} />
      <circle cx={112} cy={62} r={2.2} fill="#FFB8B0" />
      {/* muzzle + nose */}
      <ellipse cx={100} cy={80} rx={14} ry={10} fill="#8A5A32" {...S} strokeWidth={3} />
      <path d="M96 76 q4 -4 8 0 q-2 5 -4 5 q-2 0 -4 -5 z" fill="#2A1C10" stroke={INK} strokeWidth={2} />
      <path d="M100 81 v5 M100 86 q-4 3 -7 0 M100 86 q4 3 7 0" stroke={INK} strokeWidth={2.5} fill="none" />
      {/* whisker dots */}
      <circle cx={88} cy={82} r={1.6} fill={INK} />
      <circle cx={92} cy={85} r={1.6} fill={INK} />
      <circle cx={112} cy={82} r={1.6} fill={INK} />
      <circle cx={108} cy={85} r={1.6} fill={INK} />
      {/* ears — one snapped */}
      <path d="M78 38 q-8 -32 4 -34 q11 -1 13 28 l-3 12 z" fill="#6B3F1E" {...S} strokeWidth={4} />
      <path d="M84 26 q-2 -16 2 -18 q4 -1 6 16 z" fill="#8A5A32" stroke="none" />
      <path d="M122 40 q6 -18 18 -16 q2 12 -6 22 l-8 6 z" fill="#6B3F1E" {...S} strokeWidth={4} />
      {/* snapped ear hollow + drips */}
      <path d="M128 30 q8 -4 10 2 q-4 4 -9 3 z" fill="#3A2410" stroke={INK} strokeWidth={2.5} />
      <path d="M130 34 q2 6 -1 10 M135 32 q2 6 0 9" stroke="#8A5A32" strokeWidth={3} fill="none" strokeLinecap="round" />
      {/* bite on ear */}
      <path d="M84 12 q6 -3 8 2 q1 4 -3 5 q-5 1 -6 -3 q-1 -3 1 -4 z" fill="#FDF8F2" stroke={INK} strokeWidth={2.5} />
    </Art>
  );
}

export const easterCharacters = [RottenBunny, Egghead, ZombieChick, ChocolateBunny];
