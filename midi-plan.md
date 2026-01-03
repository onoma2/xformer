
 Full MIDI Integration for TeletypeTrack

 Objective

 Implement complete teletype MIDI functionality:
 1. Populate scene_state.midi with MIDI events (MI.* data ops)
 2. Support MI.$ op for assigning scripts to MIDI events
 3. Automatically trigger scripts when MIDI arrives

 Current Situation

 - TeletypeTrackEngine has NO MIDI integration
 - scene_state.midi structure exists but never populated
 - MI.$ op is already implemented in teletype VM (reads/writes on_script, off_script, cc_script)
 - MI.* ops read from empty state
 - No script triggering on MIDI events

 Implementation Approach

 Full teletype MIDI pattern (like MidiCvTrack):
 - Add MidiSourceConfig to TeletypeTrack model for port/channel selection
 - Implement receiveMidi() to actively consume MIDI when source matches
 - Translate Performer's MidiMessage → teletype's scene_midi_t structure
 - Automatically trigger assigned scripts when MIDI arrives
 - Can receive MIDI in background (not just when selected)

 Key Design Decisions

 | Decision            | Choice                                     | Rationale                                          |
 |---------------------|--------------------------------------------|----------------------------------------------------|
 | MIDI routing        | Use receiveMidi() with source filtering    | Like MidiCvTrack - can work in background          |
 | MIDI source config  | Add MidiSourceConfig _source to model      | User selects port (MIDI/USB) + channel (Omni/1-16) |
 | Message consumption | Return true if matched source              | Prevent double-processing by other tracks          |
 | Buffer clearing     | Clear BEFORE populating new data           | Fresh slate per MIDI message                       |
 | Script triggering   | Trigger AFTER populating data              | Scripts see the MIDI event that triggered them     |
 | Script execution    | Use run_script() from teletype.h           | Standard teletype VM function                      |
 | Channel storage     | 0-15 (as-is from MidiMessage)              | Internal consistency                               |
 | CC de-duplication   | No de-duplication, append only             | Simpler, predictable                               |
 | Overflow handling   | Drop new events when buffer full (10 max)  | Preserve oldest events (FIFO)                      |
 | Clock/Transport     | Defer to future work                       | Already handled by Engine's Clock system           |
 | MI.$ op             | Already works (implemented in teletype VM) | No changes needed                                  |

 Files to Modify

 1. TeletypeTrack.h (Model)

 Location: src/apps/sequencer/model/TeletypeTrack.h

 Add includes:
 #include "MidiConfig.h"

 Add property (around line 200, with other accessors):
 // midiSource
 const MidiSourceConfig &midiSource() const { return _midiSource; }
       MidiSourceConfig &midiSource()       { return _midiSource; }

 Add private member (around line 270, with other _members):
 MidiSourceConfig _midiSource;

 Update write() method:
 Add after line that writes other config:
 _midiSource.write(writer);

 Update read() method:
 Add after line that reads other config:
 _midiSource.read(reader);

 2. TeletypeTrackEngine.h

 Location: src/apps/sequencer/engine/TeletypeTrackEngine.h

 Changes:
 Add after line ~60 (after existing virtual methods):
 virtual bool receiveMidi(MidiPort port, const MidiMessage &message) override;
 virtual void clearMidiMonitoring() override;

 private:
 void runMidiTriggeredScript(int scriptIndex);
 void processMidiMessage(const MidiMessage &message);

 3. TeletypeTrackEngine.cpp

 Location: src/apps/sequencer/engine/TeletypeTrackEngine.cpp

 Add includes:
 #include "core/midi/MidiMessage.h"
 #include "core/midi/MidiUtils.h"

 extern "C" {
 #include "teletype.h"  // For run_script()
 }

 Implement clearMidiMonitoring():
 void TeletypeTrackEngine::clearMidiMonitoring() {
     scene_state_t &state = _teletypeTrack.state();
     scene_midi_t &midi = state.midi;

     // Clear event counts
     midi.on_count = 0;
     midi.off_count = 0;
     midi.cc_count = 0;

     // Reset last event tracking
     midi.last_event_type = 0;
     midi.last_channel = 0;
     midi.last_note = 0;
     midi.last_velocity = 0;
     midi.last_controller = 0;
     midi.last_cc = 0;
 }

 Implement receiveMidi() (with source filtering):
 bool TeletypeTrackEngine::receiveMidi(MidiPort port, const MidiMessage &message) {
     // Filter by MIDI source (port + channel)
     if (!MidiUtils::matchSource(port, message, _teletypeTrack.midiSource())) {
         return false;  // Not for us
     }

     // Process the message (populate data + trigger script)
     processMidiMessage(message);

     return true;  // Consumed - prevent other tracks from seeing it
 }

 Implement processMidiMessage() (extracted from old monitorMidi):
 void TeletypeTrackEngine::processMidiMessage(const MidiMessage &message) {
     // Clear buffers for fresh state
     clearMidiMonitoring();

     scene_state_t &state = _teletypeTrack.state();
     scene_midi_t &midi = state.midi;

     int8_t scriptToRun = -1;

     // Handle Note On
     if (message.isNoteOn() && message.velocity() > 0) {
         if (midi.on_count < MAX_MIDI_EVENTS) {
             midi.note_on[midi.on_count] = message.note();
             midi.note_vel[midi.on_count] = message.velocity();
             midi.on_channel[midi.on_count] = message.channel();
             midi.on_count++;
         }
         midi.last_event_type = 1;
         midi.last_channel = message.channel();
         midi.last_note = message.note();
         midi.last_velocity = message.velocity();

         // Check if script assigned to Note On
         scriptToRun = midi.on_script;
     }
     // Handle Note Off (including velocity-0 note-ons)
     else if (message.isNoteOff() || (message.isNoteOn() && message.velocity() == 0)) {
         if (midi.off_count < MAX_MIDI_EVENTS) {
             midi.note_off[midi.off_count] = message.note();
             midi.off_channel[midi.off_count] = message.channel();
             midi.off_count++;
         }
         midi.last_event_type = 0;
         midi.last_channel = message.channel();
         midi.last_note = message.note();
         midi.last_velocity = 0;

         // Check if script assigned to Note Off
         scriptToRun = midi.off_script;
     }
     // Handle Control Change
     else if (message.isControlChange()) {
         if (midi.cc_count < MAX_MIDI_EVENTS) {
             midi.cn[midi.cc_count] = message.controlNumber();
             midi.cc[midi.cc_count] = message.controlValue();
             midi.cc_channel[midi.cc_count] = message.channel();
             midi.cc_count++;
         }
         midi.last_event_type = 2;
         midi.last_channel = message.channel();
         midi.last_controller = message.controlNumber();
         midi.last_cc = message.controlValue();

         // Check if script assigned to CC
         scriptToRun = midi.cc_script;
     }

     // Trigger assigned script if valid (0-3 are S0-S3)
     if (scriptToRun >= 0 && scriptToRun < EDITABLE_SCRIPT_COUNT) {
         runMidiTriggeredScript(scriptToRun);
     }
 }

 Add helper method for script execution:
 void TeletypeTrackEngine::runMidiTriggeredScript(int scriptIndex) {
     scene_state_t &state = _teletypeTrack.state();

     // Use teletype VM's run_script function
     // Declared in teletype/src/teletype.h
     run_script(&state, scriptIndex);

     // Note: run_script() executes all lines of the script synchronously
     // CV/TR outputs are buffered and will be processed by the engine
 }

 4. TeletypeTrackListPage.cpp (UI for MIDI source)

 Location: src/apps/sequencer/ui/pages/TeletypeTrackListPage.cpp

 Add MIDI source parameter to the track list page (similar to MidiCvTrackListPage).

 Find existing parameter definitions and add:
 { "MIDI Source", [](TeletypeTrack &track, StringBuilder &str) {
     track.midiSource().print(str);
 }, [](TeletypeTrack &track, int value, bool shift) {
     track.midiSource().edit(value, shift);
 }}

 5. Create Unit Tests

 Location: src/tests/unit/sequencer/TestTeletypeTrackEngineMidi.cpp (NEW FILE)

 Test cases to implement:
 1. note_on_single_event - Verify Note On populates arrays correctly
 2. note_on_overflow - Verify 11th note is dropped (MAX_MIDI_EVENTS = 10)
 3. note_off_handling - Verify Note Off and velocity-0 handling
 4. control_change_basic - Verify CC populates cn[], cc[], cc_channel[]
 5. control_change_no_deduplication - Verify duplicate CCs create separate entries
 6. last_event_tracking - Verify last_note, last_velocity, etc. update correctly
 7. clear_monitoring - Verify clearMidiMonitoring() zeros counts
 8. buffer_clearing_per_message - Verify new monitorMidi() clears previous state
 9. script_triggering_note_on - Verify assigned script runs on Note On
 10. script_triggering_note_off - Verify assigned script runs on Note Off
 11. script_triggering_cc - Verify assigned script runs on CC
 12. no_script_assigned - Verify no crash when script = -1
 13. script_sees_midi_data - Verify triggered script can read MI.N, MI.V, etc.
 14. source_filtering_port - Verify only MIDI from matching port is processed
 15. source_filtering_channel - Verify only MIDI from matching channel is processed
 16. source_omni_mode - Verify Omni mode accepts all channels
 17. message_consumed - Verify receiveMidi() returns true when matched

 Follow UnitTest.h framework:
 #include "UnitTest.h"
 #include "apps/sequencer/engine/TeletypeTrackEngine.h"

 UNIT_TEST("TeletypeTrackEngineMidi") {

 CASE("note_on_single_event") {
     // Test implementation
 }

 // ... more test cases

 } // UNIT_TEST

 6. Register Test

 Location: src/tests/unit/sequencer/CMakeLists.txt

 Add:
 register_sequencer_test(TestTeletypeTrackEngineMidi TestTeletypeTrackEngineMidi.cpp)

 7. Update TeletypeTrack constructor/clear

 Location: src/apps/sequencer/model/TeletypeTrack.cpp

 In constructor or clear() method:
 Ensure _midiSource is initialized to default (MIDI port, Omni channel):
 _midiSource.clear();  // Sets to MIDI port, channel -1 (Omni)

 Translation & Script Triggering

 | Performer MidiMessage | →   | Teletype scene_midi_t                           | Script Triggered   |
 |-----------------------|-----|-------------------------------------------------|--------------------|
 | msg.isNoteOn()        | →   | note_on[], note_vel[], on_channel[], on_count++ | on_script if >= 0  |
 | msg.isNoteOff()       | →   | note_off[], off_channel[], off_count++          | off_script if >= 0 |
 | msg.isControlChange() | →   | cn[], cc[], cc_channel[], cc_count++            | cc_script if >= 0  |

 MidiMessage API:
 - message.note() - Note number (0-127)
 - message.velocity() - Velocity (0-127)
 - message.channel() - MIDI channel (0-15)
 - message.controlNumber() - CC number (0-127)
 - message.controlValue() - CC value (0-127)

 Teletype constants:
 - MAX_MIDI_EVENTS = 10 (defined in state.h)

 Testing Strategy

 Unit Tests

 Run from build/sim/debug/src/tests/unit/sequencer/:
 ./TestTeletypeTrackEngineMidi

 Manual Integration Testing

 Test 1: MIDI-triggered CV output
 1. Build simulator
 2. Create TeletypeTrack on any track slot
 3. Set MIDI Source to desired port (MIDI/USB) and channel (Omni or 1-16)
 4. Edit Script 0 (S0):
 CV 1 MI.NV  # Output MIDI note as voltage to CV 1
 TR.PULSE 1  # Pulse gate 1
 5. Assign Script 0 to Note On events:
 MI.$ 1 1   # Assign S0 (script 1 in 1-indexed) to Note On (event 1)
 6. Send MIDI notes from controller to matching port/channel
 7. Expected: CV 1 outputs note voltage, Gate 1 pulses on each note

 Test 1b: Source filtering
 1. Set MIDI Source to USB MIDI, Channel 2
 2. Send notes on MIDI port (not USB) → Expected: No response
 3. Send notes on USB, Channel 1 → Expected: No response
 4. Send notes on USB, Channel 2 → Expected: Script triggers ✓

 Test 2: MIDI CC control
 1. Set MIDI Source to desired source
 2. Edit Script 1 (S1):
 CV 2 MI.LCCV  # Output last CC value to CV 2
 3. Assign Script 1 to CC events:
 MI.$ 3 2   # Assign S1 (script 2) to CC (event 3)
 4. Send CC messages from matching source
 5. Expected: CV 2 follows CC value

 Test 3: No script assigned
 1. Clear script assignments:
 MI.$ 0 0   # Clear all assignments
 2. Send MIDI
 3. Expected: No crash, MI.* ops still readable manually

 Expected Behavior

 MIDI Data Ops:
 - MI.N returns MIDI note number (0-127)
 - MI.NV returns MIDI note as voltage (uses ET table lookup)
 - MI.V returns velocity (0-127)
 - MI.VV returns velocity scaled to 0-16383
 - MI.CC returns CC value
 - MI.LN returns last note received
 - MI.NL returns count of pending notes (typically 1 after clear-per-message)

 Script Assignment (MI.$ op - already works):
 - MI.$ 1 returns assigned script for Note On (1-indexed, 0 = none)
 - MI.$ 1 2 assigns Script 1 (S1) to Note On events
 - Event types: 0=all, 1=note on, 2=note off, 3=CC

 Automatic Triggering:
 - When MIDI arrives and script assigned, script runs automatically
 - Script can read MI.N, MI.V, etc. to see what triggered it
 - CV/TR outputs from script take effect immediately

 Risks & Edge Cases

 | Risk                         | Mitigation                                                          |
 |------------------------------|---------------------------------------------------------------------|
 | Buffer overflow (>10 events) | Drop newest events silently (tested)                                |
 | Velocity-0 note-ons          | Treat as Note Off (already handled by Engine)                       |
 | Rapid MIDI events            | Per-message clearing prevents accumulation; script runs per message |
 | Channel confusion            | Store 0-15 internally, document if needed                           |
 | Script execution time        | Scripts run synchronously in receiveMidi(); keep scripts short      |
 | Recursive script calls       | Teletype VM already prevents infinite recursion                     |
 | Invalid script index         | Check scriptIndex >= 0 && < EDITABLE_SCRIPT_COUNT before calling    |
 | Script modifies MIDI state   | OK - state already populated before script runs                     |
 | Multiple teletype tracks     | Each track has own source config - no conflicts if sources differ   |
 | Message consumption          | Track consumes MIDI when source matches - other tracks won't see it |

 Out of Scope (Future Work)

 - MIDI Clock/Start/Stop/Continue script triggering (clk_script, start_script, etc.)
   - Clock already handled by Engine's Clock system
   - Can be added later if transport sync needed
 - Channel filtering configuration (accept all channels in v1)
 - Program Change, Pitch Bend, Aftertouch (not in scene_midi_t structure)
 - Event accumulation modes (current: clear per message)
 - Async script execution (current: synchronous, runs immediately)

 Success Criteria

 ✅ TeletypeTrack receives MIDI when selected
 ✅ MI.N, MI.V, MI.CC ops return correct values matching MIDI input
 ✅ MI.$ op works (assign/read script assignments)
 ✅ Scripts automatically trigger when MIDI arrives and script assigned
 ✅ Scripts can read MIDI data (MI.N, MI.V, etc.) that triggered them
 ✅ CV/TR outputs work from MIDI-triggered scripts
 ✅ All unit tests pass
 ✅ Manual testing shows complete MIDI workflow
Full MIDI Integration for TeletypeTrack

 Plan Status

 ✅ READY FOR IMPLEMENTATION - All critical review points addressed:
 1. ✅ Thread safety: Added TeletypeBridge::ScopedEngine wrapper
 2. ✅ MIDI consumption policy documented with user control via source config
 3. ✅ Script index semantics verified (0-based internal, 1-based in MI.$ op)
 4. ✅ Buffer clearing behavior documented (per-message vs original accumulation)
 5. ✅ UI source config uses standard print()/edit() pattern from MidiSourceConfig

 Objective

 Implement complete teletype MIDI functionality:
 1. Populate scene_state.midi with MIDI events (MI.* data ops)
 2. Support MI.$ op for assigning scripts to MIDI events
 3. Automatically trigger scripts when MIDI arrives

 Current Situation

 - TeletypeTrackEngine has NO MIDI integration
 - scene_state.midi structure exists but never populated
 - MI.$ op is already implemented in teletype VM (reads/writes on_script, off_script, cc_script)
 - MI.* ops read from empty state
 - No script triggering on MIDI events

 Implementation Approach

 Full teletype MIDI pattern (like MidiCvTrack):
 - Add MidiSourceConfig to TeletypeTrack model for port/channel selection
 - Implement receiveMidi() to actively consume MIDI when source matches
 - Translate Performer's MidiMessage → teletype's scene_midi_t structure
 - Automatically trigger assigned scripts when MIDI arrives
 - Can receive MIDI in background (not just when selected)

 Key Design Decisions

 | Decision            | Choice                                     | Rationale                                          |
 |---------------------|--------------------------------------------|----------------------------------------------------|
 | MIDI routing        | Use receiveMidi() with source filtering    | Like MidiCvTrack - can work in background          |
 | MIDI source config  | Add MidiSourceConfig _source to model      | User selects port (MIDI/USB) + channel (Omni/1-16) |
 | Message consumption | Return true if matched source              | Prevent double-processing by other tracks          |
 | Buffer clearing     | Clear BEFORE populating new data           | Fresh slate per MIDI message                       |
 | Script triggering   | Trigger AFTER populating data              | Scripts see the MIDI event that triggered them     |
 | Script execution    | Use run_script() from teletype.h           | Standard teletype VM function                      |
 | Thread safety       | Wrap run_script() in ScopedEngine          | receiveMidi() called from MIDI thread              |
 | Channel storage     | 0-15 (as-is from MidiMessage)              | Internal consistency                               |
 | CC de-duplication   | No de-duplication, append only             | Simpler, predictable                               |
 | Overflow handling   | Drop new events when buffer full (10 max)  | Preserve oldest events (FIFO)                      |
 | Clock/Transport     | Defer to future work                       | Already handled by Engine's Clock system           |
 | MI.$ op             | Already works (implemented in teletype VM) | No changes needed                                  |

 Critical Implementation Notes

 1. MIDI Consumption Policy

 Behavior: receiveMidi() returns true when source matches, consuming the message.
 - Implication: Other tracks won't see consumed MIDI messages
 - Rationale: Matches MidiCvTrack behavior - prevents duplicate processing
 - User control: Users select different sources per track to avoid conflicts
 - Example: TeletypeTrack on "USB Ch 2" won't consume MIDI on "MIDI Ch 1"

 2. Thread Safety

 Requirement: receiveMidi() is called from the MIDI input thread, NOT the Engine thread.
 - Solution: Wrap run_script() in TeletypeBridge::ScopedEngine lock
 - Pattern: Same as existing TeletypeBridge callbacks (CV/TR/TIME hooks)
 - Critical: Without lock, concurrent access to scene_state_t causes race conditions

 3. Script Index Semantics

 Internal representation: on_script, off_script, cc_script are 0-based (0-3 for S0-S3)
 - MI.$ op: Uses 1-based for user interface (1-4, with 0 = none)
 - Conversion: teletype/src/ops/midi.c line 131 handles conversion
 - Our code: Uses 0-based directly from scene_state → no conversion needed
 - Validation: Check scriptToRun >= 0 && scriptToRun < EDITABLE_SCRIPT_COUNT before calling

 4. Buffer Clearing Behavior

 Performer implementation: Clears buffers BEFORE each new MIDI message
 - Original Teletype: Accumulates events across ticks until script processes them
 - Trade-off: Simpler implementation, prevents overflow, but differs from original
 - Impact: MI.NL (note count) will typically return 1, not accumulated count
 - Justification: receiveMidi() is per-message, not per-tick like original

 5. UI Source Config Pattern

 Reuse existing MidiSourceConfig helpers:
 // Use built-in print/edit methods - don't reinvent
 track.midiSource().print(str);     // Formats "MIDI Ch 2" or "USB Omni"
 track.midiSource().edit(value, shift);  // Handles encoder navigation
 Reference: src/apps/sequencer/ui/pages/MidiCvTrackListPage.cpp line ~85

 Files to Modify

 1. TeletypeTrack.h (Model)

 Location: src/apps/sequencer/model/TeletypeTrack.h

 Add includes:
 #include "MidiConfig.h"

 Add property (around line 200, with other accessors):
 // midiSource
 const MidiSourceConfig &midiSource() const { return _midiSource; }
       MidiSourceConfig &midiSource()       { return _midiSource; }

 Add private member (around line 270, with other _members):
 MidiSourceConfig _midiSource;

 Update write() method:
 Add after line that writes other config:
 _midiSource.write(writer);

 Update read() method:
 Add after line that reads other config:
 _midiSource.read(reader);

 2. TeletypeTrackEngine.h

 Location: src/apps/sequencer/engine/TeletypeTrackEngine.h

 Changes:
 Add after line ~60 (after existing virtual methods):
 virtual bool receiveMidi(MidiPort port, const MidiMessage &message) override;
 virtual void clearMidiMonitoring() override;

 private:
 void runMidiTriggeredScript(int scriptIndex);
 void processMidiMessage(const MidiMessage &message);

 3. TeletypeTrackEngine.cpp

 Location: src/apps/sequencer/engine/TeletypeTrackEngine.cpp

 Add includes:
 #include "core/midi/MidiMessage.h"
 #include "core/midi/MidiUtils.h"
 #include "TeletypeBridge.h"  // For ScopedEngine thread safety

 extern "C" {
 #include "teletype.h"  // For run_script()
 }

 Implement clearMidiMonitoring():
 void TeletypeTrackEngine::clearMidiMonitoring() {
     scene_state_t &state = _teletypeTrack.state();
     scene_midi_t &midi = state.midi;

     // Clear event counts
     midi.on_count = 0;
     midi.off_count = 0;
     midi.cc_count = 0;

     // Reset last event tracking
     midi.last_event_type = 0;
     midi.last_channel = 0;
     midi.last_note = 0;
     midi.last_velocity = 0;
     midi.last_controller = 0;
     midi.last_cc = 0;
 }

 Implement receiveMidi() (with source filtering):
 bool TeletypeTrackEngine::receiveMidi(MidiPort port, const MidiMessage &message) {
     // Filter by MIDI source (port + channel)
     if (!MidiUtils::matchSource(port, message, _teletypeTrack.midiSource())) {
         return false;  // Not our source - allow other tracks to process
     }

     // Process the message (populate data + trigger script)
     processMidiMessage(message);

     // CRITICAL: Return true to consume message
     // - Prevents other tracks from seeing this message
     // - User controls which track gets which MIDI via source config
     // - Example: Track 1 set to "USB Ch 2" won't consume "MIDI Ch 1"
     return true;
 }

 Implement processMidiMessage() (extracted from old monitorMidi):
 void TeletypeTrackEngine::processMidiMessage(const MidiMessage &message) {
     // Clear buffers for fresh state
     clearMidiMonitoring();

     scene_state_t &state = _teletypeTrack.state();
     scene_midi_t &midi = state.midi;

     int8_t scriptToRun = -1;

     // Handle Note On
     if (message.isNoteOn() && message.velocity() > 0) {
         if (midi.on_count < MAX_MIDI_EVENTS) {
             midi.note_on[midi.on_count] = message.note();
             midi.note_vel[midi.on_count] = message.velocity();
             midi.on_channel[midi.on_count] = message.channel();
             midi.on_count++;
         }
         midi.last_event_type = 1;
         midi.last_channel = message.channel();
         midi.last_note = message.note();
         midi.last_velocity = message.velocity();

         // Check if script assigned to Note On
         scriptToRun = midi.on_script;
     }
     // Handle Note Off (including velocity-0 note-ons)
     else if (message.isNoteOff() || (message.isNoteOn() && message.velocity() == 0)) {
         if (midi.off_count < MAX_MIDI_EVENTS) {
             midi.note_off[midi.off_count] = message.note();
             midi.off_channel[midi.off_count] = message.channel();
             midi.off_count++;
         }
         midi.last_event_type = 0;
         midi.last_channel = message.channel();
         midi.last_note = message.note();
         midi.last_velocity = 0;

         // Check if script assigned to Note Off
         scriptToRun = midi.off_script;
     }
     // Handle Control Change
     else if (message.isControlChange()) {
         if (midi.cc_count < MAX_MIDI_EVENTS) {
             midi.cn[midi.cc_count] = message.controlNumber();
             midi.cc[midi.cc_count] = message.controlValue();
             midi.cc_channel[midi.cc_count] = message.channel();
             midi.cc_count++;
         }
         midi.last_event_type = 2;
         midi.last_channel = message.channel();
         midi.last_controller = message.controlNumber();
         midi.last_cc = message.controlValue();

         // Check if script assigned to CC
         scriptToRun = midi.cc_script;
     }

     // Trigger assigned script if valid (0-3 are S0-S3)
     if (scriptToRun >= 0 && scriptToRun < EDITABLE_SCRIPT_COUNT) {
         runMidiTriggeredScript(scriptToRun);
     }
 }

 Add helper method for script execution (with thread safety):
 void TeletypeTrackEngine::runMidiTriggeredScript(int scriptIndex) {
     // CRITICAL: receiveMidi() is called from MIDI thread
     // TeletypeTrack state manipulation requires engine lock
     TeletypeBridge::ScopedEngine scopedEngine(_teletypeTrack);

     scene_state_t &state = _teletypeTrack.state();

     // Use teletype VM's run_script function
     // Declared in teletype/src/teletype.h
     run_script(&state, scriptIndex);

     // Note: run_script() executes all lines of the script synchronously
     // CV/TR outputs are buffered and will be processed by the engine
 }

 4. TeletypeTrackListPage.cpp (UI for MIDI source)

 Location: src/apps/sequencer/ui/pages/TeletypeTrackListPage.cpp

 Add MIDI source parameter to the track list page (similar to MidiCvTrackListPage).

 Find existing parameter definitions and add:
 { "MIDI Source", [](TeletypeTrack &track, StringBuilder &str) {
     track.midiSource().print(str);
 }, [](TeletypeTrack &track, int value, bool shift) {
     track.midiSource().edit(value, shift);
 }}

 5. Create Unit Tests

 Location: src/tests/unit/sequencer/TestTeletypeTrackEngineMidi.cpp (NEW FILE)

 Test cases to implement:
 1. note_on_single_event - Verify Note On populates arrays correctly
 2. note_on_overflow - Verify 11th note is dropped (MAX_MIDI_EVENTS = 10)
 3. note_off_handling - Verify Note Off and velocity-0 handling
 4. control_change_basic - Verify CC populates cn[], cc[], cc_channel[]
 5. control_change_no_deduplication - Verify duplicate CCs create separate entries
 6. last_event_tracking - Verify last_note, last_velocity, etc. update correctly
 7. clear_monitoring - Verify clearMidiMonitoring() zeros counts
 8. buffer_clearing_per_message - Verify new message clears previous state
 9. script_triggering_note_on - Verify assigned script runs on Note On
 10. script_triggering_note_off - Verify assigned script runs on Note Off
 11. script_triggering_cc - Verify assigned script runs on CC
 12. no_script_assigned - Verify no crash when script = -1 (none assigned)
 13. script_sees_midi_data - Verify triggered script can read MI.N, MI.V, etc.
 14. source_filtering_port - Verify only MIDI from matching port is processed
 15. source_filtering_channel - Verify only MIDI from matching channel is processed
 16. source_omni_mode - Verify Omni mode accepts all channels
 17. message_consumed - Verify receiveMidi() returns true when source matched
 18. message_not_consumed - Verify receiveMidi() returns false when source mismatched
 19. script_index_bounds - Verify script index validation (>= 0, < EDITABLE_SCRIPT_COUNT)

 Follow UnitTest.h framework:
 #include "UnitTest.h"
 #include "apps/sequencer/engine/TeletypeTrackEngine.h"

 UNIT_TEST("TeletypeTrackEngineMidi") {

 CASE("note_on_single_event") {
     // Test implementation
 }

 // ... more test cases

 } // UNIT_TEST

 6. Register Test

 Location: src/tests/unit/sequencer/CMakeLists.txt

 Add:
 register_sequencer_test(TestTeletypeTrackEngineMidi TestTeletypeTrackEngineMidi.cpp)

 7. Update TeletypeTrack constructor/clear

 Location: src/apps/sequencer/model/TeletypeTrack.cpp

 In constructor or clear() method:
 Ensure _midiSource is initialized to default (MIDI port, Omni channel):
 _midiSource.clear();  // Sets to MIDI port, channel -1 (Omni)

 Translation & Script Triggering

 | Performer MidiMessage | →   | Teletype scene_midi_t                           | Script Triggered   |
 |-----------------------|-----|-------------------------------------------------|--------------------|
 | msg.isNoteOn()        | →   | note_on[], note_vel[], on_channel[], on_count++ | on_script if >= 0  |
 | msg.isNoteOff()       | →   | note_off[], off_channel[], off_count++          | off_script if >= 0 |
 | msg.isControlChange() | →   | cn[], cc[], cc_channel[], cc_count++            | cc_script if >= 0  |

 MidiMessage API:
 - message.note() - Note number (0-127)
 - message.velocity() - Velocity (0-127)
 - message.channel() - MIDI channel (0-15)
 - message.controlNumber() - CC number (0-127)
 - message.controlValue() - CC value (0-127)

 Teletype constants:
 - MAX_MIDI_EVENTS = 10 (defined in state.h)

 Testing Strategy

 Unit Tests

 Run from build/sim/debug/src/tests/unit/sequencer/:
 ./TestTeletypeTrackEngineMidi

 Manual Integration Testing

 Test 1: MIDI-triggered CV output
 1. Build simulator
 2. Create TeletypeTrack on any track slot
 3. Set MIDI Source to desired port (MIDI/USB) and channel (Omni or 1-16)
 4. Edit Script 0 (S0):
 CV 1 MI.NV  # Output MIDI note as voltage to CV 1
 TR.PULSE 1  # Pulse gate 1
 5. Assign Script 0 to Note On events:
 MI.$ 1 1   # Assign S0 (script 1 in 1-indexed) to Note On (event 1)
 6. Send MIDI notes from controller to matching port/channel
 7. Expected: CV 1 outputs note voltage, Gate 1 pulses on each note

 Test 1b: Source filtering
 1. Set MIDI Source to USB MIDI, Channel 2
 2. Send notes on MIDI port (not USB) → Expected: No response
 3. Send notes on USB, Channel 1 → Expected: No response
 4. Send notes on USB, Channel 2 → Expected: Script triggers ✓

 Test 2: MIDI CC control
 1. Set MIDI Source to desired source
 2. Edit Script 1 (S1):
 CV 2 MI.LCCV  # Output last CC value to CV 2
 3. Assign Script 1 to CC events:
 MI.$ 3 2   # Assign S1 (script 2) to CC (event 3)
 4. Send CC messages from matching source
 5. Expected: CV 2 follows CC value

 Test 3: No script assigned
 1. Clear script assignments:
 MI.$ 0 0   # Clear all assignments
 2. Send MIDI
 3. Expected: No crash, MI.* ops still readable manually

 Expected Behavior

 MIDI Data Ops:
 - MI.N returns MIDI note number (0-127)
 - MI.NV returns MIDI note as voltage (uses ET table lookup)
 - MI.V returns velocity (0-127)
 - MI.VV returns velocity scaled to 0-16383
 - MI.CC returns CC value
 - MI.LN returns last note received
 - MI.NL returns count of pending notes (typically 1 after clear-per-message)

 Script Assignment (MI.$ op - already works):
 - MI.$ 1 returns assigned script for Note On (1-indexed, 0 = none)
 - MI.$ 1 2 assigns Script 1 (S1) to Note On events
 - Event types: 0=all, 1=note on, 2=note off, 3=CC

 Automatic Triggering:
 - When MIDI arrives and script assigned, script runs automatically
 - Script can read MI.N, MI.V, etc. to see what triggered it
 - CV/TR outputs from script take effect immediately

 Risks & Edge Cases

 | Risk                         | Mitigation                                                          |
 |------------------------------|---------------------------------------------------------------------|
 | Buffer overflow (>10 events) | Drop newest events silently (tested)                                |
 | Velocity-0 note-ons          | Treat as Note Off (already handled by Engine)                       |
 | Rapid MIDI events            | Per-message clearing prevents accumulation; script runs per message |
 | Channel confusion            | Store 0-15 internally, document if needed                           |
 | Script execution time        | Scripts run synchronously in receiveMidi(); keep scripts short      |
 | Recursive script calls       | Teletype VM already prevents infinite recursion                     |
 | Invalid script index         | Check scriptIndex >= 0 && < EDITABLE_SCRIPT_COUNT before calling    |
 | Thread safety                | ✅ Use TeletypeBridge::ScopedEngine to lock state access            |
 | Script modifies MIDI state   | OK - state already populated before script runs                     |
 | Multiple teletype tracks     | Each track has own source config - no conflicts if sources differ   |
 | Message consumption          | Track consumes MIDI when source matches - other tracks won't see it |
 | MIDI routing conflict        | Users must configure different sources per track to avoid issues    |

 Out of Scope (Future Work)

 - MIDI Clock/Start/Stop/Continue script triggering (clk_script, start_script, etc.)
   - Clock already handled by Engine's Clock system
   - Can be added later if transport sync needed
 - Channel filtering configuration (accept all channels in v1)
 - Program Change, Pitch Bend, Aftertouch (not in scene_midi_t structure)
 - Event accumulation modes (current: clear per message)
 - Async script execution (current: synchronous, runs immediately)

 Success Criteria

 ✅ TeletypeTrack receives MIDI when source matches (port + channel)
 ✅ MI.N, MI.V, MI.CC ops return correct values matching MIDI input
 ✅ MI.$ op works (assign/read script assignments)
 ✅ Scripts automatically trigger when MIDI arrives and script assigned
 ✅ Scripts can read MIDI data (MI.N, MI.V, etc.) that triggered them
 ✅ CV/TR outputs work from MIDI-triggered scripts
 ✅ Thread-safe execution via ScopedEngine wrapper
 ✅ All unit tests pass (19 test cases)
 ✅ Manual testing shows complete MIDI workflow
 ✅ No crashes or undefined behavior with overflow/edge cases

 Implementation Checklist

 Execute in this order to minimize compilation errors:

 1. Model Layer (no dependencies):
   - Add #include "MidiConfig.h" to TeletypeTrack.h
   - Add MidiSourceConfig _midiSource; private member
   - Add midiSource() const/non-const accessors
   - Update write() method to serialize _midiSource
   - Update read() method to deserialize _midiSource
   - Update clear() or constructor to initialize _midiSource
 2. Engine Layer (depends on model):
   - Add includes to TeletypeTrackEngine.cpp (MidiMessage, MidiUtils, TeletypeBridge, teletype.h)
   - Add method declarations to TeletypeTrackEngine.h (receiveMidi, clearMidiMonitoring, private helpers)
   - Implement clearMidiMonitoring() in TeletypeTrackEngine.cpp
   - Implement receiveMidi() with source filtering
   - Implement processMidiMessage() with MIDI data population
   - Implement runMidiTriggeredScript() with ScopedEngine wrapper
 3. UI Layer (depends on model):
   - Add MIDI source parameter to TeletypeTrackListPage.cpp parameter list
   - Use track.midiSource().print(str) for display
   - Use track.midiSource().edit(value, shift) for editing
 4. Test Layer (validates all above):
   - Create TestTeletypeTrackEngineMidi.cpp with UnitTest.h framework
   - Implement 19 test cases covering data population, script triggering, source filtering
   - Register test in CMakeLists.txt
   - Build and run: ./build/sim/debug/src/tests/unit/sequencer/TestTeletypeTrackEngineMidi
 5. Integration Testing:
   - Build simulator
   - Configure MIDI source on TeletypeTrack
   - Write test scripts using MI.$ assignments
   - Send MIDI and verify CV/TR outputs
   - Test source filtering (port + channel)
 ✅ No crashes or undefined behavior with overflow/edge cases
