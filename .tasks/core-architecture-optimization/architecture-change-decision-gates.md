# Architecture Change Decision Gates

## Rule

Major architecture change is justified only when the current model is both wrong and repeatedly expensive, and the replacement contract is simpler than the pile of exceptions it removes.

Use this document before proposing a broad refactor from any mismatch in `architecture-research-directions.md`, `assumption-matrix.md`, `state-lifecycle-contract.md`, `pattern-semantics-contract.md`, or `output-authority-map.md`.

## Gates

| Gate | Question | Pass Means |
|---|---|---|
| 1. Repeated pain | Does the mismatch cause repeated bugs, blocked features, or repeated special cases? | The same issue appears across multiple tasks, files, or user-facing workflows. |
| 2. User-visible semantics | Does it affect musical behavior, editing workflow, save/load, pattern switching, or output timing? | The problem is not just code ugliness or theoretical purity. |
| 3. Local fixes fail | Have local patches become fragile, duplicated, or contradictory? | More local patches increase risk or make the behavior harder to reason about. |
| 4. Clear new contract | Can the replacement rule be stated simply? | Example: "Routing shaper state exists only for active stateful shapers." |
| 5. Migration path | Can the change be made incrementally with old project compatibility? | No big-bang rewrite or save-format cliff is required. |
| 6. Verification path | Can simulator/hardware tests prove behavior did not regress? | Build-only verification is not enough. |
| 7. Net payoff | Does it recover enough RAM, remove enough friction, or unblock enough future work? | Payoff is high enough to justify the implementation and hardware risk. |

## Decision Rule

- If a candidate fails gates 4-6, do **documentation or measurement**, not an architecture change.
- If a candidate only saves bytes but does not clarify ownership or semantics, treat it as a **RAM optimization**, not a major architecture change.
- If a candidate changes musical behavior, output timing, pattern switching, or save/load, it needs a hardware-backed verification plan before implementation.
- Do not turn every mismatch into a rewrite. A stressed but understandable design can be kept if the replacement contract is not simpler.

## Current Candidate Decisions

| Candidate | Decision | Reason |
|---|---|---|
| RoutingEngine conditional state | **Yes — narrow architectural change** | Repeated RAM pressure, clear lifecycle contract, measurable savings, and direct verification path. Preserve all stateful shaper behavior. |
| Teletype file backup consolidation | **No — local optimization** | Transaction buffer cleanup. Useful RAM win, but not a core architecture change. |
| Teletype PatternSlot redesign | **Not yet** | Load-bearing swap semantics and unclear replacement contract. Needs source/hardware proof before redesign. |
| Engine container / Teletype engine pool | **Maybe** | Only valid if product semantics allow a cap below 8 simultaneous Teletype tracks. If 8 Teletype tracks remain supported, savings cancel. |
| Model container / lazy sequence storage | **No for now** | Huge blast radius, hard migration, and weak immediate proof. Note/Curve pattern storage is core Performer behavior. |
| Output ownership unification | **No** | Current dual path may be musically intentional. Document and verify priority rules first. |
| UI capability matrix | **No for now** | Maintenance friction exists, but 7 track types are still manageable. Avoid premature abstraction. |
| Pattern sparse defaults / diffs | **Not yet** | Could break value-copy semantics and save/load behavior. Requires project corpus evidence and a clear compatibility story. |

## How Agents Should Use This

Before proposing a major refactor:

1. Name the candidate architecture change.
2. Score it against the gates above.
3. If gates 4-6 fail, propose documentation, measurement, or a local optimization instead.
4. Separate RAM side benefit from architecture value.
5. State the new contract in one sentence.
6. State the hardware verification gate.

Do not answer "this should be redesigned" without passing the gates.
