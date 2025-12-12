//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#ifndef RME_PROCEDURAL_MAP_DIALOG_H_
#define RME_PROCEDURAL_MAP_DIALOG_H_

#include "main.h"
#include "map_generator.h"
#include <wx/notebook.h>

class Editor;

/**
 * ProceduralMapDialog - User interface for procedural map generation
 *
 * Simple dialog for configuring and generating island terrain.
 * Future versions will add tabs for dungeon and mountain generators.
 */
class ProceduralMapDialog : public wxDialog {
public:
	ProceduralMapDialog(wxWindow* parent, Editor& editor);
	virtual ~ProceduralMapDialog();

private:
	// Event handlers
	void OnGenerate(wxCommandEvent& event);
	void OnCancel(wxCommandEvent& event);
	void OnRandomizeSeed(wxCommandEvent& event);

	// UI creation
	void CreateControls();
	wxSizer* CreateMapSizeSection();
	wxSizer* CreateTileIDSection();
	wxSizer* CreateIslandShapeSection();
	wxSizer* CreateNoiseSection();
	wxSizer* CreateCleanupSection();
	wxSizer* CreateSeedSection();
	wxSizer* CreateButtonSection();

	// Island UI
	wxWindow* CreateIslandPage(wxWindow* parent);
	wxSizer* CreateMapSizeSection(wxWindow* parent);
	wxSizer* CreateTileIDSection(wxWindow* parent);
	wxSizer* CreateIslandShapeSection(wxWindow* parent);
	wxSizer* CreateNoiseSection(wxWindow* parent);
	wxSizer* CreateCleanupSection(wxWindow* parent);
	wxSizer* CreateSeedSection(wxWindow* parent);

	// Dungeon UI
	wxWindow* CreateDungeonPage(wxWindow* parent);
	wxSizer* CreateDungeonGeneralSection(wxWindow* parent);
	wxSizer* CreateDungeonRoomsSection(wxWindow* parent);
	wxSizer* CreateDungeonCavesSection(wxWindow* parent);

	// Configuration
	IslandConfig GetIslandConfiguration() const;
	DungeonConfig GetDungeonConfiguration() const;
	void SetDefaults();

	// References
	Editor& editor;
	wxNotebook* notebook;

	// Controls - Map Size (Shared or Island specific?)
	// We'll keep separate controls for simplicity or shared if possible.
	// Let's use separate controls to avoid event conflict complexity.
	wxSpinCtrl* widthCtrl;
	wxSpinCtrl* heightCtrl;

	// Dungeon Controls
	wxSpinCtrl* dngWidthCtrl;
	wxSpinCtrl* dngHeightCtrl;
	wxSpinCtrl* dngRoomCountCtrl;
	wxSpinCtrl* dngMinRoomSizeCtrl;
	wxSpinCtrl* dngMaxRoomSizeCtrl;
	wxSpinCtrl* dngCorridorWidthCtrl;
    wxCheckBox* dngGenerateCavesCheck;
	wxTextCtrl* dngWallIdCtrl;
	wxTextCtrl* dngFloorIdCtrl;
    wxTextCtrl* dngSeedCtrl;
    wxButton* dngRandomizeSeedBtn;

	// Controls - Tile IDs
	wxTextCtrl* waterIdCtrl;
	wxTextCtrl* groundIdCtrl;

	// Controls - Island Shape
	wxSlider* islandSizeSlider;
	wxStaticText* islandSizeLabel;
	wxSlider* falloffSlider;
	wxStaticText* falloffLabel;
	wxSlider* thresholdSlider;
	wxStaticText* thresholdLabel;

	// Controls - Noise
	wxTextCtrl* noiseScaleCtrl;
	wxSpinCtrl* octavesCtrl;
	wxTextCtrl* persistenceCtrl;
	wxTextCtrl* lacunarityCtrl;

	// Controls - Cleanup
	wxCheckBox* enableCleanupCheck;
	wxSpinCtrl* minPatchSizeCtrl;
	wxSpinCtrl* maxHoleSizeCtrl;
	wxSpinCtrl* smoothingPassesCtrl;

	// Controls - Seed
	wxTextCtrl* seedCtrl;
	wxButton* randomizeSeedBtn;

	// Controls - Transparency
	wxToggleButton* transparencyBtn;
	void OnToggleTransparency(wxCommandEvent& event);

	DECLARE_EVENT_TABLE()
};

#endif // RME_PROCEDURAL_MAP_DIALOG_H_
