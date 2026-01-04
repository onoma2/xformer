⏺ How Teletype LIVE Mode Works

  Architecture Overview

  LIVE mode is a REPL (Read-Eval-Print-Loop) that executes single-line commands immediately.

  Key Components

  1. Line Editor (line_editor_t)

  Location: teletype/module/line_editor.h:13-17

  typedef struct {
      char buffer[LINE_EDITOR_SIZE];  // 32 bytes (31 chars + null)
      size_t cursor;                  // Cursor position
      size_t length;                  // Current text length
  } line_editor_t;

  Where stored: Static variable in live_mode.c:34:
  static line_editor_t le;

  This is the current uncommitted text being typed by the user.

  2. Command History

  Location: live_mode.c:30-32

  static tele_command_t history[MAX_HISTORY_SIZE];  // 16 commands
  static int8_t history_line;   // -1 = not browsing history
  static int8_t history_top;    // -1 = empty

  Stores up to 16 previously executed commands as compiled binary (tele_command_t), not as text.

  3. LIVE Script Slot

  Location: script.h:23-24

  #define LIVE_SCRIPT (DELAY_SCRIPT + 1)  // Script index 11
  #define TOTAL_SCRIPT_COUNT (LIVE_SCRIPT + 1)

  LIVE_SCRIPT is a dedicated script slot in scene_state.scripts[11] used for execution.

  Execution Flow

  When User Presses Enter (execute_line() in live_mode.c:319-369)

  Stage 1: Parse & Validate (lines 323-332)
  tele_command_t command;
  status = parse(line_editor_get(&le), &command, error_msg);  // Text → Binary
  if (status != E_OK) return;  // Show error

  status = validate(&command, error_msg);  // Check stack depth, etc.
  if (status != E_OK) return;

  Stack usage here:
  - tele_command_t command (~52 bytes)
  - parse() → scanner() (Ragel, ~150-200 bytes)
  - validate() (~20 bytes)
  - Total: ~250 bytes in main loop context (not interrupt/task)

  Stage 2: Add to History (lines 335-355)
  // Check if command already in history (de-duplicate)
  for (i = history_top; i >= 0; i--)
      if (memcmp(command, history[i]) == 0) found = i;

  // Shuffle history array (newest at index 0)
  memcpy(&history[0], &command, sizeof(command));

  Stage 3: Write to LIVE Script Slot (lines 357-358)
  ss_clear_script(&scene_state, LIVE_SCRIPT);              // Clear slot 11
  ss_overwrite_script_command(&scene_state, LIVE_SCRIPT, 0, &command);  // Write binary

  LIVE_SCRIPT now contains exactly 1 command (the one you just typed).

  Stage 4: Execute (lines 359-364)
  exec_state_t es;
  es_init(&es);
  es_push(&es);
  es_variables(&es)->script_number = LIVE_SCRIPT;  // Set context

  output = run_script_with_exec_state(&scene_state, &es, LIVE_SCRIPT);

  Calls standard VM execution:
  - run_script_with_exec_state() → _run_script_with_exec_state()
  - Iterates lines 0 to SCRIPT_MAX_COMMANDS - 1 (but LIVE only has 1 line)
  - Calls process_command() which evaluates the stack-based bytecode

  Stage 5: Clean Up (lines 367-368)
  history_line = -1;              // Reset history navigation
  line_editor_set(&le, "");       // Clear input buffer

  Key Insights

  1. LIVE mode NEVER stores text after commit
  - Text exists only in line_editor_t.buffer (32 bytes)
  - After Enter: text → parse → binary → discard text
  - History stores tele_command_t (binary), not strings

  2. LIVE_SCRIPT is ephemeral
  - Only contains the last executed command
  - Overwritten on every commit
  - Never persisted to flash (only regular scripts 0-9 are saved)

  3. Parsing happens in main loop, not task/interrupt
  - execute_line() called from process_live_keys()
  - Runs in main event loop with plenty of stack (8KB+)
  - NOT in FILE task (2048 bytes) or interrupt context

  4. Script execution is identical
  - LIVE_SCRIPT runs through same run_script() as normal scripts
  - Same process_command() evaluator
  - Same op dispatch via tele_ops[value]->get()

  Comparison to Performer's Problem

  | Aspect          | Original Teletype         | Performer TeletypeTrack          |
  |-----------------|---------------------------|----------------------------------|
  | Parse context   | Main loop (8KB+ stack)    | FILE task (2048 bytes)           |
  | Parse trigger   | User Enter key (manual)   | Every project load (automatic)   |
  | Text storage    | None (binary only)        | 96 bytes × 6 lines × 4-9 scripts |
  | History storage | Binary tele_command_t[16] | N/A (no LIVE mode in Performer)  |
  | Stack pressure  | Low (occasional parsing)  | High (parse 24-54 lines on load) |

  The Lesson

  Original Teletype avoids stack overflow by:
  1. Parsing in main loop (not task/interrupt) with deep stack
  2. Storing binary in flash (no parsing on scene load)
  3. Parsing rarely (only when user types + Enter)

  Performer TeletypeTrack overflows because:
  1. Parses in FILE task (shallow 2048-byte stack)
  2. Stores text in project files (requires parsing on load)
  3. Parses frequently (every project load = 24-54 lines)

  Solution Path

  Match Teletype's architecture:
  - Store tele_command_t binary in project files (no text)
  - No parsing in FILE task
  - Text generation only for export/display (optional, in Engine/UI task)

  This is exactly what original Teletype does for flash storage, and why it never overflows.
Performer TeletypeScriptView LIVE Mode vs Original Teletype

  Architecture Comparison

  | Aspect            | Original Teletype          | Performer TeletypeScriptView  |
  |-------------------|----------------------------|-------------------------------|
  | Execution Context | Main loop (8KB+ stack)     | UI task (4096 bytes stack)    |
  | Edit Buffer       | line_editor_t.buffer[32]   | char _editBuffer[96]          |
  | History Storage   | Binary tele_command_t[16]  | Text char[4][96]              |
  | History Size      | 16 commands (binary)       | 4 commands (text)             |
  | Trigger           | Enter key → execute_line() | Shift+Encoder → commitLine()  |
  | LIVE Script Slot  | Uses LIVE_SCRIPT (slot 11) | Uses LIVE_SCRIPT context      |
  | Result Display    | Immediate console output   | Stored in _liveResult (int16) |

  Key Differences in Implementation

  1. History Storage: TEXT vs BINARY

  Original Teletype (live_mode.c:30):
  static tele_command_t history[MAX_HISTORY_SIZE];  // 16 × ~52 bytes = 832 bytes BINARY

  Performer (TeletypeScriptViewPage.h:58):
  char _history[HistorySize][EditBufferSize];  // 4 × 96 bytes = 384 bytes TEXT

  Implication:
  - Original: Can replay history without re-parsing (just memcpy to script slot)
  - Performer: Must re-parse from text when recalling history
  - Stack impact: When user navigates history and commits, Performer parses again

  2. Commit Execution Flow

  Original Teletype (live_mode.c:319-369):

  void execute_line() {
      // Stage 1: Parse text → binary (line 326-332)
      tele_command_t command;
      parse(line_editor_get(&le), &command, error_msg);
      validate(&command, error_msg);

      // Stage 2: Store binary in history (line 355)
      memcpy(&history[0], &command, sizeof(command));

      // Stage 3: Write binary to LIVE_SCRIPT slot (line 357-358)
      ss_clear_script(&scene_state, LIVE_SCRIPT);
      ss_overwrite_script_command(&scene_state, LIVE_SCRIPT, 0, &command);

      // Stage 4: Execute via run_script (line 364)
      run_script_with_exec_state(&scene_state, &es, LIVE_SCRIPT);
  }

  Performer (TeletypeScriptViewPage.cpp:452-496):

  void TeletypeScriptViewPage::commitLine() {
      // Stage 1: Parse text → binary (line 458-469)
      tele_command_t parsed = {};
      parse(_editBuffer, &parsed, errorMsg);
      validate(&parsed, errorMsg);

      // Stage 2: Store TEXT in history (line 471)
      pushHistory(_editBuffer);  // ← Stores text, not binary!

      if (_liveMode) {
          // Stage 3: NO write to LIVE_SCRIPT slot!
          TeletypeBridge::ScopedEngine scope(trackEngine);
          exec_state_t es;
          es_init(&es);
          es_set_script_number(&es, LIVE_SCRIPT);

          // Stage 4: Execute DIRECTLY via process_command (line 483)
          process_result_t result = process_command(&state, &es, &parsed);

          // Stage 5: Store result for display (line 484-487)
          _hasLiveResult = result.has_value;
          _liveResult = result.value;
          return;
      }

      // Non-live mode: write to actual script (line 494)
      ss_overwrite_script_command(&state, scriptIndex, _selectedLine, &parsed);
  }

  Critical Differences

  1. LIVE_SCRIPT Slot Usage

  Original:
  - Writes parsed command to scene_state.scripts[LIVE_SCRIPT]
  - Uses standard run_script() path
  - LIVE_SCRIPT slot contains the executed command

  Performer:
  - Never writes to LIVE_SCRIPT slot!
  - Calls process_command() directly with local parsed variable
  - LIVE_SCRIPT is just a context flag (script_number)
  - parsed exists only on stack, never persisted

  Implication: Performer's parsed command is stack-local only.

  2. Stack Frame Analysis

  commitLine() stack frame (TeletypeScriptViewPage.cpp:452-496):

  void TeletypeScriptViewPage::commitLine() {
      // Local variables:
      tele_command_t parsed = {};        // ~52 bytes
      char errorMsg[16] = {};            // 16 bytes

      // Call chain:
      parse(_editBuffer, &parsed, errorMsg);     // → scanner() (~150-200 bytes)
      validate(&parsed, errorMsg);               // (~20 bytes)

      if (_liveMode) {
          TeletypeBridge::ScopedEngine scope(...);  // ~8 bytes (pointer + state)
          exec_state_t es;                          // ~40 bytes
          es_init(&es);

          // Direct execution:
          process_command(&state, &es, &parsed);    // → stack evaluator (~100-200 bytes)
      }
  }

  Estimated stack usage:
  - commitLine() frame: ~88 bytes (parsed + errorMsg + locals)
  - parse() → scanner(): ~150-200 bytes
  - validate(): ~20 bytes
  - process_command(): ~100-200 bytes (command_state_t, sub command processing)
  - Total: ~358-508 bytes

  UI task stack budget: 4096 bytes
  - commitLine path: ~500 bytes
  - UI task overhead: ~300-500 bytes
  - PageManager/Context: ~200-300 bytes
  - Remaining: ~3000+ bytes ✅ No overflow risk

  Why Performer Doesn't Overflow (in LIVE mode)

  UI Task has 4096 bytes vs FILE Task's 2048 bytes:
  - 2× more stack space
  - commitLine() uses ~500 bytes (12% of stack)
  - FILE task's TeletypeTrack::read() uses ~900-1350 bytes (44-66% of stack)

  Only parses ONE command at a time:
  - LIVE mode: 1 command per Enter
  - FILE task: 24-54 commands in loop (accumulative stack pressure)

  No nested loops:
  - LIVE: Single parse → execute → done
  - FILE: Script loop × Line loop × parse()

  History Behavior Difference

  Original Teletype:
  // Recall history[3] (4th oldest command)
  history_prev();  // Sets history_line = 3
  execute_line();  // Just runs the BINARY command again (no re-parse!)

  Performer:
  // Recall history[2]
  recallHistory(-1);              // Copies TEXT to _editBuffer
  commitLine();                   // RE-PARSES the text!
  parse(_editBuffer, &parsed);    // Full parse again

  Stack impact: Performer re-parses every history recall, but since UI task has 4KB stack, this is fine.

  Result Storage

  Original:
  - process_result_t output (local)
  - Displayed immediately to console
  - Not persisted

  Performer:
  _hasLiveResult = result.has_value;  // Line 484
  _liveResult = result.value;         // Line 486
  - Stored in page member variables
  - Displayed in next draw() call (line 94-95)
  - Persists until next execution

  Summary Table

  | Feature         | Original Teletype    | Performer                |
  |-----------------|----------------------|--------------------------|
  | Stack context   | Main loop (8KB+)     | UI task (4KB)            |
  | History format  | Binary (832 bytes)   | Text (384 bytes)         |
  | History size    | 16 commands          | 4 commands               |
  | Parse frequency | Once per new command | Every history recall     |
  | LIVE slot usage | Writes to slot 11    | Context only (no write)  |
  | Execution       | run_script()         | process_command() direct |
  | Result display  | Immediate console    | Stored + draw()          |
  | Stack safety    | ✅ Deep stack        | ✅ Adequate (4KB)        |
  | Re-parse risk   | None (binary)        | Low (one-at-a-time)      |

  Why This Matters for FILE Task Overflow

  The lesson:
  - LIVE mode works in both because it parses one command at a time
  - FILE task fails because it parses 24-54 commands in nested loops
  - Performer's LIVE mode uses 12% of UI task stack (safe)
  - Performer's FILE load uses 44-66% of FILE task stack (dangerous)

  The solution remains:
  - Either increase FILE task stack to 4096 bytes
  - Or store binary tele_command_t in project files (like original flash storage)

  The difference in LIVE mode implementation shows that parsing itself isn't the problem - it's parsing 50+ times in a shallow stack that causes overflow.
