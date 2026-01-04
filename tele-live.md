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

