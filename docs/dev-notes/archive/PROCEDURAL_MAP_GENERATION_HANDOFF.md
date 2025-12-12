# Handoff: Procedural Map Generation & Idler Features

## Current Status
- **Idler Menu**: Implemented top-level menu with "Map Summary" working. Other items are placeholders.
- **Procedural Map Generator**:
  - Refactored to TABS (Island, Dungeon).
  - **Dungeon Mode**: Implemented with Room + Corridor logic.
  - **Positioning**: Fixed to generate at the CURRENT CAMERA view (Standardized).
- **Platform**: functionality verified on Linux (Ubuntu 24.04).

## Files Modified
- `source/main_menubar.cpp/h`: Idler menu structure.
- `source/procedural_map_dialog.cpp/h`: Tabbed interface, offset calculation.
- `source/map_generator.cpp/h`: Generation algorithms, offset support.
- `source/map_summary_window.cpp/h`: Item counting logic.

## Next Steps for Next Agent
1.  **Doodads Filling Tool**: Implement the actual logic (currently placeholder).
2.  **Monster Maker**: Implement logic or UI.
3.  **Edit Items OTB**: Port functionality if needed.
4.  **Verification**: 
    - Test "Dungeon" generation thoroughly (ensure rooms verify connectivity).
    - Check if "Map Summary" handles huge maps efficiently.
5.  **Docs**: Merge `PROCEDURAL_MAP_GENERATION_HANDOFF.md` into main docs if satisfied.

## How to Test
1.  **Build**: `cd build && make -j$(nproc)`
2.  **Run**: `./canary-map-editor`
3.  **Generate**: File -> Generate Procedural Map -> Select Dungeon -> Generate.
    - **Observe**: The map should appear WHERE YOU ARE LOOKING (Camera Center).
4.  **Summary**: Idler -> Map Summary.

## Known Issues
- Dungeon generation is basic (random rooms + corridors). Can be improved with BSP or more complex algorithms.
- Caves are simple noise overlays.
