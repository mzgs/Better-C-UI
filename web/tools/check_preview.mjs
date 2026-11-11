import assert from 'node:assert/strict';

// Exercise scheduling without a DOM, renderer, or a repeating Node timer.
const interval = globalThis.setInterval;
globalThis.setInterval = () => 0;
const { Preview, settings } = await import('../site/js/preview.js');
globalThis.setInterval = interval;

let elapsed = 0;
const preview = Object.create(Preview.prototype);
Object.assign(preview, {
  inst: 1, visible: true, disposed: false, dirty: true, last: null,
  frames: 0, stats: { build: 0 }, fpsWindow: [], opts: {}, canvas: { style: {} },
  paint() {},
  mod: { exports: {
    inst_frame(_inst, dt) { elapsed += dt; },
    inst_build_ms() { return 0; }, inst_needs_redraw() { return 0; },
    inst_next_wakeup() { return 1; }, inst_cursor() { return 0; },
  } },
});
preview.step(0);
elapsed = 0;
preview.step(1000);
assert.equal(elapsed, 1, 'idle wakeups must receive the full elapsed time');
preview.step(10000);
assert.equal(elapsed, 10, 'long gaps must not be capped');
settings.speed = 0.5;
preview.step(12000);
assert.equal(elapsed, 11, 'inspection speed scales the full elapsed time');
settings.speed = 1;
console.log('preview: idle wakeups, long gaps, and time scaling passed');

const handlers = new Map();
const keys = [];
Object.assign(preview, {
  buttons: 0,
  canvas: { style: {}, addEventListener(type, fn, opts) { if (!opts?.capture) handlers.set(type, fn); } },
  poke() {},
  mod: { text() {}, exports: { inst_key(...args) { keys.push(args); } } },
});
preview.bind();
handlers.get('keydown')({ code: 'KeyA', key: 'a', metaKey: true, preventDefault() {} });
assert.equal(keys[keys.length - 1][3], 2, 'Command must map to the WASM primary modifier (Control)');
handlers.get('keyup')({ code: 'MetaLeft', key: 'Meta', metaKey: false });
assert.equal(keys[keys.length - 1][3], 0, 'releasing Command must clear the primary modifier');
console.log('preview: Command shortcuts and modifier release passed');
