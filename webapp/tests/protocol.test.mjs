import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { Device, parseEvent, validateParam, groupParams } from '../js/protocol.js';

const fx = (n) => readFileSync(new URL(`./fixtures/${n}`, import.meta.url), 'utf8').trim();

// Fake transport: records writes, lets the test script the device's replies.
function rig(reply) {
  const t = { writes: [], onData: null, async write(s) { this.writes.push(s); setTimeout(() => reply(s, (x) => this.onData(x)), 0); } };
  return { t, dev: new Device(t) };
}

test('command reply collects lines until OK', async () => {
  const { dev } = rig((s, out) => out('line one\r\nline two\r\nOK\r\n'));
  const r = await dev.send('foo');
  assert.equal(r.ok, true);
  assert.deepEqual(r.lines, ['line one', 'line two']);
});

test('ERR reply reports reason', async () => {
  const { dev } = rig((s, out) => out('ERR out of range\n'));
  const r = await dev.send('set radar.min_energy 999');
  assert.equal(r.ok, false);
  assert.equal(r.error, 'out of range');
});

test('telemetry and events interleaved inside a reply are routed out, not into the reply', async () => {
  const { dev } = rig((s, out) => out('a\nEVT 100 RADAR_PRESENCE a=1 v=240 w=80\n' + fx('stream.json') + '\nb\nOK\n'));
  const tel = [], ev = [];
  dev.addEventListener('telemetry', (e) => tel.push(e.detail));
  dev.addEventListener('event', (e) => ev.push(e.detail));
  const r = await dev.send('x');
  assert.deepEqual(r.lines, ['a', 'b']);
  assert.equal(tel.length, 1);
  assert.equal(tel[0].radar.length, 3);
  assert.deepEqual(ev[0], { ms: 100, name: 'RADAR_PRESENCE', a: 1, v: 240, w: 80 });
});

test('fragmented chunks are reassembled', async () => {
  const { dev } = rig((s, out) => { out('fo'); out('o\nO'); out('K\n'); });
  const r = await dev.send('x');
  assert.deepEqual(r.lines, ['foo']);
});

test('commands are serialised one at a time', async () => {
  const order = [];
  const { dev, t } = rig((s, out) => { order.push(s.trim()); out('OK\n'); });
  await Promise.all([dev.send('one'), dev.send('two'), dev.send('three')]);
  assert.deepEqual(order, ['one', 'two', 'three']);
  assert.equal(t.writes.length, 3);
});

test('timeout resolves with error instead of hanging', async () => {
  const { dev } = rig(() => {});
  const r = await dev.send('x', 30);
  assert.deepEqual([r.ok, r.error], [false, 'timeout']);
});

test('loadParams parses the real firmware registry fixture', async () => {
  const { dev } = rig((s, out) => out(fx('params.json') + '\nOK\n'));
  const r = await dev.loadParams();
  assert.equal(r.ok, true);
  assert.ok(dev.params.length >= 40);
  const p = dev.params.find((x) => x.name === 'tamper.tilt_deg');
  assert.equal(p.type, 'f32');
  assert.ok(p.min < p.max);
});

test('real firmware stream fixture has the keys the UI reads', () => {
  const s = JSON.parse(fx('stream.json'));
  for (const k of ['t', 'env', 'imu', 'mag', 'gps', 'radar']) assert.ok(k in s, k);
  for (const k of ['ax', 'ay', 'az', 'roll', 'pitch', 'tilt', 'motion', 'state']) assert.ok(k in s.imu, k);
  for (const k of ['on', 'pres', 'mcm', 'me', 'scm', 'se']) assert.ok(k in s.radar[0], k);
});

test('parseEvent', () => {
  assert.equal(parseEvent('garbage'), null);
  assert.equal(parseEvent('EVT 5 BOOT a=0 v=0 w=0').name, 'BOOT');
});

test('validateParam mirrors firmware limits', () => {
  const f = { type: 'f32', min: 1, max: 180 }, u = { type: 'u32', min: 0, max: 255 }, b = { type: 'bool' }, s = { type: 'str', max: 15 };
  assert.equal(validateParam(f, '22.5'), null);
  assert.match(validateParam(f, '500'), /range/);
  assert.match(validateParam(u, '1.5'), /integer/);
  assert.match(validateParam(u, 'abc'), /number/);
  assert.equal(validateParam(b, 'on'), null);
  assert.ok(validateParam(b, '2'));
  assert.ok(validateParam(s, 'a'.repeat(16)));
  assert.ok(validateParam(s, 'a=b'));
});

test('groupParams groups by prefix', () => {
  const g = groupParams(JSON.parse(fx('params.json')));
  assert.ok(g.radar.length > 5 && g.tamper.length > 5);
});
