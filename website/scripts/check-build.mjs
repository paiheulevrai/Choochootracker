// Check the assembled site at the project subpath used by GitHub Pages.
import assert from 'node:assert/strict';
import { readFile, stat } from 'node:fs/promises';
const dist = new URL('../dist/', import.meta.url);
const repository = process.env.GITHUB_REPOSITORY || 'paiheulevrai/Choochootracker';
const name = repository.split('/')[1];
const base = process.env.CF_PAGES === '1' || name.endsWith('.github.io') ? '/' : `/${name}/`;
for (const path of ['index.html', 'learn/index.html', 'downloads/index.html', 'manual/index.html', 'play/index.html']) {
  const html = await readFile(new URL(path, dist), 'utf8');
  assert(html.includes('lang="en"'), `${path}: English document`);
  assert(!html.includes('choochootracker.js'), `${path}: presentation must not load the app`);
  for (const [, target] of html.matchAll(/(?:href|src)="([^"#?]+)[^\"]*"/g)) {
    if (/^(https?:|mailto:|data:)/.test(target)) continue;
    assert(target.startsWith(base), `${path}: link outside project base: ${target}`);
    const relative = target.slice(base.length);
    const file = relative.endsWith('/') || !relative ? `${relative}index.html` : relative;
    assert((await stat(new URL(file, dist))).isFile(), `${path}: missing ${file}`);
  }
}
for (const path of ['play/index.html', 'play/runtime.html', 'play/choochootracker.js', 'play/choochootracker.wasm', 'play/choochootracker.data', 'play/manual.html', 'manual.html', 'user-guide/index.html']) {
  assert((await stat(new URL(path, dist))).size > 0, `${path}: missing or empty`);
}
const homepage = await readFile(new URL('index.html', dist), 'utf8');
assert(homepage.includes('<title>ChooChooTracker — Free, open-source tracker with 120+ sound engines</title>'), 'Homepage: selected SEO title');
assert(/name="description" content="[^"]*PortMaster[^"]*Linux consoles/.test(homepage), 'Homepage: portable-platform description');
const schemas = [...homepage.matchAll(/<script type="application\/ld\+json">([\s\S]*?)<\/script>/g)].map(([, json]) => JSON.parse(json));
const website = schemas.find(schema => schema['@type'] === 'WebSite');
assert.equal(website?.name, 'ChooChooTracker', 'Homepage: explicit site name');
assert.equal(website.url, process.env.CF_PAGES === '1'
  ? 'https://choochootracker.pages.dev/'
  : `https://${repository.split('/')[0]}.github.io${base}`, 'Homepage: canonical site URL');
for (const name of ['Fatty', 'DigiSophie']) {
  assert(homepage.includes(`<h3>${name}</h3>`), `Homepage: ${name} engine card`);
}
assert(homepage.includes('A modular sound-design playground in a portable tracker'), 'Homepage: cross-modulation feature');
assert(homepage.includes('OUTPUT destination and VOCODER mode'), 'Homepage: vocoder screenshot description');
for (const image of ['engine-fatty', 'engine-digisophie', 'track-vocoder']) {
  const path = `homepage-screenshots/${image}.png`;
  assert(homepage.includes(`src="${base}${path}"`), `Homepage: visible ${image} screenshot`);
  const png = await readFile(new URL(path, dist));
  assert.equal(png.subarray(0, 8).toString('hex'), '89504e470d0a1a0a', `${path}: PNG image`);
  assert.equal(png.readUInt32BE(16), 640, `${path}: original width`);
  assert.equal(png.readUInt32BE(20), 480, `${path}: original height`);
}
console.log('Site checks passed: project paths, linked assets, app, guides, new screenshots and homepage SEO.');
