# Fractal Ornament Pool Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the ornament **Intensity** tier with an explicit **15-toggle ornament pool** (keep **Rate** as P(fire)), make **Lock freeze the ornament to the last-fired one**, and surface **Sleep** as an in-scope control — all in the web sim, TDD'd via the headless harness, with the KD-13 spec fold.

**Architecture:** The Fractal track exists only as a web sim — one self-contained file `.scratch/fractal-sim.html` (inline `<script>`), exercised by a headless Node harness `.scratch/fractal-harness.js` that stubs DOM + AudioContext, `vm`-evals the page script, and drives `window.FRACTAL`. Every behavior change is TDD'd against the harness: add a failing assertion → run → implement in the sim → run green. The **spec docs (KD-13 / 1-pager) are committed**; the sim is the untracked working artifact in `.scratch/` — it is **verified by the harness, not committed** (commit steps below are for the spec only).

**Tech Stack:** vanilla ES (one HTML file), Node (`node --check` for syntax + `node fractal-harness.js` for the suite). No frameworks, no build.

---

## Current state (read before starting)

In `.scratch/fractal-sim.html`:
- `ORN_2STEP` (7), `ORN_4STEP` (7), `ORN_TRILL` (1) — 15 ornament names (lines ~821-823).
- `eligibleOrnaments(depth)` — returns a tier pool by `depth` (0.40 unlocks 4-step, 0.75 unlocks trill).
- `rollOrnament(rate, depth, rng)` — rolls `< rate`, picks from `eligibleOrnaments(depth)`.
- `F.ornamentRate`, `F.ornamentDepth` (model fields ~line 444).
- UI: `fOrnRate` + `fOrnDepth` range sliders; `fractalNotes()` calls `rollOrnament(F.ornamentRate, F.ornamentDepth)`.
- `window.FRACTAL` exports include `rollOrnament`, `eligibleOrnaments`, `ORN_2STEP/4STEP/TRILL`, `F`.
- Lock (`F.lock`) currently only gates capture (`executeCaptureRule`).
- Sleep (`F.sleep`) lives in the red `.card.deferred` "Deferred" card.

Harness verify command (use after every code step):
```
cd .scratch && python3 -c "import re;open('/tmp/h.js','w').write(chr(10).join(re.findall(r'<script>(.*?)</script>',open('fractal-sim.html').read(),re.S)))" && node --check /tmp/h.js && node fractal-harness.js
```

---

### Task 1: Fold the pool into KD-13 (spec) — commit

**Files:**
- Modify: `docs/superpowers/specs/2026-05-17-fractal-track-design.md` (KD-13 "User control" paragraph)
- Modify: `scratch/fractal-1page.md` (Routable list / ledger if it names ornament intensity)

**Step 1: Rewrite the KD-13 user-control paragraph.** Replace the `ornamentIntensity` tier description with the pool. New text:

> **User control — Rate + Pool.** `ornamentRate` (uint8, 0-100%) = P(fire) per gated cell (how often). `ornamentPool` (uint16 bitmask, 15 bits, default all-on) = **which** of the 15 ornaments are eligible; when one fires it is drawn uniformly from the enabled pool. This replaces the old graded Intensity tier — you pick the palette explicitly (e.g. mordents + turns only) instead of an auto-unlock ramp. **Rate is Routable**; the pool is a setup mask.

**Step 2: Update the four-live-controls references.** Anywhere KD-12/KD-13 says the four Routables are "branch count · path · ornament rate · ornament intensity", change "ornament intensity" → drop it (now three + ... ) OR keep ornament rate only. Search: `grep -rn "ornament intensity\|ornamentIntensity" docs/superpowers/specs/2026-05-17-fractal-track-design.md scratch/fractal-1page.md`. Replace intensity mentions with "ornament pool (mask, not routable)" where they describe the control; keep **Rate** as the routable. (The routable set becomes Branch count · Path · Ornament rate — confirm with the user if a 4th routable is wanted; default to these three.)

**Step 3: Update the RAM line in KD-13.** `~2 B track-wide (ornamentRate, ornamentIntensity)` → `ornamentRate (1 B) + ornamentPool (2 B) track-wide`.

**Step 4: Commit.**
```bash
git add docs/superpowers/specs/2026-05-17-fractal-track-design.md scratch/fractal-1page.md
git commit -m "docs(fractal): KD-13 ornament pool (15-bit mask) replaces Intensity tier; Rate kept"
```

---

### Task 2: Sim — ornamentPool field + pool-based roll (TDD)

**Files:**
- Modify: `.scratch/fractal-sim.html` (`ORN_*`, `eligibleOrnaments`, `rollOrnament`, `F` model, `window.FRACTAL` exports)
- Test: `.scratch/fractal-harness.js`

**Step 1: Write the failing harness test.** Add after the existing ornament tests (the `(h)` block):
```js
/* (h2) Ornament pool: roll draws only from enabled ornaments */
(function(){
  const all = FR.ORN_ALL;                       // 15-name canonical list
  F.ornamentRate = 1;                           // always fire
  // pool = only Trill (bit 14) + Mordent Up
  const trillIdx = all.indexOf("Trill");
  const morIdx   = all.indexOf("Mordent Up");
  F.ornamentPool = (1<<trillIdx) | (1<<morIdx);
  const got = new Set();
  for(let k=0;k<200;k++){
    let call=0; const rng=()=> (call++===0 ? 0 : (k%2)/2);   // fire, then sweep pick
    const n = FR.rollOrnament(F.ornamentRate, F.ornamentPool, rng);
    if(n) got.add(n);
  }
  const onlyPool = [...got].every(n=> n==="Trill" || n==="Mordent Up") && got.size>0;
  ok("(h2) roll draws only from the enabled pool", onlyPool, [...got].join(","));
  ok("(h2) empty pool never fires", FR.rollOrnament(1, 0, ()=>0)===null);
  F.ornamentPool = 0x7FFF;   // restore all-on
})();
```

**Step 2: Run — verify it fails.** Run the harness command. Expected: FAIL — `FR.ORN_ALL` undefined / `rollOrnament` signature mismatch.

**Step 3: Implement in the sim.**
- Add `const ORN_ALL = ORN_2STEP.concat(ORN_4STEP).concat(ORN_TRILL);` after the three arrays.
- Add `const ORN_POOL_ALL = (1<<ORN_ALL.length)-1;` (`0x7FFF`).
- Model: replace `ornamentDepth: <x>,` with `ornamentPool: 0x7FFF,` (keep `ornamentRate`).
- Replace `eligibleOrnaments(depth)` with:
```js
function eligiblePool(mask){
  const out=[];
  for(let i=0;i<ORN_ALL.length;i++){ if(mask & (1<<i)) out.push(ORN_ALL[i]); }
  return out;
}
```
- Replace `rollOrnament(rate, depth, rng)` with:
```js
function rollOrnament(rate, pool, rng){
  rate = clamp(rate, 0, 1);
  if(rate <= 0) return null;
  if((rng||Math.random)() >= rate) return null;
  const elig = eligiblePool(pool);
  if(elig.length === 0) return null;
  return elig[Math.floor((rng||Math.random)() * elig.length) % elig.length];
}
```
- Update the caller in `fractalNotes()`: `rollOrnament(F.ornamentRate, F.ornamentPool)`.
- `window.FRACTAL` exports: add `ORN_ALL`, `eligiblePool`; remove `eligibleOrnaments` (and any stale `ornamentDepth`).

**Step 4: Run — verify green.** Harness command. Expected: PASS for (h2); fix the old `(h)` intensity-tier test — replace it with the pool semantics or delete it (it asserted depth-tier behavior that no longer exists).

**Step 5: Verify** `node --check` clean + full harness green. (No commit — sim is untracked.)

---

### Task 3: Sim — Lock freezes the ornament to the last-fired one (TDD)

**Files:**
- Modify: `.scratch/fractal-sim.html` (`rollOrnament` / `fractalNotes`, a `_lastOrnament` holder, `window.FRACTAL`)
- Test: `.scratch/fractal-harness.js`

**Step 1: Write the failing test.**
```js
/* (h3) Lock freezes the ornament to the last-fired one */
(function(){
  F.ornamentRate = 1; F.ornamentPool = 0x7FFF; F.lock = false;
  // fire once unlocked to set _lastOrnament
  const first = FR.rollOrnament(1, F.ornamentPool, ()=> 0.0);   // deterministic pick = index 0
  F.lock = true;
  // while locked, repeated rolls with DIFFERENT rng must all return `first`
  const a = FR.rollOrnament(1, F.ornamentPool, ()=> 0.9);
  const b = FR.rollOrnament(1, F.ornamentPool, ()=> 0.3);
  ok("(h3) locked roll repeats the last-fired ornament", a===first && b===first, `first=${first} a=${a} b=${b}`);
  // rate still gates: locked + rng>=rate -> no fire
  ok("(h3) lock keeps Rate gating (no fire when rate roll fails)", FR.rollOrnament(0.5, F.ornamentPool, ()=>0.9)===null);
  F.lock = false;
})();
```

**Step 2: Run — verify it fails** (locked rolls currently re-pick).

**Step 3: Implement.** Add a module-level `let _lastOrnament = null;`. In `rollOrnament`, after the rate gate passes:
```js
function rollOrnament(rate, pool, rng){
  rate = clamp(rate, 0, 1);
  if(rate <= 0) return null;
  if((rng||Math.random)() >= rate) return null;     // Rate still gates while locked
  if(F.lock && _lastOrnament){ return _lastOrnament; } // frozen: repeat last
  const elig = eligiblePool(pool);
  if(elig.length === 0) return null;
  const pick = elig[Math.floor((rng||Math.random)() * elig.length) % elig.length];
  _lastOrnament = pick;
  return pick;
}
```
Export `_lastOrnament` is not needed; the test reads behavior. Confirm `clamp` is in scope.

**Step 4: Run — verify green.** Harness + `node --check`.

**Step 5: Verify** full harness green.

---

### Task 4: Sim UI — 15-toggle pool picker replaces the Intensity slider

**Files:**
- Modify: `.scratch/fractal-sim.html` (the ornament card markup + the `fOrnDepth` slider/wiring)

**Step 1: Replace the Intensity slider markup.** Find `id="fOrnDepth"` (the Intensity range). Replace its `.ctl` with a pool picker grouped by tier — three rows of toggle buttons mirroring the branch-pool toggle style (`.seg` buttons or small squares), labels = the 15 names abbreviated, each toggling its `ornamentPool` bit. Keep the `fOrnRate` slider and the `fOrnLast` readout.

**Step 2: Wire the toggles.** For each pool button with `data-i="<index>"`, on click: `F.ornamentPool ^= (1<<i)`; toggle a lit class; reflect state. Add a small helper that paints lit/unlit from `F.ornamentPool`. Initialize all-lit.

**Step 3: Update the card hint** — drop "intensity ramps 2-step→4-step→trill"; describe "Rate = how often; pool = which ornaments are eligible; Lock freezes to the last-fired."

**Step 4: Verify** `node --check` clean + harness green (UI is DOM-only; the harness stubs DOM so it won't break — confirm no JS error). Open `.scratch/fractal-sim.html` and eyeball: toggling pool buttons changes which ornaments fire; Lock freezes the readout.

---

### Task 5: Sim — surface Sleep as in-scope (move out of the red Deferred card)

**Files:**
- Modify: `.scratch/fractal-sim.html` (the `.card.deferred` card + the Sleep control)

**Step 1:** Move the **Sleep** `.ctl` (the `fSleep` select) out of `<div class="card deferred">` into a normal in-scope card (e.g. next to the loop/playback controls or the ornament card). Sleep is now KD-20 in-scope.

**Step 2:** If the deferred card is now empty, remove it (and its `.card.deferred` heading/note). If anything else remains deferred there, keep the card for those.

**Step 3:** Verify `node --check` + harness green (the d3 sleep test must still pass — Sleep logic is unchanged, only its DOM location moved). Open and confirm Sleep still works and is no longer red.

---

### Task 6: Final verification — commit spec, leave sim verified

**Step 1:** Full check:
```
cd .scratch && python3 -c "import re;open('/tmp/h.js','w').write(chr(10).join(re.findall(r'<script>(.*?)</script>',open('fractal-sim.html').read(),re.S)))" && node --check /tmp/h.js && node fractal-harness.js
```
Expected: clean + all green (including h2/h3 + the existing 51).

**Step 2:** Confirm no stale references: `grep -c "ornamentDepth\|eligibleOrnaments\|fOrnDepth" fractal-sim.html` → 0.

**Step 3:** The KD-13 spec fold was committed in Task 1. The sim is verified, not committed (ephemeral `.scratch/`). Open `.scratch/fractal-sim.html` for the user.

---

## Notes
- **No firmware** in this plan — the fractal track is sim-only; "implementation" = the JS sim, "tests" = the harness.
- **One open decision for the user (Task 1, Step 2):** with Intensity gone, the routable set is Branch count · Path · Ornament rate (three). Confirm whether a 4th routable is still wanted before finalizing the four-live-controls wording.
