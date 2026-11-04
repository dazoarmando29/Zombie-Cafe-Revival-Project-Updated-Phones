import type { ReactElement } from 'react';
import type { EventId } from '../data/characters';
import { events } from '../data/events';
import { PumpkinIcon, SnowflakeIcon, EggIcon, ChefHatIcon } from '../art/ui';

const eventIcon: Record<EventId, ReactElement> = {
  original: <ChefHatIcon size={22} />,
  halloween: <PumpkinIcon size={22} />,
  christmas: <SnowflakeIcon size={22} />,
  easter: <EggIcon size={22} />,
};

export function EventSelector({
  active,
  onSelect,
}: {
  active: EventId;
  onSelect: (id: EventId) => void;
}) {
  return (
    <div className="event-selector">
      {events.map((e) => (
        <button
          key={e.id}
          className={`event-pick ${active === e.id ? 'active' : ''}`}
          data-event={e.id}
          onClick={() => onSelect(e.id)}
        >
          <span className="event-pick-icon">{eventIcon[e.id]}</span>
          <span className="event-pick-text">
            <strong>{e.name}</strong>
            <small>{e.windowShort}</small>
          </span>
        </button>
      ))}
    </div>
  );
}
