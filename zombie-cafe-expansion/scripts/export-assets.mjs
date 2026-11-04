/* Export all design artwork to standalone SVG files under public/assets/.
   Uses esbuild (already present via vite) + react-dom/server to render
   the React SVG components. Run: npm run export-assets            */
import { build } from 'esbuild';
import { mkdirSync, writeFileSync, rmSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = dirname(fileURLToPath(import.meta.url));
const proj = join(root, '..');
const bundlePath = join(root, '.export-bundle.mjs');

await build({
  entryPoints: [join(root, '_export_entry.tsx')],
  bundle: true,
  outfile: bundlePath,
  format: 'esm',
  platform: 'node',
  packages: 'external',
  jsx: 'automatic',
  logLevel: 'warning',
});

const mod = await import(pathToFileURL(bundlePath).href);
const files = mod.collect();

let count = 0;
for (const f of files) {
  const out = join(proj, 'public', 'assets', ...f.path.split('/'));
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, f.svg, 'utf8');
  count += 1;
  console.log('wrote', f.path);
}

rmSync(bundlePath, { force: true });
console.log(`\nExported ${count} SVG assets to public/assets/`);
