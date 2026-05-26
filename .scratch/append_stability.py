#!/usr/bin/env python3

appendix = r'''

---

## GLM5.1: Native Extraction — Stability, Functionality, Resources

Analysis of the "move all state out of `scene_state_t` into native Performer Model/Engine" proposal from the GLM5.1 review above.

### Stability

**Real gains:**
- Model/Engine boundary restored. Currently `scene_state_t` lives in the model but the engine mutates it every tick — variables, delay queue, RNG state all change on the hot path inside a struct the model owns and serializes. A project save captures transient RNG seeds, variable values, delay queue contents. That's a correctness bug masked by "it works."
- Undo/redo becomes meaningful. Right now undo granularity for Teletype is "the whole blob" because there's no separation between "user changed a script" and "the engine mutated variable X 400 times."
- Multi-track safety is structural, not behavioral. No `g_activeEngine`, no `ScopedEngine`, no risk of two engines writing the same struct through aliasing. State is per-engine-member, full stop.

**But:** the biggest stability risk today is `g_activeEngine` — fixable with the context pass-through (Phase 1 of the GLM5.1 plan), no extraction needed. The Model/Engine blur is impure but hasn't caused data loss. It's a latent risk, not an active one.

### Functionality

**Nothing new.** Same language, same ops, same scripts. The user sees zero difference.

**Future enabling:**
- New state subsystems (e.g., a new DSP block, a new data array) become "add a member to the engine" instead of "extend `scene_state_t`, update `offsetof` table, rebuild the enum."
- Proper diff-based undo for script edits.
- Engine hot-swap (reset engine without touching model, or vice versa) — currently impossible because the VM's active scripts live in the model.

None of this is requested or on a roadmap.

### Resources

This is where it gets painful.

**Model shrinks:** `TeletypeTrack` goes from 7,104 B to ~2,500 B (just `PatternSlot[2]` + routing config). Direct SRAM saving: 4,600 B per track.

**But the container doesn't change.** `Track::_container` is sized to `max(NoteTrack=9,544 B, ...)`. TeletypeTrack dropping below NoteTrack saves nothing in the static container. The 4,600 B x 8 = 36.8 KB is free RAM *inside* already-allocated container slots — it's unusable unless you make the container variable-sized.

**Engine grows by the same amount.** Everything removed from the model must live somewhere. Variables (~390 B), delay queue (~490 B), active scripts (~2,136 B), turtle (~32 B), RNG (~60 B), MIDI (~96 B) = ~3,200 B added to `TeletypeTrackEngine`.

**Engine container gate moves.** Currently 912 B. Would become ~4,112 B. All 8 engine slots in CCMRAM pay for the new max: (4,112 - 912) x 8 = **25,600 B more CCMRAM**. Current CCMRAM headroom is ~10 KB (54,096 / 65,536). This overflows CCMRAM by ~15 KB.

**Net: SRAM unchanged (container gate). CCMRAM overflows by ~15 KB. You'd need to find 15 KB of CCMRAM cuts elsewhere or move some of this back to SRAM.**

### Summary

| | Gain | Cost |
|---|---|---|
| **Stability** | Model/Engine boundary, clean serialization | `g_activeEngine` fix does 80% of this without the extraction |
| **Functionality** | Future extensibility | Zero user-visible change now |
| **Resources** | SRAM drops inside container (unusable) | CCMRAM overflows by ~15 KB |

The extraction is architecturally correct and the right long-term shape. But it's a net resource loss today, zero functionality gain, and the stability win is mostly achievable with the cheaper bridge context pass-through. The right move is to do the bridge fix first, then wait until there's a functional reason (new ops, new DSP, undo system) to justify the CCMRAM cost.

If the container architecture ever changes to variable-sized slots (e.g., tagged union with size-aware placement instead of max-sized buffers for all 8 tracks), the resource calculus flips dramatically — every byte saved in `TeletypeTrack` and `TeletypeTrackEngine` becomes recoverable. That would be the trigger to revisit this extraction.
'''

filepath = '/Users/foronoma/Work/Code/Eurorack/performer-phazer/docs/teletype_v2.md'
with open(filepath, 'a') as f:
    f.write(appendix)
print(f'Written {len(appendix)} bytes')
