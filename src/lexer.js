'use strict';

const { KEYWORDS } = require('./keywords');
const { JwsError } = require('./errors');

function isIdentStart(ch) {
  return /[A-Za-z_]/.test(ch);
}

function isDigit(ch) {
  return /[0-9]/.test(ch);
}

function isIdentPart(ch) {
  return isIdentStart(ch) || isDigit(ch);
}

const TWO_CHAR_OPS = new Set(['==', '!=', '<=', '>=']);
const ONE_CHAR_OPS = new Set(['+', '-', '*', '/', '%', '<', '>', '=', '(', ')', '{', '}', '[', ']', ',']);

function tokenize(source) {
  const tokens = [];
  let i = 0;
  let line = 1;

  const push = (type, value) => tokens.push({ type, value, line });

  while (i < source.length) {
    const ch = source[i];

    if (ch === '\n') {
      line++;
      i++;
      continue;
    }
    if (/\s/.test(ch)) {
      i++;
      continue;
    }

    // Komentar: // nganti pungkasan baris
    if (ch === '/' && source[i + 1] === '/') {
      while (i < source.length && source[i] !== '\n') i++;
      continue;
    }

    // Titik koma minangka pamisah ukara (opsional).
    if (ch === ';') {
      push('end', ch);
      i++;
      continue;
    }

    if (isDigit(ch)) {
      let raw = '';
      while (i < source.length && isDigit(source[i])) raw += source[i++];
      if (source[i] === '.' && i + 1 < source.length && isDigit(source[i + 1])) {
        raw += source[i++];
        while (i < source.length && isDigit(source[i])) raw += source[i++];
      }
      push('number', Number(raw));
      continue;
    }

    if (ch === '"') {
      const startLine = line;
      let value = '';
      i++;
      while (i < source.length && source[i] !== '"') {
        let c = source[i];
        if (c === '\n') line++;
        if (c === '\\') {
          i++;
          const esc = source[i];
          const map = { n: '\n', t: '\t', '"': '"', '\\': '\\' };
          if (!(esc in map)) throw new JwsError(`escape ora dikenal: \\${esc ?? ''}`, line);
          c = map[esc];
        }
        value += c;
        i++;
      }
      if (i >= source.length) throw new JwsError('tembung (string) ora ditutup', startLine);
      i++;
      tokens.push({ type: 'string', value, line: startLine });
      continue;
    }

    if (isIdentStart(ch)) {
      let name = '';
      while (i < source.length && isIdentPart(source[i])) name += source[i++];
      if (KEYWORDS.has(name)) push('keyword', name);
      else push('ident', name);
      continue;
    }

    const two = source.slice(i, i + 2);
    if (TWO_CHAR_OPS.has(two)) {
      push('op', two);
      i += 2;
      continue;
    }
    if (ONE_CHAR_OPS.has(ch)) {
      push('op', ch);
      i++;
      continue;
    }

    throw new JwsError(`karakter ora dikenal: "${ch}"`, line);
  }

  push('eof', null);
  return tokens;
}

module.exports = { tokenize };
