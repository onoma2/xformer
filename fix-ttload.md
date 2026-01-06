Teletype Track Load/Save Debug

Summary
- Hardware error: "BUFFER NOT IN SRAM" during LOAD TRACK in file selector.
- Save hang: "SAVING..." stuck when writing TELT files.
- S2/S3 scripts missing after load even when present in file.

Root Causes
- File selector slotInfo() read used FileReader on UI task stack (CCM), which SD DMA rejects.
- TELT save copied PatternSlot on file task stack (large stack use).
- TELT format only supported S0 + Metro sections; global scripts S1/S2/S3 were never loaded/saved.
- EOF propagated as error in some read paths.

Fixes Applied
Files/Functions touched
- src/apps/sequencer/model/FileManager.cpp
  - slotInfo(): readFirstLine() added to read NAME using SRAM buffer
  - writeTeletypeTrack(): uses static PatternSlot buffers; writes #S1/#S2/#S3
  - readTeletypeTrack(): static buffers; parses #S1/#S2/#S3; EOF treated as OK
  - readTeletypeScript(): EOF treated as OK
  - writeScriptSection()/writeScriptSectionRaw(): buffer size increased to 256
  - writeTeletypeScript(): buffer size increased to 256
  - readTeletypeScript(): buffer size increased to 256
  - writeLine(FixedStringBuilder): fixed recursion
  - slotInfo(): ensured NAME copy is null-terminated
- src/platform/stm32/drivers/SdCard.cpp
  - writeBlock(): added timeouts to DMA wait loops (fail instead of hang)

Fixes Applied
- slotInfo() for TeletypeTrack now reads NAME using a static SRAM buffer (no FileReader on CCM stack).
- writeTeletypeTrack() uses static PatternSlot buffers (avoids large stack copies on file task).
- readTeletypeTrack() and readTeletypeScript() treat END_OF_FILE as OK.
- TELT format extended to include global script sections:
  - #S1 / #S2 / #S3
  - readTeletypeTrack() now parses these into state.scripts[1..3].
- Increased script serialization buffers to 256 bytes to avoid print_command() overflows that could hang SAVE TRACK.

Current Status
- Working:
  - LOAD TRACK: no SRAM/stack asserts; scripts, patterns, and global S1–S3 load.
  - SAVE TRACK: completes without hang.
  - Script/track slot labels: NAME line read safely in file selector.
- Not working:
  - None (as of latest hardware test).

Notes
- TELT remains S0 + Metro per slot, plus global S1–S3.
- If LOAD/SAVE still hangs, next step is moving FileWriter buffer to SRAM or chunking writes.
