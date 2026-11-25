// Small dependency-free canvas charts. Colors come from CSS variables so light/dark
// themes work. Series colors are bound to the *entity* (radar A is always blue, etc.).

const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();

function setupCanvas(c) {
  if (!c.offsetParent) return null;                 // hidden tab: nothing to draw, don't size the buffer
  if (!c.dataset.h) c.dataset.h = c.getAttribute('height') || 200;
  c.style.height = c.dataset.h + 'px';              // fixed CSS height: stops width/height attrs feeding back
  const dpr = window.devicePixelRatio || 1;
  const r = c.getBoundingClientRect();
  const w = Math.max(10, Math.round(r.width)), h = Math.max(10, Math.round(r.height));
  if (c.width !== w * dpr || c.height !== h * dpr) { c.width = w * dpr; c.height = h * dpr; }
  const g = c.getContext('2d');
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { g, w, h };
}

const niceStep = (span, n) => {
  const raw = span / n, p = Math.pow(10, Math.floor(Math.log10(raw))), f = raw / p;
  return (f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10) * p;
};

// ---------------------------------------------------------------------------
export class LineChart {
  // series: [{key, label, color: '--s1'}], opts: {windowS, unit, min, max, digits}
  constructor(canvas, series, opts = {}) {
    this.c = canvas; this.series = series; this.o = { windowS: 60, unit: '', digits: 2, ...opts };
    this.data = []; this.hover = null;
    canvas.addEventListener('mousemove', (e) => { const r = canvas.getBoundingClientRect(); this.hover = e.clientX - r.left; this.draw(); });
    canvas.addEventListener('mouseleave', () => { this.hover = null; this.draw(); });
  }

  push(tMs, vals) {
    this.data.push({ t: tMs / 1000, v: vals });
    const cut = tMs / 1000 - this.o.windowS - 5;
    while (this.data.length && this.data[0].t < cut) this.data.shift();
  }
  clear() { this.data = []; this.draw(); }

  draw() {
    const cv = setupCanvas(this.c); if (!cv) return;
    const { g, w, h } = cv;
    const L = 46, R = 8, T = 8, B = 20, pw = w - L - R, ph = h - T - B;
    g.clearRect(0, 0, w, h);
    g.font = '11px system-ui, sans-serif';
    const tEnd = this.data.length ? this.data[this.data.length - 1].t : 0, tStart = tEnd - this.o.windowS;
    const vis = this.data.filter((d) => d.t >= tStart);
    let lo = this.o.min ?? Infinity, hi = this.o.max ?? -Infinity;
    if (this.o.min === undefined || this.o.max === undefined)
      for (const d of vis) for (const k of this.series) { const v = d.v[k.key]; if (v != null && !Number.isNaN(v)) { if (this.o.min === undefined) lo = Math.min(lo, v); if (this.o.max === undefined) hi = Math.max(hi, v); } }
    if (!isFinite(lo) || !isFinite(hi)) { lo = 0; hi = 1; }
    if (hi - lo < 1e-9) { if (this.o.min !== undefined) hi = lo + 1; else { lo -= 0.5; hi += 0.5; } }
    const pad = (hi - lo) * 0.08; if (this.o.min === undefined) lo -= pad; if (this.o.max === undefined) hi += pad;
    const step = niceStep(hi - lo, 4);
    const X = (t) => L + ((t - tStart) / this.o.windowS) * pw, Y = (v) => T + ph - ((v - lo) / (hi - lo)) * ph;

    g.strokeStyle = css('--grid'); g.fillStyle = css('--muted'); g.lineWidth = 1; g.textAlign = 'right';
    for (let v = Math.ceil(lo / step) * step; v <= hi; v += step) {
      const y = Math.round(Y(v)) + 0.5;
      g.beginPath(); g.moveTo(L, y); g.lineTo(L + pw, y); g.stroke();
      g.fillText(+v.toFixed(this.o.digits), L - 6, y + 4);
    }
    g.strokeStyle = css('--axis'); g.beginPath(); g.moveTo(L, T + ph + 0.5); g.lineTo(L + pw, T + ph + 0.5); g.stroke();
    g.textAlign = 'center';
    for (let s = 0; s <= this.o.windowS; s += this.o.windowS / 4) g.fillText(s === this.o.windowS ? 'now' : `-${this.o.windowS - s}s`, L + (s / this.o.windowS) * pw, h - 5);

    g.save(); g.beginPath(); g.rect(L, T, pw, ph); g.clip();
    g.lineWidth = 2; g.lineJoin = 'round';
    for (const k of this.series) {
      g.strokeStyle = css(k.color); g.beginPath();
      let pen = false;
      for (const d of vis) {
        const v = d.v[k.key];
        if (v == null || Number.isNaN(v)) { pen = false; continue; }
        const x = X(d.t), y = Y(v);
        if (!pen) { g.moveTo(x, y); pen = true; } else g.lineTo(x, y);
      }
      g.stroke();
    }
    g.restore();

    if (this.hover != null && this.hover >= L && this.hover <= L + pw && vis.length) {
      const t = tStart + ((this.hover - L) / pw) * this.o.windowS;
      let best = vis[0]; for (const d of vis) if (Math.abs(d.t - t) < Math.abs(best.t - t)) best = d;
      const x = X(best.t);
      g.strokeStyle = css('--axis'); g.beginPath(); g.moveTo(x, T); g.lineTo(x, T + ph); g.stroke();
      const rows = this.series.map((k) => ({ k, v: best.v[k.key] })).filter((r) => r.v != null && !Number.isNaN(r.v));
      const bw = 130, bh = 8 + rows.length * 15 + 4, bx = x + 10 + bw > w ? x - 10 - bw : x + 10;
      g.fillStyle = css('--surface-2'); g.strokeStyle = css('--axis'); g.beginPath(); g.roundRect(bx, T + 4, bw, bh, 4); g.fill(); g.stroke();
      g.textAlign = 'left';
      rows.forEach((r, i) => {
        const y = T + 4 + 16 + i * 15;
        g.fillStyle = css(r.k.color); g.beginPath(); g.arc(bx + 10, y - 4, 4, 0, 7); g.fill();
        g.fillStyle = css('--text'); g.fillText(`${r.k.label} ${r.v.toFixed(this.o.digits)}${this.o.unit}`, bx + 20, y);
      });
      for (const r of rows) { g.fillStyle = css(r.k.color); g.strokeStyle = css('--surface'); g.lineWidth = 2; g.beginPath(); g.arc(x, Y(r.v), 4, 0, 7); g.fill(); g.stroke(); }
    }
  }
}

// ---------------------------------------------------------------------------
// Top-down radar view. Sector i is centred at i*120 degrees clockwise from the
// node's forward direction. LD2410 gates are 0.75 m; range ring spacing is 1.5 m.
export class RadarView {
  constructor(canvas) { this.c = canvas; this.state = { radar: [], heading: null }; }
  set(state) { this.state = state; this.draw(); }

  draw() {
    const cv = setupCanvas(this.c); if (!cv) return;
    const { g, w, h } = cv;
    g.clearRect(0, 0, w, h);
    const cx = w / 2, cy = h / 2, maxCm = 640, R = Math.min(w, h) / 2 - 22, k = R / maxCm;
    const SECT = ['--s1', '--s2', '--s3'], names = ['A', 'B', 'C'];
    const ang = (deg) => ((deg - 90) * Math.PI) / 180;       // 0 deg = up

    // beam wedges (+/-60 deg around each sector axis)
    this.state.radar.forEach((r, i) => {
      const c = css(SECT[i]), a0 = ang(i * 120 - 60), a1 = ang(i * 120 + 60);
      g.beginPath(); g.moveTo(cx, cy); g.arc(cx, cy, R, a0, a1); g.closePath();
      g.globalAlpha = r.pres ? 0.22 : 0.07; g.fillStyle = c; g.fill();
      g.globalAlpha = r.on ? 0.55 : 0.2; g.strokeStyle = c; g.lineWidth = 1; g.stroke();
      g.globalAlpha = 1;
    });
    // range rings
    g.strokeStyle = css('--grid'); g.fillStyle = css('--muted'); g.font = '11px system-ui'; g.lineWidth = 1; g.textAlign = 'left';
    for (let ft = 5; ft <= 20; ft += 5) {
      const rr = ft * 30.48 * k;
      g.beginPath(); g.arc(cx, cy, rr, 0, 7); g.stroke();
      g.fillText(`${ft} ft`, cx + 4, cy - rr + 12);
    }
    // targets
    this.state.radar.forEach((r, i) => {
      const c = css(SECT[i]);
      g.fillStyle = css('--text'); g.font = '600 12px system-ui'; g.textAlign = 'center';
      const lx = cx + Math.cos(ang(i * 120)) * (R + 12), ly = cy + Math.sin(ang(i * 120)) * (R + 12);
      g.fillText(names[i], lx, ly + 4);
      if (!r.on || !r.pres) return;
      const d = Math.max(r.mcm || 0, r.scm || 0) || r.mcm || r.scm;
      const p = Math.min(d, maxCm) * k, x = cx + Math.cos(ang(i * 120)) * p, y = cy + Math.sin(ang(i * 120)) * p;
      g.fillStyle = c; g.strokeStyle = css('--surface'); g.lineWidth = 2;
      g.beginPath(); g.arc(x, y, 8, 0, 7); g.fill(); g.stroke();
    });
    // node
    g.fillStyle = css('--text'); g.beginPath(); g.arc(cx, cy, 4, 0, 7); g.fill();
    // north marker from compass
    if (this.state.heading != null) {
      const a = ang(-this.state.heading);
      g.strokeStyle = css('--muted'); g.setLineDash([3, 3]); g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + Math.cos(a) * R, cy + Math.sin(a) * R); g.stroke(); g.setLineDash([]);
      g.fillStyle = css('--muted'); g.textAlign = 'center'; g.font = '600 12px system-ui'; g.fillText('N', cx + Math.cos(a) * (R + 26), cy + Math.sin(a) * (R + 26) + 4);
    }
  }
}

// Compass dial: needle shows the direction the unit's forward axis points.
export class Compass {
  constructor(canvas) { this.c = canvas; this.h = null; }
  set(h) { this.h = h; this.draw(); }
  draw() {
    const cv = setupCanvas(this.c); if (!cv) return;
    const { g, w, h } = cv;
    g.clearRect(0, 0, w, h);
    const cx = w / 2, cy = h / 2, R = Math.min(w, h) / 2 - 16;
    g.strokeStyle = css('--axis'); g.lineWidth = 1.5; g.beginPath(); g.arc(cx, cy, R, 0, 7); g.stroke();
    g.fillStyle = css('--muted'); g.font = '11px system-ui'; g.textAlign = 'center';
    [['N', 0], ['E', 90], ['S', 180], ['W', 270]].forEach(([t, d]) => {
      const a = ((d - 90) * Math.PI) / 180;
      g.fillText(t, cx + Math.cos(a) * (R + 9), cy + Math.sin(a) * (R + 9) + 4);
      g.beginPath(); g.moveTo(cx + Math.cos(a) * (R - 6), cy + Math.sin(a) * (R - 6)); g.lineTo(cx + Math.cos(a) * R, cy + Math.sin(a) * R); g.stroke();
    });
    if (this.h == null || Number.isNaN(this.h)) { g.fillStyle = css('--muted'); g.fillText('no heading', cx, cy + 4); return; }
    const a = ((this.h - 90) * Math.PI) / 180;
    g.fillStyle = css('--s2'); g.beginPath();
    g.moveTo(cx + Math.cos(a) * (R - 8), cy + Math.sin(a) * (R - 8));
    g.lineTo(cx + Math.cos(a + 2.6) * 12, cy + Math.sin(a + 2.6) * 12); g.lineTo(cx + Math.cos(a - 2.6) * 12, cy + Math.sin(a - 2.6) * 12); g.closePath(); g.fill();
    g.fillStyle = css('--text'); g.font = '600 15px system-ui'; g.fillText(`${Math.round(this.h)}°`, cx, cy + R / 2 + 6);
  }
}

// Level indicator: bubble offset shows roll (x) / pitch (y); ring = tamper tilt limit.
export class Level {
  constructor(canvas) { this.c = canvas; this.s = { roll: 0, pitch: 0, limit: 15 }; }
  set(s) { this.s = { ...this.s, ...s }; this.draw(); }
  draw() {
    const cv = setupCanvas(this.c); if (!cv) return;
    const { g, w, h } = cv;
    g.clearRect(0, 0, w, h);
    const cx = w / 2, cy = h / 2, R = Math.min(w, h) / 2 - 6, range = 30;       // +/-30 deg full scale
    g.strokeStyle = css('--axis'); g.lineWidth = 1.5; g.beginPath(); g.arc(cx, cy, R, 0, 7); g.stroke();
    g.strokeStyle = css('--grid'); g.beginPath(); g.moveTo(cx - R, cy); g.lineTo(cx + R, cy); g.moveTo(cx, cy - R); g.lineTo(cx, cy + R); g.stroke();
    g.setLineDash([4, 3]); g.strokeStyle = css('--muted'); g.beginPath(); g.arc(cx, cy, (this.s.limit / range) * R, 0, 7); g.stroke(); g.setLineDash([]);
    const bx = cx + Math.max(-1, Math.min(1, this.s.roll / range)) * R, by = cy + Math.max(-1, Math.min(1, -this.s.pitch / range)) * R;
    g.fillStyle = css('--s1'); g.strokeStyle = css('--surface'); g.lineWidth = 2; g.beginPath(); g.arc(bx, by, 8, 0, 7); g.fill(); g.stroke();
  }
}

// Per-gate energy bars (engineering mode). 9 gates x 2.5 ft.
export class GateBars {
  constructor(canvas) { this.c = canvas; this.r = []; }
  set(radar) { this.r = radar; this.draw(); }
  draw() {
    const cv = setupCanvas(this.c); if (!cv) return;
    const { g, w, h } = cv;
    g.clearRect(0, 0, w, h);
    const L = 30, B = 22, T = 8, pw = w - L - 6, ph = h - T - B;
    g.font = '11px system-ui'; g.fillStyle = css('--muted'); g.strokeStyle = css('--grid'); g.textAlign = 'right';
    [0, 50, 100].forEach((v) => { const y = T + ph - (v / 100) * ph; g.beginPath(); g.moveTo(L, y + 0.5); g.lineTo(L + pw, y + 0.5); g.stroke(); g.fillText(v, L - 4, y + 4); });
    g.textAlign = 'center';
    for (let i = 0; i < 9; i++) g.fillText(`${(i * 0.75).toFixed(1)}`, L + ((i + 0.5) / 9) * pw, h - 6);
    const eng = this.r.map((r, i) => ({ r, i })).filter(({ r }) => r.eng && r.gm);
    if (!eng.length) { g.fillStyle = css('--muted'); g.fillText('Turn on engineering mode for a radar (Tools tab) to see per-gate energy', w / 2, h / 2); return; }
    const colors = ['--s1', '--s2', '--s3'], bw = (pw / 9 - 6) / eng.length;
    eng.forEach(({ r, i }, n) => r.gm.forEach((m, gi) => {
      const x = L + (gi / 9) * pw + 3 + n * bw, bh = (Math.max(m, r.gs[gi]) / 100) * ph;
      g.fillStyle = css(colors[i]); g.beginPath(); g.roundRect(x, T + ph - bh, Math.max(2, bw - 2), bh, [3, 3, 0, 0]); g.fill();
    }));
  }
}
