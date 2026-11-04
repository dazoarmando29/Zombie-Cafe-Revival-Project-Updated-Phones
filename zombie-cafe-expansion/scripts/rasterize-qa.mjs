/* Rasterize exported SVGs to PNG for visual QA. Output goes to %TEMP%/zc-art-qa/ */
import { readFileSync, mkdirSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Resvg } from '@resvg/resvg-js';

const proj = join(dirname(fileURLToPath(import.meta.url)), '..');
const outDir = join(process.env.TEMP || '.', 'zc-art-qa');
mkdirSync(outDir, { recursive: true });

const targets = process.argv.slice(2).length
  ? process.argv.slice(2)
  : [
      'zombies/halloween/pumpkin-head.svg',
      'zombies/halloween/haunted-bride.svg',
      'zombies/halloween/scarecrow.svg',
      'zombies/halloween/vampire-chef.svg',
      'zombies/christmas/zombie-santa.svg',
      'zombies/christmas/frozen-elf.svg',
      'zombies/christmas/gingerbread.svg',
      'zombies/christmas/reindeer-zombie.svg',
      'zombies/easter/rotten-bunny.svg',
      'zombies/easter/egghead.svg',
      'zombies/easter/zombie-chick.svg',
      'zombies/easter/chocolate-bunny.svg',
    ];

for (const rel of targets) {
  const svg = readFileSync(join(proj, 'public', 'assets', rel), 'utf8');
  const r = new Resvg(svg, { fitTo: { mode: 'zoom', value: 1.6 } });
  const png = r.render().asPng();
  const name = rel.replace(/[\\/]/g, '_').replace('.svg', '.png');
  writeFileSync(join(outDir, name), png);
  console.log('rendered', name);
}
console.log('QA dir:', outDir);
