# Monikit Scene and Preset System

## Overview

Monikit implements two distinct systems for saving and loading content:
1. **Scene System**: Monolithic snapshots of complete application state
2. **Preset System**: Modular storage of individual script content

## Scene System

### Structure
- Each scene is a complete snapshot of the entire application state
- Contains all 8 script slots (1-8), Metro (M), and Init (I)
- Contains all 6 pattern banks (0-5) with values and metadata
- Contains all 97+ synthesis parameters
- Contains all audio engine state, effects, envelopes, and UI settings

### Commands
```bash
SAVE <name>     # Save current complete state as a scene
LOAD <name>     # Load complete state from a scene
SCENES          # List available scenes
DELETE <name>   # Delete a scene
```

### Storage Location
- Scenes are stored in `~/.config/monokit/scenes/` (or `~/Library/Application Support/monokit/scenes/` on macOS)
- Each scene is saved as a separate JSON file
- Scene loading replaces the entire current application state

### Characteristics
- **Monolithic**: Complete state replacement
- **Atomic**: All-or-nothing loading
- **Comprehensive**: Includes all parameters and settings
- **Persistent**: Unlimited scenes (disk space limited)

### Exact Implementation

#### Scene Data Structure
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Scene {
    pub version: u32,
    pub scripts: Vec<SceneScript>,
    pub patterns: Vec<ScenePattern>,
    pub pattern_working: usize,
    #[serde(default)]
    pub notes: Vec<String>,
    #[serde(default)]
    pub script_mutes: Vec<bool>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SceneScript {
    pub lines: Vec<String>,
    pub j: i16,
    pub k: i16,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScenePattern {
    pub data: Vec<i16>,
    pub length: usize,
    pub index: usize,
}
```

#### Scene Creation from App State
```rust
impl Scene {
    pub fn from_app_state(scripts: &ScriptStorage, patterns: &PatternStorage, notes: &NotesStorage, script_mutes: &ScriptMutes) -> Self {
        let scene_scripts: Vec<SceneScript> = scripts
            .scripts
            .iter()
            .map(|s| SceneScript {
                lines: s.lines.to_vec(),
                j: s.j,
                k: s.k,
            })
            .collect();

        let scene_patterns: Vec<ScenePattern> = patterns
            .patterns
            .iter()
            .map(|p| ScenePattern {
                data: p.data.to_vec(),
                length: p.length,
                index: p.index,
            })
            .collect();

        Scene {
            version: 1,
            scripts: scene_scripts,
            patterns: scene_patterns,
            pattern_working: patterns.working,
            notes: notes.lines.to_vec(),
            script_mutes: script_mutes.muted.to_vec(),
        }
    }
}
```

#### Scene Application to App State
```rust
impl Scene {
    pub fn apply_to_app_state(&self, scripts: &mut ScriptStorage, patterns: &mut PatternStorage, notes: &mut NotesStorage, script_mutes: &mut ScriptMutes) {
        for (i, scene_script) in self.scripts.iter().enumerate() {
            if i < scripts.scripts.len() {
                for (j, line) in scene_script.lines.iter().enumerate() {
                    if j < 8 {
                        scripts.scripts[i].lines[j] = line.clone();
                    }
                }
                scripts.scripts[i].j = scene_script.j;
                scripts.scripts[i].k = scene_script.k;
            }
        }

        for (i, scene_pattern) in self.patterns.iter().enumerate() {
            if i < patterns.patterns.len() {
                for (j, val) in scene_pattern.data.iter().enumerate() {
                    if j < 64 {
                        patterns.patterns[i].data[j] = *val;
                    }
                }
                patterns.patterns[i].length = scene_pattern.length.min(64);
                patterns.patterns[i].index = scene_pattern.index.min(63);
            }
        }

        patterns.working = self.pattern_working.min(5);

        // Clear all notes first, then load from scene
        for i in 0..8 {
            notes.lines[i] = self.notes.get(i).cloned().unwrap_or_default();
        }

        // Load script mutes
        for i in 0..10 {
            script_mutes.muted[i] = self.script_mutes.get(i).copied().unwrap_or(false);
        }
    }
}
```

#### Scene Management Functions
```rust
pub fn save_scene(name: &str, scene: &Scene) -> Result<(), SceneError> {
    ensure_scenes_dir()?;
    let path = scene_path(name);
    let json = serde_json::to_string_pretty(scene)
        .map_err(|e| SceneError::ParseError(e.to_string()))?;
    fs::write(&path, json).map_err(SceneError::IoError)
}

pub fn load_scene(name: &str) -> Result<Scene, SceneError> {
    let path = scene_path(name);
    if !path.exists() {
        return Err(SceneError::NotFound(name.to_string()));
    }
    let json = fs::read_to_string(&path).map_err(SceneError::IoError)?;
    serde_json::from_str(&json).map_err(|e| SceneError::ParseError(e.to_string()))
}

pub fn list_scenes() -> Result<Vec<(String, u64)>, SceneError> {
    let dir = get_scenes_dir();
    if !dir.exists() {
        return Ok(vec![]);
    }

    let mut scenes = Vec::new();
    for entry in fs::read_dir(&dir).map_err(SceneError::IoError)? {
        let entry = entry.map_err(SceneError::IoError)?;
        let path = entry.path();
        if path.extension().map_or(false, |e| e == "json") {
            if let Some(name) = path.file_stem().and_then(|s| s.to_str()) {
                let size = entry.metadata().map(|m| m.len()).unwrap_or(0);
                scenes.push((name.to_string(), size));
            }
        }
    }
    scenes.sort_by(|a, b| a.0.cmp(&b.0));
    Ok(scenes)
}

pub fn delete_scene(name: &str) -> Result<(), SceneError> {
    let path = scene_path(name);
    if !path.exists() {
        return Err(SceneError::NotFound(name.to_string()));
    }
    fs::remove_file(&path).map_err(SceneError::IoError)
}
```

## Preset System

### Structure
- Each preset contains only the content of a single script
- Includes script lines, J and K variables
- No synthesis parameters or other state information
- Modular and reusable script components

### Commands
```bash
PSET.SAVE <script> <name>    # Save a script to a preset file
PSET <script> <name>         # Load a preset into a script slot
PSET.DEL <name>              # Delete a preset file
PSETS                        # List all available presets
```

### Storage Location
- Presets are stored in `~/.config/monokit/presets/` (or `~/Library/Application Support/monokit/presets/` on macOS)
- Each preset is saved as a separate JSON file
- Preset loading affects only the specified script slot

### Process Flow

#### Saving a Preset (`PSET.SAVE <script> <name>`)
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
    if parts.len() < 3 {
        output("ERROR: PSET.SAVE REQUIRES SCRIPT# AND NAME".to_string());
        return;
    }

    let script_num = match parts[1].parse::<usize>() {
        Ok(n) if n >= 1 && n <= 8 => n - 1,
        _ => {
            output("ERROR: SCRIPT NUMBER MUST BE 1-8".to_string());
            return;
        }
    };

    let name = parts[2..].join(" ").to_lowercase();

    let script = &scripts.scripts[script_num];
    let lines: Vec<String> = script.lines.iter()
        .take_while(|line| !line.is_empty())
        .cloned()
        .collect();

    let preset = Preset {
        version: 1,
        name: name.clone(),
        category: "user".to_string(),
        lines,
        j: script.j,
        k: script.k,
        description: format!("User preset from script {}", script_num + 1),
    };

    match preset::save_preset(&name, &preset) {
        Ok(()) => {
            if debug_level >= TIER_ESSENTIAL || out_ess {
                output(format!("SAVED PRESET: {}", name));
            }
        }
        Err(e) => output(format!("ERROR: {:?}", e)),
    }
}
```

#### Loading a Preset (`PSET <script> <name>`)
```rust
pub fn handle_pset<F>(
    parts: &[&str],
    scripts: &mut ScriptStorage,
    debug_level: u8,
    out_ess: bool,
    mut output: F,
) where
    F: FnMut(String),
{
    if parts.len() < 3 {
        output("ERROR: PSET REQUIRES SCRIPT# AND NAME".to_string());
        return;
    }

    let script_num = match parts[1].parse::<usize>() {
        Ok(n) if n >= 1 && n <= 8 => n - 1,
        _ => {
            output("ERROR: SCRIPT NUMBER MUST BE 1-8".to_string());
            return;
        }
    };

    let name = parts[2..].join(" ").to_lowercase();

    match preset::get_preset(&name) {
        Ok((preset, preset_type)) => {
            let script = &mut scripts.scripts[script_num];

            for (i, line) in preset.lines.iter().enumerate() {
                if i < 8 {
                    script.lines[i] = line.clone();
                }
            }

            for i in preset.lines.len()..8 {
                script.lines[i] = String::new();
            }

            script.j = preset.j;
            script.k = preset.k;

            let type_marker = match preset_type {
                PresetType::Factory => "[F]",
                PresetType::User => "[U]",
            };
            if debug_level >= TIER_ESSENTIAL || out_ess {
                output(format!("LOADED PRESET {} {} INTO SCRIPT {}", type_marker, name, script_num + 1));
            }
        }
        Err(preset::PresetError::NotFound(_)) => {
            output(format!("ERROR: PRESET '{}' NOT FOUND", name));
        }
        Err(e) => {
            output(format!("ERROR: {:?}", e));
        }
    }
}
```

### Characteristics
- **Modular**: Individual script storage and retrieval
- **Reusable**: Build a library of script components
- **Targeted**: Affects only specific script slots
- **Complementary**: Works alongside scene system

### Exact Implementation

#### Preset Data Structure
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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PresetType {
    Factory,
    User,
}
```

#### Preset Management Functions
```rust
pub fn save_preset(name: &str, preset: &Preset) -> Result<(), PresetError> {
    ensure_presets_dir()?;
    let path = preset_path(name);
    let json = serde_json::to_string_pretty(preset)
        .map_err(|e| PresetError::ParseError(e.to_string()))?;
    fs::write(&path, json).map_err(PresetError::IoError)
}

pub fn load_user_preset(name: &str) -> Result<Preset, PresetError> {
    let path = preset_path(name);
    if !path.exists() {
        return Err(PresetError::NotFound(name.to_string()));
    }
    let json = fs::read_to_string(&path).map_err(PresetError::IoError)?;
    serde_json::from_str(&json).map_err(|e| PresetError::ParseError(e.to_string()))
}

pub fn get_preset(name: &str) -> Result<(Preset, PresetType), PresetError> {
    if let Some(preset) = factory::get_factory_preset(name) {
        return Ok((preset, PresetType::Factory));
    }

    match load_user_preset(name) {
        Ok(preset) => Ok((preset, PresetType::User)),
        Err(e) => Err(e),
    }
}

pub fn list_user_presets() -> Result<Vec<(String, u64)>, PresetError> {
    let dir = get_presets_dir();
    if !dir.exists() {
        return Ok(vec![]);
    }

    let mut presets = Vec::new();
    for entry in fs::read_dir(&dir).map_err(PresetError::IoError)? {
        let entry = entry.map_err(PresetError::IoError)?;
        let path = entry.path();
        if path.extension().map_or(false, |e| e == "json") {
            if let Some(name) = path.file_stem().and_then(|s| s.to_str()) {
                let size = entry.metadata().map(|m| m.len()).unwrap_or(0);
                presets.push((name.to_string(), size));
            }
        }
    }
    presets.sort_by(|a, b| a.0.cmp(&b.0));
    Ok(presets)
}

pub fn delete_preset(name: &str) -> Result<(), PresetError> {
    let path = preset_path(name);
    if !path.exists() {
        return Err(PresetError::NotFound(name.to_string()));
    }
    fs::remove_file(&path).map_err(PresetError::IoError)
}
```

#### Preset Path and Directory Management
```rust
pub fn get_presets_dir() -> PathBuf {
    // Use consistent cross-platform path: ~/.config/monokit/presets
    crate::config::monokit_config_dir()
        .unwrap_or_else(|_| {
            let home = env::var("HOME").unwrap_or_else(|_| ".".to_string());
            PathBuf::from(home).join(".config").join("monokit")
        })
        .join("presets")
}

pub fn ensure_presets_dir() -> Result<(), PresetError> {
    let dir = get_presets_dir();
    fs::create_dir_all(&dir).map_err(PresetError::IoError)
}

pub fn preset_path(name: &str) -> PathBuf {
    get_presets_dir().join(format!("{}.json", sanitize_name(name)))
}

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

## Comparison with Teletype

| Feature | Teletype | Monikit Scenes | Monikit Presets |
|---------|----------|----------------|-----------------|
| Capacity | 32 scenes in flash | Unlimited (disk) | Unlimited (disk) |
| Scope | Scripts + patterns + basic vars | Complete app state | Individual scripts only |
| Hardware Control | Dedicated scene button | N/A (TUI) | N/A |
| Individual Script Handling | No | No | Yes |
| Backup System | USB drive | File system | File system |

## Use Cases

### Scene System
- Complete performance snapshots
- Song structure management
- Complex parameter state preservation
- Full system backup and restore

### Preset System
- Reusable script components
- Template management
- Sharing individual scripts
- Building script libraries
- Modular script development

## Integration

The scene and preset systems work independently but complementarily:
- Scenes provide complete state management
- Presets provide modular script component management
- Both systems use the same underlying script storage format
- Presets can be used within scenes as reusable components