# Feature: PEW|FORMER/XFORMER Optimization Plan (Practical)

This is a focused, actionable optimization plan for the firmware. It prioritizes RAM headroom and stability over speculative refactors.

## Goals
- Free 8–16 KB SRAM and 4–8 KB CCM without feature loss.
- Keep real‑time timing stable; no architectural rewrites in this phase.
- Make changes measurable and reversible.

## Ground Truth (Current Constraints)
- SRAM is the tight constraint; flash has headroom.
- Largest SRAM consumer: `Model` (NoteSequence arrays).
- Largest CCM consumers: UI pages + FreeRTOS static stacks.
- FreeRTOS heap is disabled (static allocation only).

## Primary Targets (High Impact, Low Risk)

### 1) Trim per‑track RAM in Model
**Why**: `Model` is ~93 KB SRAM.
**Actions**:
- Review NoteSequence snapshot count and pattern count; reduce where acceptable.
- Audit per‑step fields for packing opportunities (bitfields/width reduction).
- Consider moving non‑DMA buffers to CCM where safe.
**Measure**: use `scripts/analyze_map.py` before/after.

### 2) UI memory slimming
**Why**: UI is ~23 KB in CCM.
**Actions**:
- Reduce per‑page cached state where possible.
- Reuse shared buffers across pages.
- Audit list models or large arrays for size reductions.
**Measure**: check `.ccmram_bss` section sizes.

### 3) Stack right‑sizing
**Why**: static stacks in CCM eat ~13 KB.
**Actions**:
- Add a one‑time stack watermark check.
- Lower UI/Engine/USBH stacks where safe.
**Measure**: confirm high‑water marks under stress use.

## Secondary Targets (Medium Impact)

### 4) FileManager buffers
**Why**: small but easy to trim.
**Actions**:
- Share buffers across operations.
- Ensure no duplicate static buffers for mutually exclusive operations.

### 5) Routing and output buffers
**Why**: CV/gate routing arrays can grow quietly.
**Actions**:
- Verify sizes are minimal and bound to CONFIG constants.
- Remove unused routing data where not needed.

## Avoid for Now (High Risk / Low Evidence)
- Global fixed‑point refactor.
- Engine architecture rewrite into managers/factories.
- Real‑time scheduler redesign.

## Measurement & Tooling
- Use `scripts/analyze_map.py` with `--sram --ccm` and track top symbols.
- Capture before/after snapshots in a short table inside this doc.
- Add a single “RAM budget” section per release.

## Testing Notes
- No new automated tests required for memory‑only changes.
- For stack changes: run worst‑case UI navigation + USB/MIDI activity.
- For Model changes: load/save project, scan patterns, play/record.

## Proposed Order
1) Stack right‑sizing (fast feedback).
2) UI memory trimming.
3) NoteSequence/model packing.
4) Routing/output buffers.

## Tracking Table (fill in)
| Date | SRAM Used | CCM Used | Change |
|------|-----------|----------|--------|
|      |           |          |        |

