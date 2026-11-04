import type { EventId } from './characters';

export interface EventChallenge {
  id: string;
  title: string;
  detail: string;
  goal: number;
  reward: string;
}

export interface EventDef {
  id: EventId;
  name: string;
  tagline: string;
  window: string;
  windowShort: string;
  story: string;
  colors: { main: string; accent: string; glow: string };
  challenges: EventChallenge[];
  rewards: { icon: 'zombie' | 'furniture' | 'recipe' | 'coin'; label: string; detail: string }[];
  menuNote: string;
}

export const events: EventDef[] = [
  {
    id: 'original',
    name: 'Original Cafe',
    tagline: 'The classic Zombie Cafe experience',
    window: 'Always open',
    windowShort: 'Open all year',
    story:
      'The original Zombie Cafe: warm wood, green zombies, endless queues of hungry customers. The baseline every seasonal theme is compared against.',
    colors: { main: '#8C5A2B', accent: '#E8542F', glow: '#F2B632' },
    challenges: [],
    rewards: [],
    menuNote: 'Wood signboards, warm cream walls, red and teal accents.',
  },
  {
    id: 'halloween',
    name: 'Haunted Cafe',
    tagline: 'The cauldrons are bubbling and the customers never leave',
    window: 'October 15 – November 5',
    windowShort: 'Oct 15 – Nov 5',
    story:
      'When the veil thins, the cafe fills with pumpkins, ghosts and things that clatter in the kitchen. Serve enough Witch Brew and the Vampire Chef himself may apply for the night shift.',
    colors: { main: '#F4801E', accent: '#9BE23B', glow: '#7C3AA6' },
    challenges: [
      { id: 'h1', title: 'Serve 200 Witch Brews', detail: 'Any table, any shift', goal: 200, reward: 'Pumpkin Chair' },
      { id: 'h2', title: 'Cook 40 Graveyard Cakes', detail: 'Oven only — no shortcuts', goal: 40, reward: 'Eyeball Punch recipe' },
      { id: 'h3', title: 'Seat 60 masked customers', detail: 'Costume diners count double', goal: 60, reward: 'Pumpkin Head zombie' },
      { id: 'h4', title: 'Survive 5 midnight rushes', detail: '23:00–01:00 game time', goal: 5, reward: 'Coffin Couch' },
    ],
    rewards: [
      { icon: 'zombie', label: 'Pumpkin Head', detail: 'Rare Line Cook — reach 1,000 event points' },
      { icon: 'furniture', label: 'Haunted Table Set', detail: 'Complete all 4 challenges' },
      { icon: 'recipe', label: 'Eyeball Punch', detail: 'Cook 40 Graveyard Cakes' },
      { icon: 'coin', label: '5,000 Coins + 15 Brains', detail: 'Event milestone reward' },
    ],
    menuNote: 'Pumpkin buttons, spiderweb borders, haunted purple panels.',
  },
  {
    id: 'christmas',
    name: 'Undead Christmas',
    tagline: 'Frost on the windows, bones in the pudding',
    window: 'December 10 – January 5',
    windowShort: 'Dec 10 – Jan 5',
    story:
      'Snow falls on every rooftop raid, the kitchen smells of ginger and grave soil, and Zombie Santa checks his list twice before ordering the soup. Deliver enough presents and the Frozen Elf thaws out.',
    colors: { main: '#B32330', accent: '#E8B84B', glow: '#A8D8E8' },
    challenges: [
      { id: 'c1', title: 'Serve 250 Snowman Stews', detail: 'Hot food only', goal: 250, reward: 'Snow Chair' },
      { id: 'c2', title: 'Deliver 100 present boxes', detail: 'Raid and cafe deliveries both count', goal: 100, reward: 'Zombie Santa' },
      { id: 'c3', title: 'Bake 60 Ginger Ghoul Cookies', detail: 'Batch baking counts fully', goal: 60, reward: 'Candy Cane Cranium recipe' },
      { id: 'c4', title: 'Decorate 10 cafe corners', detail: 'Any holiday decoration', goal: 10, reward: 'Christmas Tree' },
    ],
    rewards: [
      { icon: 'zombie', label: 'Zombie Santa', detail: 'Legendary Manager — 2,000 event points' },
      { icon: 'furniture', label: 'Festive Kitchen Set', detail: 'Complete all 4 challenges' },
      { icon: 'recipe', label: 'Yule Slime Log', detail: 'Bake 60 Ginger Ghoul Cookies' },
      { icon: 'coin', label: '8,000 Coins + 20 Brains', detail: 'Event milestone reward' },
    ],
    menuNote: 'Candy-cane buttons, snow drifts, gold ribbon borders.',
  },
  {
    id: 'easter',
    name: 'Rotten Egg Hunt',
    tagline: 'Pastel colors. Rotten contents.',
    window: 'March 20 – April 15',
    windowShort: 'Mar 20 – Apr 15',
    story:
      'The cafe bursts into pastel color as the Rotten Egg Hunt begins. Hide eggs, paint eggs, serve eggs — and try not to think too hard about what the Chocolate Bunny is made of.',
    colors: { main: '#F5B7C6', accent: '#A8E6C9', glow: '#C9B6E4' },
    challenges: [
      { id: 'e1', title: 'Hide 120 eggs', detail: 'Cafe floor and friend cafes', goal: 120, reward: 'Egg Table' },
      { id: 'e2', title: 'Serve 200 Rotten Egg Omelettes', detail: 'Brunch shift counts double', goal: 200, reward: 'Zombie Chick' },
      { id: 'e3', title: 'Find 30 golden eggs', detail: 'Random drops from customers', goal: 30, reward: 'Bunny Chair' },
      { id: 'e4', title: 'Paint 45 eggs', detail: 'Decorating station minigame', goal: 45, reward: 'Hot Cross Brains recipe' },
    ],
    rewards: [
      { icon: 'zombie', label: 'Chocolate Bunny', detail: 'Rare Bouncer — 1,500 event points' },
      { icon: 'furniture', label: 'Spring Picnic Set', detail: 'Complete all 4 challenges' },
      { icon: 'recipe', label: 'Bunny Tail Truffles', detail: 'Find 30 golden eggs' },
      { icon: 'coin', label: '6,500 Coins + 18 Brains', detail: 'Event milestone reward' },
    ],
    menuNote: 'Pastel buttons, egg borders, spring sky backgrounds.',
  },
];

export const eventById = (id: EventId): EventDef =>
  events.find((e) => e.id === id) ?? events[0];

export function nextOccurrence(windowShort: string, now = new Date()): Date {
  const [startPart] = windowShort.split('–').map((s) => s.trim());
  const months: Record<string, number> = {
    Jan: 0, Feb: 1, Mar: 2, Apr: 3, May: 4, Jun: 5,
    Jul: 6, Aug: 7, Sep: 8, Oct: 9, Nov: 10, Dec: 11,
  };
  const m = startPart.match(/^([A-Za-z]+)\s+(\d+)/);
  if (!m) return now;
  const month = months[m[1].slice(0, 3)] ?? 0;
  const day = Number(m[2]);
  let target = new Date(now.getFullYear(), month, day);
  if (target.getTime() < now.getTime() - 24 * 3600 * 1000) {
    target = new Date(now.getFullYear() + 1, month, day);
  }
  return target;
}
