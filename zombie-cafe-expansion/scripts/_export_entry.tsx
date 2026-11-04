import type { ReactElement } from 'react';
import { renderToStaticMarkup } from 'react-dom/server';
import { characterArt } from '../src/art/characters';
import { itemArt } from '../src/art/items';
import { PumpkinIcon, SnowflakeIcon, EggIcon, GhostIcon, StarIcon, BrainIcon, CoinIcon, ChefHatIcon, LockIcon } from '../src/art/ui';

export interface ExportFile {
  path: string;
  svg: string;
}

const evOfChar: Record<string, string> = {
  'pumpkin-head': 'halloween', 'haunted-bride': 'halloween', 'scarecrow': 'halloween', 'vampire-chef': 'halloween',
  'zombie-santa': 'christmas', 'frozen-elf': 'christmas', 'gingerbread': 'christmas', 'reindeer-zombie': 'christmas',
  'rotten-bunny': 'easter', 'egghead': 'easter', 'zombie-chick': 'easter', 'chocolate-bunny': 'easter',
};

const evOfItem: Record<string, string> = {
  'haunted-table': 'halloween', 'pumpkin-chair': 'halloween', 'coffin-couch': 'halloween', 'spooky-stove': 'halloween',
  'ghost-chandelier': 'halloween', 'haunted-wallpaper': 'halloween',
  'holiday-table': 'christmas', 'snow-chair': 'christmas', 'christmas-tree': 'christmas', 'present-stack': 'christmas',
  'festive-stove': 'christmas', 'snowy-wallpaper': 'christmas',
  'egg-table': 'easter', 'bunny-chair': 'easter', 'easter-basket': 'easter', 'spring-fountain': 'easter',
  'egg-stove': 'easter', 'spring-wallpaper': 'easter',
  'witch-brew-stew': 'recipes', 'severed-finger-fries': 'recipes', 'eyeball-punch': 'recipes', 'graveyard-cake': 'recipes',
  'snowman-stew': 'recipes', 'ginger-ghoul-cookies': 'recipes', 'candy-cane-cranium': 'recipes', 'yule-slime-log': 'recipes',
  'rotten-egg-omelette': 'recipes', 'bunny-tail-truffles': 'recipes', 'jelly-bean-stew': 'recipes', 'hot-cross-brains': 'recipes',
};

const charOrder: Record<string, string[]> = {
  halloween: ['pumpkin-head', 'haunted-bride', 'scarecrow', 'vampire-chef'],
  christmas: ['zombie-santa', 'frozen-elf', 'gingerbread', 'reindeer-zombie'],
  easter: ['rotten-bunny', 'egghead', 'zombie-chick', 'chocolate-bunny'],
};

function portraitOf(svg: string): string {
  const inner = svg.replace(/^<svg[^>]*>/, '').replace(/<\/svg>\s*$/, '');
  return (
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 120" width="120" height="120">' +
    '<svg x="-40" y="-12" width="200" height="260" viewBox="0 0 200 260">' +
    inner +
    '</svg></svg>'
  );
}

function collectionSheet(ev: string, svgs: string[]): string {
  const cells = svgs
    .map((s, i) => {
      const inner = s.replace(/^<svg[^>]*>/, '').replace(/<\/svg>\s*$/, '');
      const x = 10 + (i % 2) * 200;
      const y = 56 + Math.floor(i / 2) * 262;
      return `<svg x="${x}" y="${y}" width="200" height="260" viewBox="0 0 200 260">${inner}</svg>`;
    })
    .join('');
  return (
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 410 590" width="410" height="590">' +
    '<rect x="4" y="4" width="402" height="582" rx="18" fill="#2A1C10"/>' +
    '<rect x="12" y="12" width="386" height="566" rx="12" fill="#F6E7C1"/>' +
    `<text x="205" y="42" text-anchor="middle" font-family="sans-serif" font-size="24" font-weight="bold" fill="#2A1C10">Zombie Cafe — ${ev.toUpperCase()} COLLECTION</text>` +
    cells +
    '</svg>'
  );
}

function backgroundSvg(ev: string): string {
  const palette: Record<string, [string, string, string]> = {
    original: ['#7FC3E8', '#F6E7C1', '#C98E4F'],
    halloween: ['#1D1033', '#2B1B4A', '#3D2263'],
    christmas: ['#A8D8E8', '#1E6B3A', '#17512D'],
    easter: ['#A9D6F0', '#C9B6E4', '#F5B7C6'],
  };
  const [sky, wall, floor] = palette[ev];
  return (
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 450" width="800" height="450">' +
    `<rect width="800" height="450" fill="${sky}"/>` +
    `<rect y="150" width="800" height="230" fill="${wall}"/>` +
    `<rect y="380" width="800" height="70" fill="${floor}"/>` +
    '<rect y="146" width="800" height="8" fill="rgba(42,28,16,0.35)"/>' +
    '<rect y="376" width="800" height="8" fill="rgba(42,28,16,0.35)"/>' +
    '</svg>'
  );
}

export function collect(): ExportFile[] {
  const files: ExportFile[] = [];

  for (const [id, C] of Object.entries(characterArt)) {
    const svg = renderToStaticMarkup(<C />);
    const ev = evOfChar[id];
    files.push({ path: `zombies/${ev}/${id}.svg`, svg });
    files.push({ path: `zombies/${ev}/portraits/${id}.svg`, svg: portraitOf(svg) });
  }

  for (const [id, C] of Object.entries(itemArt)) {
    const svg = renderToStaticMarkup(<C />);
    const ev = evOfItem[id];
    const dir = ev === 'recipes' ? 'recipes' : `furniture/${ev}`;
    files.push({ path: `${dir}/${id}.svg`, svg });
  }

  for (const [ev, ids] of Object.entries(charOrder)) {
    const rendered = ids.map((id) => {
      const C = characterArt[id];
      return renderToStaticMarkup(<C />);
    });
    files.push({ path: `sheets/${ev}-collection.svg`, svg: collectionSheet(ev, rendered) });
  }

  for (const ev of ['original', 'halloween', 'christmas', 'easter']) {
    files.push({ path: `backgrounds/${ev}.svg`, svg: backgroundSvg(ev) });
  }

  const icons: [string, ReactElement][] = [
    ['icon-pumpkin', <PumpkinIcon key="p" size={64} />],
    ['icon-snowflake', <SnowflakeIcon key="s" size={64} />],
    ['icon-egg', <EggIcon key="e" size={64} />],
    ['icon-ghost', <GhostIcon key="g" size={64} />],
    ['icon-star', <StarIcon key="st" size={64} />],
    ['icon-brain', <BrainIcon key="b" size={64} />],
    ['icon-coin', <CoinIcon key="c" size={64} />],
    ['icon-chefhat', <ChefHatIcon key="ch" size={64} />],
    ['icon-lock', <LockIcon key="l" size={64} />],
  ];
  for (const [name, el] of icons) {
    files.push({ path: `ui/${name}.svg`, svg: renderToStaticMarkup(el) });
  }

  return files;
}
