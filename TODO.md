# TODO - PEW|FORMER Development

## Completed ✓

### Documentation
- [x] Created CLAUDE.md with comprehensive project documentation
  - Build system setup and commands
  - Architecture overview (Model-Engine-UI separation)
  - Task priorities and FreeRTOS configuration
  - Platform abstraction layer details
  - Critical subsystems (Clock, MIDI, Display)
  - jackpf improvements integration notes
  - Development workflow best practices

- [x] Documented simulator interface in CLAUDE.md
  - Complete I/O port mapping (MIDI, CV, Gate, Clock)
  - Control matrix layout (4 rows of buttons)
  - Display and encoder controls
  - Dual-function button mappings
  - Screenshot capability
  - Simulator usage guidelines

## Pending Features

### To brainstorm
## Notes

- **Simulator-first development**: Always test new features in simulator before hardware
- **Noise reduction awareness**: Consider OLED pixel count impact on audio when modifying UI
- **Timing verification**: Hardware testing required for clock/sync related changes
- **Documentation updates**: Update CLAUDE.md when architecture changes significantly

## Reference Files

- `CLAUDE.md` - Main development reference
- `README.md` - Original project documentation
- `doc/improvements/` - jackpf improvement documentation
  - `noise-reduction.md`
  - `shape-improvements.md`
  - `midi-improvements.md`
- `doc/simulator-interface.png` - Simulator UI reference
