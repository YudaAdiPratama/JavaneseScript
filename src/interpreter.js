'use strict';

const fs = require('fs');
const { parse } = require('./parser');
const { JwsError } = require('./errors');

class ReturnSignal {
  constructor(value) {
    this.value = value;
  }
}
class BreakSignal {}
class ContinueSignal {}

class Environment {
  constructor(parent = null) {
    this.vars = new Map();
    this.parent = parent;
  }

  define(name, value) {
    this.vars.set(name, value);
  }

  find(name) {
    for (let env = this; env; env = env.parent) {
      if (env.vars.has(name)) return env;
    }
    return null;
  }

  get(name, line) {
    const env = this.find(name);
    if (!env) throw new JwsError(`variabel "${name}" durung ditetepake`, line);
    return env.vars.get(name);
  }

  assign(name, value, line) {
    const env = this.find(name);
    if (!env) throw new JwsError(`variabel "${name}" durung ditetepake (gunakna simpen)`, line);
    env.vars.set(name, value);
  }
}

class JwsFunction {
  constructor(node, closure) {
    this.node = node;
    this.closure = closure;
  }
}

class Builtin {
  constructor(name, arity, fn) {
    this.name = name;
    this.arity = arity;
    this.fn = fn;
  }
}

function typeName(v) {
  if (v === null) return 'kosong';
  if (typeof v === 'number') return 'angka';
  if (typeof v === 'string') return 'tembung';
  if (typeof v === 'boolean') return 'logika';
  if (Array.isArray(v)) return 'daftar';
  return 'gawe';
}

function isTruthy(v) {
  return !(v === null || v === false || v === 0 || v === '');
}

class Interpreter {
  constructor({ output = (s) => process.stdout.write(s + '\n'), input } = {}) {
    this.output = output;
    this.input = input || readLineSync;
    this.globals = new Environment();
    this.installBuiltins();
  }

  format(v, nested = false) {
    if (v === null) return 'kosong';
    if (v === true) return 'bener';
    if (v === false) return 'luput';
    if (typeof v === 'number') {
      return Number.isInteger(v) || !Number.isFinite(v) ? String(v) : String(Number(v.toFixed(10)));
    }
    if (typeof v === 'string') return nested ? JSON.stringify(v) : v;
    if (Array.isArray(v)) return '[' + v.map((x) => this.format(x, true)).join(', ') + ']';
    if (v instanceof JwsFunction) return `<gawe ${v.node.name ?? ''}>`.replace(' >', '>');
    if (v instanceof Builtin) return `<gawe ${v.name}>`;
    return String(v);
  }

  installBuiltins() {
    const def = (name, arity, fn) => this.globals.define(name, new Builtin(name, arity, fn));

    // dawa — dawane daftar utawa tembung
    def('dawa', 1, ([x], line) => {
      if (typeof x === 'string' || Array.isArray(x)) return [...x].length;
      throw new JwsError(`dawa butuh tembung utawa daftar, ora ${typeName(x)}`, line);
    });
    // lebokna — nambah isi ing pungkasan daftar
    def('lebokna', 2, ([list, v], line) => {
      if (!Array.isArray(list)) throw new JwsError('lebokna butuh daftar', line);
      list.push(v);
      return list;
    });
    // angka — ngowahi tembung dadi angka
    def('angka', 1, ([x], line) => {
      if (typeof x === 'number') return x;
      const n = Number(String(x).trim());
      if (String(x).trim() === '' || Number.isNaN(n)) throw new JwsError(`"${x}" dudu angka`, line);
      return n;
    });
    // tembung — ngowahi nilai dadi tembung
    def('tembung', 1, ([x]) => this.format(x));
    // urutan — daftar angka saka a nganti sadurunge b
    def('urutan', 2, ([a, b], line) => {
      if (typeof a !== 'number' || typeof b !== 'number') throw new JwsError('urutan butuh angka', line);
      const out = [];
      for (let i = a; i < b; i++) out.push(i);
      return out;
    });
    // waca — maca siji baris saka panganggo
    def('waca', 1, ([prompt]) => {
      if (prompt !== null && prompt !== undefined) process.stdout.write(this.format(prompt));
      return this.input();
    });
    // jinis — jinise nilai
    def('jinis', 1, ([x]) => typeName(x));
  }

  run(source) {
    const program = parse(source);
    let last = null;
    for (const stmt of program.body) last = this.exec(stmt, this.globals);
    return last;
  }

  execBlock(stmts, env) {
    for (const stmt of stmts) this.exec(stmt, env);
  }

  exec(node, env) {
    switch (node.type) {
      case 'Let':
        env.define(node.name, this.eval(node.value, env));
        return null;
      case 'Assign': {
        const value = this.eval(node.value, env);
        if (node.target.type === 'Ident') {
          env.assign(node.target.name, value, node.line);
        } else {
          const obj = this.eval(node.target.object, env);
          const idx = this.eval(node.target.index, env);
          if (!Array.isArray(obj)) throw new JwsError('mung daftar sing bisa diowahi isine', node.line);
          this.checkIndex(obj, idx, node.line);
          obj[idx] = value;
        }
        return null;
      }
      case 'Print':
        this.output(node.args.map((a) => this.format(this.eval(a, env))).join(' '));
        return null;
      case 'If':
        if (isTruthy(this.eval(node.test, env))) this.execBlock(node.consequent, new Environment(env));
        else if (node.alternate) this.execBlock(node.alternate, new Environment(env));
        return null;
      case 'While':
        while (isTruthy(this.eval(node.test, env))) {
          try {
            this.execBlock(node.body, new Environment(env));
          } catch (e) {
            if (e instanceof BreakSignal) break;
            if (e instanceof ContinueSignal) continue;
            throw e;
          }
        }
        return null;
      case 'ForEach': {
        const iterable = this.eval(node.iterable, env);
        let items;
        if (Array.isArray(iterable)) items = [...iterable];
        else if (typeof iterable === 'string') items = [...iterable];
        else throw new JwsError(`ora bisa diulang: ${typeName(iterable)}`, node.line);
        for (const item of items) {
          const inner = new Environment(env);
          inner.define(node.name, item);
          try {
            this.execBlock(node.body, inner);
          } catch (e) {
            if (e instanceof BreakSignal) break;
            if (e instanceof ContinueSignal) continue;
            throw e;
          }
        }
        return null;
      }
      case 'Return':
        throw new ReturnSignal(node.value ? this.eval(node.value, env) : null);
      case 'Break':
        throw new BreakSignal();
      case 'Continue':
        throw new ContinueSignal();
      case 'ExprStmt':
        return this.eval(node.expr, env);
      default:
        throw new JwsError(`ukara ora dikenal: ${node.type}`, node.line);
    }
  }

  checkIndex(list, idx, line) {
    if (!Number.isInteger(idx) || idx < 0 || idx >= list.length) {
      throw new JwsError(`indeks ${this.format(idx)} ing njaba wates (dawa ${this.format(list.length)})`, line);
    }
  }

  eval(node, env) {
    switch (node.type) {
      case 'Literal':
        return node.value;
      case 'Ident':
        return env.get(node.name, node.line);
      case 'List':
        return node.items.map((i) => this.eval(i, env));
      case 'Function':
        return new JwsFunction(node, env);
      case 'Index': {
        const obj = this.eval(node.object, env);
        const idx = this.eval(node.index, env);
        if (typeof obj === 'string') {
          const chars = [...obj];
          this.checkIndex(chars, idx, node.line);
          return chars[idx];
        }
        if (!Array.isArray(obj)) throw new JwsError(`ora bisa diindeks: ${typeName(obj)}`, node.line);
        this.checkIndex(obj, idx, node.line);
        return obj[idx];
      }
      case 'Unary': {
        const v = this.eval(node.arg, env);
        if (node.op === 'ora') return !isTruthy(v);
        if (typeof v !== 'number') throw new JwsError(`"-" butuh angka, ora ${typeName(v)}`, node.line);
        return -v;
      }
      case 'Logical': {
        const left = this.eval(node.left, env);
        if (node.op === 'lan') return isTruthy(left) ? this.eval(node.right, env) : left;
        return isTruthy(left) ? left : this.eval(node.right, env);
      }
      case 'Binary':
        return this.binary(node.op, this.eval(node.left, env), this.eval(node.right, env), node.line);
      case 'Call':
        return this.call(node, env);
      default:
        throw new JwsError(`ekspresi ora dikenal: ${node.type}`, node.line);
    }
  }

  binary(op, a, b, line) {
    switch (op) {
      case '==':
        return deepEqual(a, b);
      case '!=':
        return !deepEqual(a, b);
      case '+':
        if (typeof a === 'number' && typeof b === 'number') return a + b;
        if (typeof a === 'string' || typeof b === 'string') return this.format(a) + this.format(b);
        if (Array.isArray(a) && Array.isArray(b)) return [...a, ...b];
        break;
      default:
        break;
    }
    const comparable = (typeof a === 'number' && typeof b === 'number') || (typeof a === 'string' && typeof b === 'string');
    if (['<', '<=', '>', '>='].includes(op) && comparable) {
      if (op === '<') return a < b;
      if (op === '<=') return a <= b;
      if (op === '>') return a > b;
      return a >= b;
    }
    if (typeof a === 'number' && typeof b === 'number') {
      if (op === '-') return a - b;
      if (op === '*') return a * b;
      if ((op === '/' || op === '%') && b === 0) throw new JwsError('ora bisa dibagi nol', line);
      if (op === '/') return a / b;
      if (op === '%') return a % b;
    }
    if (op === '*' && typeof a === 'string' && Number.isInteger(b) && b >= 0) return a.repeat(b);
    throw new JwsError(`operasi "${op}" ora bisa kanggo ${typeName(a)} lan ${typeName(b)}`, line);
  }

  call(node, env) {
    const callee = this.eval(node.callee, env);
    const args = node.args.map((a) => this.eval(a, env));
    if (callee instanceof Builtin) {
      if (args.length !== callee.arity) {
        throw new JwsError(`${callee.name} butuh ${this.format(callee.arity)} argumen, nanging oleh ${this.format(args.length)}`, node.line);
      }
      return callee.fn(args, node.line);
    }
    if (callee instanceof JwsFunction) {
      const { params, body, name } = callee.node;
      if (args.length !== params.length) {
        throw new JwsError(`${name ?? 'gawe'} butuh ${this.format(params.length)} argumen, nanging oleh ${this.format(args.length)}`, node.line);
      }
      const local = new Environment(callee.closure);
      params.forEach((p, i) => local.define(p, args[i]));
      try {
        this.execBlock(body, local);
      } catch (e) {
        if (e instanceof ReturnSignal) return e.value;
        if (e instanceof BreakSignal || e instanceof ContinueSignal) {
          throw new JwsError('mandheg/terus ing njaba puteran', node.line);
        }
        if (e instanceof RangeError) throw new JwsError('rekursi kakehan (stack overflow)', node.line);
        throw e;
      }
      return null;
    }
    throw new JwsError(`${typeName(callee)} dudu gawe, ora bisa diundang`, node.line);
  }
}

function deepEqual(a, b) {
  if (Array.isArray(a) && Array.isArray(b)) {
    return a.length === b.length && a.every((x, i) => deepEqual(x, b[i]));
  }
  return a === b;
}

function readLineSync() {
  const buf = Buffer.alloc(1);
  const bytes = [];
  for (;;) {
    let n;
    try {
      n = fs.readSync(0, buf, 0, 1, null);
    } catch (e) {
      if (e.code === 'EAGAIN') continue;
      if (e.code === 'EOF') break;
      throw e;
    }
    if (n === 0 || buf[0] === 0x0a) break;
    bytes.push(buf[0]);
  }
  return Buffer.from(bytes).toString('utf8').replace(/\r$/, '');
}

module.exports = { Interpreter };
