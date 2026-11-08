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
  return { context, element, files, downloads, calls, timers };
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

test('downloads M8S, LSDj and Furnace files and reports missing files', () => {
  const app = wrapper();
  for (const extension of ['m8s', 'lsdsng', 'fur']) {
    const name = 'song.' + extension;
    const file = '/user/projects/' + name;
    app.files.set(file, Uint8Array.of(1, 2, 3));
    app.context.window.choochooDownloadFile(file);
    assert.equal(app.downloads.at(-1), name);
  }
  app.timers.forEach(callback => callback());
  app.context.window.choochooDownloadFile('/user/projects/missing.fur');
  assert.equal(app.element('#status').textContent, 'Could not download missing.fur.');
  assert.equal(app.downloads.length, 3);
});
