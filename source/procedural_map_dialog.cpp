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

#include "main.h"
#include "procedural_map_dialog.h"
#include "editor.h"
#include "map.h"
#include "map_generator.h"

#include <wx/statbox.h>
#include <wx/spinctrl.h>
#include <wx/progdlg.h>
#include <wx/tglbtn.h>
#include <random>
#include <sstream>
#include <iomanip>

BEGIN_EVENT_TABLE(ProceduralMapDialog, wxDialog)
	EVT_BUTTON(wxID_OK, ProceduralMapDialog::OnGenerate)
	EVT_BUTTON(wxID_CANCEL, ProceduralMapDialog::OnCancel)
	EVT_BUTTON(wxID_ANY, ProceduralMapDialog::OnRandomizeSeed)
	EVT_TOGGLEBUTTON(10001, ProceduralMapDialog::OnToggleTransparency)
END_EVENT_TABLE()

ProceduralMapDialog::ProceduralMapDialog(wxWindow* parent, Editor& editor)
	: wxDialog(parent, wxID_ANY, "Generate Procedural Map", wxDefaultPosition, wxDefaultSize,
	           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	  editor(editor) {

	CreateControls();
	SetDefaults();

	Fit();
	Centre();
}

ProceduralMapDialog::~ProceduralMapDialog() {
}

void ProceduralMapDialog::CreateControls() {
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	notebook = new wxNotebook(this, wxID_ANY);
	notebook->AddPage(CreateIslandPage(notebook), "Island Generator");
	notebook->AddPage(CreateDungeonPage(notebook), "Dungeon Generator");

	mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);
	mainSizer->Add(CreateButtonSection(), 0, wxEXPAND | wxALL, 10);

	SetSizer(mainSizer);
}

wxWindow* ProceduralMapDialog::CreateIslandPage(wxWindow* parent) {
	wxPanel* panel = new wxPanel(parent);
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

	// Left Column
	wxBoxSizer* leftCol = new wxBoxSizer(wxVERTICAL);
	leftCol->Add(CreateMapSizeSection(panel), 0, wxEXPAND | wxALL, 5);
	leftCol->Add(CreateTileIDSection(panel), 0, wxEXPAND | wxALL, 5);
	leftCol->Add(CreateSeedSection(panel), 0, wxEXPAND | wxALL, 5);

	// Right Column
	wxBoxSizer* rightCol = new wxBoxSizer(wxVERTICAL);
	rightCol->Add(CreateIslandShapeSection(panel), 0, wxEXPAND | wxALL, 5);
	rightCol->Add(CreateNoiseSection(panel), 0, wxEXPAND | wxALL, 5);
	rightCol->Add(CreateCleanupSection(panel), 0, wxEXPAND | wxALL, 5);

	sizer->Add(leftCol, 1, wxEXPAND | wxALL, 5);
	sizer->Add(rightCol, 1, wxEXPAND | wxALL, 5);

	panel->SetSizer(sizer);
	return panel;
}

wxWindow* ProceduralMapDialog::CreateDungeonPage(wxWindow* parent) {
	wxPanel* panel = new wxPanel(parent);
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

	// Left Column
	wxBoxSizer* leftCol = new wxBoxSizer(wxVERTICAL);
	leftCol->Add(CreateDungeonGeneralSection(panel), 0, wxEXPAND | wxALL, 5);
	leftCol->Add(CreateDungeonRoomsSection(panel), 0, wxEXPAND | wxALL, 5);
//	leftCol->Add(CreateSeedSection(panel), 0, wxEXPAND | wxALL, 5); // Seed is shared or duplicated? Use dngSeedCtrl.

    // Seed section for Dungeon specific
    wxStaticBoxSizer* seedBox = new wxStaticBoxSizer(wxHORIZONTAL, panel, "Random Seed");
	dngSeedCtrl = new wxTextCtrl(panel, wxID_ANY, "");
	seedBox->Add(dngSeedCtrl, 1, wxEXPAND | wxALL, 5);
	dngRandomizeSeedBtn = new wxButton(panel, wxID_ANY, "Randomize");
	seedBox->Add(dngRandomizeSeedBtn, 0, wxALL, 5);
    leftCol->Add(seedBox, 0, wxEXPAND | wxALL, 5);

    // Bind randomize button for dungeon
    dngRandomizeSeedBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        dngSeedCtrl->SetValue(wxString::Format("dungeon_%d", dis(gen)));
    });

	// Right Column
	wxBoxSizer* rightCol = new wxBoxSizer(wxVERTICAL);
	rightCol->Add(CreateDungeonCavesSection(panel), 0, wxEXPAND | wxALL, 5);
    // Add spacer or reuse cleanup section if applicable
    
	sizer->Add(leftCol, 1, wxEXPAND | wxALL, 5);
	sizer->Add(rightCol, 1, wxEXPAND | wxALL, 5);

	panel->SetSizer(sizer);
	return panel;
}

wxSizer* ProceduralMapDialog::CreateMapSizeSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, parent, "Map Size");

	wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
	grid->AddGrowableCol(1);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Width:"), 0, wxALIGN_CENTER_VERTICAL);
	widthCtrl = new wxSpinCtrl(parent, wxID_ANY, "256", wxDefaultPosition, wxDefaultSize,
	                           wxSP_ARROW_KEYS, 16, 4096, 256);
	grid->Add(widthCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Height:"), 0, wxALIGN_CENTER_VERTICAL);
	heightCtrl = new wxSpinCtrl(parent, wxID_ANY, "256", wxDefaultPosition, wxDefaultSize,
	                            wxSP_ARROW_KEYS, 16, 4096, 256);
	grid->Add(heightCtrl, 1, wxEXPAND);

	box->Add(grid, 1, wxEXPAND | wxALL, 5);
	return box;
}

wxSizer* ProceduralMapDialog::CreateTileIDSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, parent, "Terrain Tiles");

	wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
	grid->AddGrowableCol(1);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Water Tile ID:"), 0, wxALIGN_CENTER_VERTICAL);
	waterIdCtrl = new wxTextCtrl(parent, wxID_ANY, "4608");
	grid->Add(waterIdCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Ground Tile ID:"), 0, wxALIGN_CENTER_VERTICAL);
	groundIdCtrl = new wxTextCtrl(parent, wxID_ANY, "4526");
	grid->Add(groundIdCtrl, 1, wxEXPAND);

	box->Add(grid, 1, wxEXPAND | wxALL, 5);
	return box;
}

wxSizer* ProceduralMapDialog::CreateIslandShapeSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, parent, "Island Shape");

	wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
	grid->AddGrowableCol(1);

	// Island Size
	grid->Add(new wxStaticText(parent, wxID_ANY, "Size:"), 0, wxALIGN_CENTER_VERTICAL);
	wxBoxSizer* sizeSizer = new wxBoxSizer(wxHORIZONTAL);
	islandSizeSlider = new wxSlider(parent, wxID_ANY, 80, 10, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	islandSizeLabel = new wxStaticText(parent, wxID_ANY, "0.80");
	sizeSizer->Add(islandSizeSlider, 1, wxEXPAND | wxRIGHT, 5);
	sizeSizer->Add(islandSizeLabel, 0, wxALIGN_CENTER_VERTICAL);
	grid->Add(sizeSizer, 1, wxEXPAND);

	// Falloff
	grid->Add(new wxStaticText(parent, wxID_ANY, "Falloff:"), 0, wxALIGN_CENTER_VERTICAL);
	wxBoxSizer* falloffSizer = new wxBoxSizer(wxHORIZONTAL);
	falloffSlider = new wxSlider(parent, wxID_ANY, 20, 5, 50, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	falloffLabel = new wxStaticText(parent, wxID_ANY, "2.0");
	falloffSizer->Add(falloffSlider, 1, wxEXPAND | wxRIGHT, 5);
	falloffSizer->Add(falloffLabel, 0, wxALIGN_CENTER_VERTICAL);
	grid->Add(falloffSizer, 1, wxEXPAND);

	// Threshold
	grid->Add(new wxStaticText(parent, wxID_ANY, "Threshold:"), 0, wxALIGN_CENTER_VERTICAL);
	wxBoxSizer* thresholdSizer = new wxBoxSizer(wxHORIZONTAL);
	thresholdSlider = new wxSlider(parent, wxID_ANY, 30, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
	thresholdLabel = new wxStaticText(parent, wxID_ANY, "0.30");
	thresholdSizer->Add(thresholdSlider, 1, wxEXPAND | wxRIGHT, 5);
	thresholdSizer->Add(thresholdLabel, 0, wxALIGN_CENTER_VERTICAL);
	grid->Add(thresholdSizer, 1, wxEXPAND);

	// Update labels when sliders change
	islandSizeSlider->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) {
		islandSizeLabel->SetLabel(wxString::Format("%.2f", islandSizeSlider->GetValue() / 100.0));
	});
	falloffSlider->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) {
		falloffLabel->SetLabel(wxString::Format("%.1f", falloffSlider->GetValue() / 10.0));
	});
	thresholdSlider->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) {
		thresholdLabel->SetLabel(wxString::Format("%.2f", (thresholdSlider->GetValue() - 50) / 50.0));
	});

	box->Add(grid, 1, wxEXPAND | wxALL, 5);
	return box;
}

wxSizer* ProceduralMapDialog::CreateNoiseSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, parent, "Noise Settings");

	wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
	grid->AddGrowableCol(1);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Scale:"), 0, wxALIGN_CENTER_VERTICAL);
	noiseScaleCtrl = new wxTextCtrl(parent, wxID_ANY, "0.01");
	grid->Add(noiseScaleCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Octaves:"), 0, wxALIGN_CENTER_VERTICAL);
	octavesCtrl = new wxSpinCtrl(parent, wxID_ANY, "4", wxDefaultPosition, wxDefaultSize,
	                             wxSP_ARROW_KEYS, 1, 8, 4);
	grid->Add(octavesCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Persistence:"), 0, wxALIGN_CENTER_VERTICAL);
	persistenceCtrl = new wxTextCtrl(parent, wxID_ANY, "0.5");
	grid->Add(persistenceCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Lacunarity:"), 0, wxALIGN_CENTER_VERTICAL);
	lacunarityCtrl = new wxTextCtrl(parent, wxID_ANY, "2.0");
	grid->Add(lacunarityCtrl, 1, wxEXPAND);

	box->Add(grid, 1, wxEXPAND | wxALL, 5);
	return box;
}

wxSizer* ProceduralMapDialog::CreateCleanupSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, parent, "Post-Processing");

	enableCleanupCheck = new wxCheckBox(parent, wxID_ANY, "Enable terrain cleanup");
	enableCleanupCheck->SetValue(true);
	box->Add(enableCleanupCheck, 0, wxALL, 5);

	wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
	grid->AddGrowableCol(1);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Min land patch size:"), 0, wxALIGN_CENTER_VERTICAL);
	minPatchSizeCtrl = new wxSpinCtrl(parent, wxID_ANY, "4", wxDefaultPosition, wxDefaultSize,
	                                  wxSP_ARROW_KEYS, 0, 100, 4);
	grid->Add(minPatchSizeCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Max water hole size:"), 0, wxALIGN_CENTER_VERTICAL);
	maxHoleSizeCtrl = new wxSpinCtrl(parent, wxID_ANY, "3", wxDefaultPosition, wxDefaultSize,
	                                 wxSP_ARROW_KEYS, 0, 100, 3);
	grid->Add(maxHoleSizeCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Smoothing passes:"), 0, wxALIGN_CENTER_VERTICAL);
	smoothingPassesCtrl = new wxSpinCtrl(parent, wxID_ANY, "2", wxDefaultPosition, wxDefaultSize,
	                                     wxSP_ARROW_KEYS, 0, 10, 2);
	grid->Add(smoothingPassesCtrl, 1, wxEXPAND);

	box->Add(grid, 1, wxEXPAND | wxALL, 5);
	return box;
}

wxSizer* ProceduralMapDialog::CreateSeedSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxHORIZONTAL, parent, "Random Seed");

	seedCtrl = new wxTextCtrl(parent, wxID_ANY, "");
	box->Add(seedCtrl, 1, wxEXPAND | wxALL, 5);

	randomizeSeedBtn = new wxButton(parent, wxID_ANY, "Randomize");
	box->Add(randomizeSeedBtn, 0, wxALL, 5);

	return box;
}

wxSizer* ProceduralMapDialog::CreateButtonSection() {
	wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

	buttonSizer->AddStretchSpacer();
	buttonSizer->Add(new wxButton(this, wxID_OK, "Generate"), 0, wxALL, 5);
	buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxALL, 5);
	
	transparencyBtn = new wxToggleButton(this, 10001, "Transparent");
	buttonSizer->Add(transparencyBtn, 0, wxALL, 5);

	return buttonSizer;
}

wxSizer* ProceduralMapDialog::CreateDungeonGeneralSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, parent, "General Settings");
	wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
	grid->AddGrowableCol(1);

	// Size
	grid->Add(new wxStaticText(parent, wxID_ANY, "Width:"), 0, wxALIGN_CENTER_VERTICAL);
	dngWidthCtrl = new wxSpinCtrl(parent, wxID_ANY, "128", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 16, 4096, 128);
	grid->Add(dngWidthCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Height:"), 0, wxALIGN_CENTER_VERTICAL);
	dngHeightCtrl = new wxSpinCtrl(parent, wxID_ANY, "128", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 16, 4096, 128);
	grid->Add(dngHeightCtrl, 1, wxEXPAND);

	// IDs
	grid->Add(new wxStaticText(parent, wxID_ANY, "Wall ID:"), 0, wxALIGN_CENTER_VERTICAL);
	dngWallIdCtrl = new wxTextCtrl(parent, wxID_ANY, "1030");
	grid->Add(dngWallIdCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Floor ID:"), 0, wxALIGN_CENTER_VERTICAL);
	dngFloorIdCtrl = new wxTextCtrl(parent, wxID_ANY, "406");
	grid->Add(dngFloorIdCtrl, 1, wxEXPAND);

	box->Add(grid, 1, wxEXPAND | wxALL, 5);
	return box;
}

wxSizer* ProceduralMapDialog::CreateDungeonRoomsSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, parent, "Rooms & Corridors");
	wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
	grid->AddGrowableCol(1);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Room Count:"), 0, wxALIGN_CENTER_VERTICAL);
	dngRoomCountCtrl = new wxSpinCtrl(parent, wxID_ANY, "15", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 100, 15);
	grid->Add(dngRoomCountCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Min Room Size:"), 0, wxALIGN_CENTER_VERTICAL);
	dngMinRoomSizeCtrl = new wxSpinCtrl(parent, wxID_ANY, "5", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 3, 20, 5);
	grid->Add(dngMinRoomSizeCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Max Room Size:"), 0, wxALIGN_CENTER_VERTICAL);
	dngMaxRoomSizeCtrl = new wxSpinCtrl(parent, wxID_ANY, "12", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 3, 30, 12);
	grid->Add(dngMaxRoomSizeCtrl, 1, wxEXPAND);

	grid->Add(new wxStaticText(parent, wxID_ANY, "Corridor Width:"), 0, wxALIGN_CENTER_VERTICAL);
	dngCorridorWidthCtrl = new wxSpinCtrl(parent, wxID_ANY, "2", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 5, 2);
	grid->Add(dngCorridorWidthCtrl, 1, wxEXPAND);

	box->Add(grid, 1, wxEXPAND | wxALL, 5);
	return box;
}

wxSizer* ProceduralMapDialog::CreateDungeonCavesSection(wxWindow* parent) {
	wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, parent, "Caves");

	dngGenerateCavesCheck = new wxCheckBox(parent, wxID_ANY, "Generate Natural Caves");
	dngGenerateCavesCheck->SetValue(true);
	box->Add(dngGenerateCavesCheck, 0, wxALL, 5);
	
	return box;
}

void ProceduralMapDialog::SetDefaults() {
	// Generate random seed
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(100000, 999999);

	std::stringstream ss;
	ss << "island_" << dis(gen);
	seedCtrl->SetValue(ss.str());

    // Dungeon seed
    std::stringstream ss2;
    ss2 << "dungeon_" << dis(gen);
    dngSeedCtrl->SetValue(ss2.str());
}

IslandConfig ProceduralMapDialog::GetIslandConfiguration() const {
	IslandConfig config;

	// Map size (not part of IslandConfig, handled separately)
	// Will be passed to generateIslandMap()

	// Tile IDs
	long waterIdLong, groundIdLong;
	waterIdCtrl->GetValue().ToLong(&waterIdLong);
	groundIdCtrl->GetValue().ToLong(&groundIdLong);
	config.water_id = static_cast<uint16_t>(waterIdLong);
	config.ground_id = static_cast<uint16_t>(groundIdLong);

	// Island shape
	config.island_size = islandSizeSlider->GetValue() / 100.0;
	config.island_falloff = falloffSlider->GetValue() / 10.0;
	config.island_threshold = (thresholdSlider->GetValue() - 50) / 50.0; // [-1.0, 1.0]

	// Noise
	double scale, persistence, lacunarity;
	noiseScaleCtrl->GetValue().ToDouble(&scale);
	persistenceCtrl->GetValue().ToDouble(&persistence);
	lacunarityCtrl->GetValue().ToDouble(&lacunarity);

	config.noise_scale = scale;
	config.noise_octaves = octavesCtrl->GetValue();
	config.noise_persistence = persistence;
	config.noise_lacunarity = lacunarity;

	// Cleanup
	config.enable_cleanup = enableCleanupCheck->GetValue();
	config.min_land_patch_size = minPatchSizeCtrl->GetValue();
	config.max_water_hole_size = maxHoleSizeCtrl->GetValue();
	config.smoothing_passes = smoothingPassesCtrl->GetValue();

	return config;
}

DungeonConfig ProceduralMapDialog::GetDungeonConfiguration() const {
	DungeonConfig config;
	
	long wallId, floorId;
	dngWallIdCtrl->GetValue().ToLong(&wallId);
	dngFloorIdCtrl->GetValue().ToLong(&floorId);
	
	config.wall_id = static_cast<uint16_t>(wallId);
	config.floor_id = static_cast<uint16_t>(floorId);
	config.room_count = dngRoomCountCtrl->GetValue();
	config.min_room_size = dngMinRoomSizeCtrl->GetValue();
	config.max_room_size = dngMaxRoomSizeCtrl->GetValue();
	config.corridor_width = dngCorridorWidthCtrl->GetValue();
	config.generate_caves = dngGenerateCavesCheck->GetValue();
	
	return config;
}

void ProceduralMapDialog::OnGenerate(wxCommandEvent& event) {

	int page = notebook->GetSelection();
	MapGenerator gen;
	
	gen.setProgressCallback([this](int progress, int total) -> bool {
		wxYield(); // Allow UI to update
		return true;
	});

	// Calculate Start Position based on current view center
	int startX = 0;
	int startY = 0;
	
	if (MapTab* currentTab = g_gui.GetCurrentMapTab()) {
		Position center = currentTab->GetScreenCenterPosition();
		startX = center.x; // We will center it later by subtracting width/2
		startY = center.y;
	} else {
		// Fallback to map center if for some reason no tab is found (shouldn't happen)
		startX = editor.getMap().getWidth() / 2;
		startY = editor.getMap().getHeight() / 2;
	}

	bool success = false;

	if (page == 0) { // Island
		IslandConfig config = GetIslandConfiguration();
		int width = widthCtrl->GetValue();
		int height = heightCtrl->GetValue();
		std::string seed = seedCtrl->GetValue().ToStdString();
		
		// Center the map around the start position
		int mkX = startX - (width / 2);
		int mkY = startY - (height / 2);

		success = gen.generateIslandMap(&editor.getMap(), config, width, height, seed, mkX, mkY);
	} else { // Dungeon
		DungeonConfig config = GetDungeonConfiguration();
		int width = dngWidthCtrl->GetValue();
		int height = dngHeightCtrl->GetValue();
		std::string seed = dngSeedCtrl->GetValue().ToStdString();
		
		int mkX = startX - (width / 2);
		int mkY = startY - (height / 2);

		success = gen.generateDungeonMap(&editor.getMap(), config, width, height, seed, mkX, mkY);
	}
	
	if (success) {
		wxMessageBox("Map generated successfully!", "Success", wxOK | wxICON_INFORMATION, this);
		EndModal(wxID_OK);
	} else {
		wxMessageBox("Map generation was cancelled or failed.", "Cancelled", wxOK | wxICON_WARNING, this);
	}
}

void ProceduralMapDialog::OnCancel(wxCommandEvent& event) {
	EndModal(wxID_CANCEL);
}

void ProceduralMapDialog::OnRandomizeSeed(wxCommandEvent& event) {
	SetDefaults();
}

void ProceduralMapDialog::OnToggleTransparency(wxCommandEvent& event) {
	if (transparencyBtn->GetValue()) {
		SetTransparent(180); // ~70% opacity
	} else {
		SetTransparent(255); // Opaque
	}
}
