import { useState } from 'react';
import type { CharacterDef, EventId } from './data/characters';
import { eventById } from './data/events';
import { EventSelector } from './components/EventSelector';
import { CharacterModal } from './components/CharacterCard';
import {
  views,
  type ViewId,
  OverviewView,
  PediaView,
  CollectionView,
  ShopView,
  RecipesView,
  MenusView,
  CompareView,
} from './pages/Views';
import './styles/theme.css';
import './styles/menus.css';

export default function App() {
  const [event, setEvent] = useState<EventId>('halloween');
  const [view, setView] = useState<ViewId>('overview');
  const [selected, setSelected] = useState<CharacterDef | null>(null);
  const ev = eventById(event);

  return (
    <div className="app" data-event={event}>
      <header style={{ width: 'min(1180px, 100%)', margin: '0 auto', padding: '14px 16px 0' }}>
        <div className="row spread wrap" style={{ marginBottom: 10 }}>
          <div className="row">
            <LogoMark />
            <div>
              <div style={{ fontFamily: 'var(--font-display)', fontSize: 24, color: '#fff6e3', textShadow: '0 2px 0 rgba(0,0,0,.45)', lineHeight: 1.1 }}>
                Zombie Cafe — Seasonal Expansion
              </div>
              <div style={{ fontSize: 12, fontWeight: 700, color: 'rgba(255,255,255,.85)', textShadow: '0 1px 2px rgba(0,0,0,.4)' }}>
                Visual design project &amp; interactive prototype
              </div>
            </div>
          </div>
        </div>
        <EventSelector active={event} onSelect={(id) => setEvent(id)} />
        <div className="tab-row" style={{ marginBottom: 4 }}>
          {views.map((v) => (
            <button key={v.id} className={`pill ${view === v.id ? 'active' : ''}`} onClick={() => setView(v.id)}>
              {v.label}
            </button>
          ))}
        </div>
      </header>
      <main className="app-main">
        {view === 'overview' && <OverviewView ev={ev} />}
        {view === 'pedia' && <PediaView ev={ev} onOpen={setSelected} />}
        {view === 'collection' && <CollectionView onOpen={setSelected} />}
        {view === 'shop' && <ShopView ev={ev} />}
        {view === 'recipes' && <RecipesView ev={ev} />}
        {view === 'menus' && <MenusView ev={ev} />}
        {view === 'compare' && <CompareView ev={ev} />}
      </main>
      <footer className="footer-note">
        Zombie Cafe Seasonal Expansion — original fan design concept artwork. Not affiliated with Beeline Interactive or Capcom.
        <br />
        All character, furniture and recipe artwork is hand-built SVG source (editable) — see <code>src/art/</code>.
      </footer>
      <CharacterModal char={selected} onClose={() => setSelected(null)} />
    </div>
  );
}

function LogoMark() {
  return (
    <svg width="54" height="54" viewBox="0 0 64 64" xmlns="http://www.w3.org/2000/svg">
      <circle cx="32" cy="32" r="29" fill="#F2B632" stroke="#2A1C10" strokeWidth="4" />
      <path d="M20 34 q2 -16 12 -18 q12 2 12 18 q0 12 -12 16 q-12 -4 -12 -16 z" fill="#A8C970" stroke="#2A1C10" strokeWidth="3" />
      <circle cx="27" cy="32" r="3.4" fill="#2A1C10" />
      <circle cx="38" cy="32" r="3.4" fill="#2A1C10" />
      <path d="M25 41 q7 5 14 0 q-6 7 -14 0 z" fill="#2A1C10" />
      <path d="M28 42 l1 4 2 -4 M35 42 l1 4 2 -4" fill="#FDF8F2" />
    </svg>
  );
}
