# Good Macro + Quick Edit Guidelines

## Good Macro (function + UI)

Function
- High-leverage, multi-parameter action that is hard or impossible to do step-by-step.
- Produces a coherent, repeatable pattern (not just random noise).
- Applies across a clear scope (selection if present, else loop range).
- Respects track semantics and clamps to valid ranges.

UI
- One action + one menu choice, with short labels.
- Immediate feedback message that confirms the result.
- Discoverable shortcut (Page+Step), with LEDs indicating availability.

LEDs
- Only lit when relevant (Page held).
- Green for quick edits, yellow for special macro shortcuts.
- No extra LED states unless the macro has a modal preview.

Ranges
- Use selection when it exists; otherwise operate on the loop range.
- Never touch steps outside the target range.

Defaults
- Safe, neutral results (full range or no-op).
- Remember last choice when re-invoked, but allow explicit OFF.
- Always show a short confirmation message.

## Good Quick Edit (function + UI)

Function
- Single-gesture operation that yields meaningful structure.
- Encodes a curated preset, not free-form math.
- Supports preview/selection with minimal state.

UI
- Press-hold to enter, encoder to audition, release to apply.
- Short feedback text (e.g., PNO MAJ7) and clear OFF state.
- Minimal disruption to normal editing flow.

LEDs
- Indicate quick edit pads only while Page is held.
- Keep color semantics consistent with macros.

Ranges
- Target is explicit: active steps/stages or selection.
- Avoid modifying inactive or Off elements unless chosen.

Defaults
- Default to OFF or no-op until a choice is made.
- Keep last-used selection for repeatability.
