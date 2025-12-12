# Gap Analysis: TFS vs Canary Procedural Generation

**Date:** 2025-12-12
**Status:** In Progress
**Reference System:** TFS Custom Editor (Windows)
**Target System:** Canary Map Editor (Ubuntu 24.04)

## Executive Summary

The port of the Procedural Map Generation system from TFS to Canary is partially complete. The **Island Generator** is nearly 1:1 with the reference, offering full functionality. The **Dungeon Generator** has been implemented with a basic algorithm (Room + Corridor + Cave Overlay) but lacks significant logic for complex layouts (pathfinding, intersections, dead ends). The **Mountain Generator** and **Decoration/Brush** systems are currently completely absent.

- **Total Completeness:** ~35%
- **Critical Features Missing:** Advanced Dungeon Layouts (A*), Decorations, Brush Integration.
- **Estimated Effort to Complete:** ~25-30 hours of development time.

---

## 2. Feature Completion Matrix

### ✅ Island Generator (100% Complete)

| Feature | TFS | Canary | Status |
| :--- | :---: | :---: | :--- |
| Simplex Noise 2D | ✅ | ✅ | **Complete** |
| Fractal FBM | ✅ | ✅ | **Complete** |
| Radial Mask (Falloff) | ✅ | ✅ | **Complete** |
| Height Map Generation | ✅ | ✅ | **Complete** |
| Cleanup (Small Patches) | ✅ | ✅ | **Complete** |
| Smoothing (Coastline) | ✅ | ✅ | **Complete** |
| Config Parameters | ✅ | ✅ | **1:1 Match** |

**Notes:** The implementation logic in `MapGenerator::generateIslandMap` is effectively identical to TFS `OTMapGenerator::generateIslandMap`.

### ⚠️ Dungeon Generator (~40% Complete)

| Feature | TFS | Canary | Status | Notes |
| :--- | :---: | :---: | :--- | :--- |
| Room Placement | ✅ | ✅ | **Basic** | Non-overlapping rooms working. |
| Corridor Connection | ✅ | ⚠️ | **Simplified** | Uses simple L-shape. Chains rooms sequentially `[i] -> [i+1]`. |
| Cave Overlay | ✅ | ✅ | **Complete** | Simplex noise overlay implemented. |
| **A* Pathfinding** | ✅ | ❌ | **Missing** | `createSmartCorridor` / `findShortestPath` not implemented. |
| **Intersections** | ✅ | ❌ | **Missing** | No T-junctions or Crossroads logic. |
| **Dead Ends** | ✅ | ❌ | **Missing** | `addDeadEnds` logic missing. |
| **Circular Rooms** | ✅ | ❌ | **Missing** | Only rectangular rooms supported. |
| **Complexity Control** | ✅ | ❌ | **Missing** | No way to control maze complexity. |
| **Connectivity Check** | ✅ | ❌ | **Missing** | No guarantee that all rooms are reachable if random connection fails (though current sequential logic tries to force it). |
| **Walls Logic** | ✅ | ⚠️ | **Buggy** | Canary fills everything with walls first. TFS places walls only at borders of floors. |

### ❌ Missing Systems (0% Complete)

| Feature | Importance | Status | Notes |
| :--- | :---: | :--- | :--- |
| **Mountain Generator** | Low | ❌ | `MountainConfig` struct and tab missing. |
| **Terrain Layers** | Medium | ❌ | `TerrainLayer` struct missing. Island uses hardcoded water/ground. |
| **Decorations** | High | ❌ | `placeTreesAndVegetation`, `placeStones` missing. Maps are empty execution layers. |
| **Brush System** | Critical | ❌ | automatic bordering not integrated. Code uses raw Item IDs. |

---

## 3. Detailed Logic Gaps

### Dungeon Generation
The TFS dungeon generator produces a "maze-like" structure with loops, dead ends, and organic corridors. The current Canary implementation produces a "snake": Room A connected to Room B connected to Room C.
*   **Missing `createSmartCorridor`**: TFS uses A* to find paths around obstacles (other rooms) to avoid cutting through them. Canary just blasts L-shaped corridors, potentially cutting through other rooms or walls undesirably (though check logic handles simple overlap, it's not robust).
*   **Missing `generateIntersections`**: TFS creates dedicated "hubs" (3-way or 4-way) and connects rooms to them, creating a network. Canary connects Room to Room directly.

### Decorations (Clutter)
TFS has `addClutter` which randomly places:
*   Trees/Bushes on Grass
*   Stones/Pebbles on Mountain/Cave
This makes the map look "finished". Current Canary maps are sterile (just flat ground).

### Brush Integration
TFS uses `applyBrushToTile`. Canary sets `Item::Create(id)`.
*   **Impact**: When you generate a map in Canary, you get raw tiles (e.g., grass ID 4526). You do NOT get the auto-borders (transitions between grass and water). The map looks "blocky" and requires manual bordering.
*   **Fix**: Need to instantiate the Border Automizer or use `Brush` classes to place tiles.

---

## 4. Prioritized Roadmap

### Phase 1: Dungeon Core (Critical Fixes) - 1 Week
Focus on making the Dungeon generator produce usable, interesting layouts.
1.  **Implement A* Pathfinding**: Port `findShortestPath` and stack-based/queue-based solver.
2.  **Implement Intersections**: Create `Intersection` struct and logic to place "hubs".
3.  **Refactor Connectivity**: Switch from "Chain" (A->B->C) to "Graph" (Nearest Neighbors + Intersections).
4.  **Fix Wall Logic**: Place walls only adjacent to floors, not filling the whole void (performance + aesthetics).

### Phase 2: Aesthetics & Polish - 1-2 Weeks
Focus on making generated maps look good (not blocky).
1.  **Brush System Integration**: Instead of `tile->ground = Item::Create(id)`, look up the `Brush` by ID/Name and use `brush->Draw(map, x, y, z)`. This usually handles bordering automatically.
2.  **Decorations**: Port `addClutter` logic. Add "Details" density slider to UI.
3.  **Dead Ends**: Add `addDeadEnds` to make dungeons feel less linear.

### Phase 3: Advanced Features (Optional) - Future
1.  **Mountain Generator**: If demanded.
2.  **Terrain Layers**: Allow identifying "biomes" (Sand vs Grass areas in same map).
3.  **Presets**: Save/Load generator configs.

---

## 5. Metadata for Next Agent

*   **TFS Reference File**: `otmapgen.cpp` (lines 1200-1800 for Dungeon, 1800-2100 for Decorations).
*   **Canary File**: `map_generator.cpp`
*   **Key Missing Classes**: `std::priority_queue` usage for A*, `Intersection` struct.
