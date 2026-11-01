// The flasher's "Make your Fluffy" and "Phrases" modals (spec §5 of
// docs/superpowers/specs/2026-10-31-cyberspace-fluffy-generator-design.md).
// When the choice differs from the default, the Install button gets a
// blob-URL manifest that also writes the mascot blob to its partition.
import {
  ART_ROWS, ART_COLS, MAX_PHRASES, PHRASE_MAX, MASCOT_OFFSET,
  blobMascot, cleanMascot, encodeMascot, isDefault, validateMascot,
} from "./mascot.mjs";

const STORE_KEY = "monster.fluffy";
const select = document.getElementById("board");
const install = document.getElementById("install");
const status = document.getElementById("fluffy-status");
const fluffyBtn = document.getElementById("fluffy-open");
const phrasesBtn = document.getElementById("phrases-open");

let presets = [];
let defaults = { art: [], phrases: [] };
let state = { art: [], phrases: [] };

const asciiOnly = (s) => s.replace(/[^\x20-\x7e]/g, "");
const isStrings = (a) => Array.isArray(a) && a.every((x) => typeof x === "string");

function loadState() {
  try {
    const s = JSON.parse(localStorage.getItem(STORE_KEY));
    if (s && isStrings(s.art) && isStrings(s.phrases)) {
      const c = cleanMascot(s);
      if (validateMascot(c) === "")
        return {
          art: c.art.length ? c.art : [...defaults.art],
          phrases: c.phrases.length ? c.phrases : [...defaults.phrases],
        };
    }
  } catch { /* corrupt: use the defaults */ }
  return { art: [...defaults.art], phrases: [...defaults.phrases] };
}

function saveState() {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(state)); } catch { /* private mode */ }
}

// ---- Manifest ----

let blobUrls = [];
let generation = 0;

function revokeBlobs() {
  blobUrls.forEach((u) => URL.revokeObjectURL(u));
  blobUrls = [];
}

async function updateManifest() {
  const gen = ++generation;
  if (select.disabled || !select.value) return;
  const normal = `manifests/${select.value}.json`;
  if (isDefault(state, defaults)) {
    install.setAttribute("manifest", normal);
    revokeBlobs();
    updateStatus();
    return;
  }
  // Install still points at the previous manifest until the swap below.
  status.textContent = "Preparing…";
  try {
    const manifestUrl = new URL(normal, location.href).href;
    const manifest = await (await fetch(manifestUrl, { cache: "no-cache" })).json();
    if (gen !== generation) return;
    const bytes = encodeMascot(blobMascot(state, defaults));
    const blob = URL.createObjectURL(new Blob([bytes], { type: "application/octet-stream" }));
    // Part paths are relative to the manifest; a blob: manifest can't resolve them.
    manifest.builds = manifest.builds.map((b) => ({
      ...b,
      parts: [
        ...b.parts.map((p) => ({ ...p, path: new URL(p.path, manifestUrl).href })),
        { path: blob, offset: MASCOT_OFFSET },
      ],
    }));
    const json = URL.createObjectURL(new Blob([JSON.stringify(manifest)], { type: "application/json" }));
    revokeBlobs();
    blobUrls = [blob, json];
    install.setAttribute("manifest", json);
    updateStatus();
    console.log("[fluffy] manifest", manifest);
  } catch (e) {
    if (gen !== generation) return;
    install.setAttribute("manifest", normal);
    status.textContent = "Couldn't add your mascot; Install flashes the default.";
    console.error("[fluffy]", e);
  }
}

function updateStatus() {
  const m = blobMascot(state, defaults);
  const n = m.phrases.length;
  const words = `${n} phrase${n === 1 ? "" : "s"}`;
  if (!m.art.length && !n) status.textContent = "Installs with Fluffy and his phrases.";
  else if (!n) status.textContent = "Installs with your mascot and Fluffy's phrases.";
  else if (!m.art.length) status.textContent = `Installs with Fluffy and your ${words}.`;
  else status.textContent = `Installs with your mascot and ${words}.`;
}

function changed() {
  saveState();
  updateManifest();
}

// ---- Modals ----

function el(tag, attrs = {}, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === "class") e.className = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else e.setAttribute(k, v);
  }
  e.append(...kids);
  return e;
}

// A <dialog> that Esc and a backdrop click close; focus goes back to `opener`.
function modal(title, opener, body, onClose) {
  const d = el("dialog", { class: "fluffy-modal", "aria-label": title },
    el("h2", {}, title), ...body);
  // Clicks on the dialog's own padding also target `d`; only outside its box closes.
  d.addEventListener("click", (e) => {
    if (e.target !== d) return;
    const r = d.getBoundingClientRect();
    if (e.clientX < r.left || e.clientX > r.right || e.clientY < r.top || e.clientY > r.bottom) d.close();
  });
  d.addEventListener("close", () => { onClose(); opener.focus(); });
  document.body.append(d);
  return d;
}

// A sample bubble over the art, as on the splash.
function preview(art, phrase) {
  const p = phrase.length > 24 ? phrase.slice(0, 23) + "…" : phrase;
  return [
    ` .${"-".repeat(p.length + 2)}.`,
    `( ${p} )`,
    ` '${"-".repeat(p.length + 2)}'`,
    "   \\",
    ...(art.length ? art : defaults.art),
  ].join("\n");
}

function buildFluffyModal() {
  const editor = el("textarea", {
    class: "fluffy-art", rows: ART_ROWS, cols: ART_COLS, spellcheck: "false",
    autocomplete: "off", wrap: "off", "aria-label": "Mascot art",
  });
  const counter = el("div", { class: "muted fluffy-count", "aria-live": "polite" });
  const prev = el("pre", { class: "fluffy-preview", "aria-label": "Preview" });
  const error = el("div", { class: "fluffy-error", role: "alert" });

  const refresh = () => {
    const lines = editor.value.split("\n");
    const upto = editor.value.slice(0, editor.selectionStart).split("\n");
    const row = upto.length, col = upto[upto.length - 1].length;
    counter.textContent = `line ${row}: ${col}/${ART_COLS} · ${lines.length}/${ART_ROWS} lines`;
    const c = cleanMascot({ art: lines, phrases: [] });
    prev.textContent = preview(c.art, state.phrases[0] || defaults.phrases[0]);
    error.textContent = c.art.length ? "" : "Empty art installs Fluffy.";
  };
  const setArt = (art) => { editor.value = art.join("\n"); refresh(); };

  // Drop what can't be stored as it's typed or pasted: non-ASCII, a 14th
  // column, a 7th line.
  editor.addEventListener("input", () => {
    const caret = editor.selectionStart;
    const before = editor.value.slice(0, caret);
    const fix = (s) => s.split("\n").map((l) => asciiOnly(l).slice(0, ART_COLS));
    const all = fix(editor.value).slice(0, ART_ROWS).join("\n");
    if (all !== editor.value) {
      const head = fix(before).join("\n");
      editor.value = all;
      const at = Math.min(head.length, all.length);
      editor.setSelectionRange(at, at);
    }
    refresh();
  });
  editor.addEventListener("keydown", (e) => {
    if (e.key === "Enter" && editor.value.split("\n").length >= ART_ROWS) e.preventDefault();
  });
  ["keyup", "click", "select"].forEach((t) => editor.addEventListener(t, refresh));

  const presetRow = el("div", { class: "fluffy-row" }, el("span", { class: "muted" }, "Presets:"),
    ...presets.map((p) => el("button", { type: "button", onclick: () => setArt(p.art) }, p.name)));

  const commit = () => {
    const art = cleanMascot({ art: editor.value.split("\n"), phrases: [] }).art;
    state.art = art.length ? art : [...defaults.art];
    changed();
  };
  const d = modal("Make your Fluffy", fluffyBtn, [
    el("p", { class: "muted" }, `Up to ${ART_ROWS} lines of ${ART_COLS} plain ASCII characters.`),
    el("div", { class: "fluffy-split" }, el("div", {}, editor, counter), prev),
    error,
    presetRow,
    el("div", { class: "fluffy-row fluffy-actions" },
      el("button", { type: "button", onclick: () => setArt(defaults.art) }, "Reset to Fluffy"),
      el("button", { type: "button", class: "primary", onclick: () => d.close() }, "Done")),
  ], commit);
  return { open: () => { setArt(state.art); d.showModal(); editor.focus(); } };
}

function buildPhrasesModal() {
  const total = el("div", { class: "muted", "aria-live": "polite" });
  const rows = [];
  const refresh = () => {
    rows.forEach(({ input, count }) => { count.textContent = `${input.value.length}/${PHRASE_MAX}`; });
    total.textContent = `${rows.filter((r) => r.input.value !== "").length}/${MAX_PHRASES} phrases`;
  };
  for (let i = 0; i < MAX_PHRASES; i++) {
    const input = el("input", {
      type: "text", maxlength: PHRASE_MAX, spellcheck: "false", "aria-label": `Phrase ${i + 1}`,
    });
    input.addEventListener("input", () => {
      const v = asciiOnly(input.value).slice(0, PHRASE_MAX);
      if (v !== input.value) {
        const at = Math.min(asciiOnly(input.value.slice(0, input.selectionStart)).length, v.length);
        input.value = v;
        input.setSelectionRange(at, at);
      }
      refresh();
    });
    const count = el("span", { class: "muted fluffy-count" });
    rows.push({ input, count });
  }
  const setPhrases = (list) => {
    rows.forEach((r, i) => { r.input.value = list[i] || ""; });
    refresh();
  };
  const commit = () => {
    const phrases = cleanMascot({ art: [], phrases: rows.map((r) => r.input.value) }).phrases;
    state.phrases = phrases.length ? phrases : [...defaults.phrases];
    changed();
  };
  const d = modal("Phrases", phrasesBtn, [
    el("p", { class: "muted" }, `What the monster says in its bubble: up to ${MAX_PHRASES}, ` +
      `${PHRASE_MAX} plain ASCII characters each. Blank rows are skipped.`),
    total,
    el("ol", { class: "fluffy-phrases" },
      ...rows.map(({ input, count }) => el("li", {}, input, count))),
    el("div", { class: "fluffy-row fluffy-actions" },
      el("button", { type: "button", onclick: () => setPhrases(defaults.phrases) }, "Reset to defaults"),
      el("button", { type: "button", class: "primary", onclick: () => d.close() }, "Done")),
  ], commit);
  return { open: () => { setPhrases(state.phrases); d.showModal(); rows[0].input.focus(); } };
}

// ---- Start ----

fetch("mascots.json", { cache: "no-cache" })
  .then((r) => r.json())
  .then((json) => {
    presets = json.presets;
    defaults = { art: json.presets[0].art, phrases: json.phrases };
    state = loadState();
    const fluffy = buildFluffyModal();
    const phrases = buildPhrasesModal();
    fluffyBtn.addEventListener("click", fluffy.open);
    phrasesBtn.addEventListener("click", phrases.open);
    fluffyBtn.disabled = phrasesBtn.disabled = false;
    // index.html sets the normal manifest, then fires this.
    select.addEventListener("board-picked", updateManifest);
    updateStatus();
    updateManifest();
  })
  .catch((e) => console.error("[fluffy]", e));
