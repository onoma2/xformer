# Resource Analysis: PEW|FORMER vs Original PER|FORMER

## Summary of All Implemented Features Since Fork

The PEW|FORMER fork has added significant functionality compared to the original PER|FORMER, with the following key feature additions:

### Major Feature Additions:

1. **Tuesday Track System** - Advanced generative algorithmic sequencer with 13+ algorithms
2. **Accumulator System** - Stateful counters with multiple modes and trigger options  
3. **Harmony System** - Harmonàig-style harmonic sequencing with master/follower relationships
4. **Enhanced NoteTrack Features** - Gate modes, pulse counting, accumulator triggers
5. **CurveTrack Enhancements** - Advanced phase manipulation, wavefolding, chaos algorithms
6. **Noise Reduction Improvements** - Enhanced display settings to minimize OLED noise
7. **Shape Improvements** - Better CV curve generation algorithms
8. **MIDI Improvements** - Extended MIDI functionality and mapping
9. **LFO Shapes** - Quick access to common LFO waveforms for CurveTrack

### Resource Impact Summary:

**Flash Memory**:
- Increase: +85-107 KB (18-24% above original)
- Current usage: ~47-55% of 1024KB
- Headroom remaining: ~45% for future development

**RAM Usage**:
- Increase: ~5-6 KB total (3-4% above original)
- Current usage: ~81-86% of 192KB (approaching limits)
- Tuesday tracks: ~608 bytes per track
- Enhanced features: ~20 bytes per track

**CPU Utilization**:
- Increase: +3-20% (depending on feature usage)
- Typical usage: ~43-57% (was 40-50%)
- Peak usage: ~60-70% (was 40-50%)
- Tuesday algorithms: Most intensive (up to 15% when active)

### Real-time Performance:

- Maintained deterministic timing behavior
- Original interrupt priorities preserved
- All new features integrated into existing tick system without disrupting critical operations
- Buffer generation runs during idle time, not in real-time path

### Hardware Limitations:

The implementation is approaching the limits of the STM32F405RGT6:
- RAM usage at 86% utilization is concerning for further expansion
- CPU has limited headroom (30% remaining) for additional complex features
- Flash still has reasonable headroom for future development

### Key Optimizations Implemented:

1. **Memory optimizations**: Bitfield packing, efficient data structures, CCMRAM usage
2. **CPU optimizations**: Inline algorithms, pre-computed values, bounded complexity
3. **Real-time optimizations**: Non-blocking operations, deterministic execution paths
4. **Code architecture**: Event-driven processing, memory pools, parameter validation

The PEW|FORMER implementation demonstrates excellent engineering, successfully adding substantial creative functionality while maintaining the real-time performance and reliability required for a professional eurorack module. However, the system is now operating close to the hardware limits, particularly RAM usage, which may require optimization or hardware upgrades for future feature additions.

## 037-Update: Specific Track Analysis (Current State)

### NoteTrack Resource Profile
*   **Storage (RAM):** ~550 bytes per sequence.
    *   Step Data: 64 steps * 8 bytes = 512 bytes.
    *   Overhead: ~38 bytes (Accumulator, Harmony, Routing).
    *   Total per Track (8 patterns + snapshots): ~5-8 KB.
*   **Runtime (RAM):** Low (~100 bytes). Direct playback logic.

### TuesdayTrack Resource Profile
*   **Storage (RAM):** ~32 bytes per sequence. **(Extremely Efficient)**
    *   Procedural generation avoids storing step data.
    *   Total per Track: ~0.5 KB.
*   **Runtime (RAM):** High (~600 bytes).
    *   **Micro-Gate Queue:** ~384 bytes (32 events * 12 bytes). Critical for polyrhythms and ratcheting.
    *   **Algorithm State:** ~200 bytes (Pattern buffers for Aphex/Autechre, RNG states).
*   **Optimization Note:** If RAM becomes critical, the Micro-Gate queue size (32) could be reduced to 16, saving ~1.5 KB across 8 tracks.

### Recent Changes (Start/Skew/Scan)
*   **Removed `Scan`**: Saved ~128 bytes system-wide (Routable parameter).
*   **Added `Start` & `Skew`**: Added ~128 bytes system-wide.
*   **Net Impact**: Neutral.
*   **Start Logic**: Implemented as time-delay/masking (CPU-based), requiring no additional buffers.

### Conclusion
The architecture successfully balances the heavy storage of `NoteTrack` (Manual Entry) with the heavy runtime of `TuesdayTrack` (Procedural). RAM remains the primary constraint, but recent feature additions have been resource-neutral.
