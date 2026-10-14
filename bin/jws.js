#!/usr/bin/env node
'use strict';

const fs = require('fs');
const readline = require('readline');
const { Interpreter } = require('../src/interpreter');

const args = process.argv.slice(2);
const files = args.filter((a) => !a.startsWith('--'));

if (args.includes('--help') || args.includes('-h')) {
  console.log('Panganggon: jws [berkas.jws]\nTanpa berkas: mbukak REPL.');
  process.exit(0);
}

if (files.length > 0) {
  const interp = new Interpreter();
  try {
    interp.run(fs.readFileSync(files[0], 'utf8'));
  } catch (e) {
    if (e.name === 'JwsError') {
      console.error(e.message);
      process.exit(1);
    }
    throw e;
  }
} else {
  repl(new Interpreter());
}

function repl(interp) {
  console.log('JavaneseScript - sugeng rawuh! (Ctrl+D kanggo metu)');
  const rl = readline.createInterface({ input: process.stdin, output: process.stdout, prompt: 'jws> ' });
  let buffer = '';
  rl.prompt();
  rl.on('line', (line) => {
    buffer += line + '\n';
    const depth = (buffer.match(/[{[(]/g) || []).length - (buffer.match(/[}\])]/g) || []).length;
    if (depth > 0) {
      rl.setPrompt('...  ');
      rl.prompt();
      return;
    }
    try {
      const result = interp.run(buffer);
      if (result !== null && result !== undefined) console.log(interp.format(result, true));
    } catch (e) {
      console.error(e.name === 'JwsError' ? e.message : e);
    }
    buffer = '';
    rl.setPrompt('jws> ');
    rl.prompt();
  });
  rl.on('close', () => console.log('\nMatur nuwun!'));
}
