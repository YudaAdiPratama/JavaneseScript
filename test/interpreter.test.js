'use strict';

const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const path = require('path');
const { Interpreter } = require('../src/interpreter');

function run(source) {
  const out = [];
  new Interpreter({ output: (s) => out.push(s) }).run(source);
  return out;
}

const example = (name) => fs.readFileSync(path.join(__dirname, '..', 'examples', name), 'utf8');

test('halo donya', () => {
  assert.deepStrictEqual(run(example('halo.jws')), ['Halo donya!']);
});

test('faktorial (rekursi)', () => {
  assert.strictEqual(run(example('faktorial.jws')).at(-1), '6 ! = 720');
});

test('fibonacci (salawase + daftar)', () => {
  assert.deepStrictEqual(run(example('fibonacci.jws')), ['[0, 1, 1, 2, 3, 5, 8, 13, 21, 34]']);
});

test('yen/liyane, mandheg, terus', () => {
  assert.deepStrictEqual(run(example('gasal-genep.jws')), ['1 gasal', '2 genep', '4 genep', '5 gasal', '6 genep', '7 gasal']);
});

test('aritmetika', () => {
  assert.deepStrictEqual(run('tulis 12 + 3 * (2 - 1)'), ['15']);
  assert.deepStrictEqual(run('tulis 7 / 2'), ['3.5']);
});

test('logika: lan, utawa, ora', () => {
  assert.deepStrictEqual(run('tulis bener lan ora luput, luput utawa kosong'), ['bener kosong']);
});

test('closure lan fungsi anonim', () => {
  const src = `
    gawe pembilang() {
      simpen n = 0
      balekna gawe() { n = n + 1; balekna n }
    }
    simpen itung = pembilang()
    itung()
    tulis itung()
  `;
  assert.deepStrictEqual(run(src), ['2']);
});

test('indeks daftar lan tembung', () => {
  const src = 'simpen d = [1, 2, 3]\nd[0] = 9\ntulis d, dawa("halo"), jinis(d), "halo"[1]';
  assert.deepStrictEqual(run(src), ['[9, 2, 3] 4 daftar a']);
});

test('kesalahan nganggo nomer baris', () => {
  assert.throws(() => run('tulis 1\ntulis ora_ana'), /baris 2: variabel "ora_ana" durung ditetepake/);
  assert.throws(() => run('tulis 1 / 0'), /dibagi nol/);
  assert.throws(() => run('yen bener { tulis 1'), /blok ora ditutup/);
});
