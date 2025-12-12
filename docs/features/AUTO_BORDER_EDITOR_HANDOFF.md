# Auto Border Editor - Implementation Handoff

**Date**: 2025-12-12  
**Source**: IdlersMapEditor (`Wirless/IdlersMapEditor`)  
**Status**: Research Complete, Ready for Implementation

## Feature Overview

The **Auto Border Editor** is a visual tool that allows map editors to create and configure auto-border patterns for terrain brushes. It provides a user-friendly interface for defining how tiles should automatically connect at edges (borders).

![Auto Border Editor UI](/home/user/.gemini/antigravity/brain/b157a7ef-be6a-486d-ae13-6fb36dfe89de/uploaded_image_1765511665079.png)

### Key Capabilities
- **Visual Grid Interface**: A 3x3+ grid showing all border edge positions
- **Item Assignment**: Click grid cells to assign tile IDs to specific border positions
- **Real-time Preview**: Live preview of how the border will render
- **Load/Save**: Load existing borders for editing and save changes
- **Ground Brush Support**: Separate tab for configuring ground brushes with border alignment

## Source Code Location

### IdlersMapEditor Files
```
/home/user/workspace/remeres/IdlersMapEditor/source/
├── border_editor_window.h          # Main dialog class definition
├── border_editor_window.cpp        # Implementation (~2330 lines)
├── main_menubar.cpp                # Menu integration (OnCreateBorder)
└── palette_brushlist.cpp           # Related UI integration
```

### Core Classes

#### `BorderEditorDialog`
- **Inheritance**: `wxDialog`
- **Purpose**: Main editor window with tabbed interface
- **UI Components**:
  - `wxNotebook` with Border and Ground tabs
  - `BorderGridPanel` for visual editing
  - `BorderPreviewPanel` for live preview
  - Various controls for border properties

#### `BorderGridPanel`
- **Inheritance**: `wxPanel`
- **Purpose**: Interactive 3x3 grid for selecting border positions
- **Features**:
  - Mouse click handling for position selection
  - Visual rendering of assigned items
  - Coordinate mapping for edge positions

#### `BorderItemButton`
- **Inheritance**: `wxButton`
- **Purpose**: Custom button to represent individual border items
- **Features**: Custom paint handler for item visualization

#### `BorderPreviewPanel`
- **Inheritance**: `wxPanel`
- **Purpose**: Shows how the configured border will look in-game
- **Features**: Real-time rendering of border pattern

### Data Structures

```cpp
// Border edge positions (from brush.h)
enum BorderEdgePosition {
    NORTH,
    EAST, 
    SOUTH,
    WEST,
    NORTHWEST,
    NORTHEAST,
    SOUTHWEST,
    SOUTHEAST,
    // ... potentially more positions
};

// Border item (tile ID + position)
struct BorderItem {
    BorderEdgePosition position;
    uint16_t itemId;
    // Additional metadata
};

// Ground item (for ground brushes)
struct GroundItem {
    uint16_t itemId;
    int chance;  // Spawn probability
};
```

## Implementation Plan

### Phase 1: File Transfer
1. **Copy Core Files**:
   ```bash
   cp ../IdlersMapEditor/source/border_editor_window.h source/
   cp ../IdlersMapEditor/source/border_editor_window.cpp source/
   ```

2. **Update CMakeLists.txt**:
   ```cmake
   source/border_editor_window.cpp
   ```

### Phase 2: Integration Points

#### Menu Integration
**File**: `source/main_menubar.cpp`

Add handler:
```cpp
void MainMenuBar::OnCreateBorder(wxCommandEvent& event) {
    BorderEditorDialog* dialog = new BorderEditorDialog(g_gui.root, "Auto Border Editor");
    dialog->Show();
    g_gui.RefreshView();
}
```

Register in menu XML or event table.

#### Data Structure Compatibility
Verify that Canary's `brush.h` includes:
- `BorderEdgePosition` enum
- `BorderItem` struct
- Ground brush structures

If missing, add them based on IdlersMapEditor definitions.

### Phase 3: Dependencies

#### Required Headers
```cpp
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/combobox.h>
#include <wx/listbox.h>
#include <pugixml.hpp>
```

#### External Data Files
- **borders.xml**: Border definitions storage
- **tilesets.xml**: Tileset metadata for ground brushes

### Phase 4: Testing Checklist

- [ ] Dialog opens without errors
- [ ] Grid panel renders correctly
- [ ] Clicking grid cells selects positions
- [ ] Item ID input updates grid
- [ ] "Add" button assigns items to selected position
- [ ] Preview panel shows configured border
- [ ] "Save" writes to `borders.xml`
- [ ] "Load" reads existing borders correctly
- [ ] Ground brush tab functions properly
- [ ] Tileset dropdown populates from `tilesets.xml`

## Technical Considerations

### Rendering System
The preview panel uses wxWidgets' paint events to render tiles. Ensure:
- Sprite loading is compatible with Canary's asset system
- OpenGL context (if used) is properly initialized

### XML Parsing
Uses `pugi::xml` for reading/writing border configurations. Canary already has this dependency, so no additional setup needed.

### Border Storage Format
Example `borders.xml` structure:
```xml
<borders>
  <border id="15" name="Grass" group="1">
    <optional>true</optional>
    <ground>false</ground>
    <item position="NORTH" id="4526"/>
    <item position="EAST" id="4527"/>
    <!-- ... -->
  </border>
</borders>
```

### Performance Notes
- The grid panel redraws frequently; ensure efficient paint handlers
- Preview panel may benefit from caching rendered tiles
- Large border sets should be paginated if performance degrades

## Potential Issues

1. **Brush System Differences**: Canary's brush system may have evolved differently from Idlers. Verify compatibility.
2. **Asset Paths**: Ensure sprite paths align with Canary's directory structure.
3. **Border IDs**: Check for ID conflicts with existing borders.
4. **UI Scaling**: Test on different DPI settings to ensure the grid renders properly.

## Next Steps

1. **Port Core Files**: Copy `border_editor_window.*` and verify compilation.
2. **Add Menu Entry**: Integrate into Tools or Edit menu.
3. **Create Test Borders**: Manually create a few borders to validate XML I/O.
4. **User Testing**: Have a map editor test creating and applying borders.
5. **Documentation**: Update user manual with Auto Border Editor usage guide.

## References

- **Source Repository**: `Wirless/IdlersMapEditor`
- **Local Clone**: `/home/user/workspace/remeres/IdlersMapEditor`
- **Research Document**: `docs/features/AUTO_BORDER_EDITOR_RESEARCH.md`
- **Example Dialog**: See uploaded screenshot

---

**End of Handoff**  
For questions or clarifications, review the source files listed above or consult the IdlersMapEditor repository.
