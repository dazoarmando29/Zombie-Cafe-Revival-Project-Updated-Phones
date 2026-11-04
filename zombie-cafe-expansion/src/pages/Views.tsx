import { useState } from 'react';
import type { CharacterDef, EventId } from '../data/characters';
import { characters, rarityLabel } from '../data/characters';
import type { EventDef } from '../data/events';
import { items, categoryLabel } from '../data/items';
import { recipes, courseLabel, type RecipeDef } from '../data/recipes';
import { characterArt } from '../art/characters';
import { itemArt } from '../art/items';
import { CoinIcon, BrainIcon, StarIcon, ChefHatIcon, PumpkinIcon, SnowflakeIcon, EggIcon, GhostIcon } from '../art/ui';
import { CharacterCard } from '../components/CharacterCard';
import { Countdown } from '../components/Countdown';
import { MenuMock, menuKinds, type MenuKind } from '../components/MenuMock';

export type ViewId = 'overview' | 'pedia' | 'collection' | 'shop' | 'recipes' | 'menus' | 'compare';

export const views: { id: ViewId; label: string }[] = [
  { id: 'overview', label: 'Event Hub' },
  { id: 'pedia', label: 'Zombiepedia' },
  { id: 'collection', label: 'All Zombies' },
  { id: 'shop', label: 'Event Shop' },
  { id: 'recipes', label: 'Recipes' },
  { id: 'menus', label: 'Menu Preview' },
  { id: 'compare', label: 'Design Compare' },
];

const rewardGlyph = (kind: string) => {
  if (kind === 'zombie') return <StarIcon size={22} />;
  if (kind === 'furniture') return <ChefHatIcon size={22} />;
  if (kind === 'recipe') return <EggIcon size={22} />;
  return <CoinIcon size={22} />;
};

function Hero({ ev }: { ev: EventDef }) {
  const starId =
    ev.id === 'halloween' ? 'vampire-chef' : ev.id === 'christmas' ? 'zombie-santa' : ev.id === 'easter' ? 'chocolate-bunny' : 'zombie-chick';
  const Star = characterArt[starId];
  return (
    <div className="hero">
      <div className="zc-panel rivets">
        <div className="panel-head">
          {ev.id === 'halloween' ? <PumpkinIcon size={26} /> : ev.id === 'christmas' ? <SnowflakeIcon size={26} /> : ev.id === 'easter' ? <EggIcon size={26} /> : <ChefHatIcon size={26} />}
          {ev.name}
        </div>
        <p style={{ marginTop: 0, fontWeight: 600, lineHeight: 1.55 }}>{ev.story}</p>
        <div className="row wrap" style={{ marginBottom: 12 }}>
          <span className="tag hot">{ev.window}</span>
          <span className="tag">{ev.tagline}</span>
        </div>
        <Countdown windowShort={ev.windowShort} />
      </div>
      <div className="zc-panel hero-art" style={{ padding: 10 }}>
        <Star />
      </div>
    </div>
  );
}

export function OverviewView({ ev }: { ev: EventDef }) {
  return (
    <div className="screen stack">
      <h2 className="sign-title">{ev.name}</h2>
      <p className="sign-sub">{ev.tagline} — {ev.menuNote}</p>
      <Hero ev={ev} />
      {ev.challenges.length > 0 && (
        <div className="zc-panel rivets">
          <div className="panel-head"><GhostIcon size={22} /> Event Challenges</div>
          <div className="stack">
            {ev.challenges.map((c, i) => (
              <div className="challenge" key={c.id}>
                <div className="ch-goal">{c.goal}</div>
                <div className="ch-body">
                  <strong>{c.title}</strong>
                  <small>{c.detail} · Reward: {c.reward}</small>
                </div>
                <div style={{ width: 130 }}>
                  <div className="progress"><span style={{ width: `${[68, 42, 25, 12][i % 4]}%` }} /></div>
                </div>
              </div>
            ))}
          </div>
          <div className="divider-wood" />
          <div className="panel-head" style={{ fontSize: 17 }}>Reward Preview</div>
          <div className="stack">
            {ev.rewards.map((r) => (
              <div className="reward-row" key={r.label}>
                <div className="reward-icon">{rewardGlyph(r.icon)}</div>
                <div>
                  <strong style={{ display: 'block' }}>{r.label}</strong>
                  <small style={{ opacity: 0.85, fontWeight: 600 }}>{r.detail}</small>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
      {ev.challenges.length === 0 && (
        <div className="zc-panel rivets">
          <div className="panel-head"><ChefHatIcon size={22} /> The Baseline</div>
          <p style={{ fontWeight: 600, lineHeight: 1.55 }}>
            The Original Cafe is the reference design: warm wood signboards, cream walls, chunky gold buttons
            and green zombie staff. Switch seasons above to see every screen re-skinned for Halloween,
            Christmas or Easter while keeping the classic Zombie Cafe layout.
          </p>
          <div className="row wrap">
            <span className="tag hot">Reference UI</span>
            <span className="tag">Wood + cream palette</span>
            <span className="tag">Gold chunky buttons</span>
          </div>
        </div>
      )}
    </div>
  );
}

export function PediaView({ ev, onOpen }: { ev: EventDef; onOpen: (c: CharacterDef) => void }) {
  const chars = characters.filter((c) => c.event === ev.id);
  const visible = chars.length > 0 ? chars : characters.slice(0, 4);
  return (
    <div className="screen">
      <h2 className="sign-title">{ev.name} Zombiepedia</h2>
      <p className="sign-sub">Collection progress · locked entries shown dimmed</p>
      <div className="zc-panel rivets">
        <div className="row spread" style={{ marginBottom: 8 }}>
          <strong>{Math.min(2, visible.length)} / {visible.length} collected</strong>
          <span className="tag hot">{rarityLabel[visible[visible.length - 1].rarity]} at 4/4</span>
        </div>
        <div className="progress" style={{ marginBottom: 16 }}>
          <span style={{ width: `${(Math.min(2, visible.length) / visible.length) * 100}%` }} />
        </div>
        <div className="grid cols-4">
          {visible.map((c, i) => (
            <CharacterCard key={c.id} char={c} locked={i >= 2} onOpen={onOpen} />
          ))}
        </div>
      </div>
    </div>
  );
}

export function CollectionView({ onOpen }: { onOpen: (c: CharacterDef) => void }) {
  const groups: Exclude<EventId, 'original'>[] = ['halloween', 'christmas', 'easter'];
  return (
    <div className="screen">
      <h2 className="sign-title">Seasonal Zombie Collection</h2>
      <p className="sign-sub">All 12 new staff — click any character for full details</p>
      {groups.map((g) => (
        <div className="zc-panel rivets" key={g} style={{ marginBottom: 18 }} data-event={g}>
          <div className="panel-head">
            {g === 'halloween' ? <PumpkinIcon size={22} /> : g === 'christmas' ? <SnowflakeIcon size={22} /> : <EggIcon size={22} />}
            {g === 'halloween' ? 'Halloween Zombies' : g === 'christmas' ? 'Christmas Zombies' : 'Easter Zombies'}
          </div>
          <div className="grid cols-4">
            {characters
              .filter((c) => c.event === g)
              .map((c) => (
                <CharacterCard key={c.id} char={c} onOpen={onOpen} />
              ))}
          </div>
        </div>
      ))}
    </div>
  );
}

function ItemCard({ id, name, desc, price, currency, availability }: {
  id: string; name: string; desc: string; price: number; currency: string; availability: string;
}) {
  const Art = itemArt[id];
  return (
    <div className="zc-card item-card">
      <div className="card-art"><Art /></div>
      <div className="card-body">
        <div className="row spread">
          <h3>{name}</h3>
          <span className="price">
            {currency === 'brains' ? <BrainIcon size={14} /> : <span className="coin" />}
            {price.toLocaleString()}
          </span>
        </div>
        <p>{desc}</p>
        <span className="tag hot">{availability}</span>
      </div>
    </div>
  );
}

export function ShopView({ ev }: { ev: EventDef }) {
  const [cat, setCat] = useState<string>('all');
  const cats = ['all', 'furniture', 'decoration', 'equipment', 'wall'] as const;
  const evItems = items.filter((i) => i.event === ev.id);
  const shown = cat === 'all' ? evItems : evItems.filter((i) => i.category === cat);
  const evChars = characters.filter((c) => c.event === ev.id);
  return (
    <div className="screen">
      <h2 className="sign-title">{ev.name} Event Shop</h2>
      <p className="sign-sub">Exclusive staff, furniture, decorations and kitchen equipment</p>
      <div className="tab-row">
        {cats.map((c) => (
          <button key={c} className={`pill ${cat === c ? 'active' : ''}`} onClick={() => setCat(c)}>
            {c === 'all' ? 'All Items' : categoryLabel[c as keyof typeof categoryLabel]}
          </button>
        ))}
      </div>
      {(cat === 'all' || cat === 'zombie') && (
        <div className="zc-panel rivets" style={{ marginBottom: 18 }}>
          <div className="panel-head"><StarIcon size={20} /> Seasonal Staff</div>
          <div className="grid cols-4">
            {evChars.map((c) => (
              <CharacterCard key={c.id} char={c} onOpen={() => undefined} />
            ))}
          </div>
        </div>
      )}
      <div className="grid cols-3">
        {shown.map((i) => (
          <ItemCard key={i.id} id={i.id} name={i.name} desc={i.description} price={i.price}
            currency={i.currency} availability={i.availability} />
        ))}
      </div>
    </div>
  );
}

function RecipeCard({ r }: { r: RecipeDef }) {
  const Art = itemArt[r.id];
  return (
    <div className="zc-card recipe-card">
      <div className="card-art"><Art /></div>
      <div className="card-body">
        <div className="row spread">
          <h3>{r.name}</h3>
          <span className="price"><span className="coin" />{r.price}</span>
        </div>
        <div className="recipe-meta">
          <span className="tag">{courseLabel[r.course]}</span>
          <span className="tag">{r.cookTime}</span>
        </div>
        <p>{r.description}</p>
        <div className="recipe-meta">
          {r.ingredients.map((ing) => (
            <span className="ingredient" key={ing}>{ing}</span>
          ))}
        </div>
      </div>
    </div>
  );
}

export function RecipesView({ ev }: { ev: EventDef }) {
  const shown = recipes.filter((r) => r.event === ev.id);
  return (
    <div className="screen">
      <h2 className="sign-title">{ev.name} Recipes</h2>
      <p className="sign-sub">Four seasonal dishes — illustrated, priced and ready to cook</p>
      <div className="grid cols-2">
        {shown.map((r) => (
          <RecipeCard key={r.id} r={r} />
        ))}
      </div>
    </div>
  );
}

export function MenusView({ ev }: { ev: EventDef }) {
  return (
    <div className="screen">
      <h2 className="sign-title">Seasonal Menu Redesign</h2>
      <p className="sign-sub">Every game screen re-skinned for {ev.name} — same layout, new identity</p>
      <div className="menu-preview-grid">
        {menuKinds.map((m) => (
          <div className="menu-preview-cell" key={m.id}>
            <div className="phone-frame">
              <MenuMock kind={m.id} event={ev.id} />
            </div>
            <h3>{m.label}</h3>
            <p className="hint">{m.hint}</p>
          </div>
        ))}
      </div>
    </div>
  );
}

export function CompareView({ ev }: { ev: EventDef }) {
  const [kind, setKind] = useState<MenuKind>('main');
  const [side, setSide] = useState<Exclude<EventId, 'original'>>(
    ev.id === 'original' ? 'halloween' : (ev.id as Exclude<EventId, 'original'>),
  );
  return (
    <div className="screen">
      <h2 className="sign-title">Design Comparison</h2>
      <p className="sign-sub">Original Cafe vs seasonal version — same screen, side by side</p>
      <div className="tab-row">
        {menuKinds.map((m) => (
          <button key={m.id} className={`pill ${kind === m.id ? 'active' : ''}`} onClick={() => setKind(m.id)}>
            {m.label}
          </button>
        ))}
      </div>
      <div className="tab-row">
        {(['halloween', 'christmas', 'easter'] as const).map((s) => (
          <button key={s} className={`pill ${side === s ? 'active' : ''}`} onClick={() => setSide(s)}>
            vs {s[0].toUpperCase() + s.slice(1)}
          </button>
        ))}
      </div>
      <div className="compare-grid">
        <div className="compare-col">
          <h3>Original Cafe</h3>
          <div className="phone-frame">
            <MenuMock kind={kind} event="original" />
          </div>
        </div>
        <div className="compare-col" data-event={side}>
          <h3>{side === 'halloween' ? 'Halloween' : side === 'christmas' ? 'Christmas' : 'Easter'} Version</h3>
          <div className="phone-frame">
            <MenuMock kind={kind} event={side} />
          </div>
        </div>
      </div>
    </div>
  );
}
