const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');

const html = fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');

function wrapper() {
  const elements = new Map();
  const files = new Map();
  const downloads = [];
  const calls = [];
  const timers = [];
  const properties = new Map();
  const element = (selector) => {
    if (!elements.has(selector)) elements.set(selector, {
      listeners: {},
      style: { setProperty() {} },
      addEventListener(event, callback) { this.listeners[event] = callback; },
      click() { this.clicked = true; },
    });
    return elements.get(selector);
  };
  const context = {
    document: {
      documentElement: { style: {
        setProperty: (name, value) => properties.set(name, value),
        getPropertyValue: name => properties.get(name),
      } },
      querySelector: element,
      querySelectorAll: () => [],
      addEventListener() {},
      createElement: () => {
        const link = { click() { downloads.push(this.download); } };
        return link;
      },
    },
    window: {
      Module: { ccall: (...args) => calls.push(args) },
      addEventListener() {},
    },
    navigator: {},
    FS: {
      mkdirTree() {},
      writeFile: (name, bytes) => files.set(name, bytes),
      readFile: (name) => {
        if (!files.has(name)) throw new Error('File not found');
        return files.get(name);
      },
      syncfs: (_, done) => done(null),
    },
    URL: { createObjectURL: () => 'blob:test', revokeObjectURL() {} },
    Blob,
    Uint8Array,
    setInterval() {},
    setTimeout: (callback) => timers.push(callback),
    clearTimeout() {},
  };
  vm.runInNewContext(html.match(/<script>([\s\S]*?)<\/script>/)[1], context);
  return { context, element, files, downloads, calls, timers, properties };
}

test('imports both template formats without loading or saving the current song', async () => {
  assert.match(html, /id="template-input"[^>]*accept="\.m8s,\.lsdsng"[^>]*multiple/);
  assert.match(html, /id="import-templates" disabled/);
  const app = wrapper();
  app.element('#import-templates').listeners.click();
  assert.equal(app.element('#template-input').clicked, true);
  const input = {
    value: 'selected',
    files: ['template.m8s', 'template.lsdsng'].map(name => ({
      name,
      arrayBuffer: async () => Uint8Array.of(1, 2, 3).buffer,
    })),
  };
  await app.element('#template-input').listeners.change({ target: input });
  for (const file of input.files) {
    assert.deepEqual(Array.from(app.files.get('/user/projects/' + file.name)), [1, 2, 3]);
  }
  assert.equal(input.value, '');
  assert.deepEqual(app.calls, []);
  await app.element('#template-input').listeners.change({ target: { files: [], value: '' } });
  assert.equal(app.files.size, 2);
});

test('downloads MIDI, M8S, LSDj and Furnace files and reports missing files', () => {
  const app = wrapper();
  for (const extension of ['mid', 'm8s', 'lsdsng', 'fur']) {
    const name = 'song.' + extension;
    const file = '/user/projects/' + name;
    app.files.set(file, Uint8Array.of(1, 2, 3));
    app.context.window.choochooDownloadFile(file);
    assert.equal(app.downloads.at(-1), name);
  }
  app.timers.forEach(callback => callback());
  app.context.window.choochooDownloadFile('/user/projects/missing.fur');
  assert.equal(app.element('#status').textContent, 'Could not download missing.fur.');
  assert.equal(app.downloads.length, 4);
});

test('theme changes keep all control labels at WCAG AA contrast, including BB', () => {
  const app = wrapper();
  const luminance = hex => {
    const rgb = hex.match(/[a-f\d]{2}/gi).map(value => parseInt(value, 16) / 255)
      .map(value => value <= .04045 ? value / 12.92 : ((value + .055) / 1.055) ** 2.4);
    return rgb[0] * .2126 + rgb[1] * .7152 + rgb[2] * .0722;
  };
  const ratio = (a, b) => (Math.max(luminance(a), luminance(b)) + .05) /
    (Math.min(luminance(a), luminance(b)) + .05);
  const themes = [
    [0x2e3032, 0xbfc0c0, 0xd15000, 0xc8cae2, 0xff7000, 0xff6e00,
      0xffff4f, 0xff0f00, 0xeaeae8, 0xe45649],
    [0x050c1f, 0x10284a, 0x52799b, 0xc9bfa5, 0xffd665, 0xe6b84f,
      0x64d6ff, 0x65d8ff, 0x245072, 0xff6655],
    Array(10).fill(0x777777),
    Array(10).fill(0xffffff),
    Array(10).fill(0),
  ];
  for (const colors of themes) {
    app.context.window.choochooApplyTheme(colors, 0);
    for (const surface of ['value', 'selection', 'empty']) {
      assert.ok(ratio(app.properties.get('--' + surface),
        app.properties.get('--on-' + surface)) >= 4.5, surface + ' label contrast');
    }
    assert.ok(ratio(app.properties.get('--background'),
      app.properties.get('--control-outline')) >= 3, 'control boundary and focus contrast');
  }
  const css = html.match(/<style>([\s\S]*?)<\/style>/)[1];
  assert.match(css, /button \{ color: var\(--on-value\)/);
  assert.match(css, /\.dpad button \{[^}]*color: var\(--on-selection\)/);
  assert.match(css, /\.system button \{[^}]*color: var\(--on-empty\)/);
  assert.match(css, /\.face button:first-child \{[^}]*background: var\(--selection\); color: var\(--on-selection\)/);
  assert.doesNotMatch(css, /\.face button[^}]*var\(--(?:cursor|warning)\)/);
});
