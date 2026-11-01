// Run with `node --test site/*.test.mjs`.
import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import {
  crc32, cleanMascot, validateMascot, encodeMascot, isDefault, blobMascot,
} from "./mascot.mjs";

const golden = readFileSync(new URL("../test/fixtures/mascot_golden.h", import.meta.url), "utf8");
const json = JSON.parse(readFileSync(new URL("./mascots.json", import.meta.url), "utf8"));
const defaults = { art: json.presets[0].art, phrases: json.phrases };

test("golden", () => {
  const hex = golden.match(/kMascotGoldenHex\s*=\s*((?:\s*"[^"]*")+)/)[1]
    .replace(/[\s"]/g, "");
  const want = Uint8Array.from(hex.match(/../g), (h) => parseInt(h, 16));
  const got = encodeMascot({ art: ["/\\_/\\", "(o o)"], phrases: ["boo", "meow"] });
  assert.equal(got.length, 4096);
  assert.deepEqual(got.slice(0, want.length), want);
  assert.ok(got.slice(want.length).every((b) => b === 0xff));
});

test("crc", () => {
  assert.equal(crc32("123456789"), 0xcbf43926);
});

test("limits", () => {
  const ok = { art: ["abc"], phrases: ["hi"] };
  assert.equal(validateMascot(ok), "");
  const bad = [
    { art: ["a".repeat(14)], phrases: [] },
    { art: Array(7).fill("x"), phrases: [] },
    { art: [], phrases: Array(11).fill("x") },
    { art: [], phrases: ["a".repeat(81)] },
    { art: ["é"], phrases: [] },
    { art: [], phrases: ["café"] },
  ];
  for (const m of bad) assert.match(validateMascot(m), /\w/, JSON.stringify(m));
  assert.equal(validateMascot({ art: ["a".repeat(14)], phrases: [] }),
    "art line 1 is over 13 characters");
});

test("clean", () => {
  assert.deepEqual(cleanMascot({ art: ["ab  ", "", "c", "", " "], phrases: ["", "hi", ""] }),
    { art: ["ab", "", "c"], phrases: ["hi"] });
});

test("defaults need no blob", () => {
  assert.equal(isDefault(defaults, defaults), true);
  assert.equal(isDefault({ art: [...defaults.art, "", "  "], phrases: [...defaults.phrases, ""] },
    defaults), true);
  assert.equal(isDefault({ art: defaults.art, phrases: ["boo"] }, defaults), false);
  const art = [...defaults.art];
  art[0] = art[0] + "!";
  assert.equal(isDefault({ art, phrases: defaults.phrases }, defaults), false);
});

test("a part equal to the default is stored empty", () => {
  assert.deepEqual(blobMascot({ art: defaults.art, phrases: ["yo "] }, defaults),
    { art: [], phrases: ["yo "] });
  assert.deepEqual(blobMascot({ art: ["(o o)  "], phrases: defaults.phrases }, defaults),
    { art: ["(o o)"], phrases: [] });
});
