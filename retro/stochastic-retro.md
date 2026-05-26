# Top 10 Recommendations for Future Projects

Cross-document consensus from 4 stochastic track retrospectives.

Ranked by agreement level across documents.

---

## 4/4 Agreement

### 1. Define state ownership before writing any code
Every field gets exactly one owner (Track, Sequence, Engine, UI). Produce a table before Phase 1. Reference existing track types (Note, Curve, Tuesday) as authority. Deviations must be explicit decisions, not accidents.

### 2. Audit reference code for embedded incompatibility before porting
Ban `std::vector`, `std::mt19937`, heap allocation in tick paths, `time(NULL)`. Document mandatory replacements. The audit output is a prerequisite for Phase 1 approval.

### 3. RAM / sizeof verification on STM32 target, not host
Container gates are architectural constraints, not optimizations. Measure `sizeof` via ARM probes from the STM32 release build — host `sizeof` differs in alignment, padding, and template instantiation. Record numbers before and after feature work.

## 3/4 Agreement

### 4. Serialization round-trip test is the first test
Write → clear → read → assert equality for every field. No feature work proceeds without it passing. This catches missing fields, wrong order, wrong defaults, and enum clamp issues immediately.

### 5. Integer-division cliffs in continuous knobs
`KNOB / N` creates dead zones at low values. Multiply by 10 before dividing. Sweep-test at 0, 1, 2, 5, 10, 50, 100 — assert output changes meaningfully between adjacent test points.

### 6. Single frozen vocabulary/dictionary
One source of truth for all terms. Freeze after architecture spike. Vocabulary changes require atomic rename across code, docs, UI labels, routing targets, tests, and commit messages.

### 7. Validate core identity/storage model before building on it
"Is this stored output or recipe replay?" Answer before writing engine code. Compare ≥ 2 candidates with RAM estimates, lifecycle semantics, and migration cost. Lock the choice with user sign-off.

## 2/4 Agreement

### 8. Cache invalidation needs a single owner
Centralize invalidation so it is not distributed across every setter. Pattern: `setParameter()` calls a single `invalidateCache()` that the engine observes. Audit every knob: "If I turn this while locked, should it invalidate?"

### 9. Run adversarial review early and continuously — not as a post-merge gate
Schedule after engine/model foundations land, then again after output pipeline. Catches ownership races, lifecycle bugs, and math cliffs when they're cheap to fix.

### 10. Consolidate design docs; don't chain superseded references
After a design shift, merge surviving decisions into one current document. Mark old ones REJECTED with a top-of-file explanation. Reader should need one file, not five.

---

## Strong Signals (1–2 docs)

- **No heap allocation in engine constructors** — lint/review gate against `new`/`malloc` in engine paths
- **Defensive UI type guards** — validate data context on every render/input cycle; framework-level, not per-component
- **Freeze MVP scope explicitly** — "in" and "deferred" lists with user sign-off; additions require impact assessment
- **Known bugs block new features** — bug catalog is a blocking work item, not a reference document
- **Architecture decisions require explicit user approval** — decision brief before any core rewrite

---

*Source: dola-analysis.html, stochastic-post-mortem.html, stochastic-retrospective-deeps4.html, stochastic-retrospective-glm.html*
