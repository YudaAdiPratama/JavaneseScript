'use strict';

class JwsError extends Error {
  constructor(message, line) {
    super(line ? `Kesalahan ing baris ${line}: ${message}` : `Kesalahan: ${message}`);
    this.name = 'JwsError';
    this.line = line;
  }
}

module.exports = { JwsError };
