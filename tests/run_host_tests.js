const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { runCpp } = require('./host_cpp');

if (!process.env.ARDUINOJSON_HEADER) {
  throw new Error('Set ARDUINOJSON_HEADER to the ArduinoJson v6.21.5 single-header file');
}

const tests = fs.readdirSync(__dirname).filter(name => /\.test\.(cpp|js)$/.test(name) && name !== 'web_browser.test.js').sort();
for (const name of tests) {
  console.log(`\n[TEST] ${name}`);
  const file = path.join(__dirname, name);
  if (name.endsWith('.js')) {
    execFileSync(process.execPath, [file], { stdio: 'inherit' });
    continue;
  }
  const args = ['-DARDUINOJSON_HEADER="' + process.env.ARDUINOJSON_HEADER + '"'];
  if (name === 'sms_storage.test.cpp') {
    const targetFlags = process.env.STORAGE_CXX_ARGS || (path.basename(process.env.CXX || '').startsWith('zig') ? '-target x86-linux-musl' : '-m32');
    args.push(...targetFlags.split(/\s+/).filter(Boolean));
  }
  runCpp(`#include "${file}"\n`, args);
}
console.log(`\nAll ${tests.length} host test files passed.`);