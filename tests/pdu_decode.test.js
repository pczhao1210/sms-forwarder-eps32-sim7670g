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
    if (code >= 0xD800 && code <= 0xDBFF && i + 7 < hex.length) {
      const low = parseInt(hex.slice(i + 4, i + 8), 16);
      if (low >= 0xDC00 && low <= 0xDFFF) {
        const codePoint = 0x10000 + (((code - 0xD800) << 10) | (low - 0xDC00));
        out += String.fromCodePoint(codePoint);
        i += 4;
        continue;
      }
    }
    if (code >= 0xDC00 && code <= 0xDFFF) {
      out += '\uFFFD';
      continue;
    }
    out += String.fromCodePoint(code);
  }
  return out;
}

function encodeUCS2BE(text) {
  let hex = '';
  for (let i = 0; i < text.length; i += 1) {
    const codeUnit = text.charCodeAt(i);
    hex += codeUnit.toString(16).toUpperCase().padStart(4, '0');
  }
  return hex;
}

function decode8Bit(hex) {
  let out = '';
  const cp1252Map = [
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
  ];
  for (let i = 0; i + 1 < hex.length; i += 2) {
    const b = parseInt(hex.slice(i, i + 2), 16);
    if (b === 0x0A) out += '\n';
    else if (b === 0x0D) out += '\r';
    else if (b >= 32 && b <= 126) out += String.fromCharCode(b);
    else if (b >= 0x80 && b <= 0x9F) {
      const mapped = cp1252Map[b - 0x80];
      out += (mapped >= 0x20 && ![0x0081, 0x008D, 0x008F, 0x0090, 0x009D].includes(mapped))
        ? String.fromCodePoint(mapped)
        : '.';
    } else if (b >= 0xA0) {
      out += String.fromCodePoint(b);
    } else out += '.';
  }
  return out;
}

function parseConcatInfo(hex) {
  let pos = hex.indexOf('050003');
  if (pos >= 0 && pos + 12 <= hex.length) {
    return {
      refNum: parseInt(hex.slice(pos + 6, pos + 8), 16),
      totalParts: parseInt(hex.slice(pos + 8, pos + 10), 16),
      currentPart: parseInt(hex.slice(pos + 10, pos + 12), 16),
    };
  }

  pos = hex.indexOf('060804');
  if (pos >= 0 && pos + 14 <= hex.length) {
    return {
      refNum: parseInt(hex.slice(pos + 6, pos + 10), 16),
      totalParts: parseInt(hex.slice(pos + 10, pos + 12), 16),
      currentPart: parseInt(hex.slice(pos + 12, pos + 14), 16),
    };
  }

  return { refNum: 0, totalParts: 0, currentPart: 0 };
}

function normalizeSender(sender) {
  const s = String(sender || '').trim();
  if (!s) return 'Unknown';
  let hasLetter = false;
  let hasNonAscii = false;
  let digits = '';
  const keepPlus = s.startsWith('+');
  for (const ch of s) {
    const code = ch.codePointAt(0);
    if (code >= 0x30 && code <= 0x39) digits += ch;
    else if ((code >= 0x41 && code <= 0x5A) || (code >= 0x61 && code <= 0x7A)) hasLetter = true;
    else if (code > 0x7F) hasNonAscii = true;
  }
  if (hasLetter || hasNonAscii) return s;
  if (digits.length >= 3) return keepPlus ? `+${digits}` : digits;
  return 'Unknown';
}

function hexByte(value) {
  return Number(value).toString(16).toUpperCase().padStart(2, '0');
}

function encodeSemiOctetNumber(sender) {
  const digits = String(sender || '').replace(/[^0-9]/g, '');
  const padded = (digits.length % 2 === 0) ? digits : `${digits}F`;
  let out = '';
  for (let i = 0; i < padded.length; i += 2) {
    out += padded[i + 1] + padded[i];
  }
  return { digits, encoded: out };
}

function buildConcatUdh(refNum, totalParts, currentPart, use16Bit = false) {
  if (use16Bit) {
    return `060804${Number(refNum).toString(16).toUpperCase().padStart(4, '0')}${hexByte(totalParts)}${hexByte(currentPart)}`;
  }
  return `050003${hexByte(refNum)}${hexByte(totalParts)}${hexByte(currentPart)}`;
}

function buildSmsDeliverPdu({ sender, dcs, text = '', userDataHex = '', udhHex = '' }) {
  const address = encodeSemiOctetNumber(sender);
  const firstOctet = udhHex ? 0x44 : 0x04;
  let payloadHex = '';
  let udl = 0;

  if (dcs === 0x00) {
    const udhBytes = udhHex.length / 2;
    const fillBits = udhBytes > 0 ? (7 - ((udhBytes * 8) % 7)) % 7 : 0;
    const encoded = encode7bitWithOffset(text, fillBits);
    payloadHex = `${udhHex}${encoded.hex}`;
    const udhSeptets = udhBytes > 0 ? Math.ceil((udhBytes * 8) / 7) : 0;
    udl = encoded.septetCount + udhSeptets;
  } else if (dcs === 0x08) {
    payloadHex = `${udhHex}${userDataHex || encodeUCS2BE(text)}`;
    udl = payloadHex.length / 2;
  } else if (dcs === 0x04) {
    payloadHex = `${udhHex}${userDataHex}`;
    udl = payloadHex.length / 2;
  } else {
    throw new Error(`unsupported DCS: ${dcs}`);
  }

  return [
    '00',
    hexByte(firstOctet),
    hexByte(address.digits.length),
    sender.startsWith('+') ? '91' : '81',
    address.encoded,
    '00',
    hexByte(dcs),
    '00000000000000',
    hexByte(udl),
    payloadHex,
  ].join('');
}

function parsePdu(pduData) {
  const bytes = Buffer.from(pduData, 'hex');
  const info = {
    sender: '',
    dcs: 0,
    hasUDH: false,
    refNum: 0,
    totalParts: 0,
    currentPart: 0,
    userDataHex: '',
    septetCount: 0,
    skipBits: 0,
  };
  if (bytes.length < 10) return info;

  let idx = 0;
  const smscLen = bytes[idx++];
  idx += smscLen;
  if (idx >= bytes.length) return info;

  const firstOctet = bytes[idx++];
  info.hasUDH = (firstOctet & 0x40) !== 0;

  if (idx >= bytes.length) return info;
  const senderLen = bytes[idx++];
  if (idx >= bytes.length) return info;
  const toa = bytes[idx++];
  const senderBytes = Math.ceil(senderLen / 2);
  if (idx + senderBytes > bytes.length) return info;

  let sender = '';
  for (let i = 0; i < senderBytes && sender.length < senderLen; i += 1) {
    const value = bytes[idx + i];
    const lo = value & 0x0F;
    const hi = (value >> 4) & 0x0F;
    if (lo <= 9 && sender.length < senderLen) sender += String(lo);
    if (hi <= 9 && hi !== 0x0F && sender.length < senderLen) sender += String(hi);
  }
  if ((toa & 0x90) === 0x90) sender = `+${sender}`;
  info.sender = normalizeSender(sender);
  idx += senderBytes;

  if (idx + 9 > bytes.length) return info;
  idx += 1; // PID
  info.dcs = bytes[idx++];
  idx += 7; // SCTS
  if (idx >= bytes.length) return info;

  const udl = bytes[idx++];
  const is7bit = (info.dcs & 0x0C) === 0x00;
  const udBytes = is7bit ? Math.ceil((udl * 7) / 8) : udl;
  if (idx + udBytes > bytes.length) return info;

  let userDataBytes = bytes.slice(idx, idx + udBytes);
  info.septetCount = is7bit ? udl : 0;

  if (info.hasUDH && userDataBytes.length > 0) {
    const udhl = userDataBytes[0];
    const udhBytes = 1 + udhl;
    if (udhBytes <= userDataBytes.length) {
      const udh = userDataBytes.slice(0, udhBytes);
      userDataBytes = userDataBytes.slice(udhBytes);
      if (is7bit) {
        info.skipBits = (7 - ((udhBytes * 8) % 7)) % 7;
        info.septetCount = udl - Math.ceil((udhBytes * 8) / 7);
      }
      for (let pos = 1; pos + 1 < udh.length;) {
        const iei = udh[pos];
        const iedl = udh[pos + 1];
        if (pos + 1 + iedl >= udh.length + 1) break;
        if (iei === 0x00 && iedl === 0x03 && pos + 4 < udh.length) {
          info.refNum = udh[pos + 2];
          info.totalParts = udh[pos + 3];
          info.currentPart = udh[pos + 4];
          break;
        }
        if (iei === 0x08 && iedl === 0x04 && pos + 5 < udh.length) {
          info.refNum = (udh[pos + 2] << 8) | udh[pos + 3];
          info.totalParts = udh[pos + 4];
          info.currentPart = udh[pos + 5];
          break;
        }
        pos += 2 + iedl;
      }
    }
  }

  info.userDataHex = Buffer.from(userDataBytes).toString('hex').toUpperCase();
  return info;
}

function extractContentFromPdu(pduData) {
  const info = parsePdu(pduData);
  if (!info.userDataHex) return '';
  if ((info.dcs & 0x0C) === 0x08) return decodeUCS2BE(info.userDataHex);
  if ((info.dcs & 0x0C) === 0x04) return decode8Bit(info.userDataHex);
  const sevenBitText = decode7bitWithOffset(info.userDataHex, info.septetCount, info.skipBits);
  const ucs2Text = decodeUCS2BE(info.userDataHex);
  return shouldPreferUcs2(sevenBitText, ucs2Text) ? ucs2Text : sevenBitText;
}

function assembleConcatPdus(pdus) {
  const sessions = new Map();
  for (const pdu of pdus) {
    const info = parsePdu(pdu);
    const content = extractContentFromPdu(pdu);
    if (!info.totalParts || !info.currentPart) {
      sessions.set('single', content);
      continue;
    }
    const key = `${info.sender}:${info.refNum}`;
    if (!sessions.has(key)) {
      sessions.set(key, { total: info.totalParts, parts: new Array(info.totalParts).fill('') });
    }
    const session = sessions.get(key);
    session.parts[info.currentPart - 1] = content;
  }

  const output = [];
  for (const value of sessions.values()) {
    if (typeof value === 'string') {
      output.push(value);
    } else {
      output.push(value.parts.join(''));
    }
  }
  return output;
}

function splitIntoParts(text, totalParts) {
  const parts = [];
  for (let index = 0; index < totalParts; index += 1) {
    const start = Math.floor((text.length * index) / totalParts);
    const end = Math.floor((text.length * (index + 1)) / totalParts);
    parts.push(text.slice(start, end));
  }
  return parts;
}

function splitIntoCodePointParts(text, totalParts) {
  const codePoints = Array.from(text);
  const parts = [];
  for (let index = 0; index < totalParts; index += 1) {
    const start = Math.floor((codePoints.length * index) / totalParts);
    const end = Math.floor((codePoints.length * (index + 1)) / totalParts);
    parts.push(codePoints.slice(start, end).join(''));
  }
  return parts;
}

function countOccurrences(text, token) {
  let count = 0;
  let pos = 0;
  while (true) {
    const found = text.indexOf(token, pos);
    if (found < 0) break;
    count += 1;
    pos = found + token.length;
  }
  return count;
}

function countUtf8CjkLeadBytes(text) {
  const bytes = Buffer.from(text, 'utf8');
  let count = 0;
  for (const b of bytes) {
    if (b >= 0xE4 && b <= 0xE9) count += 1;
  }
  return count;
}

function countGsmArtifactChars(text) {
  const artifacts = ['£','¥','è','é','ù','ì','ò','Ç','Ø','ø','Å','å','Δ','Φ','Γ','Λ','Ω','Π','Ψ','Σ','Θ','Ξ','Æ','æ','ß','É','¤','¡','Ä','Ö','Ñ','Ü','§','¿','ä','ö','ñ','ü','à'];
  return artifacts.reduce((sum, token) => sum + countOccurrences(text, token), 0);
}

function countUnicodeReplacementChars(text) {
  return countOccurrences(text, '�');
}

function countVisibleUnicodeChars(text) {
  const bytes = Buffer.from(text, 'utf8');
  let count = 0;
  for (const b of bytes) {
    if ((b >= 32 && b <= 126) || b === 0x0A || b === 0x0D || b === 0x09) count += 1;
    else if ((b & 0xC0) === 0x80) continue;
    else if (b >= 0xC2) count += 1;
  }
  return count;
}

function countNonAsciiUnicodeChars(text) {
  const bytes = Buffer.from(text, 'utf8');
  let count = 0;
  for (const b of bytes) {
    if ((b & 0xC0) === 0x80) continue;
    if (b >= 0xC2) count += 1;
  }
  return count;
}

function shouldPreferUcs2(sevenBitText, ucs2Text) {
  if (!ucs2Text) return false;
  if (!sevenBitText) return true;
  const sevenCjk = countUtf8CjkLeadBytes(sevenBitText);
  const ucs2Cjk = countUtf8CjkLeadBytes(ucs2Text);
  const sevenArtifacts = countGsmArtifactChars(sevenBitText);
  const ucs2Replacement = countUnicodeReplacementChars(ucs2Text);
  const sevenReplacement = countUnicodeReplacementChars(sevenBitText);
  const ucs2Visible = countVisibleUnicodeChars(ucs2Text);
  const sevenVisible = countVisibleUnicodeChars(sevenBitText);
  const ucs2NonAscii = countNonAsciiUnicodeChars(ucs2Text);
  if (ucs2Cjk >= 2 && sevenCjk === 0 && sevenArtifacts >= 4) return true;
  if (ucs2NonAscii >= 2 && sevenArtifacts >= 4 && ucs2Replacement <= sevenReplacement) return true;
  if (ucs2Visible >= 6 && sevenArtifacts >= 6 && ucs2Visible > sevenVisible && ucs2Replacement === 0) return true;
  return false;
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
  const skipBits = (7 - ((udhBytes * 8) % 7)) % 7;
  const text = 'PART1';
  const encoded = encode7bitWithOffset(text, skipBits);
  const decoded = decode7bitWithOffset(encoded.hex, encoded.septetCount, skipBits);
  assert(decoded === text, `7bit UDH offset failed: ${decoded}`);
});

tests.push(() => {
  const udhl = 0x05;
  const udhBytes = 1 + udhl;
  const fillBits = (7 - ((udhBytes * 8) % 7)) % 7;
  const wrongSkipBits = (udhBytes * 8) % 7;
  const text = 'Welcome to CHINA. Top up your account';
  const encoded = encode7bitWithOffset(text, fillBits);
  const decoded = decode7bitWithOffset(encoded.hex, encoded.septetCount, fillBits);
  const wrongDecoded = decode7bitWithOffset(encoded.hex, encoded.septetCount, wrongSkipBits);
  assert(decoded === text, `7bit long UDH decode failed: ${decoded}`);
  assert(wrongDecoded !== text, 'wrong UDH bit offset unexpectedly decoded correctly');
});

tests.push(() => {
  const hex = '4F60597D';
  const decoded = decodeUCS2BE(hex);
  assert(decoded === '你好', `UCS2 failed: ${decoded}`);
});

tests.push(() => {
  const text = '歡迎使用簡訊服務';
  const hex = encodeUCS2BE(text);
  const decoded = decodeUCS2BE(hex);
  assert(decoded === text, `UCS2 traditional Chinese failed: ${decoded}`);
});

tests.push(() => {
  const hex = 'D83DDE00';
  const decoded = decodeUCS2BE(hex);
  assert(decoded === '😀', `UCS2 emoji failed: ${decoded}`);
});

tests.push(() => {
  const hex = '48656C6C6F';
  const decoded = decode8Bit(hex);
  assert(decoded === 'Hello', `8bit failed: ${decoded}`);
});

tests.push(() => {
  const hex = '809394A0E9';
  const decoded = decode8Bit(hex);
  const expected = String.fromCodePoint(0x20AC, 0x201C, 0x201D, 0x00A0, 0x00E9);
  assert(decoded === expected, `8bit cp1252 failed: ${decoded}`);
});

tests.push(() => {
  const part1 = '4F60597D';
  const part2 = '65E5672C';
  const decoded = decodeUCS2BE(part1) + decodeUCS2BE(part2);
  assert(decoded === '你好日本', `UCS2 long concat failed: ${decoded}`);
});

tests.push(() => {
  const fullText = '歡迎來到台灣，請留意您的漫遊設定並保持網路連線，以便接收重要通知。祝您旅途平安順利。';
  const splitIndex = 18;
  const part1 = encodeUCS2BE(fullText.slice(0, splitIndex));
  const part2 = encodeUCS2BE(fullText.slice(splitIndex));
  const decoded = decodeUCS2BE(part1) + decodeUCS2BE(part2);
  assert(decoded === fullText, `UCS2 long traditional Chinese failed: ${decoded}`);
});

tests.push(() => {
  const info8 = parseConcatInfo('AA0500037B0201BB');
  assert(info8.refNum === 0x7B && info8.totalParts === 2 && info8.currentPart === 1,
    `8-bit concat parse failed: ${JSON.stringify(info8)}`);

  const info16 = parseConcatInfo('AA06080412340201BB');
  assert(info16.refNum === 0x1234 && info16.totalParts === 2 && info16.currentPart === 1,
    `16-bit concat parse failed: ${JSON.stringify(info16)}`);
});

tests.push(() => {
  assert(normalizeSender('Почта') === 'Почта', 'non-ASCII sender should be preserved');
  assert(normalizeSender('ギフト') === 'ギフト', 'Japanese sender should be preserved');
  assert(normalizeSender('+8613800138000') === '+8613800138000', 'numeric sender normalization failed');
});

tests.push(() => {
  const sevenBitGarbage = 'Πg¥é2Ψc3£ÑΨ3å?7Ψ¥Q';
  const russian = 'Привет мир';
  assert(shouldPreferUcs2(sevenBitGarbage, russian) === true, 'fallback should prefer UCS2 for non-Latin readable text');
  const traditionalChinese = '歡迎來到台灣，請保持聯絡';
  assert(shouldPreferUcs2(sevenBitGarbage, traditionalChinese) === true, 'fallback should prefer UCS2 for traditional Chinese text');
  assert(shouldPreferUcs2('Normal ASCII text', 'Normal ASCII text') === false, 'fallback should not switch good ASCII text');
});

tests.push(() => {
  const sender = '+447700900123';
  const text = 'Verification code 974296. Do not share it.';
  const pdu = buildSmsDeliverPdu({ sender, dcs: 0x00, text });
  const info = parsePdu(pdu);
  const decoded = extractContentFromPdu(pdu);
  assert(info.sender === sender, `full PDU 7bit sender failed: ${info.sender}`);
  assert(decoded === text, `full PDU 7bit content failed: ${decoded}`);
});

tests.push(() => {
  const sender = '+886912345678';
  const text = '歡迎使用繁體中文簡訊通知';
  const pdu = buildSmsDeliverPdu({ sender, dcs: 0x08, text });
  const info = parsePdu(pdu);
  const decoded = extractContentFromPdu(pdu);
  assert(info.sender === sender, `full PDU UCS2 sender failed: ${info.sender}`);
  assert(decoded === text, `full PDU UCS2 content failed: ${decoded}`);
});

tests.push(() => {
  const sender = '+33123456789';
  const userDataHex = '809394A0E9';
  const pdu = buildSmsDeliverPdu({ sender, dcs: 0x04, userDataHex });
  const decoded = extractContentFromPdu(pdu);
  const expected = String.fromCodePoint(0x20AC, 0x201C, 0x201D, 0x00A0, 0x00E9);
  assert(decoded === expected, `full PDU 8bit content failed: ${decoded}`);
});

tests.push(() => {
  const sender = '+447700900456';
  const text = 'Welcome to CHINA. Top up your account and keep in touch with your friends and family while on your travels. Calls cost £1/min to make and receive, 30p/text and data is 20p/MB. Top up your account at giffgaff.com/top-up Safe travels. Enjoy your stay and keep this notice for reference.';
  const parts = splitIntoParts(text, 3);
  const pdus = parts.map((part, index) => buildSmsDeliverPdu({
    sender,
    dcs: 0x00,
    text: part,
    udhHex: buildConcatUdh(0x7B, 3, index + 1, false),
  }));
  const assembled = assembleConcatPdus([pdus[1], pdus[0], pdus[2]]);
  assert(assembled.length === 1, `3-part 7bit assemble count failed: ${assembled.length}`);
  assert(assembled[0] === text, '3-part 7bit concat content failed');
});

tests.push(() => {
  const sender = '+886912000001';
  const text = '歡迎來到台灣，請留意您的漫遊設定並保持網路連線，以便接收重要通知。祝您旅途平安順利，也歡迎隨時查看本服務提供的最新提醒內容。';
  const parts = splitIntoParts(text, 3);
  const pdus = parts.map((part, index) => buildSmsDeliverPdu({
    sender,
    dcs: 0x08,
    text: part,
    udhHex: buildConcatUdh(0x44, 3, index + 1, false),
  }));
  const assembled = assembleConcatPdus([pdus[2], pdus[0], pdus[1]]);
  assert(assembled.length === 1, `3-part traditional Chinese assemble count failed: ${assembled.length}`);
  assert(assembled[0] === text, '3-part traditional Chinese concat content failed');
});

tests.push(() => {
  const sender = '+819012345678';
  const text = '日本語の長いメッセージを四つの連結セグメントに分けて、受信後に正しい順序で結合できることを確認します。大切なお知らせを見落とさないように、設定内容もあわせて確認してください。';
  const parts = splitIntoParts(text, 4);
  const pdus = parts.map((part, index) => buildSmsDeliverPdu({
    sender,
    dcs: 0x08,
    text: part,
    udhHex: buildConcatUdh(0x1234, 4, index + 1, true),
  }));
  const assembled = assembleConcatPdus([pdus[3], pdus[1], pdus[0], pdus[2]]);
  assert(assembled.length === 1, `4-part Japanese assemble count failed: ${assembled.length}`);
  assert(assembled[0] === text, '4-part Japanese concat content failed');
});

tests.push(() => {
  const sender = '+79161234567';
  const text = 'Это длинное русскоязычное сообщение разделено на три сегмента, чтобы проверить корректную обработку полного PDU, сборку частей и восстановление исходного текста после получения всех фрагментов.';
  const parts = splitIntoCodePointParts(text, 3);
  const pdus = parts.map((part, index) => buildSmsDeliverPdu({
    sender,
    dcs: 0x08,
    text: part,
    udhHex: buildConcatUdh(0x55, 3, index + 1, false),
  }));
  const assembled = assembleConcatPdus([pdus[2], pdus[0], pdus[1]]);
  assert(assembled.length === 1, `3-part Russian assemble count failed: ${assembled.length}`);
  assert(assembled[0] === text, '3-part Russian concat content failed');
});

tests.push(() => {
  const sender = '+201001234567';
  const text = 'هذه رسالة عربية طويلة مقسمة إلى ثلاثة أجزاء حتى نتحقق من أن تحليل PDU الكامل وتجميع الرسائل المتسلسلة يعملان بشكل صحيح دون فقدان أي حرف أثناء إعادة التركيب.';
  const parts = splitIntoCodePointParts(text, 3);
  const pdus = parts.map((part, index) => buildSmsDeliverPdu({
    sender,
    dcs: 0x08,
    text: part,
    udhHex: buildConcatUdh(0x66, 3, index + 1, false),
  }));
  const assembled = assembleConcatPdus([pdus[1], pdus[2], pdus[0]]);
  assert(assembled.length === 1, `3-part Arabic assemble count failed: ${assembled.length}`);
  assert(assembled[0] === text, '3-part Arabic concat content failed');
});

tests.push(() => {
  const sender = '+12025550123';
  const text = 'Travel update 😀: مرحبا بكم في الرحلة، welcome aboard, и пожалуйста проверьте gate 24B before departure. Спасибо и safe travels ✈️🌍';
  const parts = splitIntoCodePointParts(text, 4);
  const pdus = parts.map((part, index) => buildSmsDeliverPdu({
    sender,
    dcs: 0x08,
    text: part,
    udhHex: buildConcatUdh(0x2345, 4, index + 1, true),
  }));
  const assembled = assembleConcatPdus([pdus[3], pdus[1], pdus[0], pdus[2]]);
  assert(assembled.length === 1, `4-part emoji mixed assemble count failed: ${assembled.length}`);
  assert(assembled[0] === text, '4-part emoji mixed concat content failed');
});

console.log(`Running ${tests.length} PDU decode tests...`);
tests.forEach((t, i) => {
  t();
  console.log(`ok ${i + 1}`);
});
console.log('All tests passed.');
