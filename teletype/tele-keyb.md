# Teletype Keyboard-less Editing Requirements

This document outlines the absolute requirements for character input and functional interaction needed to edit Teletype scripts on the Performer firmware without a physical keyboard.

## 1. Character Set Requirements (55 Total)

### Alphanumeric (36)
*   **Letters (26):** `A` through `Z` (Required for all op tokens and variables).
*   **Numbers (10):** `0` through `9` (Required for values and pattern indices).

### Required Symbols (19)
*   **Syntax & Structure (7):**
    1.  `.` (Dot) - Sub-ops and namespaces (e.g., `TR.P`)
    2.  `:` (Colon) - Modifiers (e.g., `IF X:`)
    3.  `;` (Semicolon) - Multi-command separator
    4.  ` ` (Space) - Token separator (**Crucial**)
    5.  `$` (Dollar) - Script alias
    6.  `@` (At) - Turtle operations
    7.  `?` (Question Mark) - Ternary "if" operator
*   **Math & Logic Aliases (12):**
    *   Arithmetic: `+`, `-`, `*`, `/`, `%`
    *   Comparison: `=`, `>`, `<`
    *   Bitwise/Logical: `!`, `&`, `|`, `^`

---

## 2. Functional Action Requirements
These represent the non-typing physical interactions needed to manage the editor.

1.  **Enter / Confirm:** Commit a line or execute a command.
2.  **Backspace / Delete:** Correct errors.
3.  **Line Navigation (Up/Down):** Select line (1-6) within a script.
4.  **Cursor Navigation (Left/Right):** Position the cursor within a line.
5.  **Script Selector:** Switch between the 10 available scripts (1-8, Metro, Init).
6.  **Mode Toggle:** Switch between Editor, Tracker, and Dashboard.

---

## 3. Step Button T9 Map (Draft)
T9-style cycling per step key. Base layer includes digits plus full A-Z. Modifiers add symbols and op tokens.

### Base (no modifier)
1: `1 A B`  
2: `2 C D`  
3: `3 E F`  
4: `4 G H`  
5: `5 I J`  
6: `6 K L`  
7: `7 M N`  
8: `8 O P`  
9: `9 Q R`  
10: `0 S T`  
11: `. U V`  
12: `:` `W X`  
13: `;` `Y Z`  
14: `CV` `IN`  
15: `TR` `PARAM`  
16: `EVERY` `IF`

### Shift (Shift + Step)
1: `!`  
2: `<`  
3: `>`  
4: `*`  
5: `/`  
6: `%`  
7: `(`  
8: `)`  
9: `[`  
10: `]`  
11: `&`  
12: `|`  
13: `^`  
14: `'`  
15: `,`  
16: `#`

### Page (Page + Step) = Op Tokens
1: `CV`  
2: `CV.SLEW`  
3: `CV.OFF`  
4: `CV.SET`  
5: `TR.P`  
6: `TR`  
7: `TR.TOG`  
8: `TR.TIME`  
9: `IN`  
10: `PARAM`  
11: `STATE`  
12: `P`  
13: `PN`  
14: `P.NEXT`  
15: `P.I`  
16: `P.L`

### Shift+Page (Shift + Page + Step) = Logic/Math
1: `IF`  
2: `ELIF`  
3: `ELSE`  
4: `EVERY`  
5: `SKIP`  
6: `L`  
7: `W`  
8: `ADD`  
9: `SUB`  
10: `MUL`  
11: `DIV`  
12: `MOD`  
13: `MIN`  
14: `MAX`  
15: `LIM`  
16: `AVG`

---

## 4. Estimated Frequency & Priority
Prioritizing these inputs is essential for a smooth "encoder + button" workflow.

### Token Frequency Breakdown

| Tier | Category | Tokens / Characters | Purpose |
| :--- | :--- | :--- | :--- |
| **1. Constant** | **Essential** | `Space`, `0-9`, `.` (Dot) | The dot is critical for namespacing (P.N, TR.P). Used in 90% of lines. |
| **2. Very High** | **Core IO & Logic** | `CV`, `TR`, `IF`, `:`, `X`, `Y`, `A`, `B` | The colon is mandatory for all logic/timing modifiers. |
| **3. High** | **Math & Structure** | `+`, `-`, `;`, `RAND`, `PARAM` | Plus/Minus are popular aliases. Semicolon essential for multi-cmd lines. |
| **4. Medium** | **State & Timing** | `M`, `DEL`, `IN`, `P.NEXT`, `PN` | Setup, metro, hardware inputs, and pattern access. |
| **5. Low** | **Advanced** | `ELSE`, `ELIF`, `L`, `W`, `Q.*`, `@`, `$`, `?` | Loops, complex logic, queues, and turtle graphics. |
| **unused** | **Ignored** | `,` (Comma) | Not used in script syntax. Only for comments. |

### Character vs. Token Priority

| Frequency | Items | Integration Priority |
| :--- | :--- | :--- |
| **Very High** | `Enter`, `Space`, `0-9`, `Navigation`, `.` | **Tier 1 (Direct Physical Access):** Map to dedicated buttons or encoder clicks. |
| **High** | `Backspace`, `X, Y, A, B`, `:`, `CV`, `TR`, `IF` | **Tier 2 (Top-Level UI):** First items in the character scroll or Op palette. |
| **Medium** | `;`, `+ -`, `DEL`, `P.N`, `M`, `Script Select` | **Tier 3 (Sub-Menus):** Accessible via category-based context menus. |
| **Low** | `A-Z` (Full list), `* /`, `! & \| ^`, `@`, `$`, `?` | **Tier 4 (Full Picker):** Buried in the full alphanumeric list or advanced menus. |

---

## 5. Design Insights for Performer
*   **Space is King:** Since every command requires multiple spaces (e.g., `CV 1 N 60`), mapping `Space` to a dedicated physical button (like a Shift+Encoder click) will double the editing speed.
*   **Token over Character:** Selecting "ADD" from a menu is significantly faster than scrolling to `A`, then `D`, then `D`. The UI should favor inserting full Op tokens.
*   **Numbers:** Direct access to numbers 0-9 (perhaps via the 8 Step buttons) would greatly benefit the 50% of scripting that involves entering values.

## 6. Proposed Op Palette Layout (Context Menu)

To minimize character-by-character typing, the UI should provide a category-based context menu. Here is the layout for the two most frequent categories:

### CV Palette (Control Voltage)
1.  **`CV`** (Core) - Sets a CV output. *Usage: `CV 1 N 60`*
2.  **`CV.SLEW`** (High Freq) - Sets slew time. *Usage: `CV.SLEW 1 100`*
3.  **`CV.OFF`** (Medium) - Sets offset. *Usage: `CV.OFF 1 12`*
4.  **`CV.SET`** (Medium) - Sets value instantly. *Usage: `CV.SET 1 0`*
5.  **`V`** (Helper) - Volt helper (0-10V). *Usage: `CV 1 V 5`*
6.  **`N`** (Helper) - Note helper. *Usage: `CV 1 N 60`*
7.  **`IN`** (Input) - Read input jack. *Usage: `CV 1 IN`*
8.  **`PARAM`** (Input) - Read parameter knob. *Usage: `CV 2 PARAM`*

### TR Palette (Trigger/Gate)
1.  **`TR.P`** (Highest Freq) - Pulse a trigger. *Usage: `TR.P 1`*
2.  **`TR`** (High Freq) - Set gate state. *Usage: `TR 1 1`*
3.  **`TR.TOG`** (Medium) - Toggle state. *Usage: `TR.TOG 1`*
4.  **`TR.TIME`** (Medium) - Set pulse duration. *Usage: `TR.TIME 1 50`*
5.  **`TR.POL`** (Low) - Set polarity. *Usage: `TR.POL 1 1`*
6.  **`STATE`** (Input) - Read trigger input state. *Usage: `IF STATE 1`*

### Math Palette
1.  **`+`** / **`-`** / **`*`** / **`/`** / **`%`** (Aliases for `ADD`, `SUB`, `MUL`, `DIV`, `MOD`)
2.  **`RAND`** / **`RRAND`** - Random numbers.
3.  **`MIN`** / **`MAX`** - Comparison.
4.  **`LIM`** - Limit value to range.
5.  **`AVG`** - Average.
6.  **`SGN`** - Sign of value.

### Logic & Flow Palette
1.  **`IF`** / **`ELSE`** / **`ELIF`** - Conditionals.
2.  **`:`** (Colon) - Required modifier separator.
3.  **`GT`** / **`LT`** / **`EQ`** / **`NE`** - Comparators.
4.  **`AND`** / **`OR`** / **`XOR`** / **`NOT`** - Logical/Bitwise ops.
5.  **`EVERY`** / **`SKIP`** - Rhythmic execution modifiers.
6.  **`L`** / **`W`** - Loops (For / While).
7.  **`BREAK`** - Exit a loop.
8.  **`SCRIPT`** - Run another script manually.

### Pattern Palette
1.  **`P`** (Get/Set) - Access values in the current pattern.
2.  **`PN`** - Direct bank/index access.
3.  **`P.NEXT`** - Advance index and return new value.
4.  **`P.PUSH`** / **`P.POP`** - Add/Remove from end of pattern.
5.  **`P.L`** - Get/Set pattern length.
6.  **`P.I`** - Get/Set playhead index.
7.  **`P.ROT`** - Rotate pattern data.
8.  **`P.SHUF`** - Shuffle pattern data.
9.  **`P.REV`** - Reverse pattern data.
