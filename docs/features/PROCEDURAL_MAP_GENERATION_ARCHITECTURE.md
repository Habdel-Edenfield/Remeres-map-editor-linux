# Procedural Map Generation Architecture

**Feature:** Procedural terrain generation using Simplex Noise algorithm
**Target Version:** v4.1.0
**Status:** 🚧 In Development
**Date:** 2025-12-11
**Platform:** Ubuntu 24.04 LTS

---

## Overview

This document defines the architectural design for implementing procedural map generation in the Canary Map Editor. The feature ports proven algorithms from the TFS Custom Editor while adapting them to the Canary architecture and event-driven rendering model.

### Feature Scope for v4.1.0

**Included:**
- ✅ Simplex Noise implementation (2D fractal noise)
- ✅ Island terrain generator (water/land with configurable tiles)
- ✅ Post-processing cleanup (remove small patches, fill holes, smooth terrain)
- ✅ Simple configuration dialog
- ✅ Menu integration (`Map > Generate > Procedural Map`)
- ✅ Undo/redo support via ActionQueue
- ✅ Progress reporting for large maps

**Deferred to Future Versions:**
- ⏸️ Dungeon generator (BSP algorithm)
- ⏸️ Mountain generator (height-based)
- ⏸️ Multi-layer terrain system
- ⏸️ Advanced UI with tabbed interface
- ⏸️ Brush system integration for borders

---

## Design Principles

### 1. **Compatibility with Event-Driven Model**

The procedural generator must respect the Canary editor's event-driven architecture:

- **No blocking UI**: Large map generation runs with progress callbacks
- **Batch processing**: Generate in chunks to allow UI updates
- **Event compression**: Trigger single refresh after generation completes
- **Memory efficiency**: Stream tiles instead of buffering entire map

Reference: [`docs/architecture/ARCHITECTURE.md`](../architecture/ARCHITECTURE.md)

### 2. **Code Portability**

Port TFS implementations with minimal changes:

- Preserve algorithm logic and constants
- Adapt to Canary's class structure (`BaseMap`, `Tile`, `Editor`)
- Use existing utilities (`mt_rand.h` for RNG, `Position` for coordinates)
- Maintain GPL-3.0 license compatibility

### 3. **User Experience**

- **Reasonable defaults**: Water ID 4608, Grass ID 4526 (Tibia standard)
- **Fast preview**: 256x256 map generates in < 2 seconds
- **Clear feedback**: Progress bar with percentage and time estimate
- **Undo support**: Full integration with `ActionQueue`

---

## System Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    User Interface Layer                      │
│  ┌────────────────┐      ┌──────────────────────────────┐  │
│  │ MainMenuBar    │──────│ ProceduralMapDialog          │  │
│  │ (Menu Handler) │      │ - Configuration inputs       │  │
│  │                │      │ - Preview (future)           │  │
│  └────────────────┘      │ - Generate/Cancel buttons    │  │
│                          └──────────────────────────────┘  │
└───────────────────────────────────┬─────────────────────────┘
                                    │ (user clicks Generate)
                                    ▼
┌─────────────────────────────────────────────────────────────┐
│                    Editor Layer                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Editor::generateProceduralMap()                        │ │
│  │ - Validates configuration                              │ │
│  │ - Creates Action for undo/redo                         │ │
│  │ - Delegates to MapGenerator                            │ │
│  └────────────────────────────────────────────────────────┘ │
└───────────────────────────────────┬─────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────┐
│                Generation Engine Layer                       │
│  ┌────────────────┐      ┌──────────────────────────────┐  │
│  │ MapGenerator   │──────│ SimplexNoise                 │  │
│  │ - Island gen   │      │ - 2D noise(x, y)            │  │
│  │ - Cleanup      │      │ - fractal() for octaves     │  │
│  │ - Progress     │      │ - Seed-based permutation    │  │
│  └────────────────┘      └──────────────────────────────┘  │
└───────────────────────────────────┬─────────────────────────┘
                                    │ (places tiles)
                                    ▼
┌─────────────────────────────────────────────────────────────┐
│                    Map Data Layer                            │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ BaseMap / Map                                          │ │
│  │ - setTile(Position, Tile*)                            │ │
│  │ - getTile(Position)                                   │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Tile                                                   │ │
│  │ - ground (Item*)                                      │ │
│  │ - items (ItemVector)                                  │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## Core Components

### 1. SimplexNoise Class

**Purpose:** Generate 2D coherent noise for natural terrain patterns

**File:** `source/simplex_noise.h`, `source/simplex_noise.cpp`

**Interface:**
```cpp
class SimplexNoise {
public:
    SimplexNoise(unsigned int seed);

    // Single-octave 2D noise [-1.0, 1.0]
    double noise(double x, double y);

    // Multi-octave fractal noise (Fractal Brownian Motion)
    double fractal(double x, double y, int octaves,
                   double persistence, double lacunarity);

private:
    int perm[512];       // Permutation table
    int permMod12[512];  // Pre-computed modulo 12

    static const int SIMPLEX[64][4];  // Simplex lookup table
    static const double F2, G2;        // Skew/unskew constants
};
```

**Algorithm:** Ken Perlin's Simplex Noise (2001)
- **Input:** 2D coordinates (x, y) and seed
- **Output:** Noise value in range [-1.0, 1.0]
- **Characteristics:**
  - Continuous and smooth
  - Gradient-based (no artifacts)
  - O(1) time complexity per sample
  - Deterministic (same seed → same output)

**Reference:** [otmapgen.cpp:37-173](../../Downloads/Remeres%20Software/tfs_custom_editor/source/otmapgen.cpp#L37-L173)

---

### 2. MapGenerator Class

**Purpose:** Orchestrate map generation using noise algorithms

**File:** `source/map_generator.h`, `source/map_generator.cpp`

**Interface:**
```cpp
struct IslandConfig {
    // Noise parameters
    double noise_scale = 0.01;
    int noise_octaves = 4;
    double noise_persistence = 0.5;
    double noise_lacunarity = 2.0;

    // Island shape
    double island_size = 0.8;        // 0.0-1.0 (affects radius)
    double island_falloff = 2.0;     // Sharpness of coastline
    double island_threshold = 0.3;   // Water/land cutoff

    // Tile IDs
    uint16_t water_id = 4608;        // Default: water
    uint16_t ground_id = 4526;       // Default: grass

    // Cleanup
    bool enable_cleanup = true;
    int min_land_patch_size = 4;     // Remove islands < 4 tiles
    int max_water_hole_size = 3;     // Fill lakes < 3 tiles
    int smoothing_passes = 2;        // Smoothing iterations
};

class MapGenerator {
public:
    MapGenerator();
    ~MapGenerator();

    // Main generation function
    bool generateIslandMap(Map* map, const IslandConfig& config,
                          int width, int height, const std::string& seed);

    // Progress callback
    typedef std::function<bool(int current, int total)> ProgressCallback;
    void setProgressCallback(ProgressCallback callback);

private:
    SimplexNoise* noise;
    ProgressCallback progress_callback;

    // Generation steps
    std::vector<std::vector<double>> generateHeightMap(
        const IslandConfig& config, int width, int height);

    void applyIslandMask(std::vector<std::vector<double>>& heightMap,
                        const IslandConfig& config, int width, int height);

    void placeTiles(Map* map, const std::vector<std::vector<double>>& heightMap,
                   const IslandConfig& config, int width, int height);

    // Post-processing
    void cleanupTerrain(Map* map, const IslandConfig& config,
                       int width, int height);
    void removeSmallPatches(Map* map, uint16_t tile_id, int min_size,
                           int width, int height);
    void fillSmallHoles(Map* map, uint16_t fill_id, int max_size,
                       int width, int height);
    void smoothCoastline(Map* map, const IslandConfig& config,
                        int width, int height);
};
```

---

### 3. ProceduralMapDialog (UI)

**Purpose:** User interface for generator configuration

**File:** `source/procedural_map_dialog.h`, `source/procedural_map_dialog.cpp`

**Simplified Layout (v4.1.0):**
```
┌─────────────────────────────────────────────────┐
│ Generate Procedural Map                         │
├─────────────────────────────────────────────────┤
│                                                  │
│  Map Size:                                       │
│    Width:  [256      ] tiles                    │
│    Height: [256      ] tiles                    │
│                                                  │
│  Terrain Configuration:                          │
│    Water Tile ID:  [4608    ]  [Browse...]     │
│    Ground Tile ID: [4526    ]  [Browse...]     │
│                                                  │
│  Island Shape:                                   │
│    Size:      [0.80    ] (0.0-1.0)             │
│    Falloff:   [2.0     ] (sharper coastline)   │
│    Threshold: [0.30    ] (water/land cutoff)   │
│                                                  │
│  Noise Settings:                                 │
│    Scale:       [0.01   ] (zoom level)         │
│    Octaves:     [4      ] (detail layers)      │
│    Persistence: [0.5    ] (roughness)          │
│    Lacunarity:  [2.0    ] (frequency mult)     │
│                                                  │
│  Cleanup:                                        │
│    [✓] Remove small land patches (< 4 tiles)   │
│    [✓] Fill small water holes (< 3 tiles)      │
│    [✓] Smooth coastline (2 passes)             │
│                                                  │
│  Seed: [random_seed_123        ] [Randomize]   │
│                                                  │
│  [         Generate         ] [    Cancel    ]  │
│                                                  │
└─────────────────────────────────────────────────┘
```

**wxWidgets Implementation:**
- `wxDialog` base class
- `wxBoxSizer` for vertical layout
- `wxSpinCtrl` / `wxTextCtrl` for numeric inputs
- `wxSlider` with labels for visual feedback
- `wxCheckBox` for cleanup options
- `wxButton` for actions

---

## Generation Algorithm (Island)

### Step-by-Step Flow

```
1. Initialize SimplexNoise with seed
   ├─→ Hash seed string to numeric value
   └─→ Create permutation table [0-255] shuffled

2. Generate Height Map (2D array of doubles)
   ├─→ For each coordinate (x, y):
   │   ├─→ Sample noise at (x * scale, y * scale)
   │   ├─→ Apply fractal octaves (4 layers by default)
   │   └─→ Result: height value [-1.0, 1.0]
   └─→ Output: width × height grid of heights

3. Apply Island Mask
   ├─→ Calculate distance from center for each point
   ├─→ Apply radial falloff: height -= falloff_curve(distance)
   └─→ Result: Island shape (high center, low edges)

4. Place Tiles on Map
   ├─→ For each point in height map:
   │   ├─→ if height < threshold: place water_id
   │   └─→ else: place ground_id
   └─→ All tiles placed at floor z=7 (ground level)

5. Cleanup (if enabled)
   ├─→ Remove small land patches (flood fill < min_size)
   ├─→ Fill small water holes (flood fill < max_size)
   └─→ Smooth coastline (neighbor majority voting)

6. Refresh UI
   ├─→ Update minimap
   ├─→ Trigger canvas refresh (event-driven)
   └─→ Show completion message
```

### Mathematical Details

**Fractal Brownian Motion (FBM):**
```
fbm(x, y) = Σ(i=0 to octaves-1)
              noise(x * frequency_i, y * frequency_i) * amplitude_i

where:
  frequency_i = lacunarity^i  (default: 2.0^i)
  amplitude_i = persistence^i (default: 0.5^i)
```

**Island Falloff:**
```
distance = sqrt((x - centerX)^2 + (y - centerY)^2) / (size * max_radius)
falloff = distance^island_falloff
final_height = height - falloff
```

**Water/Land Decision:**
```
if final_height < island_threshold:
    place water_id
else:
    place ground_id
```

---

## Integration Points

### 1. Menu System

**File:** `source/main_menubar.cpp`

**Action ID:** Add `MenuBar::GENERATE_PROCEDURAL_MAP`

**Event Handler:**
```cpp
void MainMenuBar::OnGenerateProceduralMap(wxCommandEvent& event) {
    if (!g_gui.IsEditorOpen()) {
        wxMessageBox("Please create or open a map first.", "No Map",
                    wxOK | wxICON_WARNING);
        return;
    }

    ProceduralMapDialog dialog(this, g_gui.GetCurrentEditor());
    if (dialog.ShowModal() == wxID_OK) {
        // Generation already executed, just refresh
        g_gui.RefreshView();
        g_gui.UpdateMinimap();
    }
}
```

**Menu Structure:**
```
Map
├─ Edit Towns
├─ Edit Items
├─ Edit Monsters
├─ Map Properties
├─ Map Statistics
├─ ────────────────
├─ Generate         ← New submenu
│  └─ Procedural Map...
└─ ────────────────
```

---

### 2. Undo/Redo System

**Action Class:**
```cpp
class Action_GenerateProceduralMap : public Action {
public:
    Action_GenerateProceduralMap(const IslandConfig& config,
                                 int width, int height);

    void perform() override;
    void undo() override;

private:
    IslandConfig config;
    int width, height;
    std::vector<TileBackup> previous_tiles;  // For undo
};
```

**Integration:**
```cpp
// In ProceduralMapDialog::OnGenerate()
auto action = new Action_GenerateProceduralMap(config, width, height);
editor->addAction(action);
```

---

### 3. Progress Reporting

**Callback Interface:**
```cpp
bool ProgressCallback(int current, int total) {
    // Update wxProgressDialog
    // Return false to cancel
}
```

**Implementation:**
```cpp
// In ProceduralMapDialog
wxProgressDialog progress("Generating Map", "Processing...",
                         total_tiles, this,
                         wxPD_AUTO_HIDE | wxPD_CAN_ABORT);

generator.setProgressCallback([&](int current, int total) {
    return progress.Update(current,
        wxString::Format("Generated %d/%d tiles", current, total));
});
```

---

## Performance Considerations

### Benchmarks (Target)

| Map Size | Generation Time | Memory Usage | Notes |
|----------|----------------|--------------|-------|
| 256×256  | < 2 seconds    | ~15 MB       | Fast preview |
| 512×512  | < 8 seconds    | ~60 MB       | Standard |
| 1024×1024| < 30 seconds   | ~240 MB      | Large map |
| 2048×2048| < 2 minutes    | ~960 MB      | Maximum |

**Optimization Strategies:**

1. **Batch Processing:**
   - Generate 1000 tiles per batch
   - Update progress between batches
   - Allow event loop to process UI updates

2. **Memory Efficiency:**
   - Stream tiles directly to map
   - Don't buffer entire height map (use row-by-row)
   - Release noise generator after completion

3. **Early Exit:**
   - Check progress callback return value
   - Clean up partial generation on cancel

---

## Error Handling

### Validation Checks

1. **Pre-generation:**
   - Map must be open
   - Width/height > 0 and < 4096
   - Tile IDs must exist in items database
   - Noise parameters within valid ranges

2. **During generation:**
   - Memory allocation checks
   - Progress callback cancellation
   - Tile placement validation

3. **Post-generation:**
   - Verify tiles were placed
   - Validate minimap update
   - Check action queue state

### Error Recovery

```cpp
try {
    generator.generateIslandMap(map, config, width, height, seed);
    editor->addAction(action);
    g_gui.RefreshView();
} catch (const std::exception& e) {
    wxMessageBox(wxString::Format("Generation failed: %s", e.what()),
                "Error", wxOK | wxICON_ERROR);
    // Cleanup partial generation
    action->undo();
    delete action;
}
```

---

## Testing Strategy

### Unit Tests

1. **SimplexNoise:**
   - Same seed produces same output
   - Output range [-1.0, 1.0]
   - Continuity (neighboring samples are similar)
   - Performance (1M samples < 100ms)

2. **MapGenerator:**
   - Valid tile placement
   - Island shape correctness
   - Cleanup effectiveness
   - Progress callback invocation

### Integration Tests

1. **UI Flow:**
   - Dialog opens with valid map
   - Dialog blocks when no map
   - Configuration persists between opens
   - Generate button triggers generation

2. **Map Integration:**
   - Tiles placed at correct z-level (7)
   - Previous tiles are cleared
   - Undo restores previous state
   - Minimap updates correctly

### Visual Tests

1. **Island Shape:**
   - Circular coastline
   - No isolated pixels
   - Smooth transitions
   - Variety with different seeds

2. **Cleanup:**
   - Small patches removed
   - Holes filled
   - Coastline smoothed
   - No artifacts

---

## Future Enhancements (v4.2.0+)

### Dungeon Generator
- BSP (Binary Space Partition) algorithm
- Room and corridor generation
- Wall/floor/door placement
- Multi-level dungeons

### Advanced Terrain
- Multi-layer system (grass, sand, mountain)
- Height-based generation
- Moisture maps for biomes
- Cave systems (3D)

### Enhanced UI
- Tabbed interface (Island | Dungeon | Mountain)
- Live preview panel
- Preset configurations
- Export/import config files

### Brush Integration
- Use existing brush system for borders
- Apply decorations (trees, rocks)
- Automatic borderization
- Cluster placement (villages, forests)

---

## References

### Internal Documentation
- [`ARCHITECTURE.md`](../architecture/ARCHITECTURE.md) - Event-driven model
- [`LINUX_PORT_AUDIT.md`](../linux-port/LINUX_PORT_AUDIT.md) - Platform specifics
- [`FPS_TELEMETRY_ANALYSIS.md`](../architecture/FPS_TELEMETRY_ANALYSIS.md) - Performance

### External Resources
- **TFS Custom Editor**: `/home/user/Downloads/Remeres Software/tfs_custom_editor/`
  - `source/otmapgen.h` - Interface definitions
  - `source/otmapgen.cpp` - Algorithm implementations
- **Simplex Noise Paper**: Ken Perlin (2001) - "Simplex Noise Demystified"
- **wxWidgets Docs**: https://docs.wxwidgets.org/3.0/ - UI framework

### Related Files
- `source/map.h` - Map data structures
- `source/basemap.h` - Spatial indexing
- `source/tile.h` - Tile manipulation
- `source/editor.h` - Editor actions
- `source/mt_rand.h` - Random number generation

---

## Glossary

**Simplex Noise:** Gradient noise algorithm by Ken Perlin, improved version of Perlin Noise
**Fractal Brownian Motion (FBM):** Summation of multiple noise octaves at different frequencies
**Octave:** A single layer of noise in FBM
**Lacunarity:** Frequency multiplier between octaves (typically 2.0)
**Persistence:** Amplitude multiplier between octaves (typically 0.5)
**Island Falloff:** Radial gradient that creates island shape from center
**Flood Fill:** Algorithm to find connected regions of same tile type
**Event-driven Rendering:** UI updates only on state changes, not continuous polling

---

**Document Version:** 1.0
**Last Updated:** 2025-12-11
**Next Review:** After v4.1.0 release
