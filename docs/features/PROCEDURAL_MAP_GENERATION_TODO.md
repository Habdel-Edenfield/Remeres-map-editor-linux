# Procedural Map Generation - To-Do List

Based on [Gap Analysis](features/PROCEDURAL_MAP_GENERATION_GAP_ANALYSIS.md).

## Phase 1: Dungeon Core (Critical Fixes)
- [ ] **Implement A* Pathfinding** (`createSmartCorridor`)
    - [ ] Port `findShortestPath` logic
    - [ ] Implement path cost/heuristic
- [ ] **Implement Intersections**
    - [ ] Create `Intersection` struct
    - [ ] Implement `generateIntersections`
    - [ ] Implement `connectRoomsViaIntersections`
- [ ] **Refactor Connectivity**
    - [ ] Change from sequential chain to graph-based connectivity
- [ ] **Fix Wall Placement Logic**
    - [ ] Only place walls adjacent to floors (not filling the void)

## Phase 2: Aesthetics & Polish
- [ ] **Brush System Integration**
    - [ ] Replace `Item::Create(id)` with Brush lookup
    - [ ] Implement `applyBrushToTile` or equivalent
- [ ] **Decorations (Clutter)**
    - [ ] Port `addClutter` logic
    - [ ] Implement `placeTreesAndVegetation`
    - [ ] Implement `placeStones`
- [ ] **Dungeon Layout Variety**
    - [ ] Implement `addDeadEnds`
    - [ ] Add circular rooms support

## Phase 3: Advanced Features (Optional)
- [ ] **Mountain Generator**
    - [ ] Implement `MountainConfig` UI tab
    - [ ] Implement logic
- [ ] **Terrain Layers**
    - [ ] Support multi-biome maps
- [ ] **System**
    - [ ] Preset Save/Load
