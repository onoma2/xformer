This is a comprehensive architectural plan to port the **ER-101/102 (Indexed/List)** workflow to the **Westlicht Performer** hardware.

This plan assumes you are essentially forking the firmware to create a "Westlicht ER-Edition."

---

### **Phase 1: The Core Engine Rewrite (Time Accumulator)**

*Goal: Move from "Step per Clock Division" to "Step per Duration."*

**1. Data Structure Updates (`Sequence.h` / `Track.h`)**
The `Step` struct must be expanded. In standard x0x, "Gate Length" is a percentage. In ER-101, it is an absolute clock count.

```cpp
struct Step {
    uint8_t note_index;   // Points to Voltage Table (0-99)
    uint32_t duration;    // TICKS to wait before next step (0-max)
    uint32_t gate_length; // TICKS to hold the gate high
    uint8_t group_mask;   // Bitmask for ER-102 Groups (A-D)

    // Legacy support (optional)
    uint8_t velocity;
    uint8_t ratchet;
};

```

**2. The Track Class (`Track.cpp`)**
You need two new counters that persist between clock interrupts.

```cpp
class Track {
    uint32_t step_timer;      // Counts up to Step.duration
    uint32_t gate_timer;      // Counts down from Step.gate_length
    // ... existing members
};

```

**3. The Clock Interrupt (`Engine::tick`)**
This is the heart of the rewrite.

```cpp
void Track::tick() {
    // 1. Handle Gate (Counts down independently of step progress)
    if (gate_timer > 0) {
        gate_timer--;
        if (gate_timer == 0) {
            hardware_gate_write(LOW);
        }
    }

    // 2. Handle Duration (The Accumulator)
    step_timer++;
    Step& current = steps[current_step_index];

    // 3. Check for Step Transition
    if (step_timer >= current.duration) {
        advance_step(); // Logic to move index +1
    }
}

```

---

### **Phase 2: The "Indexed" Voltage System**

*Goal: Reutilize the `UserScale` system to act as a raw Voltage Table.*

**1. The "No-Modulo" Lookup**
Modify the pitch generation logic in `Track::process_cv`.

```cpp
float Track::calculate_pitch(Step& s) {
    if (CONFIG_ER_MODE) {
        // Direct Lookup: Index 13 is just the 13th voltage in the array.
        // No octave math (note / 12). No modulo (note % 12).
        return UserScale::get_voltage(s.note_index);
    } else {
        // Standard Westlicht Logic
        return Scale::get_voltage(s.note_index);
    }
}

```

**2. Table Editor UI**

* **Task:** Clone the existing `Scale Edit` page.
* **Modification:** Change the display formatting.
* *Old:* "C#", "D", "Eb"
* *New:* "+0.083V", "+0.166V" (Displaying the raw float value).


* **Input:** Allow the encoder to increment voltage in `0.001V` (1mV) steps or coarse `0.083V` (semitone) steps.

---

### **Phase 3: The List Editor (Insert/Delete/Copy)**

*Goal: Implement the "ER-101" editing workflow on a grid controller.*

**1. The "Insert" Logic**
The Performer normally has a fixed number of steps. You need a "Max Steps" buffer (e.g., 64) and a dynamic "Active Length."

```cpp
void Track::insert_step(int insertion_index) {
    // 1. Check bounds
    if (track_length >= MAX_STEPS) return;

    // 2. Shift data to the right
    for (int i = track_length; i > insertion_index; i--) {
        steps[i] = steps[i - 1];
    }

    // 3. Clone previous step into new slot (ER-Philosophy)
    steps[insertion_index] = steps[insertion_index - 1];

    // 4. Update length
    track_length++;
}

```

**2. The UI Mappings**

* **Insert:** Mapped to `FUNC + Encoder Click` (or similar). Triggers `insert_step(cursor_pos)`.
* **Delete:** Mapped to `FUNC + Encoder Long Press`. Shifts array left.
* **Copy/Paste:**
* **Action:** Copy Step `X` to Clipboard.
* **Action:** Paste at `Y`. Calls `insert_step(Y)` then overwrites data with Clipboard.



---

### **Phase 4: The Math Engine (Destructive)**

*Goal: Reutilize the "Edit" menu for batch processing.*

**1. The Math Function**
Create a generic function that takes an Operator, a Target, and a Value.

```cpp
void Track::apply_math(Range selection, Target target, Op operator, float value) {
    for (int i = selection.start; i <= selection.end; i++) {
        switch (target) {
            case CV:
                // Direct modification of the Index pointer
                if (operator == ADD) steps[i].note_index += (int)value;
                break;
            case DURATION:
                if (operator == MUL) steps[i].duration *= value; // Time stretch
                break;
            // ... cases for Gate, etc.
        }
    }
}

```

**2. Menu Integration**

* Add a **"Math"** entry to the Track Menu.
* **Sub-menu:**
1. `Target` (Note, Dur, Gate)
2. `Op` (+, -, *, /, Set, Rand)
3. `Value` (Encoder variable)
4. `Apply` (Executes the loop).



---

### **Phase 5: ER-102 Modifiers (Non-Destructive)**

*Goal: Use the Modulation Matrix with "Group" logic.*

**1. Group Assignment UI**

* **Mode:** Enter "Group Mode".
* **Action:** Pressing Grid buttons 1-16 toggles the bitmask on those steps.
* `Step[i].group_mask |= (1 << current_group_index);`



**2. The Routing Matrix Update**
Modify `Routing::update()` to check the mask.

```cpp
// In the CV calculation loop
if (route.source == CV_IN_1 && route.flags & USE_GROUPS) {
    Step& s = tracks[current_track].get_current_step();

    // Check if the current step is a member of the Target Group
    if (s.group_mask & route.target_group) {
        apply_modulation(route);
    } else {
        // Do nothing (CV is ignored for this step)
    }
}

```

### **Feasibility Checklist**

| Feature | Difficulty | Hardware Limitation? | Notes |
| --- | --- | --- | --- |
| **Duration Engine** | 🟡 Medium | No | Requires rewriting `tick()` logic completely. |
| **Voltage Tables** | 🟢 Easy | No | Just a display hack of `UserScale`. |
| **Insert/Delete** | 🟡 Medium | RAM | Max steps per track might need to be limited (e.g., 64) to prevent RAM overflow. |
| **Math Ops** | 🟢 Easy | No | Standard C++ math logic. |
| **OLED UI** | 🔴 Hard | Screen Size | Visualizing variable step lengths on a 128x64 screen is tricky. Stick to a numerical list view or a "current step" focused view. |

### **Recommended Starting Point**

Start with **Phase 2 (User Scales as Voltage Tables)**. It requires the least code change but gives immediate "ER-101" vibes. Then tackle **Phase 1 (Duration Engine)**.
