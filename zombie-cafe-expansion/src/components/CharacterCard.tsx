import { useState } from 'react';
import type { CharacterDef } from '../data/characters';
import { rarityLabel } from '../data/characters';
import { characterArt } from '../art/characters';
import { LockIcon } from '../art/ui';

export function CharacterCard({
  char,
  locked = false,
  onOpen,
}: {
  char: CharacterDef;
  locked?: boolean;
  onOpen: (c: CharacterDef) => void;
}) {
  const Art = characterArt[char.id];
  return (
    <button className={`zc-card char-card ${locked ? 'is-locked' : ''}`} onClick={() => onOpen(char)}>
      <div className="card-art">
        <div className={`char-art-wrap ${locked ? 'locked-art' : ''}`}>
          <Art />
        </div>
        {locked && (
          <span className="lock-badge">
            <LockIcon size={16} /> Locked
          </span>
        )}
      </div>
      <div className="card-body">
        <span className={`rarity ${char.rarity}`}>{rarityLabel[char.rarity]}</span>
        <h3>{char.name}</h3>
        <p>{char.role}</p>
      </div>
    </button>
  );
}

export function CharacterModal({
  char,
  onClose,
}: {
  char: CharacterDef | null;
  onClose: () => void;
}) {
  const [unlocked, setUnlocked] = useState(true);
  if (!char) return null;
  const Art = characterArt[char.id];
  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal zc-panel rivets" data-event={char.event} onClick={(e) => e.stopPropagation()}>
        <div className="panel-head">
          <span>{char.name}</span>
          <button className="zc-btn small danger" style={{ marginLeft: 'auto' }} onClick={onClose}>
            Close
          </button>
        </div>
        <div className="modal-grid">
          <div className={`modal-art ${unlocked ? '' : 'locked-art'}`}>
            <Art />
          </div>
          <div className="stack">
            <div className="row wrap">
              <span className={`rarity ${char.rarity}`}>{rarityLabel[char.rarity]}</span>
              <span className="tag">{char.event} event</span>
              <span className="tag">{char.role}</span>
            </div>
            <p style={{ margin: 0, fontWeight: 600, lineHeight: 1.5 }}>{char.description}</p>
            <p style={{ margin: 0, fontStyle: 'italic', opacity: 0.85 }}>{char.flavor}</p>
            <div className="row spread">
              <span className="tag">Height: {char.height}</span>
              <button className="zc-btn small" onClick={() => setUnlocked((u) => !u)}>
                {unlocked ? 'Preview locked' : 'Preview unlocked'}
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
