import { Device, validateParam, groupParams } from './protocol.js';
import { WebSerialTransport, DemoTransport } from './transport.js';
import { LineChart, RadarView, Compass, Level, GateBars } from './charts.js';

const $ = (id) => document.getElementById(id);
const fmt = (v, dp = 1, unit = '') => (v == null || Number.isNaN(v) ? '—' : `${Number(v).toFixed(dp)}${unit}`);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let dev = null, transport = null, demo = false, streamOn = true, last = null;
let edits = {};                      // param name -> text typed by the user

// ------------------------------------------------------------------ helpers
function toast(msg) {
  const t = $('toast'); t.textContent = msg; t.classList.add('show');
  clearTimeout(toast.t); toast.t = setTimeout(() => t.classList.remove('show'), 2600);
}
function setConn(text, cls) { const c = $('conn'); c.textContent = text; c.className = 'chip ' + (cls || ''); }
const connected = () => !!dev;

// -------------------------------------------------------------------- theme
const savedTheme = localStorage.getItem('theme');
if (savedTheme) document.documentElement.dataset.theme = savedTheme;
$('btnTheme').onclick = () => {
  const dark = matchMedia('(prefers-color-scheme: dark)').matches;
  const cur = document.documentElement.dataset.theme || (dark ? 'dark' : 'light');
  const next = cur === 'dark' ? 'light' : 'dark';
  document.documentElement.dataset.theme = next; localStorage.setItem('theme', next);
  requestAnimationFrame(redrawAll);
};

// --------------------------------------------------------------------- tabs
document.querySelectorAll('.tab').forEach((b) => b.addEventListener('click', () => {
  document.querySelectorAll('.tab').forEach((x) => x.setAttribute('aria-selected', x === b));
  document.querySelectorAll('.panel').forEach((p) => p.classList.toggle('active', p.id === 'tab-' + b.dataset.tab));
  redrawAll(); requestAnimationFrame(redrawAll);
}));

// ------------------------------------------------------------------- charts
const radarView = new RadarView($('cvRadar')), compass = new Compass($('cvCompass')), level = new Level($('cvLevel')), gates = new GateBars($('cvGates'));
const S = ['--s1', '--s2', '--s3'];
const radarSeries = (p) => ['A', 'B', 'C'].map((n, i) => ({ key: p + i, label: 'Radar ' + n, color: S[i] }));
const charts = {
  dist: new LineChart($('cvDist'), radarSeries('d'), { unit: ' ft', digits: 1, min: 0 }),
  energy: new LineChart($('cvEnergy'), radarSeries('e'), { digits: 0, min: 0, max: 100 }),
  acc: new LineChart($('cvAcc'), [{ key: 'ax', label: 'X', color: S[0] }, { key: 'ay', label: 'Y', color: S[1] }, { key: 'az', label: 'Z', color: S[2] }], { unit: ' g', digits: 3 }),
  tilt: new LineChart($('cvTilt'), [{ key: 'tilt', label: 'Tilt', color: S[0] }], { unit: '°', digits: 1, min: 0 }),
  vib: new LineChart($('cvVib'), [{ key: 'motion', label: 'Vibration', color: S[1] }], { unit: ' mg', digits: 0, min: 0 }),
  temp: new LineChart($('cvTemp'), [{ key: 'temp', label: 'Temp', color: S[0] }], { unit: ' °F', digits: 1 }),
  press: new LineChart($('cvPress'), [{ key: 'press', label: 'Pressure', color: S[0] }], { unit: ' inHg', digits: 3 }),
  head: new LineChart($('cvHead'), [{ key: 'heading', label: 'Heading', color: S[0] }], { unit: '°', digits: 0, min: 0, max: 360 }),
};
const legends = { radar: [['Radar A', S[0]], ['Radar B', S[1]], ['Radar C', S[2]]], xyz: [['X', S[0]], ['Y', S[1]], ['Z', S[2]]] };
document.querySelectorAll('[data-legend]').forEach((el) => {
  el.innerHTML = (legends[el.dataset.legend] || []).map(([n, c]) => `<span><i style="background:var(${c})"></i>${n}</span>`).join('');
});
$('radarLegend').innerHTML = legends.radar.map(([n, c], i) => `<span><i style="background:var(${c})"></i>${n} — sector ${i * 120}°</span>`).join('');
$('winSel').onchange = () => { const w = Number($('winSel').value); Object.values(charts).forEach((c) => { c.o.windowS = w; c.draw(); }); };
$('btnGClear').onclick = () => Object.values(charts).forEach((c) => c.clear());
function redrawAll() { radarView.draw(); compass.draw(); level.draw(); gates.draw(); Object.values(charts).forEach((c) => c.draw()); }
addEventListener('resize', redrawAll);

// --------------------------------------------------------------- telemetry
let drawPending = false;
function onTelemetry(f) {
  last = f;
  const { env, imu, mag, gps, radar } = f;
  const vals = {
    temp: env.temp == null ? null : env.temp * 1.8 + 32, press: env.press == null ? null : env.press * 0.02953, heading: mag.heading,
    ax: imu.ok ? imu.ax : null, ay: imu.ok ? imu.ay : null, az: imu.ok ? imu.az : null,
    tilt: imu.ok ? imu.tilt : null, motion: imu.ok ? imu.motion : null,
  };
  radar.forEach((r, i) => {
    vals['d' + i] = r.on && r.pres ? Math.max(r.mcm, r.scm) / 30.48 : null;
    vals['e' + i] = r.on && r.pres ? Math.max(r.me, r.se) : null;
  });
  for (const c of Object.values(charts)) c.push(f.t, vals);

  // cards
  $('eTemp').textContent = fmt(env.temp == null ? null : env.temp * 1.8 + 32, 1, ' °F');
  $('ePress').textContent = fmt(env.press == null ? null : env.press * 0.02953, 2, ' inHg');
  $('eHum').textContent = env.hum == null ? (env.chip === 'BMP280' ? 'n/a (BMP280)' : '—') : fmt(env.hum, 0, ' %');
  $('eAlt').textContent = fmt(env.alt == null ? null : env.alt * 3.28084, 0, ' ft');
  $('envChip').textContent = env.chip === 'none' ? '(sensor not found)' : env.chip;

  $('gFix').innerHTML = gps.fix ? '<span class="chip ok">Fix</span>' : '<span class="chip warn">No fix</span>';
  $('gSats').textContent = `${gps.sats} / ${fmt(gps.hdop, 1)}`;
  $('gLat').textContent = gps.fix ? gps.lat.toFixed(6) : '—';
  $('gLon').textContent = gps.fix ? gps.lon.toFixed(6) : '—';
  $('gAlt').textContent = gps.fix ? `${fmt(gps.alt * 3.28084, 0, ' ft')} / ${fmt(gps.kmh * 0.621371, 1, ' mph')}` : '—';
  $('gPps').textContent = gps.pps;

  const st = $('tState'); st.textContent = imu.ok ? imu.state : 'no IMU'; st.className = 'state ' + (imu.ok ? imu.state : 'DISARMED');
  $('tRP').textContent = imu.ok ? `${fmt(imu.roll, 1, '°')} / ${fmt(imu.pitch, 1, '°')}` : '—';
  $('tTilt').textContent = imu.ok && imu.state !== 'DISARMED' ? fmt(imu.tilt, 1, '°') : '—';
  $('tMotion').textContent = imu.ok ? fmt(imu.motion, 0, ' mg') : '—';
  const tb = $('tamperBanner');
  tb.hidden = imu.state !== 'ALARM';
  tb.className = 'banner alarm'; tb.textContent = '⚠ TAMPER ALARM — the unit was tilted or shaken. It re-arms at its new pose after the hold-off time.';
  $('headingNote').textContent = mag.ok ? '' : 'Magnetometer not found';

  if (!drawPending) {
    drawPending = true;
    requestAnimationFrame(() => {
      drawPending = false;
      radarView.set({ radar: last.radar, heading: last.mag.ok ? last.mag.heading : null });
      compass.set(last.mag.ok ? last.mag.heading : null);
      level.set({ roll: last.imu.roll, pitch: last.imu.pitch, limit: Number(getParam('tamper.tilt_deg')) || 15 });
      gates.set(last.radar);
      Object.values(charts).forEach((c) => c.draw());
    });
  }
}

// ------------------------------------------------------------------ events
const EV = {
  RADAR_PRESENCE: (e) => `Radar ${'ABC'[e.a]}: presence at ${(e.v / 30.48).toFixed(1)} ft (energy ${e.w})`,
  RADAR_CLEAR: (e) => `Radar ${'ABC'[e.a]}: clear`,
  TAMPER_TILT: (e) => `TAMPER: tilted ${(e.v / 10).toFixed(1)}° from armed pose`,
  TAMPER_MOTION: (e) => `TAMPER: shaken / moved (${e.v} mg)`,
  GPS_FIX: (e) => (e.v ? 'GPS fix acquired' : 'GPS fix lost'),
  BOOT: () => 'Node booted',
};
function onEvent(e) {
  const log = $('evLog');
  if (log.firstElementChild?.classList.contains('muted')) log.innerHTML = '';
  const d = document.createElement('div');
  const s = Math.floor(e.ms / 1000);
  d.textContent = `${new Date().toLocaleTimeString()}  [up ${Math.floor(s / 60)}m${String(s % 60).padStart(2, '0')}s]  ${(EV[e.name] || (() => e.name))(e)}`;
  if (e.name.startsWith('TAMPER')) d.style.color = 'var(--crit)';
  log.prepend(d);
  while (log.children.length > 200) log.lastChild.remove();
}
$('btnClearEv').onclick = () => { $('evLog').innerHTML = '<div class="muted">No events yet.</div>'; };

// -------------------------------------------------------------- connection
async function attach(t, isDemo) {
  transport = t; demo = isDemo;
  dev = new Device(t);
  dev.addEventListener('telemetry', (e) => onTelemetry(e.detail));
  dev.addEventListener('event', (e) => onEvent(e.detail));
  dev.addEventListener('raw', (e) => termLine(e.detail));
  dev.addEventListener('tx', (e) => termLine('> ' + e.detail, 'tx'));
  t.onClose = () => { if (dev) lostConnection(); };
  await t.open();
  setConn(isDemo ? 'Demo node' : 'Connected', 'ok');
  $('btnConnect').hidden = $('btnDemo').hidden = true; $('btnDisc').hidden = false;
  await sleep(isDemo ? 120 : 600);
  const v = await dev.send('ver');
  if (v.ok) { $('fw').hidden = false; $('fw').textContent = v.lines[0] || ''; }
  await refreshParams();
  if (streamOn) await dev.startStream();
}

let reconnecting = false;
async function lostConnection() {
  if (!dev) return;
  const wasDemo = demo, port = transport?.port;
  dev = null; $('fw').hidden = true;
  setConn('Disconnected', 'bad'); $('btnDisc').hidden = true; $('btnConnect').hidden = $('btnDemo').hidden = false;
  if (!wasDemo && port && !reconnecting) tryReconnect(port);
}

// After 'reboot' the USB device vanishes and returns a moment later; reopen the same port.
async function tryReconnect(port) {
  reconnecting = true; setConn('Reconnecting…', 'warn');
  for (let i = 0; i < 25; i++) {
    await sleep(800);
    const ports = await navigator.serial.getPorts();
    if (!ports.includes(port)) continue;
    try { await attach(Object.assign(new WebSerialTransport(), { port }), false); reconnecting = false; toast('Reconnected'); return; } catch { /* still rebooting */ }
  }
  reconnecting = false; setConn('Disconnected', 'bad');
}

$('btnConnect').onclick = async () => {
  try {
    const t = new WebSerialTransport();
    await t.pick();
    await attach(t, false);
  } catch (e) { if (e.name !== 'NotFoundError') toast('Connect failed: ' + e.message); setConn('Disconnected', 'bad'); dev = null; }
};
$('btnDemo').onclick = async () => {
  const p = await (await fetch('tests/fixtures/params.json')).text();
  await attach(new DemoTransport(p), true);
};
$('btnDisc').onclick = async () => { const t = transport, d = dev; dev = null; await t.close(); lostConnection.call(null); setConn('Disconnected', 'bad'); $('btnDisc').hidden = true; $('btnConnect').hidden = $('btnDemo').hidden = false; $('fw').hidden = true; void d; };
if (!WebSerialTransport.supported()) { $('unsupported').hidden = false; $('btnConnect').disabled = true; }

// ------------------------------------------------------------------ console
function termLine(text, cls) {
  if (!$('cShowTel').checked && text[0] === '{') return;
  const t = $('term'), d = document.createElement('div');
  d.textContent = text; if (cls) d.className = cls; else if (text.startsWith('ERR')) d.className = 'err';
  t.append(d);
  while (t.children.length > 500) t.firstChild.remove();
  t.scrollTop = t.scrollHeight;
}
const hist = []; let hi = 0;
$('cmdForm').onsubmit = async (e) => {
  e.preventDefault();
  const v = $('cmdIn').value.trim(); if (!v) return;
  if (!connected()) return toast('Not connected');
  hist.push(v); hi = hist.length; $('cmdIn').value = '';
  await dev.send(v, 4000);
};
$('cmdIn').onkeydown = (e) => {
  if (e.key === 'ArrowUp' && hi > 0) { $('cmdIn').value = hist[--hi]; e.preventDefault(); }
  if (e.key === 'ArrowDown') { $('cmdIn').value = hi < hist.length - 1 ? hist[++hi] : ''; if (hi >= hist.length - 1) hi = hist.length; }
};
$('cClear').onclick = () => { $('term').innerHTML = ''; };
$('cStream').onclick = async () => {
  streamOn = !streamOn; $('cStream').textContent = 'Stream: ' + (streamOn ? 'on' : 'off');
  if (connected()) await (streamOn ? dev.startStream() : dev.stopStream());
};

// --------------------------------------------------------------- parameters
const getParam = (n) => dev?.params.find((p) => p.name === n)?.value;
const curText = (row) => { const i = row.querySelector('input'); return i.type === 'checkbox' ? (i.checked ? '1' : '0') : i.value; };

async function refreshParams() {
  const r = await dev.loadParams();
  if (!r.ok) { toast('Could not load parameters: ' + r.error); return; }
  edits = {}; renderParams();
}

function renderParams() {
  const list = $('pList'); list.innerHTML = '';
  const filter = $('pFilter').value.toLowerCase();
  for (const [grp, ps] of Object.entries(groupParams(dev.params))) {
    const rows = ps.filter((p) => !filter || (p.name + ' ' + p.desc).toLowerCase().includes(filter));
    if (!rows.length) continue;
    const det = document.createElement('details'); det.className = 'pgroup card'; det.open = !!filter || ['tamper', 'radar', 'sys'].includes(grp) || true;
    det.innerHTML = `<summary>${grp} <span class="muted small">(${rows.length})</span></summary>`;
    for (const p of rows) det.append(paramRow(p));
    list.append(det);
  }
  updateApply();
}

function paramRow(p) {
  const row = document.createElement('div'); row.className = 'prow'; row.dataset.name = p.name;
  const isBool = p.type === 'bool';
  const val = edits[p.name] ?? p.value;
  const inp = isBool ? `<input type="checkbox" ${val === '1' ? 'checked' : ''} aria-label="${p.name}">`
    : p.type === 'str' ? `<input type="text" value="${esc(val)}" maxlength="${p.max}" aria-label="${p.name}">`
      : `<input type="number" value="${esc(val)}" min="${p.min}" max="${p.max}" step="${p.type === 'f32' ? 'any' : 1}" aria-label="${p.name}">`;
  const range = isBool || p.type === 'str' ? '' : ` · range ${p.min}…${p.max}`;
  row.innerHTML = `<div><code>${p.name}</code>${p.reboot ? '<span class="tag reboot">reboot</span>' : ''}${p.value !== p.def ? '<span class="tag">changed from default</span>' : ''}</div>
    <div>${inp}</div>
    <div><span>${esc(p.desc)}</span><div class="muted small">default ${esc(p.def)}${range} <a href="#" class="rst">reset</a> <span class="err"></span></div></div>`;
  const input = row.querySelector('input');
  const onchange = () => {
    const t = curText(row), err = validateParam(p, t);
    row.classList.toggle('invalid', !!err); row.querySelector('.err').textContent = err || '';
    const changed = t !== p.value && !(p.type !== 'str' && p.type !== 'bool' && Number(t) === Number(p.value));
    row.classList.toggle('changed', changed && !err);
    if (changed) edits[p.name] = t; else delete edits[p.name];
    updateApply();
  };
  input.addEventListener('input', onchange);
  row.querySelector('.rst').onclick = (e) => { e.preventDefault(); if (isBool) input.checked = p.def === '1'; else input.value = p.def; onchange(); };
  if (edits[p.name] !== undefined) onchange();
  return row;
}
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

function updateApply() {
  const n = Object.keys(edits).length, bad = document.querySelectorAll('.prow.invalid').length;
  $('pApply').disabled = !connected() || !n || bad > 0;
  $('pApply').textContent = n ? `Apply ${n} change${n > 1 ? 's' : ''}` : 'Apply changes';
}
$('pFilter').oninput = () => connected() && renderParams();
$('pApply').onclick = async () => {
  let ok = 0, fail = [], reboot = false;
  for (const [name, val] of Object.entries(edits)) {
    const r = await dev.setParam(name, val);
    if (r.ok) { ok++; if (dev.params.find((p) => p.name === name)?.reboot) reboot = true; } else fail.push(`${name}: ${r.error}`);
  }
  await refreshParams();
  $('pStatus').textContent = `${ok} applied (not yet saved to flash)` + (reboot ? ' — some need a reboot' : '');
  toast(fail.length ? 'Failed: ' + fail.join('; ') : `${ok} applied — click “Save to flash” to keep them`);
};
$('pSave').onclick = async () => {
  if (!connected()) return toast('Not connected');
  const r = await dev.save(); $('pStatus').textContent = r.ok ? 'Saved to flash' : 'Save failed: ' + r.error; toast(r.ok ? 'Saved to flash' : 'Save failed');
};
$('pReload').onclick = async () => connected() && (await refreshParams(), toast('Reloaded from node'));
$('pExport').onclick = () => {
  if (!connected()) return;
  const o = { _format: 'perimeter-sensor-config-v1', ...Object.fromEntries(dev.params.map((p) => [p.name, p.value])) };
  const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([JSON.stringify(o, null, 2)], { type: 'application/json' }));
  a.download = `perimeter-config-${(o['node.name'] || 'node')}.json`; a.click();
};
$('pImport').onclick = () => $('pFile').click();
$('pFile').onchange = async () => {
  if (!connected()) return toast('Connect first');
  try {
    const o = JSON.parse(await $('pFile').files[0].text()); let n = 0;
    for (const p of dev.params) if (p.name in o && String(o[p.name]) !== p.value && !validateParam(p, String(o[p.name]))) { edits[p.name] = String(o[p.name]); n++; }
    renderParams(); toast(`${n} value(s) staged — review, then Apply`);
  } catch { toast('Not a valid config file'); }
  $('pFile').value = '';
};

// -------------------------------------------------------------------- tools
const need = () => (connected() ? true : (toast('Not connected'), false));
$('calStart').onclick = async () => { if (!need()) return; const r = await dev.send('magcal start'); $('calMsg').textContent = r.ok ? 'Rotating… rotate the unit through every orientation' : r.error; };
$('calStop').onclick = async () => {
  if (!need()) return; const r = await dev.send('magcal stop');
  $('calMsg').textContent = r.ok ? 'Done — now Save to flash' : r.error; if (r.ok) await refreshParams();
};
$('radApply').onclick = async () => { if (!need()) return; const r = await dev.send('radar apply', 8000); $('radOut').textContent = r.text || r.error; };
['A', 'B', 'C'].forEach((n, i) => {
  const c = n.toLowerCase(), d = document.createElement('div'); d.className = 'row';
  d.innerHTML = `<b style="color:var(--s${i + 1})">Radar ${n}</b><button class="btn" data-a="ver">Version</button><button class="btn" data-a="on">Eng. mode on</button><button class="btn" data-a="off">off</button><button class="btn" data-a="restart">Restart</button>`;
  d.onclick = async (e) => {
    const a = e.target.dataset?.a; if (!a || !need()) return;
    const cmd = a === 'ver' ? `radar ver ${c}` : a === 'restart' ? `radar restart ${c}` : `radar eng ${c} ${a}`;
    const r = await dev.send(cmd, 3000); $('radOut').textContent = `${n}: ${r.ok ? (r.text || 'OK') : r.error}`;
  };
  $('radRows').append(d);
});
$('i2cBtn').onclick = async () => { if (need()) $('diagOut').textContent = (await dev.send('i2cscan')).text; };
$('statBtn').onclick = async () => { if (need()) $('diagOut').textContent = (await dev.send('status')).text; };
$('gpsRawBtn').onclick = async () => {
  if (!need()) return; const out = []; const h = (e) => out.push(e.detail);
  dev.addEventListener('log', h); await dev.send('gps raw on'); $('diagOut').textContent = 'listening 5 s…';
  await sleep(5000); await dev.send('gps raw off'); dev.removeEventListener('log', h);
  $('diagOut').textContent = out.join('\n') || 'no NMEA received — check GPS wiring, EN pin (LOW = on) and baud';
};
$('btnReboot').onclick = async () => { if (need()) { await dev.send('reboot', 700); } };
$('btnBoot').onclick = async () => { if (need() && confirm('Reboot into the UF2 bootloader? The node will stop until you flash firmware or power-cycle it.')) { await dev.send('bootsel', 700); } };
$('btnArm').onclick = async () => { if (need()) { await dev.send('arm'); toast('Armed at current pose'); } };
$('btnDisarm').onclick = async () => { if (need()) { await dev.send('disarm'); toast('Disarmed'); } };

updateApply(); redrawAll();

if (new URLSearchParams(location.search).has("demo")) $("btnDemo").click();
if (location.hash.length > 1) document.querySelector(`.tab[data-tab="${location.hash.slice(1)}"]`)?.click();
