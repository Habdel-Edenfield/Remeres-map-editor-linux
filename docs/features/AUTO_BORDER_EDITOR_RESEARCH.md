# Auto Border Editor Research

**Source Repository**: `Wirless/IdlersMapEditor` (Locally available at `../IdlersMapEditor`)
**Feature Name**: Auto Border Editor
**Entry Point**: `MainMenuBar::OnCreateBorder` (`main_menubar.cpp`)

## Implementation Details

The feature is implemented as a standalone modal dialog that allows users to create and edit auto-border configurations visually.

### Key Files
- **Header**: `source/border_editor_window.h`
- **Implementation**: `source/border_editor_window.cpp`

### Structure
The main class is `BorderEditorDialog` (inherits from `wxDialog`). It contains:

1.  **Tabs**:
    - **Border**: For configuring standard borders.
    - **Ground**: For configuring ground brushes.

2.  **Custom Controls**:
    - `BorderGridPanel`: A custom `wxPanel` that renders a 3x3 (or similar) grid allowing users to click and define border edges.
    - `BorderPreviewPanel`: Renders a live preview of the border.
    - `BorderItemButton`: Custom buttons representing border parts.

### Logic
- **Loading**: It reads existing borders likely from `borders.xml` or similar, parsing them into the UI.
- **Editing**: Users can visually toggle parts of the border mask.
- **Saving**: It writes the changes back to the configuration files.

### dependencies
- Standard `wxWidgets` components (`wxNotebook`, `wxSpinCtrl`, etc.).
- `pugi::xml` for XML parsing.
- Access to global `GUI` root for parenting.

## Integration Plan (Potential)
To port this to Canary Map Editor:
1.  Copy `border_editor_window.h` and `.cpp`.
2.  Add `OnCreateBorder` handler to `MainMenuBar`.
3.  Ensure `brushes.h` and other data structures are compatible (Idlers might have different brush structures).
4.  Copy/Adapt the `BorderGridPanel` and `BorderPreviewPanel` logic.
