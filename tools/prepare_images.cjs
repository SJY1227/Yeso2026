// Render original Figma image/vector layers into a static RGB background.
// The full-frame Figma screenshots under design/references are never inputs here.
const fs = require('node:fs');
const path = require('node:path');
const { createRequire } = require('node:module');
const runtime = process.env.CODEX_NODE_MODULES || path.join(process.env.USERPROFILE, '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules');
const sharp = createRequire(path.join(runtime, '_resolver.cjs'))('sharp');
const root = path.resolve(__dirname, '..');
const manifest = JSON.parse(fs.readFileSync(path.join(root, 'design/home-assets.json'), 'utf8'));

(async () => {
  fs.mkdirSync(path.join(root, 'assets/processed'), { recursive: true });
  for (const theme of manifest.themes) {
    const filename = name => path.join(root, 'assets/source', theme.assets.find(a => a.name === name).file);
    const resize = (name, w, h, fit = 'fill') => sharp(filename(name)).resize(w, h, { fit, position: 'centre' }).png().toBuffer();
    const layers = [];
    layers.push({ input: await resize(theme.background.name, 240, 320), left: 0, top: 0 });
    if (theme.key === 'pink') {
      const gradient = Buffer.alloc(240 * 120 * 4);
      for (let y = 0; y < 120; y++) for (let x = 0; x < 240; x++) gradient[(y * 240 + x) * 4 + 3] = Math.round(255 * 0.61 * y / 119);
      layers.push({ input: gradient, raw: { width: 240, height: 120, channels: 4 }, left: 0, top: 200 });
    }
    // Intrinsic SVG attributes remain unchanged; raster output fits the Figma slot.
    layers.push({ input: await resize(theme.topbar, 240, 27), left: 0, top: 0 });
    layers.push({ input: await resize(theme.bubble, 207, 61), left: 16, top: 43 });
    layers.push({ input: await resize(theme.icons, 240, 27), left: 0, top: 0 });
    const c = theme.character;
    layers.push({ input: await resize(c.name, c.w, c.h, 'cover'), left: c.x, top: c.y });
    await sharp({ create: { width: 240, height: 320, channels: 4, background: '#ffffff' } })
      .composite(layers).removeAlpha().png().toFile(path.join(root, 'assets/processed', `${theme.key}-base.png`));
    console.log(`Prepared ${theme.key}: ${layers.length} original asset/gradient layers`);
  }
})().catch(e => { console.error(e); process.exitCode = 1; });

