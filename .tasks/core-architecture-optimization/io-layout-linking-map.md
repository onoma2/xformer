# IO / layout / linking map

_Surveyed 2026-05-30. Research artifact — no direction chosen. Maps three subsystems that all assume the Note/Curve-only era: track **linking**, physical **output layout** (channel→track/modulator assignment + rotation), and **input** routing. Sibling to `routing-five-sources-map.md` and `engine-update-contention-map.md`._

## 1. Linking — a Curve-follows-stepped-source mechanism

**Contract** (`engine/TrackEngine.h:18`):
```
struct TrackLinkData { uint32_t divisor; uint32_t relativeTick; SequenceState *sequenceState; };
virtual const TrackLinkData *linkData() const { return nullptr; }   // default
```
The link shares the *source's step-traversal state* — a `SequenceState*`, divisor, and relativeTick. Wiring: `Track::_linkTrack` (model) → `Engine` resolves `linkedTrackEngine` at `updateTrackSetups` (`Engine.cpp:521`), passed to every engine ctor.

**Producer / consumer matrix** (`*TrackEngine.h`):

| Track type | Produces `linkData` | Consumes source |
|------------|:---:|:---:|
| Note | yes | **no** |
| Curve | yes | **yes** (`CurveTrackEngine.cpp:154`) |
| MidiCv / Tuesday / DiscreteMap / Indexed / Teletype / Stochastic / PhaseFlux | no (nullptr) | no |

So linking actually does exactly one thing: a **Curve** track adopts a stepped source's `sequenceState`/divisor and triggers its steps in lockstep. Everything else is inert:
- A **Note** track set to link a source still produces its own data but ignores the source (no consume).
- Any **procedural** type as follower → ignores the source.
- Any **procedural** type as source → `nullptr`, so even a Curve following it gets nothing.

**The Note/Curve assumption:** the only state worth sharing was "play these two step-windows in sync," so the contract carries a `SequenceState*`. Procedural types don't traverse a fixed step window (Stochastic seed-rolls, Tuesday algo-generates, PhaseFlux stateless-ramp), so they have no `sequenceState` to hand over and the contract can't represent their state (RNG seeds, phase, accumulators). Linking silently dead-ends at them — and the UI still offers the link assignment.

## 2. Output layout — per-channel assignment + rotation

Physical outputs are assigned **per channel → track (or modulator)** in the model (`model/Project.h`):
- `_cvOutputTracks[CONFIG_CHANNEL_COUNT]` — CV channel → track index (`:356-360`).
- `_gateOutputTracks` — gate channel → track index (`:368-372`).
- `cvOutputModulator(index)` — CV channel → modulator (`:392`), an alternative source for a CV channel.

`Engine::updateTrackOutputs()` resolves these each call, layering **gate/CV rotation pools** on top (tracks flagged `isGateOutputRotated`/`isCvOutputRotated` share a rotating pool of channels) and folding in CV-route lanes. This part is **track-type-agnostic** (any engine emits gate/CV), so it's less Note/Curve-bound than linking — but it is the global recompute the engine-update-contention map flags as invoked per-firing-track inside the tick loop.

## 3. Input — CV in + CvRoute

Physical CV inputs (`CvInput`, updated at `Engine.cpp:146`) feed two consumers: `Routing::Source` (the routing source enum, see routing map) and `CvRoute` input lanes (`CvRoute::InputSource::CvIn`). Input assignment is comparatively simple and not track-type-specific; the complexity lives downstream in routing/CvRoute, already mapped.

## Where it's stuck in the Note/Curve era

- **Linking** is the sharp one: a step-traversal clone that only Curve consumes and only Note/Curve produce. Six of eight track types can be assigned links that do nothing.
- **Output layout** is general but entangled with the per-tick rotation recompute (see engine map).
- **Input** is fine; its complexity is already covered by the routing maps.

## Direction

**Lean: remove linking** (product-owner call, 2026-05-30 — never used). The original purpose was a Curve track slaved to a Note track's playhead for synced per-step modulation, from the Note/Curve-only era. That need is now served other ways — global modulators (`global-modulators-v1`), routing, and the Curve track's own clock-mult / global-phase. So the option is **not** "scope vs generalise" but: drop the `linkTrack` UI + the `linkedTrackEngine` wiring + `TrackLinkData` / `linkData()` overrides. Removes a Note/Curve-era subsystem entirely rather than carrying or generalising it.
- Before removing: confirm no project-file dependency (linkTrack is serialized — a removal needs a forward-compatible read that ignores the stored value, no version bump).
- Removing linking also **unblocks the engine-update hoist** — no linked-track intra-loop dependency to worry about.
- **Output layout** rationalisation stays coupled to the engine-update-contention hoist (rotation recompute) — do those together.

(Superseded options, kept for context: scope the UI to capable types; or generalise the contract for procedural types. Both dropped in favour of removal.)

## Relation to other maps

- The engine-update-contention hoist is **gated** on this: the linked-track mid-loop dependency (does Curve need its source's freshly-composed output in the same loop iteration?) — `CurveTrackEngine::tick` reads `linkData()` which is `sequenceState`/timing, *not* composed CV output, so the coupling may be weaker than feared. Worth confirming as the unblock for the engine hoist.
- Routing maps cover the input/source side; this covers link + output assignment.
