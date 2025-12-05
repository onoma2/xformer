# How to Create Good Musical Algorithms

Based on an analysis of the PEW|FORMER firmware, particularly the "Tuesday" algorithms, a clear pattern emerges for what makes an algorithm musically effective and "good" versus what makes it sound generic and random.

This document synthesizes those findings into a set of design principles.

## The Core Principle: Purpose-Driven, Constrained Generation

The fundamental difference between a good algorithm and a bad one is **purpose**. Good algorithms are not random note generators; they are specialized engines designed to emulate a specific musical idea, pattern, or process. They succeed by using randomness in a controlled, constrained manner, rather than as the primary driver of the output.

---

## 1. Prioritize a Single, Clear Concept

The best algorithms are built around a single, strong, and well-defined concept. They do one thing well.

*   **Good Example (TriTrance):** The concept is "3-step arpeggio." The code is a simple `switch` statement based on `step % 3`. It produces a predictable and musically useful arpeggiator.
*   **Good Example (DRILL):** The concept is "emulate a two-part drum machine (bass + hi-hats)." The logic is a clear `if/else` statement that separates the generation of low-frequency bass notes from high-frequency hi-hat patterns.
*   **Bad Example (Aphex/Autechre):** The concept is "emulate the entire complex style of a groundbreaking artist." This is too broad and vague, leading to a shallow implementation that relies on generic ideas like "glitch" or "chaos" without a solid structural foundation.

**Guideline:** Start with a clear, simple musical idea. "Make a bassline that alternates between the root and the fifth" is a better starting point than "Make a jazz algorithm."

## 2. Rhythm First, or a Strong Pitch Structure

A good algorithm needs a solid foundation. This can be either rhythmic or melodic, but it must be intentional.

*   **Rhythm-First (DRILL, Stomper):** The `DRILL` algorithm first decides if a step is a bass note or a hi-hat based on a rhythmic pattern. The pitch is chosen *after* the rhythmic role is established. This creates a strong groove. The `Stomper` algorithm uses a state machine to create rhythmic call-and-response phrases.
*   **Strong Pitch Structure (TriTrance, Wobble, Markov):** The `TriTrance` algorithm has a fixed melodic contour. `Wobble` uses the predictable interference pattern of two LFOs. `Markov` uses statistical probability to ensure the next note is musically related to the previous ones.

**Guideline:** Decide on the fundamental driver of your algorithm. Is it a beat? A melodic contour? A mathematical process? Build everything around that core driver.

## 3. Use Constrained, Purposeful Pitch Palettes

Do not pick notes from the full chromatic scale at random. Good algorithms are effective because they limit the available notes to a small, curated palette that serves a specific sonic purpose.

*   **Good Example (DRILL):**
    *   The "hi-hat" voice only chooses from 5 high-register notes (`note = 7 + (_rng.next() % 5)`).
    *   The "bass" voice only chooses from 5 low-register notes, and only changes its note infrequently.
    *   This separation is critical. The bass never plays in the hi-hat register, and vice versa. This mimics the discipline of a human producer.
*   **Good Example (Saiko):** This algorithm uses lookup tables of hand-picked notes that are characteristic of specific genres (Goa, etc.). It generates variety by randomly selecting from this pre-approved, curated list.
*   **Bad Example (Ambient):** The "random walk" `_ambientLastNote = (_ambientLastNote + _ambientDriftDir + 12) % 12;` has no harmonic anchor. The note can drift anywhere, which sounds aimless. Adding a random harmonic on top further muddies the musical intent.

**Guideline:** Define a small, deliberate set of notes for your algorithm to use. If you have multiple "voices," give each its own constrained palette.

## 4. Employ Intelligent Use of Randomness

Randomness should be used for **flair and variation**, not for fundamental structure.

*   **Good Usage:**
    *   **"Should I vary the pattern now?":** `TriTrance` and `Stomper` use randomness to decide *when* to change their underlying notes, but the structure of the pattern itself remains.
    *   **"Which of these few good options should I pick?":** `Markov` uses randomness to pick between two statistically likely next notes. `Saiko` uses it to pick from a list of genre-appropriate notes.
    *   **"How long should this gate be?":** Most original algorithms use a helper function (`RandomSlideAndLength`) to randomize gate and slide values, decoupling rhythm from the core pitch logic.

*   **Bad Usage:**
    *   **"What should the next note be?":** `_autechreCurrentNote = (_autechreCurrentNote + _rng.nextRange(5) - 2 + 12) % 12;`. This is a random walk. The next note has no meaningful relationship to the last, resulting in a chaotic and unmusical sequence.
    *   **"What should the entire pattern be?":** The `APHEX` algorithm initializes its 8-step pattern with purely random notes. The foundation is random, so the result sounds random.

**Guideline:** Use randomness to make small decisions within a larger, deterministic structure. Never let randomness be in charge of the foundational elements of your pattern.
