# Research: Teletype Script Saving/Loading and Ideas from Monikit Rust

## Current State Analysis

### Current TeletypeTrack Structure
The current `TeletypeTrack` class contains:
1. **`scene_state_t _state`** - The main teletype state structure
2. **Script text storage** - `_scriptLines` array storing scripts as text
3. **Pattern storage** - `_patterns` array for pattern data
4. **I/O mapping configuration** - Various enums for routing
5. **Timing parameters** - Clock divisor/multiplier settings
6. **CV/Gate output configuration** - Range and offset settings

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

### 5. UI Integration

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

## Ideas from Monikit Rust Implementation

### 1. JSON Serialization Approach

#### Monikit Implementation
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Preset {
    pub version: u32,
    pub name: String,
    pub category: String,
    pub lines: Vec<String>,
    pub j: i16,
    pub k: i16,
    pub description: String,
}
```

#### Adaptation for Embedded C++
```cpp
// Use a lightweight JSON library like cjson or implement a simple serializer
struct TeletypePreset {
    uint32_t version;
    char name[32];
    char category[16];
    std::array<char[64], 6> lines;  // Assuming max 6 lines per script
    int16_t j;
    int16_t k;
    char description[64];

    void serialize(char* buffer, size_t bufferSize) const;
    bool deserialize(const char* buffer);
};
```

### 2. Factory vs User Preset Distinction

#### Monikit Implementation
```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PresetType {
    Factory,
    User,
}
```

#### Adaptation for Embedded C++
```cpp
enum class PresetType : uint8_t {
    Factory,
    User,
    Last
};

// In preset loading function
PresetType getPresetType(const char* name) {
    // Check if it's a factory preset first
    if (isFactoryPreset(name)) {
        return PresetType::Factory;
    }
    // Otherwise check user presets
    return PresetType::User;
}
```

### 3. Sanitization and Path Management

#### Monikit Implementation
```rust
pub fn sanitize_name(name: &str) -> String {
    name.chars()
        .map(|c| {
            if c.is_alphanumeric() || c == '-' || c == '_' {
                c
            } else {
                '-'
            }
        })
        .collect::<String>()
        .trim_matches('-')
        .to_string()
}
```

#### Adaptation for Embedded C++
```cpp
class PresetUtils {
public:
    static void sanitizeName(char* name, size_t maxLength) {
        size_t len = strlen(name);
        for (size_t i = 0; i < len && i < maxLength - 1; i++) {
            char c = name[i];
            if (!isalnum(c) && c != '-' && c != '_') {
                name[i] = '_';
            }
        }
        // Trim leading/trailing underscores
        trimEdges(name, maxLength, '_');
    }

private:
    static void trimEdges(char* str, size_t maxLength, char trimChar) {
        // Implementation to trim leading/trailing chars
        size_t len = strlen(str);
        size_t start = 0;
        while (start < len && str[start] == trimChar) start++;
        if (start > 0) {
            memmove(str, str + start, len - start + 1);
            len -= start;
        }
        while (len > 0 && str[len-1] == trimChar) len--;
        str[len] = '\0';
    }
};
```

### 4. Error Handling Pattern

#### Monikit Implementation
```rust
pub enum PresetError {
    IoError(std::io::Error),
    NotFound(String),
    ParseError(String),
}
```

#### Adaptation for Embedded C++
```cpp
enum class PresetError {
    OK,
    IoError,
    NotFound,
    ParseError,
    InvalidFormat,
    BufferTooSmall,
    OutOfMemory
};

// Using a result pattern similar to Rust
template<typename T>
class Result {
private:
    T _value;
    PresetError _error;
    bool _hasValue;

public:
    Result(T value) : _value(value), _error(PresetError::OK), _hasValue(true) {}
    Result(PresetError error) : _error(error), _hasValue(false) {}

    bool isOk() const { return _hasValue; }
    PresetError error() const { return _error; }
    T& value() { return _value; }
    const T& value() const { return _value; }
};
```

### 5. Factory Preset Integration

#### Monikit Implementation
```rust
pub fn get_preset(name: &str) -> Result<(Preset, PresetType), PresetError> {
    if let Some(preset) = factory::get_factory_preset(name) {
        return Ok((preset, PresetType::Factory));
    }
    // ... check user presets
}
```

#### Adaptation for Embedded C++
```cpp
class TeletypePresetManager {
private:
    static const TeletypePreset _factoryPresets[];
    static const size_t _factoryPresetCount;

public:
    static Result<TeletypePreset> getPreset(const char* name) {
        // Check factory presets first
        for (size_t i = 0; i < _factoryPresetCount; i++) {
            if (strcmp(_factoryPresets[i].name, name) == 0) {
                return Result<TeletypePreset>(_factoryPresets[i]);
            }
        }

        // Then check user presets on SD card
        return loadUserPreset(name);
    }

    static void listAllPresets(std::vector<std::pair<std::string, PresetType>>& presets) {
        // Add factory presets
        for (size_t i = 0; i < _factoryPresetCount; i++) {
            presets.emplace_back(_factoryPresets[i].name, PresetType::Factory);
        }

        // Add user presets
        addSdCardPresets(presets);
    }
};
```

### 6. Command Processing Pattern

#### Monikit Implementation
```rust
pub fn handle_pset_save<F>(
    parts: &[&str],
    scripts: &ScriptStorage,
    debug_level: u8,
    out_ess: bool,
    mut output: F,
) where
    F: FnMut(String),
{
    // Process command parts
    // Validate inputs
    // Save preset
    // Output result
}
```

#### Adaptation for Embedded C++
```cpp
class TeletypePresetCommands {
public:
    static void handlePsetSave(const std::vector<std::string>& parts,
                              const TeletypeTrack& track,
                              StringBuilder& output) {
        if (parts.size() < 3) {
            output("ERROR: PSET.SAVE REQUIRES SCRIPT# AND NAME");
            return;
        }

        int scriptNum = atoi(parts[1].c_str());
        if (scriptNum < 1 || scriptNum > 8) {
            output("ERROR: SCRIPT NUMBER MUST BE 1-8");
            return;
        }

        std::string name = parts[2];
        // Additional parts get concatenated to name
        for (size_t i = 3; i < parts.size(); i++) {
            name += " " + parts[i];
        }

        // Sanitize name
        char sanitizedName[32];
        strcpy(sanitizedName, name.c_str());
        PresetUtils::sanitizeName(sanitizedName, sizeof(sanitizedName));

        // Extract script data
        TeletypePreset preset = extractPresetFromScript(track, scriptNum - 1);

        // Save to SD card
        PresetError result = TeletypePresetManager::savePreset(sanitizedName, preset);

        if (result == PresetError::OK) {
            output("SAVED PRESET: ");
            output(sanitizedName);
        } else {
            output("ERROR SAVING PRESET");
        }
    }

private:
    static TeletypePreset extractPresetFromScript(const TeletypeTrack& track, int scriptIndex) {
        TeletypePreset preset = {};
        // Extract script lines and variables
        for (int i = 0; i < TeletypeTrack::ScriptLineCount; i++) {
            const char* line = track.scriptLine(scriptIndex, i);
            if (line && strlen(line) > 0) {
                strncpy(preset.lines[i].data(), line, sizeof(preset.lines[i]) - 1);
                preset.lines[i][sizeof(preset.lines[i]) - 1] = '\0';
            }
        }
        // Extract J, K variables if available
        // This would require accessing the state variables
        return preset;
    }
};
```

### 7. Directory Management

#### Monikit Implementation
```rust
pub fn get_presets_dir() -> PathBuf {
    crate::config::monokit_config_dir()
        .unwrap_or_else(|_| {
            let home = env::var("HOME").unwrap_or_else(|_| ".".to_string());
            PathBuf::from(home).join(".config").join("monokit")
        })
        .join("presets")
}
```

#### Adaptation for Embedded C++
```cpp
class PresetPaths {
public:
    static void getPresetPath(const char* name, char* path, size_t pathSize) {
        // Construct path: "TEL/SCRIPTS/{name}.TTS"
        snprintf(path, pathSize, "TEL/SCRIPTS/%s.TTS", name);
    }

    static void getPresetsDir(char* path, size_t pathSize) {
        strncpy(path, "TEL/SCRIPTS/", pathSize);
    }

    static bool ensurePresetDirectories() {
        // Create directory structure on SD card if needed
        // This would use the filesystem API
        return FileSystem::createDirectory("TEL") &&
               FileSystem::createDirectory("TEL/SCRIPTS");
    }
};
```

### 8. Lazy Loading Pattern

#### Monikit Implementation
The Rust implementation loads presets on demand, which is similar to the lazy loading concept in feat-propored.

#### Adaptation for Embedded C++
```cpp
class LazyPresetLoader {
private:
    std::unordered_map<std::string, TeletypePreset> _cache;
    static const size_t _maxCacheSize = 16;  // Limited by embedded constraints

public:
    const TeletypePreset* getPreset(const char* name) {
        auto it = _cache.find(name);
        if (it != _cache.end()) {
            return &it->second;
        }

        // Load from SD card
        TeletypePreset preset;
        if (loadPresetFromSd(name, preset)) {
            if (_cache.size() >= _maxCacheSize) {
                // Simple eviction: remove first element
                _cache.erase(_cache.begin());
            }
            auto result = _cache.emplace(name, preset);
            return &result.first->second;
        }

        return nullptr;  // Not found
    }

private:
    bool loadPresetFromSd(const char* name, TeletypePreset& preset) {
        char path[64];
        PresetPaths::getPresetPath(name, path, sizeof(path));

        File file = FileSystem::open(path, "r");
        if (!file.isValid()) {
            return false;
        }

        // Read and parse the preset file
        // Implementation depends on chosen serialization format
        return preset.deserialize(file);
    }
};
```

## Implementation Plan

### Phase 1: Basic File Support
1. Add new file types for teletype scripts/patterns
2. Implement basic read/write functions
3. Create directory structure on SD card

### Phase 2: UI Integration
1. Add script browser pages
2. Integrate with existing teletype UI
3. Add load/save functionality

### Phase 3: Advanced Features
1. Implement factory preset system
2. Add command processing (PSET.SAVE, PSET)
3. Add error handling system

This approach would provide robust script saving/loading capabilities to the SD card, making the system much more flexible for complex musical arrangements, while incorporating valuable design patterns from the monikit Rust implementation.