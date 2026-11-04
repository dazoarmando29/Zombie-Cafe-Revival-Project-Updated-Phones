/* Server-side smoke render of every view/component. Run: node scripts/smoke-render.mjs */
import { build } from 'esbuild';
import { rmSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = dirname(fileURLToPath(import.meta.url));
const bundlePath = join(root, '.smoke-bundle.mjs');

await build({
  entryPoints: [join(root, '_smoke_entry.tsx')],
  bundle: true,
  outfile: bundlePath,
  format: 'esm',
  platform: 'node',
  packages: 'external',
  jsx: 'automatic',
  loader: { '.css': 'empty' },
  logLevel: 'warning',
});

const mod = await import(pathToFileURL(bundlePath).href);
const html = mod.runSmoke();
const results = mod.assertContent(html);
for (const line of results) console.log(line);
rmSync(bundlePath, { force: true });
if (results.some((r) => r.startsWith('MISSING') || r.startsWith('TOO FEW'))) {
  process.exitCode = 1;
} else {
  console.log('SMOKE PASS');
}
