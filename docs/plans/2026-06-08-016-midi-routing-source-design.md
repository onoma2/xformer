# 016 — MIDI as a routing source (phase 7)

_Design. Branch `refactor/routing-matrix`. Supersedes the brainstorm notes in
`.tasks/routing-matrix/PHASE-7-MIDI.md`._

## Goal

Re-expose MIDI as a route source in the modulation matrix, porting the legacy routing
page's MIDI capabilities **1:1 — no more, no less**: LEARN capture plus manual editing of
Port / Channel / Event / CC-or-Note (all `MidiSource::Event` types, including the note
variants). The matrix rewrite removed the legacy per-route detail screen; this design gives
those fields a new home as a dedicated MIDI tab, styled like the bus hub.

## What already exists (do not rebuild)

- **Engine path complete.** `RoutingEngine.cpp:290-343` matches a learned `MidiSource` and
  turns every event type (CC abs/rel, pitch bend, note momentary/toggle/velocity/range) into
  a normalized source value; `:404`, `:473` consume it. A route modulates from MIDI the
  instant its source is `Source::Midi` with a populated `MidiSource`.
- **Model + serialize complete.** `MidiSource` on every `Route` (`Routing.h:621-704`, `:839`);
  port/channel via `MidiSourceConfig` (`MidiConfig.h`, `IsSource` → omni default). No
  ProjectVersion bump.
- **Learn unit complete.** `MidiLearn` (`engine/MidiLearn.h`, `_engine.midiLearn()`);
  `RoutingPage::exit()` already stops it.
- **Proven mapping in git history.** The pre-refactor `assignMidiLearn(MidiLearn::Result&)`
  mapped a captured event to `MidiSource` (CC abs/rel from knob/encoder, pitch bend from the
  wheel, note momentary from a key). Reuse that logic verbatim.
- **The one gate.** `RouteBrowse::sourceList` skips `Source::Midi` (`RouteBrowse.h:105`).

## Flow (capture decoupled from source-selection)

Arming LEARN the instant a grid cell's source is dialed to MIDI risks hanging on a silent or
disconnected device, and makes the source un-parkable. So capture moves off the grid:

1. **Grid (SOURCE view).** MIDI is a plain value in the source list, after Bus/Mod. Dial to
   it, COMMIT. The route now has `source == Midi` with a default `MidiSource` (port DIN,
   channel omni, CC Absolute, CC#0). No capture, no popup, no hang.
2. **MIDI page (new 12th tab, after BUS).** The route appears as a row automatically — the
   page lists every route where `source == Midi`. EDIT opens that route's draft. **LEARN**
   (footer) arms capture into the draft: popup "waiting for MIDI…", next matching event fills
   port/channel/event/CC via `assignMidiLearn`, CANCEL aborts at any time. Or skip LEARN and
   hand-edit: **VIEW** walks the editable columns (Port → Channel → Event → CC/Note), encoder
   dials the focused one. COMMIT persists the draft; the live route is untouched until then.

Routes are born in the grid; this page is their editor/overview. No "add" here. Removing a
row = clearing that route (Shift+CANCEL, as elsewhere).

## MIDI page layout (bus-styled)

Renders in `ui-preview/midi/` (`midi-proposed`, `midi-empty-proposed`, `midi-edit-proposed`,
`midi-learn-proposed`). 256×64, `tiny5x5`.

- Header: `MIDI` title; dim column labels `PORT · CH · EVENT · CC/NOTE` (nav) or the active
  column tag (edit). Divider at x=76 separating target from the source fields.
- Rows (dynamic, one per MIDI route): left = target it drives (param + track scope); right =
  Port / Channel / Event / CC-or-Note. Left cursor bar marks focus, mirroring the bus lanes.
  More rows than fit → reuse the engine-page vertical scroll + right-edge scrollbar.
- Edit: focused row bright, VIEW-selected column bright, others medium. Event column is
  abbreviated (CCa / CCr / PB / Nt…) — legacy's full names don't fit a column.
- **Dynamic columns by event:** Pitch Bend has no CC/Note value, so VIEW skips that column
  (legacy dropped the row). Note vs CC relabels the last column.
- Footer: `VIEW · EDIT · LEARN · CANCEL · COMMIT`. LEARN live only inside EDIT.
- Empty state: centered hint "no MIDI assignments — learn from the matrix".
- LEARN popup: centered single-line box "waiting for MIDI…"; drawn by the page while
  `midiLearn().isActive()` (not `showMessage`, which is time-boxed); cleared on capture or
  CANCEL.

## Build order (TDD slices) — gate flips LAST

The `sourceList` gate opens only once the editing surface is live, so MIDI is never selectable
without a place to configure it.

1. **Page read model.** A pure helper enumerating routes where `source == Midi` (target,
   route index), for the row list. `TestRouteBrowse`-style. (Empty, one, many; ordering.)
2. **Page render + nav.** Tab ring → `kTabCount = 12`; `MIDI` tab after BUS; row cursor.
3. **Edit draft + VIEW column walk + field editing** of port/channel/event/CC, dynamic by
   event type. Draft→COMMIT integrity (live route untouched until commit).
4. **LEARN** — arm `midiLearn()` into the draft, popup, map `MidiLearn::Result` → `MidiSource`
   (port `assignMidiLearn` logic), CANCEL/stop on exit.
5. **Flip `RouteBrowse::sourceList`** to include `Source::Midi`. Grid cell renders the generic
   "MIDI" token (existing `printSource` fallthrough — no new display code).

## Constraints

- Frozen spec `docs/plans/2026-06-04-013-modulation-ui-spec.md` (+ §15 amendments).
- **No ProjectVersion bump** — `MidiSource` already serializes.
- TDD per slice; sim + STM32 release green each slice (flash ceiling 983040 bytes).
- ui-preview render before any OLED change (done for all four states).

## Out of scope

Shaper UI (spec §13, deferred until the shaper port). Note-variant *interpretations*
(Toggle/Velocity/Range) are reachable via the Event column — that is the legacy capability,
ported, not a new feature.
