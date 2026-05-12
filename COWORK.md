# AGENTS Coworker Rules (Modular Interface Contracts)

Copy the block below into a repo's `AGENTS.md` when that repo should use the global coworker skills. These rules ensure external agent delegations follow high-rigor engineering patterns.

Assumptions:
- `~/.agent/skills/coworker-{main,task,brainstorm,vote,iterate,meeting,vision-check,grillme}/` exist globally.
- The wrapper scripts inside `coworker-main` are functional in the user's terminal.

---

## Reusable AGENTS.md Block

```md
### 1. Global Constraints & Safety (The Iron Laws)
- **Search First:** ALWAYS search the vault (`coworker-search.py` or `get_consensus_summary`) for relevant past decisions before delegating to a coworker. Ground the coworker's prompt in the retrieved architectural "ground truth."
- **User Independence:** The user NEVER runs scripts directly. All functionality must be exposed through agent tools or automated skills. Do not propose manual script execution as a verification or testing step.
- **Mandatory Decision Recording:** The primary agent MUST formalize all architectural and high-level technical decisions in the consensus ledger (`coworker-decide.py`). Technical conclusions reached in chat are not "ground truth" until they are committed to the vault.
- **Recursion Guard:** Do not allow self-recursive agent calls. Every coworker wrapper must increment and check the `COWORKER_LEVEL` environment variable. Abort if depth > 1.
- **Primary Agent Ownership:** Do not delegate architecture decisions, high-level debugging strategy, behavioral tradeoffs, or safety-critical reasoning. Use coworkers only for bulk work, exploration, or draft generation.
- **Adversarial Stance:** When auditing or designing, always adopt a skeptical stance. Prioritize the **Attack Surface** (auth, data loss, race conditions, partial failure). Assume the change can fail in subtle, user-visible ways.
- **Escalated Recovery:** If a runner fails due to sandbox, logging, or permission issues, retry with `sudo` or an elevated shell before switching runners or falling back to local reading.
- **Context Preservation:** Always resolve a small, bounded file set (1-5 files) locally before delegating. Do not pass repo-wide context to coworkers.

### 2. Runner Selection Protocol
- **Prefer `codex`**: For harder decisions, architecture audit, and high-rigor debugging where project-aware indexing is critical.
- **Prefer `opencode`**: For bulk reading, narrative exploration, and easier bounded tasks where explicit file attachment helps.
- **Prefer `goose`, `gemini`, or `ollama`**: For medium bounded design tasks where a tighter one-shot prompt is preferable. (Note: Ollama in this repo is configured for `:cloud` models).
- **Secondary `codex` usage**: Low-cost rote generation (tests, documentation).

### 3. Modular Skill Contracts

#### [Execution] `coworker-task`
- **Input:** Task spec + explicit reference file.
- **Output:** Code artifact (tests, boilerplate, config).
- **Gate (Stop-Gate Verification):** Mandate a **Verification Transcript** (commands used + success output). Specifically challenge the latest changes for second-order failures (e.g., race conditions, stale state) and empty-state behavior.

#### [Discovery] `coworker-brainstorm` (Design Synthesizer)
- **Input:** Interrogation question requiring 2-3 technical approaches + **Adversarial Pass**.
- **Output:** Analysis of Architecture, Data Flow, and potential failure modes.
- **Gate:** Synthesize into a formal **Design Doc** (Architecture, Data Flow, Error Handling). Do not dump raw agent output.

#### [Discovery] `coworker-grillme`
- **Input:** Plan/spec for interrogation.
- **Output:** Dependency-ordered question tree.
- **Gate (Evidence-First):** For bugs, prioritize root cause evidence (errors, logs, repro steps) before fix preferences. Ask only the distilled high-leverage questions locally.

#### [Validation] `coworker-audit`
- **Input:** Audit target (Security, Performance, Style) + 3-7 files.
- **Output:** Findings list with severity ratings and pattern deviations.
- **Gate (Pattern Analysis):** Coworkers MUST identify working examples of the same pattern in the codebase and explain deviations to reduce false positives.

#### [Onboarding] `coworker-onboard`
- **Input:** Subsystem entry points + core logic files.
- **Output:** Architecture summary / newcomer guide.
- **Gate (Design for Isolation):** Guides MUST identify Component Boundaries, Public Interfaces, and Shared State management.

#### [Validation] `coworker-testmatrix`
- **Input:** Source file + optional style reference file.
- **Output:** Exhaustive matrix of test cases and boundary conditions.
- **Gate (Test Boundaries):** Merge unique cases from both agents; ensure assertions follow project conventions.

### 4. Synchronization Protocol
- **Blocking Mode:** Treat all coworker runs as blocking steps. Do not start local implementation or deep inspection while wrappers are active.
- **Parallel Isolation:** All multi-provider swarms (`brainstorm`, `vote`) MUST use automated git-worktree isolation (`--use-worktrees`) to prevent file-system collisions and ensure safe convergence.
- **Brainstorm Wait:** When using `coworker-brainstorm`, wait for BOTH runs to reach a terminal state before synthesizing.
- **Single-Agent Fallback:** If one agent in a dual-run fails after escalation, report the failure and synthesize from the survivor.
```

## Minimal Trigger Examples

- `Use coworker-task to draft boilerplate tests from this reference file. Mandate a verification transcript.`
- `Use coworker-vote to decide if we should use camelCase or snake_case for this module. Use Gemini and Ollama.`
- `Use coworker-iterate to refine the implementation of the parser until it converges.`
- `Use coworker-meeting with Gemini and Goose to debate the new auth architecture.`
- `Use coworker-vision-check to audit the current project for scope creep.`
- `Use coworker-grillme to interrogate this bug report. Prioritize root cause evidence.`
- `Use coworker-brainstorm to propose 2-3 approaches for this refactor and synthesize into a design doc.`

## Implementation Notes

- **Interface Isolation:** These rules prevent "rule interleaving" by forcing the primary agent to follow a specific contract per skill.
- **Evidence-Based:** The gates (Verification Transcript, Pattern Analysis) shift coworker output from "speculative" to "verified."
- **Clean Fallback:** Escalation and survivor-synthesis rules ensure the workflow is robust to individual agent failures.
- **Recall Ready:** The system is designed for long-term project memory via the `.cowork/` directory.
