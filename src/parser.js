'use strict';

const { tokenize } = require('./lexer');
const { JwsError } = require('./errors');

class Parser {
  constructor(source) {
    this.tokens = tokenize(source);
    this.pos = 0;
  }

  peek(offset = 0) {
    return this.tokens[this.pos + offset];
  }

  next() {
    return this.tokens[this.pos++];
  }

  check(type, value) {
    const t = this.peek();
    return t.type === type && (value === undefined || t.value === value);
  }

  match(type, value) {
    if (this.check(type, value)) return this.next();
    return null;
  }

  expect(type, value, what) {
    const t = this.peek();
    if (this.check(type, value)) return this.next();
    const found = t.type === 'eof' ? 'pungkasan berkas' : `"${t.value}"`;
    throw new JwsError(`dikarepake ${what}, nanging ketemu ${found}`, t.line);
  }

  skipEnds() {
    while (this.match('end'));
  }

  parseProgram() {
    const body = [];
    this.skipEnds();
    while (!this.check('eof')) {
      body.push(this.statement());
      this.skipEnds();
    }
    return { type: 'Program', body };
  }

  block() {
    this.expect('op', '{', '"{"');
    const body = [];
    this.skipEnds();
    while (!this.check('op', '}')) {
      if (this.check('eof')) throw new JwsError('blok ora ditutup nganggo "}"', this.peek().line);
      body.push(this.statement());
      this.skipEnds();
    }
    this.next();
    return body;
  }

  statement() {
    const t = this.peek();
    if (t.type === 'keyword') {
      switch (t.value) {
        case 'simpen': {
          this.next();
          const name = this.expect('ident', undefined, 'jeneng variabel').value;
          this.expect('op', '=', '"="');
          return { type: 'Let', name, value: this.expression(), line: t.line };
        }
        case 'tulis': {
          this.next();
          const args = [this.expression()];
          while (this.match('op', ',')) args.push(this.expression());
          return { type: 'Print', args, line: t.line };
        }
        case 'yen':
          this.next();
          return this.ifRest(t.line);
        case 'salawase': {
          this.next();
          const test = this.expression();
          return { type: 'While', test, body: this.block(), line: t.line };
        }
        case 'saben': {
          this.next();
          const name = this.expect('ident', undefined, 'jeneng variabel').value;
          this.expect('keyword', 'ing', '"ing"');
          const iterable = this.expression();
          return { type: 'ForEach', name, iterable, body: this.block(), line: t.line };
        }
        case 'gawe':
          if (this.peek(1).type === 'ident') {
            this.next();
            const name = this.next().value;
            const fn = this.functionRest(name, t.line);
            return { type: 'Let', name, value: fn, line: t.line };
          }
          break;
        case 'balekna': {
          this.next();
          const n = this.peek();
          const bare = n.type === 'end' || n.type === 'eof' || (n.type === 'op' && n.value === '}') || n.line !== t.line;
          return { type: 'Return', value: bare ? null : this.expression(), line: t.line };
        }
        case 'mandheg':
          this.next();
          return { type: 'Break', line: t.line };
        case 'terus':
          this.next();
          return { type: 'Continue', line: t.line };
        default:
          break;
      }
    }

    const expr = this.expression();
    if (this.check('op', '=')) {
      const eq = this.next();
      if (expr.type !== 'Ident' && expr.type !== 'Index') {
        throw new JwsError('sisih kiwa "=" kudu variabel utawa indeks', eq.line);
      }
      return { type: 'Assign', target: expr, value: this.expression(), line: eq.line };
    }
    return { type: 'ExprStmt', expr, line: t.line };
  }

  ifRest(line) {
    const test = this.expression();
    const consequent = this.block();
    let alternate = null;
    if (this.match('keyword', 'liyane')) {
      if (this.match('keyword', 'yen')) alternate = [this.ifRest(this.peek().line)];
      else alternate = this.block();
    }
    return { type: 'If', test, consequent, alternate, line };
  }

  functionRest(name, line) {
    this.expect('op', '(', '"("');
    const params = [];
    if (!this.check('op', ')')) {
      do {
        params.push(this.expect('ident', undefined, 'jeneng parameter').value);
      } while (this.match('op', ','));
    }
    this.expect('op', ')', '")"');
    return { type: 'Function', name, params, body: this.block(), line };
  }

  expression() {
    return this.or();
  }

  or() {
    let left = this.and();
    while (this.check('keyword', 'utawa')) {
      const t = this.next();
      left = { type: 'Logical', op: 'utawa', left, right: this.and(), line: t.line };
    }
    return left;
  }

  and() {
    let left = this.not();
    while (this.check('keyword', 'lan')) {
      const t = this.next();
      left = { type: 'Logical', op: 'lan', left, right: this.not(), line: t.line };
    }
    return left;
  }

  not() {
    if (this.check('keyword', 'ora')) {
      const t = this.next();
      return { type: 'Unary', op: 'ora', arg: this.not(), line: t.line };
    }
    return this.equality();
  }

  binaryLevel(ops, nextLevel) {
    let left = nextLevel();
    while (this.peek().type === 'op' && ops.includes(this.peek().value)) {
      const t = this.next();
      left = { type: 'Binary', op: t.value, left, right: nextLevel(), line: t.line };
    }
    return left;
  }

  equality() {
    return this.binaryLevel(['==', '!='], () => this.comparison());
  }

  comparison() {
    return this.binaryLevel(['<', '<=', '>', '>='], () => this.additive());
  }

  additive() {
    return this.binaryLevel(['+', '-'], () => this.multiplicative());
  }

  multiplicative() {
    return this.binaryLevel(['*', '/', '%'], () => this.unary());
  }

  unary() {
    if (this.check('op', '-')) {
      const t = this.next();
      return { type: 'Unary', op: '-', arg: this.unary(), line: t.line };
    }
    return this.postfix();
  }

  postfix() {
    let expr = this.primary();
    for (;;) {
      // Panggilan lan indeks kudu ing baris sing padha, supaya ukara anyar
      // sing diwiwiti "(" utawa "[" ora kacampur karo ukara sadurunge.
      const t = this.peek();
      if (t.type === 'op' && t.value === '(' && t.line === this.tokens[this.pos - 1].line) {
        this.next();
        const args = [];
        if (!this.check('op', ')')) {
          do {
            args.push(this.expression());
          } while (this.match('op', ','));
        }
        this.expect('op', ')', '")"');
        expr = { type: 'Call', callee: expr, args, line: t.line };
      } else if (t.type === 'op' && t.value === '[' && t.line === this.tokens[this.pos - 1].line) {
        this.next();
        const index = this.expression();
        this.expect('op', ']', '"]"');
        expr = { type: 'Index', object: expr, index, line: t.line };
      } else {
        return expr;
      }
    }
  }

  primary() {
    const t = this.next();
    switch (t.type) {
      case 'number':
      case 'string':
        return { type: 'Literal', value: t.value, line: t.line };
      case 'ident':
        return { type: 'Ident', name: t.value, line: t.line };
      case 'keyword':
        if (t.value === 'bener') return { type: 'Literal', value: true, line: t.line };
        if (t.value === 'luput') return { type: 'Literal', value: false, line: t.line };
        if (t.value === 'kosong') return { type: 'Literal', value: null, line: t.line };
        if (t.value === 'gawe') return this.functionRest(null, t.line);
        break;
      case 'op':
        if (t.value === '(') {
          const expr = this.expression();
          this.expect('op', ')', '")"');
          return expr;
        }
        if (t.value === '[') {
          const items = [];
          if (!this.check('op', ']')) {
            do {
              items.push(this.expression());
            } while (this.match('op', ','));
          }
          this.expect('op', ']', '"]"');
          return { type: 'List', items, line: t.line };
        }
        break;
      default:
        break;
    }
    const found = t.type === 'eof' ? 'pungkasan berkas' : `"${t.value}"`;
    throw new JwsError(`ekspresi ora sah, ketemu ${found}`, t.line);
  }
}

function parse(source) {
  return new Parser(source).parseProgram();
}

module.exports = { parse };
