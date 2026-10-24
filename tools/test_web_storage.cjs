// Browser storage sequencing without a browser or third-party dependencies.
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const html = fs.readFileSync(path.join(__dirname, '../web/index.html'), 'utf8');
const start = html.indexOf('    const syncUserStorage =');
const end = html.indexOf('    window.choochooDownloadFile =', start);
assert(start >= 0 && end > start);

function storage() {
  const pending = [];
  const context = {
    window: {}, status: { textContent: '' },
    fs: { syncfs(populate, done) { assert.equal(populate, false); pending.push(done); } },
  };
  vm.createContext(context);
  vm.runInContext(`
    let storageSyncing = false, storagePending = false, storageCallbacks = [];
    const getFs = () => fs;
    ${html.slice(start, end)}
    this.sync = syncUserStorage;
  `, context);
  return { context, pending };
}

test('a save arriving during a write gets another flush before its callback', () => {
  const { context: c, pending } = storage();
  const completed = [];
  c.sync(error => { assert.equal(error, null); completed.push('first'); });
  c.sync(error => { assert.equal(error, null); completed.push('second'); });
  c.sync(error => { assert.equal(error, null); completed.push('third'); });
  assert.equal(pending.length, 1, 'no concurrent IDBFS writes');
  pending.shift()(null);
  assert.deepEqual(completed, ['first']);
  assert.equal(pending.length, 1, 'queued work was not dropped');
  pending.shift()(null);
  assert.deepEqual(completed, ['first', 'second', 'third']);
  assert.equal(pending.length, 0);
});

test('settings notifications use the same queue as a waiting reload', () => {
  const { context: c, pending } = storage();
  let reloadReady = false;
  c.window.choochooSettingsSaved();
  c.window.choochooSettingsSaved();
  c.sync(error => { assert.equal(error, null); reloadReady = true; });
  pending.shift()(null);
  assert.equal(reloadReady, false);
  pending.shift()(null);
  assert.equal(reloadReady, true);
});

test('storage errors reach callbacks and do not strand queued work', () => {
  const { context: c, pending } = storage();
  const error = new Error('storage unavailable');
  const results = [];
  c.sync(e => results.push(e));
  c.sync(e => results.push(e));
  pending.shift()(error);
  assert.equal(results[0], error);
  assert.match(c.status.textContent, /Could not save browser storage/);
  pending.shift()(null);
  assert.deepEqual(results, [error, null]);
});

test('missing filesystem reports an error instead of silently losing completion', () => {
  const { context: c, pending } = storage();
  c.fs = null;
  let error;
  c.sync(e => { error = e; });
  assert.match(error.message, /not ready/);
  assert.equal(pending.length, 0);
});
