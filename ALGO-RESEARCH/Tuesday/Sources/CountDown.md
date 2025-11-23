# The `CountDown` Mechanism in the Stomper Algorithm

The `CountDown` feature is the core mechanism for creating pauses or rests in the generated sequences. This is what gives the Stomper algorithm its characteristic syncopated and "broken" rhythmic feel.

Here’s a step-by-step breakdown:

### 1. Initialization

*   In the `Algo_Stomper_Init` function, `CountDown` is initialized to `0`.
    ```c
    Output->Stomper.CountDown = 0;
    ```
*   This ensures that every new 16-step sequence starts "fresh," without an active pause, and will generate a note on the very first step.

### 2. How `CountDown` is Activated

*   `CountDown` is not set on every step. It is only set at the **end** of certain pattern modes, and even then, only if a random chance passes.
*   The line of code responsible for activating the `CountDown` is:
    ```c
    if (Tuesday_BoolChance(&PS->ExtraRandom)) PS->Stomper.CountDown = Tuesday_Rand(&PS->ExtraRandom) % maxticklen;
    ```
*   Let's dissect this:
    *   `if (Tuesday_BoolChance(&PS->ExtraRandom))`: This is a probabilistic check. `Tuesday_BoolChance` has a 50% chance of returning `true`. This means there's a 50% chance that a `CountDown` will be initiated *at the end of a pattern that includes this check*. The `ExtraRandom` generator is seeded by `seed1`.
    *   `PS->Stomper.CountDown = Tuesday_Rand(&PS->ExtraRandom) % maxticklen;`: If the random check passes, `CountDown` is set to a new value.
        *   The variable `maxticklen` is hardcoded to `2`.
        *   The expression `Tuesday_Rand(&PS->ExtraRandom) % 2` will result in either `0` or `1`.
        *   So, if `CountDown` is activated, it will be set to either `0` or `1`.
*   This activation check is strategically placed in most of the `case` statements within the `switch (PS->Stomper.LastMode)` block (e.g., `STOMPER_SLIDEDOWN2`, `STOMPER_LOW2`, `STOMPER_PAUSE2`, `STOMPER_HILOW2`).

### 3. The Effect of an Active `CountDown`

*   At the very beginning of the `Algo_Stomper_Gen` function, there is a crucial check:
    ```c
    if (PS->Stomper.CountDown > 0)
    {
        veloffset = 100 - PS->Stomper.CountDown * 20;
        NOTEOFF();
        PS->Stomper.CountDown--;
    }
    ```
*   If `CountDown` is greater than `0`, this block of code is executed, and the rest of the note generation logic in the `else` block is **skipped** for that step.
*   Here’s what happens inside this `if` block:
    *   `NOTEOFF()`: This macro sets the current step's note to `TUESDAY_NOTEOFF`, effectively creating a rest (a "gate-off").
    *   `PS->Stomper.CountDown--;`: This is the most important part. The `CountDown` value is decremented by 1.

### 4. The `CountDown` Cycle in Action

Let's trace a quick example:

1.  **Step 5**: The algorithm is in a mode like `STOMPER_LOW2`. A note is generated for this step. At the end of the step's logic, the `if (Tuesday_BoolChance(...))` check is performed.
2.  Let's assume the random check passes, and `CountDown` is set to `1`.
3.  **Step 6**: The `Algo_Stomper_Gen` function is called for step 6.
4.  The `if (PS->Stomper.CountDown > 0)` check is now `true` (since `CountDown` is 1).
5.  A `NOTEOFF` is generated for step 6, creating a pause in the sequence.
6.  `CountDown` is decremented to `0`.
7.  **Step 7**: The `Algo_Stomper_Gen` function is called for step 7.
8.  The `if (PS->Stomper.CountDown > 0)` check is now `false` (since `CountDown` is 0).
9.  The `else` block is executed, and a normal note is generated for step 7, resuming the melodic pattern.

### Summary

In essence, `CountDown` is a simple yet powerful mechanism for adding rhythmic variation:

*   It functions as a **pause timer**.
*   It is **probabilistically triggered** at the end of many pattern types, with the probability controlled by `seed1`.
*   When triggered, it typically creates a **1-step pause** (since it's set to `1` and the check is `> 0`).
*   This mechanism is the source of the characteristic **rests and syncopation** of the Stomper algorithm, making its rhythm feel more dynamic and less monotonous than a straight sequence of notes.
