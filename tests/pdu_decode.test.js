/* eslint-disable no-console */
const assert = (cond, msg) => {
  if (!cond) {
    throw new Error(msg || 'assertion failed');
  }
};

const basicMap = (() => {
  const m = new Array(128).fill('');
  const set = (code, ch) => { m[code] = ch; };
  set(0x00, '@'); set(0x01, '£'); set(0x02, '$'); set(0x03, '¥');
  set(0x04, 'è'); set(0x05, 'é'); set(0x06, 'ù'); set(0x07, 'ì');
  set(0x08, 'ò'); set(0x09, 'Ç'); set(0x0A, '\n'); set(0x0B, 'Ø');
  set(0x0C, 'ø'); set(0x0D, '\r'); set(0x0E, 'Å'); set(0x0F, 'å');
  set(0x10, 'Δ'); set(0x11, '_'); set(0x12, 'Φ'); set(0x13, 'Γ');
  set(0x14, 'Λ'); set(0x15, 'Ω'); set(0x16, 'Π'); set(0x17, 'Ψ');
  set(0x18, 'Σ'); set(0x19, 'Θ'); set(0x1A, 'Ξ'); set(0x1C, 'Æ');
  set(0x1D, 'æ'); set(0x1E, 'ß'); set(0x1F, 'É'); set(0x24, '¤');
  set(0x40, '¡'); set(0x5B, 'Ä'); set(0x5C, 'Ö'); set(0x5D, 'Ñ');
  set(0x5E, 'Ü'); set(0x5F, '§'); set(0x60, '¿'); set(0x7B, 'ä');
  set(0x7C, 'ö'); set(0x7D, 'ñ'); set(0x7E, 'ü'); set(0x7F, 'à');
  for (let i = 0x20; i <= 0x7E; i++) {
    if (!m[i]) m[i] = String.fromCharCode(i);
  }
  return m;
})();

const extMap = new Map([
  ['\f', 0x0A],
  ['^', 0x14],
  ['{', 0x28],
  ['}', 0x29],
  ['\\', 0x2F],
  ['[', 0x3C],
  ['~', 0x3D],
  [']', 0x3E],
  ['|', 0x40],
  ['€', 0x65],
]);

const basicReverse = new Map();
for (let i = 0; i < basicMap.length; i++) {
  if (basicMap[i]) basicReverse.set(basicMap[i], i);
}

function encode7bitWithOffset(text, skipBits = 0) {
  const septets = [];
  for (const ch of text) {
    if (extMap.has(ch)) {
      septets.push(0x1B, extMap.get(ch));
      continue;
    }
    if (basicReverse.has(ch)) {
      septets.push(basicReverse.get(ch));
      continue;
    }
    septets.push(0x3F); // '?'
  }
  return { hex: packSeptets(septets, skipBits), septetCount: septets.length };
}

function packSeptets(septets, skipBits) {
  const totalBits = septets.length * 7 + skipBits;
  const byteLen = Math.ceil(totalBits / 8);
  const bytes = new Uint8Array(byteLen);
  for (let i = 0; i < septets.length; i++) {
    const v = septets[i] & 0x7F;
    for (let b = 0; b < 7; b++) {
      if (v & (1 << b)) {
        const bitIndex = i * 7 + skipBits + b;
        const byteIndex = Math.floor(bitIndex / 8);
        const bitOffset = bitIndex % 8;
        bytes[byteIndex] |= (1 << bitOffset);
      }
    }
  }
  return Buffer.from(bytes).toString('hex').toUpperCase();
}

function decode7bitWithOffset(hexData, septetCount, skipBits) {
  const bytes = Buffer.from(hexData, 'hex');
  let out = '';
  let i = 0;
  while (i < septetCount) {
    const bitIndex = i * 7 + skipBits;
    const byteIndex = Math.floor(bitIndex / 8);
    const bitOffset = bitIndex % 8;
    if (byteIndex >= bytes.length) break;
    let v = 0;
    if (bitOffset <= 1) {
      v = (bytes[byteIndex] >> bitOffset) & 0x7F;
    } else {
      v = (bytes[byteIndex] >> bitOffset) & (0x7F >> (bitOffset - 1));
      if (byteIndex + 1 < bytes.length) {
        v |= (bytes[byteIndex + 1] << (8 - bitOffset)) & 0x7F;
      }
    }
    if (v === 0x1B) {
      if (i + 1 < septetCount) {
        const nextBitIndex = (i + 1) * 7 + skipBits;
        const nextByteIndex = Math.floor(nextBitIndex / 8);
        const nextBitOffset = nextBitIndex % 8;
        let ext = 0;
        if (nextByteIndex < bytes.length) {
          if (nextBitOffset <= 1) {
            ext = (bytes[nextByteIndex] >> nextBitOffset) & 0x7F;
          } else {
            ext = (bytes[nextByteIndex] >> nextBitOffset) & (0x7F >> (nextBitOffset - 1));
            if (nextByteIndex + 1 < bytes.length) {
              ext |= (bytes[nextByteIndex + 1] << (8 - nextBitOffset)) & 0x7F;
            }
          }
        }
        const extChar = [...extMap.entries()].find(([, code]) => code === ext)?.[0] || '';
        out += extChar;
        i += 2;
        continue;
      }
    }
    out += basicMap[v] || '';
    i += 1;
  }
  return out;
}

function decodeUCS2BE(hex) {
  let out = '';
  for (let i = 0; i + 3 < hex.length; i += 4) {
    const code = parseInt(hex.slice(i, i + 4), 16);
    if (!code) continue;
    out += String.fromCharCode(code);
  }
  return out;
}

function decode8Bit(hex) {
  let out = '';
  for (let i = 0; i + 1 < hex.length; i += 2) {
    const b = parseInt(hex.slice(i, i + 2), 16);
    if (b === 0x0A) out += '\n';
    else if (b === 0x0D) out += '\r';
    else if (b >= 32 && b <= 126) out += String.fromCharCode(b);
    else out += '.';
  }
  return out;
}

const tests = [];

tests.push(() => {
  const text = 'hello';
  const encoded = encode7bitWithOffset(text, 0);
  const decoded = decode7bitWithOffset(encoded.hex, encoded.septetCount, 0);
  assert(decoded === text, `7bit basic failed: ${decoded}`);
});

tests.push(() => {
  const text = '^{}[]~|€\\';
  const encoded = encode7bitWithOffset(text, 0);
  const decoded = decode7bitWithOffset(encoded.hex, encoded.septetCount, 0);
  assert(decoded === text, `7bit ext failed: ${decoded}`);
});

tests.push(() => {
  const udhl = 0x05;
  const udhBytes = 1 + udhl;
  const skipBits = (udhBytes * 8) % 7;
  const text = 'PART1';
  const encoded = encode7bitWithOffset(text, skipBits);
  const decoded = decode7bitWithOffset(encoded.hex, encoded.septetCount, skipBits);
  assert(decoded === text, `7bit UDH offset failed: ${decoded}`);
});

tests.push(() => {
  const hex = '4F60597D';
  const decoded = decodeUCS2BE(hex);
  assert(decoded === '你好', `UCS2 failed: ${decoded}`);
});

tests.push(() => {
  const hex = '48656C6C6F';
  const decoded = decode8Bit(hex);
  assert(decoded === 'Hello', `8bit failed: ${decoded}`);
});

console.log(`Running ${tests.length} PDU decode tests...`);
tests.forEach((t, i) => {
  t();
  console.log(`ok ${i + 1}`);
});
console.log('All tests passed.');
