# Research: Reducing Teletype Track Size and Implementing Script Saving/Loading

## Current State Analysis

### Memory Usage of TeletypeTrack
The current `TeletypeTrack` class contains:
1. **`scene_state_t _state`** - The main teletype state (large structure)
2. **Script text storage** - `_scriptLines` array storing scripts as text
3. **Pattern storage** - `_patterns` array for pattern data
4. **I/O mapping configuration** - Various enums for routing
5. **Timing parameters** - Clock divisor/multiplier settings
6. **CV/Gate output configuration** - Range and offset settings

### Size Issues
The main memory concerns are:
- `scene_state_t` contains the full teletype state including all variables, patterns, scripts, delay buffer, etc.
- Script text is stored in addition to the compiled `tele_command_t` in the state
- Pattern data is duplicated between the state and the `_patterns` array

## Hacking Approaches to Reduce Size

### 1. Lazy State Initialization
Instead of storing the full `scene_state_t` in each track, only initialize it when needed:

```cpp
class TeletypeTrack {
private:
    // Instead of: scene_state_t _state;
    // Use a pointer that's only allocated when needed
    mutable scene_state_t *_state = nullptr;
    
    scene_state_t &state() const {
        if (!_state) {
            _state = new scene_state_t;
            ss_init(_state);
        }
        return *_state;
    }
};
```

### 2. Script Text Compression
Store scripts in a more compact format:
- Store only the text when different from compiled state
- Use a hash to detect changes and only store diffs
- Implement compression for common patterns

### 3. Selective State Storage
Only store the parts of the state that are different from default:
```cpp
// Instead of storing the full state, store only deltas
struct TeletypeStateDelta {
    std::vector<std::pair<int, int16_t>> variables;  // var_index, value
    std::vector<scene_pattern_t> patterns;           // Only non-default patterns
    // etc.
};
```

### 4. On-Demand Script Loading
Load scripts from SD card only when needed:
- Store script references (file paths) instead of full text
- Load scripts into memory only when editing or executing

## Implementing Script Saving/Loading to SD Card

### 1. New File Types for Teletype Scripts
Add new file types to the existing system:

```cpp
enum class FileType : uint8_t {
    Project     = 0,
    UserScale   = 1,
    NoteSequence= 2,
    CurveSequence=3,
    LogicSequence=4,
    ArpSequence=5,
    TeletypeScript = 6,  // New: Individual script files
    TeletypeScene = 7,   // New: Complete teletype scene
    Settings    = 255
};
```

### 2. Script File Format
Create a simple text-based format for individual scripts:

```
# Teletype Script File
# File: SCRIPT.TTS
# Description: Individual script with metadata

[INFO]
name=My Script
author=Performer User
date=2024-01-02
description=A sample teletype script

[SCRIPT]
#1
CV 1 V 5
TR.P A

#2
P.NEXT
CV 2 N P.HERE

#3
IF GT X 10: X 0
X ADD X 1
```

### 3. Pattern File Format
Create a format for patterns:

```
# Teletype Pattern File
# File: PATTERN.TTP

[PATTERN]
bank=0
length=8
start=0
end=7

[VALUES]
0=60
1=62
2=64
3=65
4=67
5=69
6=71
7=72
```

### 4. Implementation Strategy

#### A. FileManager Extensions
Add new methods to handle teletype files:

```cpp
// In FileManager
fs::Error writeTeletypeScript(const TeletypeTrack &track, int scriptIndex, const char *path);
fs::Error readTeletypeScript(TeletypeTrack &track, int scriptIndex, const char *path);
fs::Error writeTeletypeScene(const TeletypeTrack &track, const char *path);
fs::Error readTeletypeScene(TeletypeTrack &track, const char *path);
```

#### B. Directory Structure
Add a new directory for teletype files:

```
SD Card/
├── PROJECTS/
├── SCALES/
├── SEQS/
├── TEL/
│   ├── SCRIPTS/      # Individual scripts
│   │   ├── 001.TTS
│   │   ├── 002.TTS
│   │   └── ...
│   ├── PATTERNS/     # Individual patterns
│   │   ├── 001.TTP
│   │   ├── 002.TTP
│   │   └── ...
│   └── SCENES/       # Complete scenes
│       ├── 001.TTS
│       ├── 002.TTS
│       └── ...
└── LAST.DAT
```

#### C. Asynchronous Loading
Implement background loading for large script libraries:

```cpp
class TeletypeScriptManager {
private:
    struct ScriptInfo {
        std::string name;
        std::string path;
        time_t lastModified;
        bool loaded;
    };
    
    std::vector<ScriptInfo> _scriptLibrary;
    std::queue<std::string> _loadQueue;
    
public:
    void scanScriptDirectory();
    void loadScriptAsync(const std::string &path);
    void update(); // Call regularly to process background loads
};
```

### 5. Memory Optimization Techniques

#### A. String Pool for Common Commands
Use string interning for frequently used commands:

```cpp
class StringPool {
private:
    static std::unordered_map<std::string, const char*> _pool;
    
public:
    static const char* intern(const std::string &str) {
        auto it = _pool.find(str);
        if (it != _pool.end()) {
            return it->second;
        }
        return _pool.emplace(str, strdup(str.c_str())).first->second;
    }
};
```

#### B. Command Compression
Store commands in a more compact binary format:

```cpp
// Instead of storing full text, store opcodes
struct CompactCommand {
    uint8_t opCode;      // Compressed opcode
    uint8_t argCount;    // Number of arguments
    int16_t args[4];     // Arguments
    uint8_t flags;       // Modifiers, etc.
};
```

#### C. Script Template System
Allow for script templates to reduce storage:

```cpp
// Template: "CV {output} V {voltage}"
// Instance: "CV 1 V 5" (just the parameters)
struct ScriptTemplate {
    std::string templateString;  // "CV {output} V {voltage}"
    std::vector<std::string> placeholders;  // ["output", "voltage"]
};
```

### 6. UI Integration

#### A. Script Browser
Add a page to browse and load scripts from SD:

```cpp
class TeletypeScriptBrowserPage : public BasePage {
private:
    std::vector<std::string> _scriptFiles;
    int _selectedIndex = 0;
    int _offset = 0;
    
public:
    void loadScript(int index);
    void loadScriptToSlot(int scriptSlot, int fileIndex);
};
```

#### B. Pattern Library
Similar interface for patterns:

```cpp
class TeletypePatternBrowserPage : public BasePage {
    // Similar implementation for pattern files
};
```

### 7. Performance Considerations

#### A. Caching System
Cache frequently used scripts in RAM:

```cpp
template<typename T, size_t N>
class LRUCache {
private:
    std::array<T, N> _items;
    std::array<bool, N> _valid;
    std::array<uint32_t, N> _accessTime;
    
public:
    T* get(const std::string &key);
    void put(const std::string &key, const T &value);
};
```

#### B. Lazy Loading
Only load scripts when they're actually needed:

```cpp
class LazyScript {
private:
    std::string _filePath;
    std::unique_ptr<tele_command_t[]> _commands;
    bool _loaded = false;
    
public:
    const tele_command_t* getCommands() {
        if (!_loaded) {
            loadFromFile(_filePath);
            _loaded = true;
        }
        return _commands.get();
    }
};
```

## Implementation Plan

### Phase 1: Basic File Support
1. Add new file types for teletype scripts/patterns
2. Implement basic read/write functions
3. Create directory structure on SD card

### Phase 2: Memory Optimization
1. Implement lazy state initialization
2. Add script compression
3. Create string pooling system

### Phase 3: UI Integration
1. Add script browser pages
2. Integrate with existing teletype UI
3. Add load/save functionality

### Phase 4: Advanced Features
1. Implement caching system
2. Add template support
3. Optimize for performance

This approach would significantly reduce the memory footprint of teletype tracks while providing robust script saving/loading capabilities to the SD card, making the system much more flexible for complex musical arrangements.