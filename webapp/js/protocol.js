// Device protocol: turns a raw character stream from the node's USB CLI into
// events + awaitable commands. No DOM / browser APIs here, so it is unit-testable in Node.
//
// Wire format (see firmware src/cli.cpp):
//   command reply   : data lines, then "OK"  or  "ERR <reason>"
//   telemetry       : one JSON object per line, starts with '{'   (stream on)
//   events          : "EVT <ms> <NAME> a=.. v=.. w=.."
// Telemetry/event lines can arrive in the middle of a command reply, so they are
// routed out first and never pollute the reply.

export class Device extends EventTarget {
  constructor(transport) {
    super();
    this.t = transport;
    this.buf = '';
    this.queue = [];       // pending commands {cmd, resolve, timeout, lines, timer}
    this.current = null;
    this.params = [];      // registry from `params`
    this.t.onData = (s) => this.feed(s);
  }

  emit(type, detail) { this.dispatchEvent(new CustomEvent(type, { detail })); }

  // ---- inbound ----------------------------------------------------------
  feed(chunk) {
    this.buf += chunk;
    let i;
    while ((i = this.buf.search(/\r?\n/)) >= 0) {
      const line = this.buf.slice(0, i);
      this.buf = this.buf.slice(this.buf[i] === '\r' ? i + 2 : i + 1);
      this.line(line);
    }
    if (this.buf.length > 8192) this.buf = '';   // runaway line: resync
  }

  line(line) {
    if (!line) return;
    this.emit('raw', line);
    if (line[0] === '{') {
      try { this.emit('telemetry', JSON.parse(line)); return; }
      catch { /* fall through: treat as ordinary text */ }
    }
    if (line.startsWith('EVT ')) { const e = parseEvent(line); if (e) { this.emit('event', e); return; } }
    const c = this.current;
    if (!c) { this.emit('log', line); return; }
    if (line === 'OK') return this.finish({ ok: true });
    if (line.startsWith('ERR')) return this.finish({ ok: false, error: line.slice(3).trim() });
    c.lines.push(line);
  }

  finish(res) {
    const c = this.current;
    if (!c) return;
    clearTimeout(c.timer);
    this.current = null;
    c.resolve({ ...res, lines: c.lines, text: c.lines.join('\n') });
    this.pump();
  }

  // ---- outbound ---------------------------------------------------------
  // Commands run strictly one at a time (the firmware has no request ids).
  send(cmd, timeoutMs = 2500) {
    return new Promise((resolve) => {
      this.queue.push({ cmd, resolve, timeoutMs, lines: [] });
      this.pump();
    });
  }

  pump() {
    if (this.current || !this.queue.length) return;
    const c = this.queue.shift();
    this.current = c;
    c.timer = setTimeout(() => this.finish({ ok: false, error: 'timeout' }), c.timeoutMs);
    this.emit('tx', c.cmd);
    this.t.write(c.cmd + '\n').catch((e) => this.finish({ ok: false, error: String(e) }));
  }

  // ---- high level -------------------------------------------------------
  async loadParams() {
    const r = await this.send('params');
    if (!r.ok) return r;
    const j = r.lines.find((l) => l.startsWith('['));
    try { this.params = JSON.parse(j); } catch { return { ok: false, error: 'bad params JSON' }; }
    return { ok: true, params: this.params };
  }

  async setParam(name, value) { return this.send(`set ${name} ${value}`); }
  save() { return this.send('save', 5000); }
  startStream() { return this.send('stream on'); }
  stopStream() { return this.send('stream off'); }
}

export function parseEvent(line) {
  const m = /^EVT (\d+) (\w+)(.*)$/.exec(line);
  if (!m) return null;
  const e = { ms: Number(m[1]), name: m[2] };
  for (const kv of m[3].trim().split(/\s+/)) {
    const [k, v] = kv.split('=');
    if (k && v !== undefined) e[k] = Number(v);
  }
  return e;
}

// Param helpers shared by UI + tests -----------------------------------------
export function validateParam(p, text) {
  const t = String(text).trim();
  if (p.type === 'bool') return /^(0|1|on|off|true|false)$/i.test(t) ? null : 'use 0/1 or on/off';
  if (p.type === 'str') return t.length > p.max ? `max ${p.max} characters` : (/[=\n]/.test(t) ? 'illegal character' : null);
  const n = Number(t);
  if (t === '' || Number.isNaN(n)) return 'not a number';
  if (n < p.min || n > p.max) return `range ${p.min} .. ${p.max}`;
  if (p.type !== 'f32' && !Number.isInteger(n)) return 'integer required';
  return null;
}

export function groupParams(params) {
  const g = {};
  for (const p of params) (g[p.name.split('.')[0]] ??= []).push(p);
  return g;
}
