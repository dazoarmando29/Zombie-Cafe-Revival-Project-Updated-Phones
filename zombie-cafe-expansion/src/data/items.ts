import type { EventId } from './characters';

export interface ItemDef {
  id: string;
  name: string;
  event: Exclude<EventId, 'original'>;
  category: 'zombie' | 'furniture' | 'decoration' | 'equipment' | 'wall' | 'floor';
  price: number;
  currency: 'coins' | 'brains' | 'event-points';
  availability: string;
  description: string;
}

export const items: ItemDef[] = [
  /* ================= HALLOWEEN ================= */
  {
    id: 'haunted-table', name: 'Haunted Table', event: 'halloween', category: 'furniture',
    price: 2400, currency: 'coins', availability: 'Event exclusive',
    description: 'A crooked table that rattles its own plates. Customers seated here order 20% more dessert.',
  },
  {
    id: 'pumpkin-chair', name: 'Pumpkin Chair', event: 'halloween', category: 'furniture',
    price: 1200, currency: 'coins', availability: 'Challenge reward',
    description: 'Carved from a single enormous pumpkin. Smells faintly of cinnamon and the grave.',
  },
  {
    id: 'coffin-couch', name: 'Coffin Couch', event: 'halloween', category: 'furniture',
    price: 4800, currency: 'coins', availability: 'Challenge reward',
    description: 'Velvet lined, permanently reclined. The most requested seat in the haunted cafe.',
  },
  {
    id: 'spooky-stove', name: 'Spooky Stove', event: 'halloween', category: 'equipment',
    price: 60, currency: 'brains', availability: 'Event shop only',
    description: 'Its burners glow green and cook Halloween recipes 15% faster.',
  },
  {
    id: 'ghost-chandelier', name: 'Ghost Chandelier', event: 'halloween', category: 'decoration',
    price: 3200, currency: 'coins', availability: 'Limited — 2 per cafe',
    description: 'Three friendly ghosts hold the candles. They hum when the cafe is busy.',
  },
  {
    id: 'haunted-wallpaper', name: 'Haunted Wallpaper', event: 'halloween', category: 'wall',
    price: 1800, currency: 'coins', availability: 'Event exclusive',
    description: 'The eyes in the pattern follow customers to their tables. Charming.',
  },

  /* ================= CHRISTMAS ================= */
  {
    id: 'holiday-table', name: 'Holiday Table', event: 'christmas', category: 'furniture',
    price: 2600, currency: 'coins', availability: 'Event exclusive',
    description: 'Red cloth, gold trim, and a centerpiece that refuses to stop jingling.',
  },
  {
    id: 'snow-chair', name: 'Snow Chair', event: 'christmas', category: 'furniture',
    price: 1400, currency: 'coins', availability: 'Challenge reward',
    description: 'Packed snow that never melts. Bring a towel. Actually — bring two.',
  },
  {
    id: 'christmas-tree', name: 'Christmas Tree', event: 'christmas', category: 'decoration',
    price: 5200, currency: 'coins', availability: 'Challenge reward',
    description: 'Dripping with ornaments, tinsel, and one suspiciously twitching star.',
  },
  {
    id: 'present-stack', name: 'Present Stack', event: 'christmas', category: 'decoration',
    price: 2200, currency: 'coins', availability: 'Limited — 3 per cafe',
    description: 'Do not open the one with the skull wrapping. Zombie Santa is watching.',
  },
  {
    id: 'festive-stove', name: 'Festive Stove', event: 'christmas', category: 'equipment',
    price: 75, currency: 'brains', availability: 'Event shop only',
    description: 'Bakes Christmas recipes 15% faster and fills the cafe with gingerbread scent.',
  },
  {
    id: 'snowy-wallpaper', name: 'Snowy Wallpaper', event: 'christmas', category: 'wall',
    price: 1900, currency: 'coins', availability: 'Event exclusive',
    description: 'Frosted window scenes with actual fake frost. It is extremely convincing.',
  },

  /* ================= EASTER ================= */
  {
    id: 'egg-table', name: 'Egg Table', event: 'easter', category: 'furniture',
    price: 2200, currency: 'coins', availability: 'Challenge reward',
    description: 'A giant pastel egg, hinged open into a table. Seats four if they squeeze.',
  },
  {
    id: 'bunny-chair', name: 'Bunny Chair', event: 'easter', category: 'furniture',
    price: 1300, currency: 'coins', availability: 'Challenge reward',
    description: 'Ears for a backrest, cotton tail for a cushion. Customers squeak when they sit.',
  },
  {
    id: 'easter-basket', name: 'Easter Basket', event: 'easter', category: 'decoration',
    price: 1600, currency: 'coins', availability: 'Event exclusive',
    description: 'Filled with eggs of unknown origin. Refills itself when nobody is looking.',
  },
  {
    id: 'spring-fountain', name: 'Spring Fountain', event: 'easter', category: 'decoration',
    price: 5800, currency: 'coins', availability: 'Limited — 1 per cafe',
    description: 'Pastel water bubbles over stacked chocolate eggs. Extremely drinkable. Do not.',
  },
  {
    id: 'egg-stove', name: 'Egg Stove', event: 'easter', category: 'equipment',
    price: 55, currency: 'brains', availability: 'Event shop only',
    description: 'Shaped like an egg, cooks like a dream. Easter recipes 15% faster.',
  },
  {
    id: 'spring-wallpaper', name: 'Spring Wallpaper', event: 'easter', category: 'wall',
    price: 1700, currency: 'coins', availability: 'Event exclusive',
    description: 'Rolling pastel hills with tiny painted eggs hidden in the pattern.',
  },
];

export const categoryLabel: Record<ItemDef['category'], string> = {
  zombie: 'Zombies',
  furniture: 'Furniture',
  decoration: 'Decorations',
  equipment: 'Kitchen Equipment',
  wall: 'Wallpaper',
  floor: 'Flooring',
};
