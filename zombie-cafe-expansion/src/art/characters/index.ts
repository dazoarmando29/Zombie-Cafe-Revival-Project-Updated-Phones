import type { ReactElement } from 'react';
import { PumpkinHead, HauntedBride, Scarecrow, VampireChef } from './halloween';
import { ZombieSanta, FrozenElf, Gingerbread, ReindeerZombie } from './christmas';
import { RottenBunny, Egghead, ZombieChick, ChocolateBunny } from './easter';

export {
  PumpkinHead,
  HauntedBride,
  Scarecrow,
  VampireChef,
  ZombieSanta,
  FrozenElf,
  Gingerbread,
  ReindeerZombie,
  RottenBunny,
  Egghead,
  ZombieChick,
  ChocolateBunny,
};

export const characterArt: Record<string, () => ReactElement> = {
  'pumpkin-head': PumpkinHead,
  'haunted-bride': HauntedBride,
  'scarecrow': Scarecrow,
  'vampire-chef': VampireChef,
  'zombie-santa': ZombieSanta,
  'frozen-elf': FrozenElf,
  'gingerbread': Gingerbread,
  'reindeer-zombie': ReindeerZombie,
  'rotten-bunny': RottenBunny,
  'egghead': Egghead,
  'zombie-chick': ZombieChick,
  'chocolate-bunny': ChocolateBunny,
};
