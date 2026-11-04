import { useEffect, useState } from 'react';
import { nextOccurrence } from '../data/events';

function diff(target: Date, now: Date) {
  const ms = Math.max(0, target.getTime() - now.getTime());
  const days = Math.floor(ms / 86400000);
  const hours = Math.floor((ms % 86400000) / 3600000);
  const mins = Math.floor((ms % 3600000) / 60000);
  const secs = Math.floor((ms % 60000) / 1000);
  return { days, hours, mins, secs, live: ms > 0 };
}

export function Countdown({ windowShort, label = 'Event starts in' }: { windowShort: string; label?: string }) {
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const t = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(t);
  }, []);
  const target = nextOccurrence(windowShort, now);
  const d = diff(target, now);
  if (windowShort.includes('Always')) {
    return (
      <div className="countdown">
        <div className="countdown-label">Open 24/7</div>
        <div className="countdown-boxes">
          <span className="cd-box">ALWAYS</span>
          <span className="cd-box">OPEN</span>
        </div>
      </div>
    );
  }
  return (
    <div className="countdown">
      <div className="countdown-label">{d.live ? label : 'Event is live!'}</div>
      <div className="countdown-boxes">
        <span className="cd-box"><b>{d.days}</b><i>days</i></span>
        <span className="cd-sep">:</span>
        <span className="cd-box"><b>{String(d.hours).padStart(2, '0')}</b><i>hrs</i></span>
        <span className="cd-sep">:</span>
        <span className="cd-box"><b>{String(d.mins).padStart(2, '0')}</b><i>min</i></span>
        <span className="cd-sep">:</span>
        <span className="cd-box"><b>{String(d.secs).padStart(2, '0')}</b><i>sec</i></span>
      </div>
    </div>
  );
}
