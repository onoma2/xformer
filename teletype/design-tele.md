# Performer Teletype Integration: Design Document

## Executive Summary

This document synthesizes key insights from multiple Teletype integration proposals for the Performer sequencer firmware. The integration is architecturally feasible with careful attention to memory constraints and a well-designed adapter layer to bridge Teletype's hardware expectations with Performer's infrastructure.

**Feasibility: HIGH** - Core logic is portable, adapter layers are well-defined, but implementation faces significant memory constraints.

## Key Similarities Across Proposals

### 1. Core Architecture Pattern
All proposals follow the same fundamental architecture:
- **Model Layer**: `TeletypeTrack` containing scripts, patterns, and variables
- **Engine Layer**: `TeletypeTrackEngine` running the Teletype VM
- **Adapter Layer**: Hardware I/O bridge mapping Teletype ops to Performer functions
- **UI Layer**: Script editor, pattern tracker, variable monitor

### 2. Memory Management Strategy
All proposals recognize the critical RAM constraint and suggest:
- Removing grid structures from `scene_state_t` to save 8-10KB per track
- Single Teletype track implementation as MVP
- Union-based allocation (only one track type active per slot)

### 3. I/O Mapping Approach
All proposals agree on mapping Teletype's fixed hardware model to Performer's flexible routing:
- CV 1-4 → Performer CV outputs (configurable routing)
- TR 1-4 → Performer gate outputs (configurable routing)
- IN/PARAM → Performer CV inputs (configurable routing)
- TR inputs → Performer trigger sources (configurable routing)

### 4. Track Parity Features
All proposals emphasize making Teletype behave like other Performer tracks:
- Divisor control for timing
- Octave/transpose for pitch
- Scale integration
- CV update modes (gated vs free)

## Key Differences and Design Decisions

### 1. Memory Allocation Strategy

**Gemini's Approach (Recommended):**
- Leverages Performer's existing `Container` allocation
- Each track slot pre-allocated to max size (~11.2KB for Note track)
- Teletype track fits within existing allocation (9.2KB optimized)
- **Advantage**: No additional RAM cost
- **Decision**: Use this approach - memory cost is effectively zero

**Claude's Approach:**
- Detailed RAM breakdown with concerns about 2+ tracks
- Suggests profiling current RAM usage first
- Conservative approach with 1-2 track limit

### 2. Script Storage and Management

**Qwen's Approach:**
- Scripts stored as text in model (6 lines × 64 chars each)
- Serialization of full script text
- Script library concept with file I/O

**Claude's Approach:**
- Emphasis on routable variables (A-Z) as Performer routing targets
- More detailed routing target definitions
- Focus on externally modulated scripting

**Gemini's Approach:**
- Standalone `.TT` file format for script library
- Split-pane UI design (256x64 OLED advantage)
- File management system for algorithm library

**Decision:**
- Use Gemini's standalone file approach with Qwen's text storage
- Implement routable variables as Claude suggests
- Design split-pane UI to leverage OLED real estate

### 3. Metro and Timing Integration

**Qwen's Approach:**
- Dual metro modes: Realtime (ms) vs Synced (divisor)
- Transport-synced metro respects tempo changes
- Metro tied to track divisor in synced mode

**Claude's Approach:**
- Focus on divisor-driven metro
- Integration with Performer's clock system
- Tempo change awareness

**Decision:**
- Implement Qwen's dual metro modes (Realtime/Synced)
- Synced mode uses track divisor for transport alignment
- Realtime mode maintains original Teletype behavior

### 4. Scale Integration Strategy

**Qwen's Approach:**
- Override Teletype's `N` op to use Performer scales
- Quantize note values through project scale
- Apply octave/transpose offsets

**Claude's Approach:**
- Bridge layer between Teletype ops and Performer scales
- Quantization through Performer's scale system

**Decision:**
- Use Qwen's approach with op override
- Maintain option to use Teletype's internal scale system
- Apply Performer track properties (octave, transpose) to quantized notes

### 5. CV Update Modes

**Qwen's Approach:**
- Gate-triggered vs Always modes
- Gate mode: CV changes only when gate is active
- Always mode: CV updates continuously

**Decision:**
- Implement both modes as Qwen suggests
- This provides classic envelope behavior vs LFO behavior

## Critical Design Decisions

### 1. Memory Optimization (PRIORITY 1)
**Decision**: Remove grid structures from `scene_state_t`
- **Implementation**: Define `NO_GRID` or manually strip grid fields
- **Impact**: Reduces per-track RAM from 13-15KB to 5-7KB
- **Rationale**: Essential for fitting within Performer's allocation

### 2. Track Limitation
**Decision**: Support 1-2 Teletype tracks maximum
- **Rationale**: Even with optimization, multiple tracks consume significant RAM
- **Implementation**: Validate single track first, expand if resources allow

### 3. I/O Adapter Layer Design
**Decision**: Create comprehensive adapter layer
- **Implementation**: `TeletypeHardwareAdapter` class
- **Function**: Map `tele_*` functions to Performer I/O
- **Rationale**: Clean separation between Teletype core and Performer integration

### 4. Routing Integration
**Decision**: Implement comprehensive routing targets
- **Standard targets**: Octave, Transpose, Divisor (shared with other tracks)
- **Teletype-specific targets**: Variables A-Z, Metro period, Pattern index
- **Implementation**: Extend `Routing::Target` enum with Teletype-specific values

### 5. UI Layout Strategy
**Decision**: Implement split-pane UI leveraging 256x64 display
- **Left pane**: Script editor (6 lines + status)
- **Right pane**: Live monitor (CV/Gate levels, variables, patterns)
- **Rationale**: Better use of available display real estate than original Teletype

## Implementation Phases

### Phase 0: Pre-Implementation (Critical)
1. Profile current Performer RAM usage
2. Verify available headroom for Teletype integration
3. Confirm memory optimization strategy (grid removal)

### Phase 1: Core VM Integration
1. Port Teletype core (interpreter, parser, ops)
2. Remove grid-dependent ops and structures
3. Create basic `TeletypeTrack` and `TeletypeTrackEngine`
4. Implement I/O adapter layer

### Phase 2: Track Parity
1. Add standard track properties (divisor, octave, transpose)
2. Implement metro modes (realtime/synced)
3. Add scale integration
4. Implement CV update modes

### Phase 3: Routing Integration
1. Add Teletype-specific routing targets
2. Implement routable variables (A-Z)
3. Connect routing system to Teletype state

### Phase 4: UI Implementation
1. Script editor page
2. Pattern tracker page
3. Variable monitor page
4. Split-pane layout optimization

### Phase 5: File System Integration
1. Standalone script loading/saving
2. Script library browser
3. Project serialization

## Resource Requirements Summary

### RAM (Critical Constraint)
- **Optimized Teletype track**: ~9.2KB per track
- **Performer allocation headroom**: ~20KB available after current usage
- **Maximum tracks**: 2 (recommended), 1 (safe)

### Flash (Adequate Headroom)
- **Teletype code**: ~75-115KB
- **Available flash**: ~600KB+
- **Feasibility**: Excellent

### CPU (Acceptable Load)
- **Script execution**: 100-500 μs per script
- **Engine::update() target**: <1ms
- **Feasibility**: Excellent with WHILE_DEPTH protection

## Risk Mitigation

1. **Memory Risk**: Implement grid structure removal as mandatory
2. **Performance Risk**: Profile with complex scripts using WHILE_DEPTH limits
3. **Integration Risk**: Start with minimal op set, expand gradually
4. **UI Risk**: Implement basic editor first, add advanced features later

## Conclusion

The Teletype integration is feasible with careful implementation focusing on memory optimization. The gemini approach of leveraging existing allocation is the most promising. The design should prioritize single-track implementation with potential for expansion to two tracks based on actual resource usage measurements.