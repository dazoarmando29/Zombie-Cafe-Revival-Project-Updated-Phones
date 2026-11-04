export type Rarity = 'common' | 'uncommon' | 'rare' | 'legendary';
export type EventId = 'original' | 'halloween' | 'christmas' | 'easter';

export interface CharacterDef {
  id: string;
  name: string;
  event: Exclude<EventId, 'original'>;
  rarity: Rarity;
  role: string;
  description: string;
  flavor: string;
  height: string;
}

export const characters: CharacterDef[] = [
  /* ---------------- HALLOWEEN ---------------- */
  {
    id: 'pumpkin-head',
    name: 'Pumpkin Head',
    event: 'halloween',
    rarity: 'rare',
    role: 'Line Cook',
    description:
      'A walking jack-o-lantern with a carved grin that glows brighter the hungrier he gets. His lantern keeps the kitchen open past midnight.',
    flavor: '"Stop playing with your food!" he cackles, while being food.',
    height: '1.7 m (plus stem)',
  },
  {
    id: 'haunted-bride',
    name: 'Haunted Bride',
    event: 'halloween',
    rarity: 'rare',
    role: 'Hostess',
    description:
      'She has waited 130 years for her wedding banquet. Now she seats every customer personally — and cries glowing tears over slow service.',
    flavor: 'Her RSVP list is written on a tombstone.',
    height: '1.8 m (with veil)',
  },
  {
    id: 'scarecrow',
    name: 'Scarecrow',
    event: 'halloween',
    rarity: 'uncommon',
    role: 'Busboy',
    description:
      'Stuffed with straw and spite. Scares away crows, health inspectors, and anyone who asks for a refund. His crow is the real manager.',
    flavor: 'Fell off his post in 1887. Never stopped commuting.',
    height: '1.9 m (hat included)',
  },
  {
    id: 'vampire-chef',
    name: 'Vampire Chef',
    event: 'halloween',
    rarity: 'legendary',
    role: 'Head Chef',
    description:
      'A master of the dark culinary arts. His sauces are legendary, his cape is dramatic, and the red on his apron is definitely marinara.',
    flavor: 'He only cooks what he can pronounce. And rare steaks.',
    height: '2.0 m (cape volume excluded)',
  },

  /* ---------------- CHRISTMAS ---------------- */
  {
    id: 'zombie-santa',
    name: 'Zombie Santa',
    event: 'christmas',
    rarity: 'legendary',
    role: 'Manager',
    description:
      'He sees you when youre sleeping. He knows when youre awake. He knows when youve ordered the soup. His sack of presents never stops rustling.',
    flavor: 'Making a list. Checking it twice. Eating it.',
    height: '2.1 m (beard included)',
  },
  {
    id: 'frozen-elf',
    name: 'Frozen Elf',
    event: 'christmas',
    rarity: 'uncommon',
    role: 'Pastry Chef',
    description:
      'Left in the walk-in freezer for one winter too many. Her frosting work is flawless because the frosting never melts off her sleeves.',
    flavor: 'Her candy cane has been "in progress" since 1998.',
    height: '1.3 m (ears add 20 cm)',
  },
  {
    id: 'gingerbread',
    name: 'Gingerbread Zombie',
    event: 'christmas',
    rarity: 'common',
    role: 'Taster',
    description:
      'Built in the bakery, eaten in the bakery, risen from the bakery. Missing several buttons and one eye, but still the sweetest worker on staff.',
    flavor: 'Runs from no one. Mostly because of the broken leg.',
    height: '1.2 m (bite reduces height)',
  },
  {
    id: 'reindeer-zombie',
    name: 'Reindeer Zombie',
    event: 'christmas',
    rarity: 'rare',
    role: 'Delivery',
    description:
      'One antler snapped, bell dented, nose glowing a suspicious red. Delivers takeaway across the rooftop circuit in record time. Occasionally eats it.',
    flavor: 'Rudolf called in sick. This is who showed up.',
    height: '1.8 m (antlers: wide load)',
  },

  /* ---------------- EASTER ---------------- */
  {
    id: 'rotten-bunny',
    name: 'Rotten Easter Bunny',
    event: 'easter',
    rarity: 'uncommon',
    role: 'Host',
    description:
      'His costume is torn, his stuffing is out, and his ears are at half mast. Still hands out eggs. Nobody dares ask what is inside them.',
    flavor: 'Hides eggs. Forgets where. Every year.',
    height: '1.7 m (ears droop to 1.4 m)',
  },
  {
    id: 'egghead',
    name: 'Egghead',
    event: 'easter',
    rarity: 'common',
    role: 'Decorator',
    description:
      'A zombie whose head is a cracked, painted Easter egg. Some say a chick is piloting him. The paintbrush never dries.',
    flavor: 'Shell integrity: 62%. Morale: 100%.',
    height: '1.4 m (shell adds volume)',
  },
  {
    id: 'zombie-chick',
    name: 'Zombie Chick',
    event: 'easter',
    rarity: 'common',
    role: 'Mascot',
    description:
      'Hatched, got pecked, got back up. A pocket-sized puff of undead cheer with a bandaged wing and a stolen painted egg.',
    flavor: 'Cheep. Cheep. Braaaains. Cheep.',
    height: '0.8 m (fluff included)',
  },
  {
    id: 'chocolate-bunny',
    name: 'Chocolate Bunny',
    event: 'easter',
    rarity: 'rare',
    role: 'Bouncer',
    description:
      'Half-melted, cracked down the middle, one ear snapped off. The foil is real gold. The eyes are candy. The attitude is 100% dark chocolate.',
    flavor: 'Do not bite him. That is how this started.',
    height: '1.6 m (0.3 m of foil)',
  },
];

export const rarityLabel: Record<Rarity, string> = {
  common: 'Common',
  uncommon: 'Uncommon',
  rare: 'Rare',
  legendary: 'Legendary',
};
