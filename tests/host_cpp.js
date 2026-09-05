const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

function extractFunction(source, signature) {
  const start = source.indexOf(signature);
  if (start < 0) throw new Error(`Missing production function: ${signature}`);
  const end = source.indexOf('\n}', start);
  if (end < 0) throw new Error(`Unterminated function: ${signature}`);
  return source.slice(start, end + 2);
}

function runCpp(harness, extraArgs = []) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'sms-host-test-'));
  const executable = path.join(directory, 'test');
  try {
    const compiler = process.env.CXX || 'c++';
    const args = (process.env.CXX_ARGS || '').split(/\s+/).filter(Boolean);
    execFileSync(compiler, [...args, '-std=c++17', '-Wall', '-Wextra', '-I', path.join(__dirname, 'support'), ...extraArgs,
      '-x', 'c++', '-', '-o', executable], { input: harness, stdio: ['pipe', 'inherit', 'inherit'] });
    execFileSync(executable, { stdio: 'inherit' });
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
}

module.exports = { extractFunction, runCpp };