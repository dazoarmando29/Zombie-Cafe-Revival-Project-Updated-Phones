import type { EventId } from './characters';

export interface RecipeDef {
  id: string;
  name: string;
  event: Exclude<EventId, 'original'>;
  course: 'main' | 'dessert' | 'drink' | 'snack';
  cookTime: string;
  price: number;
  description: string;
  ingredients: string[];
}

export const recipes: RecipeDef[] = [
  /* ================= HALLOWEEN ================= */
  {
    id: 'witch-brew-stew', name: "Witch's Brew Stew", event: 'halloween', course: 'main',
    cookTime: '45 min', price: 320,
    description: 'Bubbling green stew with floating crook-neck squash and something that blinks back at you.',
    ingredients: ['Toxic squash', 'Swamp herbs', 'Newt (optional)', 'Eyeball garnish'],
  },
  {
    id: 'severed-finger-fries', name: 'Severed Finger Fries', event: 'halloween', course: 'snack',
    cookTime: '20 min', price: 180,
    description: 'Crispy breaded fingers with ketchup nails. Vegetarian. Probably.',
    ingredients: ['Root vegetable digits', 'Crunchy breading', 'Tomato nails', 'Parsley dirt'],
  },
  {
    id: 'eyeball-punch', name: 'Eyeball Punch', event: 'halloween', course: 'drink',
    cookTime: '10 min', price: 150,
    description: 'Lime punch with lychee eyeballs bobbing in the bowl. They follow your spoon.',
    ingredients: ['Lime cordial', 'Lychee eyeballs', 'Grape pupils', 'Dry ice fog'],
  },
  {
    id: 'graveyard-cake', name: 'Graveyard Cake', event: 'halloween', course: 'dessert',
    cookTime: '60 min', price: 420,
    description: 'Chocolate soil cake with tombstone cookies and a marzipan hand reaching out of the icing.',
    ingredients: ['Dark cocoa', 'Cookie tombstones', 'Marzipan hand', 'Crumb soil'],
  },

  /* ================= CHRISTMAS ================= */
  {
    id: 'snowman-stew', name: 'Snowman Stew', event: 'christmas', course: 'main',
    cookTime: '50 min', price: 360,
    description: 'Hearty winter stew served in a snowman tureen. The carrot nose is edible and the coal eyes are not.',
    ingredients: ['Root vegetables', 'Beef brisket', 'Snowman tureen', 'Carrot nose'],
  },
  {
    id: 'ginger-ghoul-cookies', name: 'Ginger Ghoul Cookies', event: 'christmas', course: 'dessert',
    cookTime: '35 min', price: 220,
    description: 'Gingerbread ghouls with royal icing scars. Some are missing limbs. That is the recipe, not a mistake.',
    ingredients: ['Molasses', 'Ginger spice', 'Royal icing', 'Candy buttons'],
  },
  {
    id: 'candy-cane-cranium', name: 'Candy Cane Cranium', event: 'christmas', course: 'dessert',
    cookTime: '25 min', price: 280,
    description: 'A striped peppermint skull, hollow and filled with hot cocoa. Tap the temple to crack it open.',
    ingredients: ['Peppermint sugar', 'Cocoa filling', 'Skull mold', 'Striped ribbon'],
  },
  {
    id: 'yule-slime-log', name: 'Yule Slime Log', event: 'christmas', course: 'dessert',
    cookTime: '55 min', price: 450,
    description: 'A chocolate roulade oozing green slime cream, decorated with meringue mushrooms and sugar snow.',
    ingredients: ['Chocolate sponge', 'Green slime cream', 'Meringue mushrooms', 'Sugar snow'],
  },

  /* ================= EASTER ================= */
  {
    id: 'rotten-egg-omelette', name: 'Rotten Egg Omelette', event: 'easter', course: 'main',
    cookTime: '15 min', price: 200,
    description: 'Green-tinged, fluffy, and surprisingly popular. The secret ingredient is not caring.',
    ingredients: ['Suspicious eggs', 'Spring herbs', 'Butter', 'Mystery cheese'],
  },
  {
    id: 'bunny-tail-truffles', name: 'Bunny Tail Truffles', event: 'easter', course: 'dessert',
    cookTime: '30 min', price: 240,
    description: 'Cocoa-dusted truffles with fluffy coconut tails. Adorable. Haunted. Delicious.',
    ingredients: ['Dark chocolate', 'Coconut fluff', 'Cocoa dust', 'Vanilla'],
  },
  {
    id: 'jelly-bean-stew', name: 'Jelly Bean Stew', event: 'easter', course: 'snack',
    cookTime: '20 min', price: 170,
    description: 'A warm, sticky, candy-colored stew. Every spoonful is a different flavor. Some flavors are mistakes.',
    ingredients: ['Jelly beans', 'Fruit syrup', 'Marshmallow', 'Regret'],
  },
  {
    id: 'hot-cross-brains', name: 'Hot Cross Brains', event: 'easter', course: 'dessert',
    cookTime: '40 min', price: 300,
    description: 'Spiced buns with pink icing brains and a sugar cross. Traditionally eaten on Good Friday. Traditionally.',
    ingredients: ['Spiced dough', 'Pink icing', 'Sugar cross', 'Cinnamon'],
  },
];

export const courseLabel: Record<RecipeDef['course'], string> = {
  main: 'Main Course',
  dessert: 'Dessert',
  drink: 'Drink',
  snack: 'Snack',
};
