# Teletype Track Loading Optimization (TLLOAD)

## Overview
This feature addresses the stack overflow issues that occur when loading projects containing teletype tracks. The current implementation causes crashes or incomplete data loading due to excessive stack usage in the FILE task during project loading.

## Problem Statement
- **Current Issue**: Loading projects with teletype tracks causes crashes or incomplete data loading
- **Root Cause**: Stack overflow in FILE task (2048 bytes) when loading teletype track data
- **Impact**: Users cannot reliably load projects containing teletype tracks on hardware
- **Current Stack Usage**: FILE task has only 2048 bytes vs Engine/UI tasks with 4096 bytes

## Optimization Strategies

### 1. Chunked Loading Approach
Instead of loading all teletype data at once, implement a chunked loading approach:

```cpp
// Instead of loading all scripts at once, load in smaller chunks
for (int script = 0; script < EditableScriptCount; ++script) {
    ss_clear_script(&_state, script);
    uint8_t scriptLen = 0;
    reader.read(scriptLen);
    _state.scripts[script].l = clamp<uint8_t>(scriptLen, 0, ScriptLineCount);
    
    // Load in chunks of 16 lines instead of all 64 at once
    const int CHUNK_SIZE = 16;
    for (int line = 0; line < ScriptLineCount; line += CHUNK_SIZE) {
        int endLine = std::min(line + CHUNK_SIZE, ScriptLineCount);
        for (int i = line; i < endLine; ++i) {
            reader.read(_state.scripts[script].c[i], 0);
        }
        // Yield control to allow other tasks to run
        os::thread::yield();
    }
}
```

### 2. Lazy Loading Implementation
Load teletype scripts only when they are actually needed:

```cpp
class TeletypeTrack {
private:
    // Instead of storing all scripts in memory, store serialized data
    std::array<std::vector<uint8_t>, EditableScriptCount> _serializedScripts;
    bool _scriptsLoaded = false;
    
public:
    void loadScriptsIfNeeded() {
        if (!_scriptsLoaded) {
            deserializeScripts();
            _scriptsLoaded = true;
        }
    }
    
    void deserializeScripts() {
        // Deserialize only when needed, potentially in a different task
        // with more stack space
    }
};
```

### 3. Memory Pool for Teletype Commands
Use a memory pool to reduce allocation overhead:

```cpp
class TeletypeTrack {
private:
    static constexpr size_t MAX_COMMANDS = EditableScriptCount * ScriptLineCount;
    std::array<tele_command_t, MAX_COMMANDS> _commandPool;
    size_t _nextFreeCommand = 0;
    
public:
    tele_command_t* allocateCommand() {
        if (_nextFreeCommand < MAX_COMMANDS) {
            return &_commandPool[_nextFreeCommand++];
        }
        return nullptr; // Out of memory
    }
};
```

### 4. Asynchronous Loading with Progress Tracking
Move teletype loading to a higher-stack task and track progress:

```cpp
class TeletypeTrack {
private:
    struct LoadState {
        bool loading = false;
        int currentScript = 0;
        int currentLine = 0;
        float progress = 0.0f;
    } _loadState;
    
public:
    bool continueLoading(VersionedSerializedReader &reader) {
        // Load one script at a time, returning control between scripts
        if (_loadState.currentScript < EditableScriptCount) {
            // Load next few lines of current script
            for (int i = 0; i < 8 && _loadState.currentLine < ScriptLineCount; ++i) {
                reader.read(_state.scripts[_loadState.currentScript].c[_loadState.currentLine], 0);
                _loadState.currentLine++;
            }
            
            if (_loadState.currentLine >= ScriptLineCount) {
                // Move to next script
                _loadState.currentScript++;
                _loadState.currentLine = 0;
            }
            
            _loadState.progress = (float(_loadState.currentScript * ScriptLineCount + _loadState.currentLine) / 
                                  float(EditableScriptCount * ScriptLineCount));
            
            return _loadState.currentScript < EditableScriptCount; // Continue loading?
        }
        return false; // Done
    }
};
```

### 5. Compressed Storage Format
Compress the teletype data to reduce I/O time and memory usage:

```cpp
// Before writing to file, compress the script data
void TeletypeTrack::writeCompressed(VersionedSerializedWriter &writer) const {
    // Serialize to temporary buffer
    std::vector<uint8_t> tempBuffer;
    serializeToBuffer(tempBuffer);
    
    // Compress the buffer
    std::vector<uint8_t> compressedBuffer = compress(tempBuffer);
    
    // Write compressed size and data
    writer.write(static_cast<uint32_t>(compressedBuffer.size()));
    writer.write(compressedBuffer.data(), compressedBuffer.size());
}

// During loading, decompress in chunks to manage memory usage
void TeletypeTrack::readCompressed(VersionedSerializedReader &reader) {
    uint32_t compressedSize;
    reader.read(compressedSize);
    
    std::vector<uint8_t> compressedBuffer(compressedSize);
    reader.read(compressedBuffer.data(), compressedSize);
    
    // Decompress in chunks to manage memory usage
    std::vector<uint8_t> decompressedBuffer = decompress(compressedBuffer);
    
    // Deserialize from decompressed buffer
    deserializeFromBuffer(decompressedBuffer);
}
```

### 6. Task Migration for Loading
Temporarily move the loading process to a task with more stack space:

```cpp
// In FileManager, when encountering a teletype track, temporarily
// switch to the Engine task for loading the teletype-specific data
class FileManager {
    static fs::Error loadTeletypeTrackAsync(TeletypeTrack &track, const char *path) {
        // Create a deferred loading task that runs on Engine task
        // which has 4096 bytes of stack space vs FILE task's 2048
        return deferredLoadOnEngineTask(track, path);
    }
};
```

### 7. Streaming Parser Approach
Implement a streaming approach that processes data as it's read:

```cpp
class TeletypeTrack {
public:
    struct StreamingLoadContext {
        int currentScript = 0;
        int currentLine = 0;
        uint8_t scriptLength = 0;
        bool scriptInitialized = false;
    };
    
    bool processNextChunk(VersionedSerializedReader &reader, StreamingLoadContext &ctx) {
        // Process only a small chunk of data at a time
        for (int i = 0; i < 4 && ctx.currentLine < ScriptLineCount; ++i) {
            if (!ctx.scriptInitialized) {
                ss_clear_script(&_state, ctx.currentScript);
                reader.read(ctx.scriptLength);
                _state.scripts[ctx.currentScript].l = clamp<uint8_t>(ctx.scriptLength, 0, ScriptLineCount);
                ctx.scriptInitialized = true;
            }
            
            reader.read(_state.scripts[ctx.currentScript].c[ctx.currentLine], 0);
            ctx.currentLine++;
        }
        
        if (ctx.currentLine >= std::min(ctx.scriptLength, static_cast<uint8_t>(ScriptLineCount))) {
            // Move to next script
            ctx.currentScript++;
            ctx.currentLine = 0;
            ctx.scriptInitialized = false;
        }
        
        return ctx.currentScript < EditableScriptCount;
    }
};
```

### 8. Memory-Mapped Loading
For platforms that support it, use memory mapping to avoid loading all data into RAM at once:

```cpp
class TeletypeTrack {
private:
    struct MappedScriptData {
        uint8_t length;
        std::array<uint32_t, ScriptLineCount> offsets; // Offsets to each command in file
    };
    
public:
    // Only load metadata initially, load individual commands on demand
    tele_command_t getCommand(int scriptIndex, int lineIndex) {
        // Load only the specific command needed
        // This reduces memory usage during initial loading
    }
};
```

## Implementation Strategy

### Phase 1: Immediate Fix
1. Increase FILE task stack size from 2048 to 4096 bytes in Config.h
2. This provides immediate relief while longer-term solutions are developed

### Phase 2: Short-term Improvements
1. Implement chunked loading with yield() calls to prevent blocking
2. Add progress tracking for user feedback during loading
3. Optimize memory operations in TeletypeTrack::read()

### Phase 3: Long-term Solutions
1. Implement lazy loading for teletype scripts
2. Consider task migration for heavy loading operations
3. Evaluate compressed storage if I/O is a bottleneck

## Benefits
- Eliminate stack overflow crashes when loading teletype projects
- Improve loading performance for projects with complex teletype scripts
- Better memory management during project loading
- More responsive UI during loading operations
- Future-proof for more complex teletype projects

## Risks
- Complexity increase in loading code
- Potential for partial loading states
- Need for careful testing to ensure compatibility
- Possible increase in loading time with chunked approach

## Testing Strategy
- Test with projects containing maximum teletype scripts (10 scripts × 64 lines each)
- Verify no crashes occur during loading on hardware
- Ensure all script data is loaded completely and correctly
- Test with various project sizes and complexities
- Verify backward compatibility with existing projects