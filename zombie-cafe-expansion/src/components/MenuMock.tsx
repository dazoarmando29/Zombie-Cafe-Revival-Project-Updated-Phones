import type { ReactElement } from 'react';
import type { EventId } from '../data/characters';
import { characterArt } from '../art/characters';
import { itemArt } from '../art/items';
import { CoinIcon, BrainIcon, PumpkinIcon, SnowflakeIcon, EggIcon, GhostIcon, StarIcon, LockIcon } from '../art/ui';

export type MenuKind =
  | 'main'
  | 'pedia'
  | 'zombie-shop'
  | 'furniture-shop'
  | 'decoration-shop'
  | 'recipes'
  | 'event-select'
  | 'reward';

export const menuKinds: { id: MenuKind; label: string; hint: string }[] = [
  { id: 'main', label: 'Main Menu', hint: 'Home screen with play + navigation' },
  { id: 'pedia', label: 'Zombiepedia', hint: 'Seasonal collection page' },
  { id: 'zombie-shop', label: 'Zombie Shop', hint: 'Hire seasonal staff' },
  { id: 'furniture-shop', label: 'Furniture Shop', hint: 'Tables, chairs, equipment' },
  { id: 'decoration-shop', label: 'Decoration Shop', hint: 'Wallpaper + decor' },
  { id: 'recipes', label: 'Recipe Selection', hint: 'Seasonal menu of dishes' },
  { id: 'event-select', label: 'Event Selection', hint: 'Switch active season' },
  { id: 'reward', label: 'Reward Screen', hint: 'Claim event prizes' },
];

const starByEvent: Record<EventId, string> = {
  original: 'zombie-chick',
  halloween: 'vampire-chef',
  christmas: 'zombie-santa',
  easter: 'chocolate-bunny',
};

const titleByEvent: Record<EventId, string> = {
  original: 'ZOMBIE CAFE',
  halloween: 'HAUNTED CAFE',
  christmas: 'UNDEAD CHRISTMAS',
  easter: 'ROTTEN EGG HUNT',
};

function Decor({ event }: { event: EventId }) {
  const spots = [
    { top: 6, left: 4, s: 26 },
    { top: 30, left: 88, s: 20 },
    { top: 62, left: 6, s: 22 },
    { top: 84, left: 86, s: 24 },
    { top: 46, left: 92, s: 18 },
    { top: 12, left: 70, s: 16 },
  ];
  let icon: ReactElement;
  if (event === 'halloween') icon = <GhostIcon size={24} />;
  else if (event === 'christmas') icon = <SnowflakeIcon size={24} />;
  else if (event === 'easter') icon = <EggIcon size={24} />;
  else icon = <StarIcon size={22} />;
  return (
    <div className="mock-decor" aria-hidden>
      {spots.map((p, i) => (
        <span key={i} style={{ top: `${p.top}%`, left: `${p.left}%`, width: p.s, height: p.s }}>
          {icon}
        </span>
      ))}
    </div>
  );
}

function MiniBtn({ children, wide = false, gold = false }: { children: ReactElement | string; wide?: boolean; gold?: boolean }) {
  return <span className={`mock-btn ${wide ? 'wide' : ''} ${gold ? 'gold' : ''}`}>{children}</span>;
}

function CoinTag({ v }: { v: string }) {
  return (
    <span className="mock-price">
      <CoinIcon size={13} /> {v}
    </span>
  );
}

function Body({ kind, event }: { kind: MenuKind; event: EventId }) {
  const Star = characterArt[starByEvent[event]];

  if (kind === 'main') {
    return (
      <div className="mock-main">
        <div className="mock-btn mock-play">PLAY</div>
        <div className="mock-btn-grid">
          <MiniBtn>Zombiepedia</MiniBtn>
          <MiniBtn>Shop</MiniBtn>
          <MiniBtn>Recipes</MiniBtn>
          <MiniBtn>Events</MiniBtn>
        </div>
        <div className="mock-star">
          <Star />
        </div>
      </div>
    );
  }

  if (kind === 'pedia') {
    const ids =
      event === 'halloween'
        ? ['pumpkin-head', 'haunted-bride', 'scarecrow', 'vampire-chef']
        : event === 'christmas'
          ? ['zombie-santa', 'frozen-elf', 'gingerbread', 'reindeer-zombie']
          : event === 'easter'
            ? ['rotten-bunny', 'egghead', 'zombie-chick', 'chocolate-bunny']
            : ['zombie-chick', 'pumpkin-head', 'gingerbread', 'egghead'];
    return (
      <div className="mock-pedia">
        <div className="mock-progress">
          <span style={{ width: '50%' }} />
        </div>
        <div className="mock-sub">2 / 4 collected</div>
        <div className="mock-portrait-grid">
          {ids.map((id, i) => {
            const C = characterArt[id];
            return (
              <div key={id} className={`mock-portrait ${i > 1 ? 'locked' : ''}`}>
                <div className="portrait">
                  <C />
                </div>
                {i > 1 ? <LockIcon size={13} /> : <StarIcon size={13} />}
              </div>
            );
          })}
        </div>
        <MiniBtn wide>View Details</MiniBtn>
      </div>
    );
  }

  if (kind === 'zombie-shop') {
    const ids =
      event === 'christmas'
        ? ['zombie-santa', 'frozen-elf']
        : event === 'easter'
          ? ['rotten-bunny', 'chocolate-bunny']
          : ['pumpkin-head', 'vampire-chef'];
    return (
      <div className="mock-shop">
        {ids.map((id, i) => {
          const C = characterArt[id];
          return (
            <div key={id} className="mock-shop-card">
              <C />
              <span className="mock-shop-name">{id.replace(/-/g, ' ')}</span>
              <CoinTag v={i === 0 ? '4,800' : '60 brains'} />
            </div>
          );
        })}
      </div>
    );
  }

  if (kind === 'furniture-shop' || kind === 'decoration-shop') {
    const ids =
      kind === 'furniture-shop'
        ? event === 'christmas'
          ? ['holiday-table', 'snow-chair', 'festive-stove', 'christmas-tree']
          : event === 'easter'
            ? ['egg-table', 'bunny-chair', 'egg-stove', 'easter-basket']
            : ['haunted-table', 'pumpkin-chair', 'spooky-stove', 'coffin-couch']
        : event === 'christmas'
          ? ['christmas-tree', 'present-stack', 'snowy-wallpaper', 'snow-chair']
          : event === 'easter'
            ? ['easter-basket', 'spring-fountain', 'spring-wallpaper', 'egg-table']
            : ['ghost-chandelier', 'haunted-wallpaper', 'coffin-couch', 'pumpkin-chair'];
    return (
      <div className="mock-shop four">
        {ids.map((id, i) => {
          const C = itemArt[id];
          return (
            <div key={id} className="mock-shop-card">
              <C />
              <CoinTag v={`${[2400, 1200, 4800, 3200][i].toLocaleString()}`} />
            </div>
          );
        })}
      </div>
    );
  }

  if (kind === 'recipes') {
    const ids =
      event === 'christmas'
        ? ['snowman-stew', 'ginger-ghoul-cookies', 'candy-cane-cranium', 'yule-slime-log']
        : event === 'easter'
          ? ['rotten-egg-omelette', 'bunny-tail-truffles', 'jelly-bean-stew', 'hot-cross-brains']
          : ['witch-brew-stew', 'severed-finger-fries', 'eyeball-punch', 'graveyard-cake'];
    return (
      <div className="mock-shop four">
        {ids.map((id, i) => {
          const C = itemArt[id];
          return (
            <div key={id} className="mock-shop-card">
              <C />
              <CoinTag v={`${[320, 180, 150, 420][i]}`} />
            </div>
          );
        })}
      </div>
    );
  }

  if (kind === 'event-select') {
    return (
      <div className="mock-events">
        {[
          { id: 'halloween' as const, n: 'Haunted Cafe', d: 'Oct 15 – Nov 5' },
          { id: 'christmas' as const, n: 'Undead Christmas', d: 'Dec 10 – Jan 5' },
          { id: 'easter' as const, n: 'Rotten Egg Hunt', d: 'Mar 20 – Apr 15' },
        ].map((e) => (
          <div key={e.id} className={`mock-event-card ${e.id === event ? 'live' : ''}`} data-event={e.id}>
            <span className="mock-event-dot" />
            <div>
              <strong>{e.n}</strong>
              <small>{e.d}</small>
            </div>
            {e.id === event ? <span className="mock-live-tag">ACTIVE</span> : <MiniBtn>Enter</MiniBtn>}
          </div>
        ))}
      </div>
    );
  }

  /* reward */
  return (
    <div className="mock-reward">
      <div className="mock-chest">
        <svg viewBox="0 0 120 100" width="150" height="120">
          <ellipse cx="60" cy="92" rx="40" ry="6" fill="rgba(20,10,4,0.3)" />
          <path d="M24 44 q36 -22 72 0 l0 38 q-36 12 -72 0 z" fill="#8A5A2E" stroke="#2A1C10" strokeWidth="4" strokeLinejoin="round" />
          <path d="M24 44 q36 -22 72 0 q-36 -8 -72 0 z" fill="#B97C3E" stroke="#2A1C10" strokeWidth="4" strokeLinejoin="round" />
          <rect x="50" y="42" width="20" height="18" rx="3" fill="#E8B84B" stroke="#2A1C10" strokeWidth="3" />
          <circle cx="60" cy="52" r="3.4" fill="#2A1C10" />
          <path d="M24 58 h72" stroke="#2A1C10" strokeWidth="3" />
        </svg>
      </div>
      <div className="mock-reward-chips">
        <span className="mock-chip"><StarIcon size={14} /> Legendary Zombie</span>
        <span className="mock-chip"><CoinIcon size={13} /> 5,000 Coins</span>
        <span className="mock-chip"><BrainIcon size={13} /> 15 Brains</span>
      </div>
      <div className="mock-btn mock-play small">CLAIM</div>
    </div>
  );
}

export function MenuMock({ kind, event }: { kind: MenuKind; event: EventId }) {
  return (
    <div className="menu-mock" data-event={event}>
      <Decor event={event} />
      <div className="mock-status">
        <span>9:41</span>
        <span className="mock-status-notch" />
        <span>ZC-4G · 87%</span>
      </div>
      <div className="mock-currency">
        <span className="mock-pill"><CoinIcon size={13} /> 12,480</span>
        <span className="mock-pill"><BrainIcon size={13} /> 63</span>
        <span className="mock-pill">LV 24</span>
      </div>
      <div className="mock-title-bar">
        <div className="mock-title">{titleByEvent[event]}</div>
        <div className="mock-subtitle">
          {kind === 'main' ? 'Serve the undead. Feed the living.' : menuKinds.find((m) => m.id === kind)?.hint}
        </div>
      </div>
      <div className="mock-body">
        <Body kind={kind} event={event} />
      </div>
      <div className="mock-tabs">
        <span className="on"><ChefGlyph /> Home</span>
        <span><PumpkinIcon size={15} /> Menu</span>
        <span><CoinIcon size={14} /> Shop</span>
        <span><StarIcon size={13} /> Me</span>
      </div>
    </div>
  );
}

function ChefGlyph() {
  return (
    <svg width="15" height="15" viewBox="0 0 20 20">
      <path d="M6 12 q-4 0 -4 -4 q0 -4 4 -4 q1 -4 4 -4 q3 0 4 4 q4 0 4 4 q0 4 -4 4 z" fill="#FDF8F2" stroke="#2A1C10" strokeWidth="1.6" />
      <path d="M6 12 v3 q4 2 8 0 v-3" fill="#EDE4D2" stroke="#2A1C10" strokeWidth="1.6" />
    </svg>
  );
}
