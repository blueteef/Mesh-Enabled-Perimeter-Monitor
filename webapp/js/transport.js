// Transports: WebSerialTransport (real node over USB) and DemoTransport (simulated
// node for trying the UI with no hardware). Both expose:
//   onData(string)  set by the protocol layer      write(string) -> Promise
//   open() / close()                               onClose() callback

export class WebSerialTransport {
  constructor() { this.port = null; this.onData = null; this.onClose = null; this.reading = false; }
  static supported() { return 'serial' in navigator; }

  async pick() { this.port = await navigator.serial.requestPort(); }

  async open(port = this.port) {
    this.port = port;
    await this.port.open({ baudRate: 115200 });   // baud is ignored by USB CDC but required
    this.enc = new TextEncoder();
    this.writer = this.port.writable.getWriter();
    this.reading = true;
    this.readLoop();
  }

  async readLoop() {
    const dec = new TextDecoder();
    while (this.port.readable && this.reading) {
      const reader = this.port.readable.getReader();
      this.reader = reader;
      try {
        for (;;) {
          const { value, done } = await reader.read();
          if (done) break;
          if (value && this.onData) this.onData(dec.decode(value, { stream: true }));
        }
      } catch { /* device unplugged / reset */ }
      finally { try { reader.releaseLock(); } catch {} }
    }
    this.reading = false;
    if (this.onClose) this.onClose();
  }

  async write(s) { await this.writer.write(this.enc.encode(s)); }

  async close() {
    this.reading = false;
    try { await this.reader?.cancel(); } catch {}
    try { this.writer?.releaseLock(); } catch {}
    try { await this.port?.close(); } catch {}
  }
}

// ---------------------------------------------------------------------------
// Demo node: speaks the same CLI protocol as the firmware, generates plausible data.
// Parameter registry is loaded from the real firmware dump so the UI shows real params.
export class DemoTransport {
  constructor(paramsJson) {
    this.params = JSON.parse(paramsJson);
    this.vals = Object.fromEntries(this.params.map((p) => [p.name, p.value]));
    this.onData = null; this.onClose = null;
    this.streaming = false; this.t0 = performance.now(); this.timer = null;
    this.armed = false; this.dirty = false; this.engineering = [false, false, false];
    this.magCal = false;
  }

  async open() {
    this.timer = setInterval(() => this.tick(), 100);
    setTimeout(() => this.out('perimeter-sensor 0.1.0-dev DEMO ready. Type \'help\'.\n'), 50);
  }
  async close() { clearInterval(this.timer); }

  out(s) { if (this.onData) this.onData(s); }
  ms() { return Math.floor(performance.now() - this.t0); }

  async write(s) {
    const [cmd, ...a] = s.trim().split(/\s+/);
    const o = (x) => this.out(x + '\n');
    switch (cmd) {
      case 'params': o(JSON.stringify(this.params.map((p) => ({ ...p, value: this.vals[p.name] })))); break;
      case 'set': {
        const p = this.params.find((x) => x.name === a[0]);
        if (!p) return o('ERR unknown parameter');
        if (p.type !== 'str' && p.type !== 'bool') { const n = Number(a[1]); if (Number.isNaN(n) || n < p.min || n > p.max) return o('ERR out of range'); }
        this.vals[p.name] = p.type === 'bool' ? (/^(1|on|true)$/i.test(a[1]) ? '1' : '0') : a[1];
        this.dirty = true; o(`${p.name} = ${this.vals[p.name]}`); break;
      }
      case 'save': this.dirty = false; break;
      case 'defaults': this.params.forEach((p) => { if (a[0] === 'all' || a[0] === p.name) this.vals[p.name] = p.def; }); break;
      case 'stream': if (a[0] === 'on') this.streaming = true; else if (a[0] === 'off') this.streaming = false; else if (a[0] === 'once') this.out(JSON.stringify(this.frame()) + '\n'); break;
      case 'ver': o('perimeter-sensor 0.1.0-dev DEMO'); break;
      case 'status': o(`uptime_s=${Math.floor(this.ms() / 1000)}`); o(`node=${this.vals['node.name']}`); o(`cfg_dirty=${this.dirty ? 1 : 0}`); break;
      case 'arm': this.armed = true; break;
      case 'disarm': this.armed = false; break;
      case 'i2cscan': o('0x0D QMC5883L'); o('0x68 BMI323 (SAO)'); o('0x76 BMP280/BME280'); o('3 device(s)'); break;
      case 'radar':
        if (a[0] === 'eng') { this.engineering['abc'.indexOf(a[1])] = a[2] === 'on'; }
        else if (a[0] === 'ver') o('V2.44.25070917');
        else if (a[0] === 'apply') ['A', 'B', 'C'].forEach((c) => o(`${c} applied`));
        else if (!a.length) [0, 1, 2].forEach((i) => o(`${'ABC'[i]} online=1 (demo)`));
        break;
      case 'magcal': if (a[0] === 'stop') { this.vals['mag.off_x'] = '-12'; this.vals['mag.off_y'] = '48'; this.vals['mag.off_z'] = '-30'; this.dirty = true; o('offsets/scales updated in RAM; run \'save\' to keep'); } break;
      case 'reboot': o('rebooting'); break;
      case 'help': o('  (demo node) help params set save stream radar magcal arm disarm i2cscan reboot'); break;
      default: o('ERR unknown command (try \'help\')'); return;
    }
    o('OK');
  }

  frame() {
    const t = this.ms() / 1000;
    const walk = (phase) => { // a person walking in/out of a radar's range
      const d = 300 + 220 * Math.sin(t / 5 + phase);
      const pres = Math.sin(t / 5 + phase) > -0.2;
      return pres ? d : 0;
    };
    const radar = [0, 1, 2].map((i) => {
      const d = walk(i * 2.1), pres = d > 0;
      const o = { on: 1, pres: pres ? 1 : 0, st: pres ? 1 : 0, mcm: Math.round(d), me: pres ? 60 + Math.round(30 * Math.abs(Math.sin(t * 1.3 + i))) : 0, scm: pres ? Math.round(d) : 0, se: pres ? 40 : 0, eng: this.engineering[i] ? 1 : 0 };
      if (this.engineering[i]) {
        o.gm = Array.from({ length: 9 }, (_, g) => (pres ? Math.max(0, Math.round(90 - Math.abs(g * 75 - d) / 3)) : 3));
        o.gs = o.gm.map((x) => Math.round(x * 0.5));
      }
      return o;
    });
    const tilt = this.armed ? 0.5 : 0;
    return {
      t: this.ms(),
      env: { chip: 'BMP280', temp: +(22 + Math.sin(t / 30)).toFixed(2), press: +(853.9 + Math.sin(t / 40)).toFixed(2), hum: null, alt: 1420.4 },
      imu: { ok: 1, state: this.armed ? 'ARMED' : 'DISARMED', ax: +(0.02 * Math.sin(t)).toFixed(3), ay: +(0.01 * Math.cos(t)).toFixed(3), az: 1.0, gx: 0, gy: 0, gz: 0, roll: +(2 * Math.sin(t / 7)).toFixed(1), pitch: +(3 * Math.cos(t / 9)).toFixed(1), tilt, motion: Math.round(4 + 3 * Math.abs(Math.sin(t * 3))) },
      mag: { ok: 1, raw: [500, -200, -1100], heading: +(((t * 12) % 360)).toFixed(1) },
      gps: { fix: 1, sats: 9, lat: 0, lon: 0, alt: 1542.0, hdop: 1.1, kmh: 0, pps: Math.floor(t) },
      radar,
    };
  }

  tick() {
    if (!this.streaming) return;
    const hz = Number(this.vals['sys.stream_hz']) || 5;
    this._n = (this._n || 0) + 1;
    if (this._n % Math.max(1, Math.round(10 / hz)) === 0) this.out(JSON.stringify(this.frame()) + '\n');
    const f = this._prev;
    const cur = this.frame().radar.map((r) => r.pres);
    if (f) cur.forEach((p, i) => {
      if (p && !f[i]) this.out(`EVT ${this.ms()} RADAR_PRESENCE a=${i} v=300 w=70\n`);
      if (!p && f[i]) this.out(`EVT ${this.ms()} RADAR_CLEAR a=${i} v=0 w=0\n`);
    });
    this._prev = cur;
  }
}
