// The mascot blob (spec §3 of
// docs/superpowers/specs/2026-10-31-cyberspace-fluffy-generator-design.md),
// byte for byte as lib/core/src/mascot/mascot.cpp. Pure: no DOM, so Node
// tests import it. A mascot is {art: [string], phrases: [string]}; an empty
// list means the default.

export const BLOB_SIZE = 4096;
export const ART_ROWS = 6;
export const ART_COLS = 13;
export const MAX_PHRASES = 10;
export const PHRASE_MAX = 80;
export const MASCOT_OFFSET = 0xcb0000;

// Release slugs built without the mascot partition (RC_FEAT_MASCOT=0): the
// flasher hides Make your Fluffy and Phrases and flashes no blob for them.
export const NO_MASCOT_BOARDS = ["cardputer-adv"];

export function hasMascot(slug) {
  return !NO_MASCOT_BOARDS.includes(slug);
}

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();

// IEEE CRC32, as zlib's crc32. Strings are taken as Latin-1 bytes.
export function crc32(data) {
  const bytes = typeof data === "string" ? Uint8Array.from(data, (ch) => ch.charCodeAt(0)) : data;
  let c = 0xffffffff;
  for (const b of bytes) c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

const printable = (s) => /^[\x20-\x7e]*$/.test(s);

export function cleanMascot(m) {
  const art = m.art.map((l) => l.replace(/ +$/, ""));
  while (art.length && art[art.length - 1] === "") art.pop();
  return { art, phrases: m.phrases.filter((p) => p !== "") };
}

// "" when `m` fits the limits, else a short reason (the C++ wording).
export function validateMascot(m) {
  if (m.art.length > ART_ROWS) return `art is over ${ART_ROWS} lines`;
  for (const [i, l] of m.art.entries()) {
    if (l.length > ART_COLS) return `art line ${i + 1} is over ${ART_COLS} characters`;
    if (!printable(l)) return `art line ${i + 1} has a character that isn't plain ASCII`;
  }
  if (m.phrases.length > MAX_PHRASES) return `over ${MAX_PHRASES} phrases`;
  for (const [i, p] of m.phrases.entries()) {
    if (p === "") return `phrase ${i + 1} is empty`;
    if (p.length > PHRASE_MAX) return `phrase ${i + 1} is over ${PHRASE_MAX} characters`;
    if (!printable(p)) return `phrase ${i + 1} has a character that isn't plain ASCII`;
  }
  return "";
}

// The 4096-byte blob. The caller validates first.
export function encodeMascot(m) {
  const out = new Uint8Array(BLOB_SIZE).fill(0xff);
  const payload = [];
  for (const s of [...m.art, ...m.phrases]) {
    payload.push(s.length);
    for (const ch of s) payload.push(ch.charCodeAt(0));
  }
  const n = payload.length;
  out.set([0x46, 0x4c, 0x55, 0x46, 1, m.art.length, m.phrases.length, 0, n & 0xff, n >> 8]);
  out.set(payload, 10);
  const crc = crc32(out.subarray(0, 10 + n));
  for (let i = 0; i < 4; i++) out[10 + n + i] = (crc >>> (8 * i)) & 0xff;
  return out;
}

const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);

// `m` cleaned, with a part equal to the default emptied, as the device stores it.
export function blobMascot(m, defaults) {
  const c = cleanMascot(m);
  return {
    art: same(c.art, defaults.art) ? [] : c.art,
    phrases: same(c.phrases, defaults.phrases) ? [] : c.phrases,
  };
}

// True when the flasher needs no blob: both parts are the default.
export function isDefault(m, defaults) {
  const b = blobMascot(m, defaults);
  return b.art.length === 0 && b.phrases.length === 0;
}
