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
#include "border_editor_window.h"
#include "browse_tile_window.h"
#include "find_item_window.h"
#include "common_windows.h"
#include "graphics.h"
#include "gui.h"
#include "artprovider.h"
#include "items.h"
#include "brush.h"
#include "ground_brush.h"
#include <wx/sizer.h>
#include <wx/gbsizer.h>
#include <wx/statline.h>
#include <wx/tglbtn.h>
#include <wx/dcbuffer.h>
#include <wx/filename.h>
#include <wx/filepicker.h>
#include <pugixml.hpp>

#define BORDER_GRID_SIZE 32
#define BORDER_PREVIEW_SIZE 192
#define BORDER_GRID_CELL_SIZE 32
#define ID_BORDER_GRID_SELECT wxID_HIGHEST + 1
#define ID_GROUND_ITEM_LIST wxID_HIGHEST + 2
#define ID_EXISTING_BORDERS wxID_HIGHEST + 3

// Utility functions for edge string/position conversion
BorderEdgePosition edgeStringToPosition(const std::string& edgeStr) {
    if (edgeStr == "n") return EDGE_N;
    if (edgeStr == "e") return EDGE_E;
    if (edgeStr == "s") return EDGE_S;
    if (edgeStr == "w") return EDGE_W;
    if (edgeStr == "cnw") return EDGE_CNW;
    if (edgeStr == "cne") return EDGE_CNE;
    if (edgeStr == "cse") return EDGE_CSE;
    if (edgeStr == "csw") return EDGE_CSW;
    if (edgeStr == "dnw") return EDGE_DNW;
    if (edgeStr == "dne") return EDGE_DNE;
    if (edgeStr == "dse") return EDGE_DSE;
    if (edgeStr == "dsw") return EDGE_DSW;
    return EDGE_NONE;
}

std::string edgePositionToString(BorderEdgePosition pos) {
    switch (pos) {
        case EDGE_N: return "n";
        case EDGE_E: return "e";
        case EDGE_S: return "s";
        case EDGE_W: return "w";
        case EDGE_CNW: return "cnw";
        case EDGE_CNE: return "cne";
        case EDGE_CSE: return "cse";
        case EDGE_CSW: return "csw";
        case EDGE_DNW: return "dnw";
        case EDGE_DNE: return "dne";
        case EDGE_DSE: return "dse";
        case EDGE_DSW: return "dsw";
        default: return "";
    }
}

// Helper function to get the borders.xml file path
wxString GetBordersFilePath() {
    return g_gui.GetDataDirectory() + "materials" + wxFileName::GetPathSeparator() +
           "borders" + wxFileName::GetPathSeparator() + "borders.xml";
}

// Helper function to get the grounds.xml file path
wxString GetGroundsFilePath() {
    return g_gui.GetDataDirectory() + "materials" + wxFileName::GetPathSeparator() +
           "brushs" + wxFileName::GetPathSeparator() + "grounds.xml";
}

// Helper function to get the tilesets.xml file path
wxString GetTilesetsFilePath() {
    return g_gui.GetDataDirectory() + "materials" + wxFileName::GetPathSeparator() + "tilesets.xml";
}

// Add a helper function at the top of the file to get item ID from brush
uint16_t GetItemIDFromBrush(Brush* brush) {
    if (!brush) {
        wxLogDebug("GetItemIDFromBrush: Brush is null");
        return 0;
    }
    
    uint16_t id = 0;
    
    wxLogDebug("GetItemIDFromBrush: Checking brush type: %s", wxString(brush->getName()).c_str());
    
    // First prioritize RAW brush - this is the most direct approach
    if (brush->isRaw()) {
        RAWBrush* rawBrush = brush->asRaw();
        if (rawBrush) {
            id = rawBrush->getItemID();
            wxLogDebug("GetItemIDFromBrush: Found RAW brush ID: %d", id);
            if (id > 0) {
                return id;
            }
        }
    } 
    
    // Then try getID which sometimes works directly
    id = brush->getID();
    if (id > 0) {
        wxLogDebug("GetItemIDFromBrush: Got ID from brush->getID(): %d", id);
        return id;
    }
    
    // Try getLookID which works for most other brush types
    id = brush->getLookID();
    if (id > 0) {
        wxLogDebug("GetItemIDFromBrush: Got ID from getLookID(): %d", id);
        return id;
    }
    
    // Try specific brush type methods - when all else fails
    if (brush->isGround()) {
        wxLogDebug("GetItemIDFromBrush: Detected Ground brush");
        GroundBrush* groundBrush = brush->asGround();
        if (groundBrush) {
            // For ground brush, id is usually the server_lookid from grounds.xml
            // Try to find something else
            wxLogDebug("GetItemIDFromBrush: Failed to get ID for Ground brush");
        }
    }
    else if (brush->isWall()) {
        wxLogDebug("GetItemIDFromBrush: Detected Wall brush");
        WallBrush* wallBrush = brush->asWall();
        if (wallBrush) {
            wxLogDebug("GetItemIDFromBrush: Failed to get ID for Wall brush");
        }
    }
    else if (brush->isDoodad()) {
        wxLogDebug("GetItemIDFromBrush: Detected Doodad brush");
        DoodadBrush* doodadBrush = brush->asDoodad();
        if (doodadBrush) {
            wxLogDebug("GetItemIDFromBrush: Failed to get ID for Doodad brush");
        }
    }
    
    if (id == 0) {
        wxLogDebug("GetItemIDFromBrush: Failed to get item ID from brush %s", wxString(brush->getName()).c_str());
    }
    
    return id;
}

// Event table for BorderEditorDialog
BEGIN_EVENT_TABLE(BorderEditorDialog, wxDialog)
    EVT_BUTTON(wxID_CLEAR, BorderEditorDialog::OnClear)
    EVT_BUTTON(wxID_SAVE, BorderEditorDialog::OnSave)
    EVT_BUTTON(wxID_CANCEL, BorderEditorDialog::OnClose)
    // Removed IDs for FIND, ADD
    EVT_COMBOBOX(ID_EXISTING_BORDERS, BorderEditorDialog::OnLoadBorder)
    EVT_NOTEBOOK_PAGE_CHANGED(wxID_ANY, BorderEditorDialog::OnPageChanged)
    EVT_BUTTON(wxID_ADD + 100, BorderEditorDialog::OnAddGroundItem)
    EVT_BUTTON(wxID_REMOVE, BorderEditorDialog::OnRemoveGroundItem)
    EVT_BUTTON(wxID_FIND + 100, BorderEditorDialog::OnGroundBrowse)
    EVT_COMBOBOX(wxID_ANY + 100, BorderEditorDialog::OnLoadGroundBrush)
END_EVENT_TABLE()

// Event table for BorderItemButton
BEGIN_EVENT_TABLE(BorderItemButton, wxButton)
    EVT_PAINT(BorderItemButton::OnPaint)
END_EVENT_TABLE()

// Event table for BorderGridPanel
BEGIN_EVENT_TABLE(BorderGridPanel, wxPanel)
    EVT_PAINT(BorderGridPanel::OnPaint)
    EVT_LEFT_UP(BorderGridPanel::OnMouseClick)
    EVT_LEFT_DOWN(BorderGridPanel::OnMouseDown)
    EVT_MOTION(BorderGridPanel::OnMouseMove)
    EVT_RIGHT_DOWN(BorderGridPanel::OnRightDown)
    EVT_LEFT_DCLICK(BorderGridPanel::OnLeftDClick)
    EVT_LEAVE_WINDOW(BorderGridPanel::OnLeaveWindow)
END_EVENT_TABLE()

// Event table for BorderPreviewPanel
BEGIN_EVENT_TABLE(BorderPreviewPanel, wxPanel)
    EVT_PAINT(BorderPreviewPanel::OnPaint)
END_EVENT_TABLE()

BorderEditorDialog::BorderEditorDialog(wxWindow* parent, const wxString& title) :
    wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(650, 520),
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    m_nextBorderId(1),
    m_activeTab(0) {
    
    CreateGUIControls();
    LoadExistingBorders();
    LoadExistingGroundBrushes();
    LoadTilesets();  // Load available tilesets
    
    // Set ID to next available ID
    m_idCtrl->SetValue(m_nextBorderId);
    
    // Center the dialog
    CenterOnParent();
}

BorderEditorDialog::~BorderEditorDialog() {
    // Nothing to destroy manually
}

void BorderEditorDialog::CreateGUIControls() {
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    
    // Header Section (Name & ID) - Clean row without StaticBox
    wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Name
    wxBoxSizer* nameSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* nameLabel = new wxStaticText(this, wxID_ANY, "Name:");
    nameLabel->SetFont(nameLabel->GetFont().Bold());
    nameSizer->Add(nameLabel, 0, wxBOTTOM, 2);
    m_nameCtrl = new wxTextCtrl(this, wxID_ANY);
    m_nameCtrl->SetToolTip("Descriptive name for the border/brush");
    nameSizer->Add(m_nameCtrl, 0, wxEXPAND);
    headerSizer->Add(nameSizer, 1, wxEXPAND | wxRIGHT, 15);
    
    // ID
    wxBoxSizer* idSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* idLabel = new wxStaticText(this, wxID_ANY, "ID:");
    idLabel->SetFont(idLabel->GetFont().Bold());
    idSizer->Add(idLabel, 0, wxBOTTOM, 2);
    m_idCtrl = new wxSpinCtrl(this, wxID_ANY, "1", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 1000);
    m_idCtrl->SetToolTip("Unique identifier for this border/brush");
    idSizer->Add(m_idCtrl, 0, wxEXPAND);
    headerSizer->Add(idSizer, 0, wxEXPAND);
    
    topSizer->Add(headerSizer, 0, wxEXPAND | wxALL, 10);
    
    // Create notebook with Border and Ground tabs
    m_notebook = new wxNotebook(this, wxID_ANY);
    
    // ========== BORDER TAB ==========
    m_borderPanel = new wxPanel(m_notebook);
    wxBoxSizer* borderSizer = new wxBoxSizer(wxVERTICAL);
    
    // Top Controls (Group, Type, Load) - Single row
    wxBoxSizer* borderTopRow = new wxBoxSizer(wxHORIZONTAL);
    
    // Group
    wxBoxSizer* groupSizer = new wxBoxSizer(wxVERTICAL);
    groupSizer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Group:"), 0, wxBOTTOM, 2);
    m_groupCtrl = new wxSpinCtrl(m_borderPanel, wxID_ANY, "0", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 1000);
    m_groupCtrl->SetToolTip("Optional group identifier");
    groupSizer->Add(m_groupCtrl, 0, wxEXPAND);
    borderTopRow->Add(groupSizer, 0, wxRIGHT, 15);
    
    // Type Options
    wxBoxSizer* typeSizer = new wxBoxSizer(wxVERTICAL);
    typeSizer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Type:"), 0, wxBOTTOM, 4);
    wxBoxSizer* checkSizer = new wxBoxSizer(wxHORIZONTAL);
    m_isOptionalCheck = new wxCheckBox(m_borderPanel, wxID_ANY, "Optional");
    m_isGroundCheck = new wxCheckBox(m_borderPanel, wxID_ANY, "Ground");
    checkSizer->Add(m_isOptionalCheck, 0, wxRIGHT, 10);
    checkSizer->Add(m_isGroundCheck, 0);
    typeSizer->Add(checkSizer, 0);
    borderTopRow->Add(typeSizer, 0, wxRIGHT, 20);
    
    // Load Existing (Spacer pushes it to right, or just keep left)
    wxBoxSizer* loadSizer = new wxBoxSizer(wxVERTICAL);
    loadSizer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Load Existing:"), 0, wxBOTTOM, 2);
    m_existingBordersCombo = new wxComboBox(m_borderPanel, ID_EXISTING_BORDERS, "", wxDefaultPosition, wxSize(200, -1), 0, nullptr, wxCB_READONLY | wxCB_DROPDOWN);
    loadSizer->Add(m_existingBordersCombo, 0, wxEXPAND);
    borderTopRow->Add(loadSizer, 0);
    
    borderSizer->Add(borderTopRow, 0, wxEXPAND | wxALL, 10);
    borderSizer->Add(new wxStaticLine(m_borderPanel), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    
    // Main Content (Grid + Preview)
    wxBoxSizer* contentSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Left: Grid
    wxBoxSizer* gridContainer = new wxBoxSizer(wxVERTICAL);
    gridContainer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Border Pattern (Click to Set, Right-Click to Clear)"), 0, wxBOTTOM, 5);
    
    m_gridPanel = new BorderGridPanel(m_borderPanel);
    gridContainer->Add(m_gridPanel, 1, wxEXPAND | wxBOTTOM, 10);
    
    // Item Selection Controls (Under Grid)
    wxBoxSizer* itemSelectSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Instructions
    wxStaticText* instructions = new wxStaticText(m_borderPanel, wxID_ANY, "Left-click a cell to set item. Right-click to clear.");
    instructions->SetForegroundColour(wxColour(100, 100, 100));
    itemSelectSizer->Add(instructions, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    gridContainer->Add(itemSelectSizer, 0, wxEXPAND);
    
    contentSizer->Add(gridContainer, 1, wxEXPAND | wxALL, 10);
    
    // Separator
    contentSizer->Add(new wxStaticLine(m_borderPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL), 0, wxEXPAND | wxTOP | wxBOTTOM, 10);
    
    // Right: Preview
    wxBoxSizer* previewContainer = new wxBoxSizer(wxVERTICAL);
    previewContainer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Preview"), 0, wxBOTTOM, 5);
    
    m_previewPanel = new BorderPreviewPanel(m_borderPanel);
    previewContainer->Add(m_previewPanel, 1, wxEXPAND);
    
    contentSizer->Add(previewContainer, 0, wxEXPAND | wxALL, 10);
    
    borderSizer->Add(contentSizer, 1, wxEXPAND);
    
    // Bottom Buttons
    wxBoxSizer* bottomBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    bottomBtnSizer->Add(new wxButton(m_borderPanel, wxID_CLEAR, "Clear All"), 0, wxRIGHT, 10);
    bottomBtnSizer->AddStretchSpacer();
    
    wxButton* saveBtn = new wxButton(m_borderPanel, wxID_SAVE, "Save Border");
    saveBtn->SetFont(saveBtn->GetFont().Bold());
    bottomBtnSizer->Add(saveBtn, 0, wxRIGHT, 10);
    bottomBtnSizer->Add(new wxButton(m_borderPanel, wxID_CLOSE, "Close"), 0);
    
    borderSizer->Add(new wxStaticLine(m_borderPanel), 0, wxEXPAND | wxALL, 0);
    borderSizer->Add(bottomBtnSizer, 0, wxEXPAND | wxALL, 10);
    
    m_borderPanel->SetSizer(borderSizer);
    
    // ========== GROUND TAB ==========
    m_groundPanel = new wxPanel(m_notebook);
    wxBoxSizer* groundSizer = new wxBoxSizer(wxVERTICAL);
    
    // Top Controls
    wxBoxSizer* groundTopRow = new wxBoxSizer(wxHORIZONTAL);
    
    // Tileset
    wxBoxSizer* tilesetBox = new wxBoxSizer(wxVERTICAL);
    tilesetBox->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Tileset:"), 0, wxBOTTOM, 2);
    m_tilesetChoice = new wxChoice(m_groundPanel, wxID_ANY, wxDefaultPosition, wxSize(150, -1));
    tilesetBox->Add(m_tilesetChoice, 0, wxEXPAND);
    groundTopRow->Add(tilesetBox, 0, wxRIGHT, 15);
    
    // Server ID
    wxBoxSizer* servIdBox = new wxBoxSizer(wxVERTICAL);
    servIdBox->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Server ID:"), 0, wxBOTTOM, 2);
    m_serverLookIdCtrl = new wxSpinCtrl(m_groundPanel, wxID_ANY, "0", wxDefaultPosition, wxSize(80, -1));
    servIdBox->Add(m_serverLookIdCtrl, 0, wxEXPAND);
    groundTopRow->Add(servIdBox, 0, wxRIGHT, 15);
    
    // Z-Order
    wxBoxSizer* zBox = new wxBoxSizer(wxVERTICAL);
    zBox->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Z-Order:"), 0, wxBOTTOM, 2);
    m_zOrderCtrl = new wxSpinCtrl(m_groundPanel, wxID_ANY, "0", wxDefaultPosition, wxSize(60, -1));
    zBox->Add(m_zOrderCtrl, 0, wxEXPAND);
    groundTopRow->Add(zBox, 0, wxRIGHT, 15);
    
    // Load
    wxBoxSizer* gLoadBox = new wxBoxSizer(wxVERTICAL);
    gLoadBox->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Load Existing:"), 0, wxBOTTOM, 2);
    m_existingGroundBrushesCombo = new wxComboBox(m_groundPanel, wxID_ANY + 100, "", wxDefaultPosition, wxSize(200, -1), 0, nullptr, wxCB_READONLY | wxCB_DROPDOWN);
    gLoadBox->Add(m_existingGroundBrushesCombo, 0, wxEXPAND);
    groundTopRow->Add(gLoadBox, 0);
    
    groundSizer->Add(groundTopRow, 0, wxEXPAND | wxALL, 10);
    groundSizer->Add(new wxStaticLine(m_groundPanel), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    
    // Random Items List
    wxBoxSizer* listSection = new wxBoxSizer(wxVERTICAL);
    listSection->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Random Items (Variations)"), 0, wxTOP | wxBOTTOM, 5);
    
    m_groundItemsList = new wxListBox(m_groundPanel, ID_GROUND_ITEM_LIST, wxDefaultPosition, wxSize(-1, 120));
    listSection->Add(m_groundItemsList, 1, wxEXPAND | wxBOTTOM, 5);
    
    // List Controls
    wxBoxSizer* listControls = new wxBoxSizer(wxHORIZONTAL);
    
    m_groundItemIdCtrl = new wxSpinCtrl(m_groundPanel, wxID_ANY, "0", wxDefaultPosition, wxSize(80, -1));
    listControls->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Item:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    listControls->Add(m_groundItemIdCtrl, 0, wxRIGHT, 5);
    
    listControls->Add(new wxButton(m_groundPanel, wxID_FIND + 100, "Browse...", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0, wxRIGHT, 15);
    
    m_groundItemChanceCtrl = new wxSpinCtrl(m_groundPanel, wxID_ANY, "10", wxDefaultPosition, wxSize(60, -1));
    listControls->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Chance:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    listControls->Add(m_groundItemChanceCtrl, 0, wxRIGHT, 15);
    
    listControls->Add(new wxButton(m_groundPanel, wxID_ADD + 100, "Add"), 0, wxRIGHT, 5);
    listControls->Add(new wxButton(m_groundPanel, wxID_REMOVE, "Remove"), 0);
    
    listSection->Add(listControls, 0, wxEXPAND);
    
    groundSizer->Add(listSection, 0, wxEXPAND | wxALL, 10);
    groundSizer->Add(new wxStaticLine(m_groundPanel), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    
    // Border Settings for Ground
    wxBoxSizer* borderSettings = new wxBoxSizer(wxVERTICAL);
    borderSettings->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Border Integration"), 0, wxTOP | wxBOTTOM, 5);
    
    wxBoxSizer* bsRow = new wxBoxSizer(wxHORIZONTAL);
    
    wxArrayString alignOptions;
    alignOptions.Add("Outer");
    alignOptions.Add("Inner");
    m_borderAlignmentChoice = new wxChoice(m_groundPanel, wxID_ANY, wxDefaultPosition, wxSize(100, -1), alignOptions);
    m_borderAlignmentChoice->SetSelection(0);
    bsRow->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Alignment:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    bsRow->Add(m_borderAlignmentChoice, 0, wxRIGHT, 20);
    
    m_includeToNoneCheck = new wxCheckBox(m_groundPanel, wxID_ANY, "To None");
    m_includeToNoneCheck->SetValue(true);
    bsRow->Add(m_includeToNoneCheck, 0, wxRIGHT, 10);
    
    m_includeInnerCheck = new wxCheckBox(m_groundPanel, wxID_ANY, "Inner Border");
    bsRow->Add(m_includeInnerCheck, 0);
    
    borderSettings->Add(bsRow, 0, wxEXPAND | wxBOTTOM, 5);
    
    wxStaticText* hint = new wxStaticText(m_groundPanel, wxID_ANY, "Note: Configure the border pattern in the 'Border' tab.");
    hint->SetForegroundColour(wxColour(100, 100, 100));
    borderSettings->Add(hint, 0);
    
    groundSizer->Add(borderSettings, 0, wxEXPAND | wxALL, 10);
    
    groundSizer->AddStretchSpacer();
    
    // Bottom Buttons (Copy from Border Tab style)
    wxBoxSizer* gBottomBtn = new wxBoxSizer(wxHORIZONTAL);
    gBottomBtn->Add(new wxButton(m_groundPanel, wxID_CLEAR, "Clear All"), 0, wxRIGHT, 10);
    gBottomBtn->AddStretchSpacer();
    
    wxButton* gSaveBtn = new wxButton(m_groundPanel, wxID_SAVE, "Save Brush");
    gSaveBtn->SetFont(gSaveBtn->GetFont().Bold());
    gBottomBtn->Add(gSaveBtn, 0, wxRIGHT, 10);
    gBottomBtn->Add(new wxButton(m_groundPanel, wxID_CLOSE, "Close"), 0);
    
    groundSizer->Add(new wxStaticLine(m_groundPanel), 0, wxEXPAND | wxALL, 0);
    groundSizer->Add(gBottomBtn, 0, wxEXPAND | wxALL, 10);
    
    m_groundPanel->SetSizer(groundSizer);
    
    m_notebook->AddPage(m_borderPanel, "Border Loop");
    m_notebook->AddPage(m_groundPanel, "Ground Brush");
    
    topSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 5);
    
    SetSizer(topSizer);
    Layout();
}

void BorderEditorDialog::LoadExistingBorders() {
    // Clear the combobox
    m_existingBordersCombo->Clear();
    
    // Add an empty entry
    m_existingBordersCombo->Append("<Create New>");
    m_existingBordersCombo->SetSelection(0);
    
    // Find the borders.xml file in the data/materials/borders directory
    wxString dataDir = g_gui.GetDataDirectory();
    wxString bordersFile = dataDir + "materials" + wxFileName::GetPathSeparator() +
                          "borders" + wxFileName::GetPathSeparator() + "borders.xml";
    
    if (!wxFileExists(bordersFile)) {
        wxMessageBox("Cannot find borders.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(bordersFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load borders.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid borders.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    int highestId = 0;
    
    // Parse all borders
    for (pugi::xml_node borderNode = materials.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
        pugi::xml_attribute idAttr = borderNode.attribute("id");
        if (!idAttr) continue;
        
        int id = idAttr.as_int();
        if (id > highestId) {
            highestId = id;
        }
        
        // Get the comment node before this border for its description
        std::string description;
        pugi::xml_node commentNode = borderNode.previous_sibling();
        if (commentNode && commentNode.type() == pugi::node_comment) {
            description = commentNode.value();
            // Extract the actual comment text by removing XML comment markers
            description = description.c_str(); // Ensure we have a clean copy
            
            // Trim leading and trailing whitespace first
            description.erase(0, description.find_first_not_of(" \t\n\r"));
            description.erase(description.find_last_not_of(" \t\n\r") + 1);
            
            // Remove leading "<!--" if present
            if (description.substr(0, 4) == "<!--") {
                description.erase(0, 4);
                // Trim whitespace after removing the marker
                description.erase(0, description.find_first_not_of(" \t\n\r"));
            }
            
            // Remove trailing "-->" if present
            if (description.length() >= 3 && description.substr(description.length() - 3) == "-->") {
                description.erase(description.length() - 3);
                // Trim whitespace after removing the marker
                description.erase(description.find_last_not_of(" \t\n\r") + 1);
            }
        }
        
        // Add to combobox
        wxString label = wxString::Format("Border %d", id);
        if (!description.empty()) {
            label += wxString::Format(" (%s)", wxstr(description));
        }
        
        m_existingBordersCombo->Append(label, new wxStringClientData(wxString::Format("%d", id)));
    }
    
    // Set the next border ID to one higher than the highest found
    m_nextBorderId = highestId + 1;
    m_idCtrl->SetValue(m_nextBorderId);
}

void BorderEditorDialog::OnLoadBorder(wxCommandEvent& event) {
    int selection = m_existingBordersCombo->GetSelection();
    if (selection <= 0) {
        // Selected "Create New" or nothing
        ClearItems();
        return;
    }
    
    wxStringClientData* data = static_cast<wxStringClientData*>(m_existingBordersCombo->GetClientObject(selection));
    if (!data) return;
    
    int borderId = wxAtoi(data->GetData());
    
    // Find the borders.xml file in the data/materials/borders directory
    wxString dataDir = g_gui.GetDataDirectory();
    wxString bordersFile = dataDir + "materials" + wxFileName::GetPathSeparator() +
                          "borders" + wxFileName::GetPathSeparator() + "borders.xml";

    if (!wxFileExists(bordersFile)) {
        wxMessageBox("Cannot find borders.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }

    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(bordersFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load borders.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    // Clear existing items
    ClearItems();
    
    // Look for the border with the specified ID
    pugi::xml_node materials = doc.child("materials");
    for (pugi::xml_node borderNode = materials.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
        pugi::xml_attribute idAttr = borderNode.attribute("id");
        if (!idAttr || idAttr.as_int() != borderId) continue;
        
        // Set the ID in the control
        m_idCtrl->SetValue(borderId);
        
        // Check for border type
        pugi::xml_attribute typeAttr = borderNode.attribute("type");
        if (typeAttr) {
            std::string type = typeAttr.as_string();
            m_isOptionalCheck->SetValue(type == "optional");
        } else {
            m_isOptionalCheck->SetValue(false);
        }
        
        // Check for border group
        pugi::xml_attribute groupAttr = borderNode.attribute("group");
        if (groupAttr) {
            m_groupCtrl->SetValue(groupAttr.as_int());
        } else {
            m_groupCtrl->SetValue(0);
        }
        
        // Get the comment node before this border for its description
        pugi::xml_node commentNode = borderNode.previous_sibling();
        if (commentNode && commentNode.type() == pugi::node_comment) {
            std::string description = commentNode.value();
            // Extract the actual comment text by removing XML comment markers
            description = description.c_str(); // Ensure we have a clean copy
            
            // Trim leading and trailing whitespace first
            description.erase(0, description.find_first_not_of(" \t\n\r"));
            description.erase(description.find_last_not_of(" \t\n\r") + 1);
            
            // Remove leading "<!--" if present
            if (description.substr(0, 4) == "<!--") {
                description.erase(0, 4);
                // Trim whitespace after removing the marker
                description.erase(0, description.find_first_not_of(" \t\n\r"));
            }
            
            // Remove trailing "-->" if present
            if (description.length() >= 3 && description.substr(description.length() - 3) == "-->") {
                description.erase(description.length() - 3);
                // Trim whitespace after removing the marker
                description.erase(description.find_last_not_of(" \t\n\r") + 1);
            }
            
            m_nameCtrl->SetValue(wxstr(description));
        } else {
            m_nameCtrl->SetValue("");
        }
        
        // Load all border items
        for (pugi::xml_node itemNode = borderNode.child("borderitem"); itemNode; itemNode = itemNode.next_sibling("borderitem")) {
            pugi::xml_attribute edgeAttr = itemNode.attribute("edge");
            pugi::xml_attribute itemAttr = itemNode.attribute("item");
            
            if (!edgeAttr || !itemAttr) continue;
            
            BorderEdgePosition pos = edgeStringToPosition(edgeAttr.as_string());
            uint16_t itemId = itemAttr.as_uint();
            
            if (pos != EDGE_NONE && itemId > 0) {
                m_borderItems.push_back(BorderItem(pos, itemId));
                m_gridPanel->SetItemId(pos, itemId);
            }
        }
        
        break;
    }
    
    // Update the preview
    UpdatePreview();
    // Keep selection
    m_existingBordersCombo->SetSelection(selection);
}


void BorderEditorDialog::OnClear(wxCommandEvent& event) {
    if (m_activeTab == 0) {
        // Border tab
    ClearItems();
    } else {
        // Ground tab
        ClearGroundItems();
    }
}

void BorderEditorDialog::ClearItems() {
    m_borderItems.clear();
    m_gridPanel->Clear();
    m_previewPanel->Clear();
    
    // Reset controls to defaults
    m_idCtrl->SetValue(m_nextBorderId);
    m_nameCtrl->SetValue("");
    m_isOptionalCheck->SetValue(false);
    m_isGroundCheck->SetValue(false);
    m_groupCtrl->SetValue(0);
    
    // Set combo selection to "Create New"
    m_existingBordersCombo->SetSelection(0);
}

void BorderEditorDialog::UpdatePreview() {
    m_previewPanel->SetBorderItems(m_borderItems);
    m_previewPanel->Refresh();
}

bool BorderEditorDialog::ValidateBorder() {
    // Check for empty name
    if (m_nameCtrl->GetValue().IsEmpty()) {
        wxMessageBox("Please enter a name for the border.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    if (m_borderItems.empty()) {
        wxMessageBox("The border must have at least one item.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    // Check that there are no duplicate positions
    std::set<BorderEdgePosition> positions;
    for (const BorderItem& item : m_borderItems) {
        if (positions.find(item.position) != positions.end()) {
            wxMessageBox("The border contains duplicate positions.", "Validation Error", wxICON_ERROR);
            return false;
        }
        positions.insert(item.position);
    }
    
    // Check for ID validity
    int id = m_idCtrl->GetValue();
    if (id <= 0) {
        wxMessageBox("Border ID must be greater than 0.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    return true;
}

void BorderEditorDialog::SaveBorder() {
    if (!ValidateBorder()) {
        return;
    }
    
    // Get the border properties
    int id = m_idCtrl->GetValue();
    wxString name = m_nameCtrl->GetValue();
    
    // Double check that we have a name (it's also checked in ValidateBorder)
    if (name.IsEmpty()) {
        wxMessageBox("You must provide a name for the border.", "Error", wxICON_ERROR);
        return;
    }
    
    bool isOptional = m_isOptionalCheck->GetValue();
    bool isGround = m_isGroundCheck->GetValue();
    int group = m_groupCtrl->GetValue();
    
    // Find the borders.xml file
    wxString bordersFile = GetBordersFilePath();

    if (!wxFileExists(bordersFile)) {
        wxMessageBox("Cannot find borders.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }

    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(bordersFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load borders.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid borders.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    // Check if a border with this ID already exists
    bool borderExists = false;
    pugi::xml_node existingBorder;
    
    for (pugi::xml_node borderNode = materials.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
        pugi::xml_attribute idAttr = borderNode.attribute("id");
        if (idAttr && idAttr.as_int() == id) {
            borderExists = true;
            existingBorder = borderNode;
            break;
        }
    }
    
    if (borderExists) {
        // Check if there's a comment node before the existing border
        pugi::xml_node commentNode = existingBorder.previous_sibling();
        bool hadComment = (commentNode && commentNode.type() == pugi::node_comment);
        
        // Ask for confirmation to overwrite
        if (wxMessageBox("A border with ID " + wxString::Format("%d", id) + " already exists. Do you want to overwrite it?", 
                        "Confirm Overwrite", wxYES_NO | wxICON_QUESTION) != wxYES) {
            return;
        }
        
        // If there was a comment node, remove it too
        if (hadComment) {
            materials.remove_child(commentNode);
        }
        
        // Remove the existing border
        materials.remove_child(existingBorder);
    }
    
 
    
    // Create the new border node
    pugi::xml_node borderNode = materials.append_child("border");
    borderNode.append_attribute("id").set_value(id);
    
    if (isOptional) {
        borderNode.append_attribute("type").set_value("optional");
    }
    
    if (isGround) {
        borderNode.append_attribute("ground").set_value("true");
    }
    
    if (group > 0) {
        borderNode.append_attribute("group").set_value(group);
    }
    
    // Add all border items
    for (const BorderItem& item : m_borderItems) {
        pugi::xml_node itemNode = borderNode.append_child("borderitem");
        itemNode.append_attribute("edge").set_value(edgePositionToString(item.position).c_str());
        itemNode.append_attribute("item").set_value(item.itemId);
    }
    
    // Save the file
    if (!doc.save_file(nstr(bordersFile).c_str())) {
        wxMessageBox("Failed to save changes to borders.xml", "Error", wxICON_ERROR);
        return;
    }
    
    wxMessageBox("Border saved successfully.", "Success", wxICON_INFORMATION);
    
    // Reload the existing borders list
    LoadExistingBorders();
}

void BorderEditorDialog::OnSave(wxCommandEvent& event) {
    if (m_activeTab == 0) {
        // Border tab
    SaveBorder();
    } else {
        // Ground tab
        SaveGroundBrush();
    }
}

void BorderEditorDialog::OnClose(wxCommandEvent& event) {
    Close();
}

void BorderEditorDialog::OnGridCellClicked(wxMouseEvent& event) {
    // This event is handled by the BorderGridPanel directly
    event.Skip();
}

void BorderEditorDialog::SetBorderItem(BorderEdgePosition pos, uint16_t itemId) {
    if (pos == EDGE_NONE) return;
    
    // Update or add the item
    bool updated = false;
    for (auto& item : m_borderItems) {
        if (item.position == pos) {
            item.itemId = itemId;
            updated = true;
            break;
        }
    }
    
    if (!updated) {
        m_borderItems.push_back(BorderItem(pos, itemId));
    }
    
    // Update visual components
    m_gridPanel->SetItemId(pos, itemId);
    
    // Update preview
    UpdatePreview();
}

void BorderEditorDialog::RemoveBorderItem(BorderEdgePosition pos) {
    if (pos == EDGE_NONE) return;
    
    // Remove from vector
    for (auto it = m_borderItems.begin(); it != m_borderItems.end(); ++it) {
        if (it->position == pos) {
            m_borderItems.erase(it);
            break;
        }
    }
    
    // Update visual components
    m_gridPanel->SetItemId(pos, 0); // 0 clears it in grid panel
    
    // Update preview
    UpdatePreview();
    
    // Clear item ID control if it currently matches the removed item?
    // Maybe unnecessary.
}

// ============================================================================
// BorderItemButton

BorderItemButton::BorderItemButton(wxWindow* parent, BorderEdgePosition position, wxWindowID id) :
    wxButton(parent, id, "", wxDefaultPosition, wxSize(32, 32)),
    m_itemId(0),
    m_position(position) {
    // Set up the button to show sprite graphics
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

BorderItemButton::~BorderItemButton() {
    // No need to destroy anything manually
}

void BorderItemButton::SetItemId(uint16_t id) {
    m_itemId = id;
    Refresh();
}

void BorderItemButton::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    
    // Draw the button background
    wxRect rect = GetClientRect();
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(rect);
    
    // Draw the item sprite if available
    if (m_itemId > 0) {
        const ItemType& type = g_items.getItemType(m_itemId);
        if (type.id != 0) {
            Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
            if (sprite) {
                sprite->DrawTo(&dc, SPRITE_SIZE_32x32, 0, 0, rect.GetWidth(), rect.GetHeight());
            }
        }
    }
    
    // Draw a border around the button if it's focused
    if (HasFocus()) {
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(rect);
    }
}

// ============================================================================
// BorderGridPanel

// ============================================================================
// BorderGridPanel

BorderGridPanel::BorderGridPanel(wxWindow* parent, wxWindowID id) :
    wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN)
{
    m_items.clear();
    m_selectedPosition = EDGE_NONE;
    m_hoveredPosition = EDGE_NONE;
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    // Ensure we have enough space for 3 sections of 128px + padding
    SetMinSize(wxSize(450, 160));
}

BorderGridPanel::~BorderGridPanel() {
    // Nothing to destroy manually
}

void BorderGridPanel::SetItemId(BorderEdgePosition pos, uint16_t itemId) {
    if (pos >= 0 && pos < EDGE_COUNT) {
        m_items[pos] = itemId;
        Refresh();
    }
}

uint16_t BorderGridPanel::GetItemId(BorderEdgePosition pos) const {
    auto it = m_items.find(pos);
    if (it != m_items.end()) {
        return it->second;
    }
    return 0;
}

void BorderGridPanel::Clear() {
    for (auto& item : m_items) {
        item.second = 0;
    }
    Refresh();
}

void BorderGridPanel::SetSelectedPosition(BorderEdgePosition pos) {
    m_selectedPosition = pos;
    Refresh();
}

void BorderGridPanel::OnPaint(wxPaintEvent& event) {
    wxAutoBufferedPaintDC dc(this);
    
    // Draw the panel background
    wxRect rect = GetClientRect();
    // Use system background for a cleaner, minimalist look (or white)
    dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)));
    dc.Clear();
    
    // Calculate sizes for the three grid sections
    const int total_width = rect.GetWidth();
    const int total_height = rect.GetHeight();
    
    // Reduced cell size for tighter 32x32 fit (48px provides 8px padding)
    const int grid_cell_size = 48;
    
    // Ensure we have positive offsets
    
    // Section 1: Normal directions (N, E, S, W) - 2x2 grid
    const int normal_grid_size = 2;
    const int normal_grid_width = normal_grid_size * grid_cell_size;
    const int normal_grid_height = normal_grid_size * grid_cell_size;
    
    // Center vertically
    const int start_y = (total_height - normal_grid_height) / 2;
    // Distribute horizontally
    const int spacing = (total_width - (3 * normal_grid_width)) / 4;
    const int safe_spacing = std::max(10, spacing);
    
    const int normal_offset_x = safe_spacing;
    const int normal_offset_y = start_y;
    
    // Section 2: Corner positions
    const int corner_grid_size = 2;
    const int corner_grid_width = corner_grid_size * grid_cell_size;
    const int corner_offset_x = normal_offset_x + normal_grid_width + safe_spacing;
    const int corner_offset_y = start_y;
    
    // Section 3: Diagonal positions
    const int diag_grid_size = 2;
    const int diag_grid_width = diag_grid_size * grid_cell_size;
    const int diag_offset_x = corner_offset_x + corner_grid_width + safe_spacing;
    const int diag_offset_y = start_y;
    
    // Section labels
    dc.SetTextForeground(wxColour(60, 60, 60));
    dc.SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    
    // Draw text with safety check for Y
    int text_y = normal_offset_y - 20;
    if (text_y < 2) text_y = 2;

    dc.DrawText("Sides", normal_offset_x + (normal_grid_width - dc.GetTextExtent("Sides").GetWidth()) / 2, text_y);
    dc.DrawText("Corners", corner_offset_x + (corner_grid_width - dc.GetTextExtent("Corners").GetWidth()) / 2, text_y);
    dc.DrawText("Diagonals", diag_offset_x + (diag_grid_width - dc.GetTextExtent("Diagonals").GetWidth()) / 2, text_y);
    
    // Helper function to draw a grid background
    auto drawGridBg = [&](int offsetX, int offsetY, int gridSize, int cellSize) {
        dc.SetPen(wxPen(wxColour(180, 180, 180)));
        dc.SetBrush(*wxWHITE_BRUSH); // Keep cells white for contrast
        dc.DrawRectangle(offsetX, offsetY, gridSize * cellSize, gridSize * cellSize);
        
        // Inner lines
        for (int i = 1; i < gridSize; i++) {
             dc.DrawLine(offsetX + i * cellSize, offsetY, offsetX + i * cellSize, offsetY + gridSize * cellSize);
             dc.DrawLine(offsetX, offsetY + i * cellSize, offsetX + gridSize * cellSize, offsetY + i * cellSize);
        }
    };
    
    drawGridBg(normal_offset_x, normal_offset_y, normal_grid_size, grid_cell_size);
    drawGridBg(corner_offset_x, corner_offset_y, corner_grid_size, grid_cell_size);
    drawGridBg(diag_offset_x, diag_offset_y, diag_grid_size, grid_cell_size);
    
    // Set font for position labels
    dc.SetTextForeground(wxColour(100, 100, 100));
    dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    
    // Function to draw an item at a position
    auto drawItemAtPos = [&](BorderEdgePosition pos, int gridX, int gridY, int offsetX, int offsetY) {
        int x = offsetX + gridX * grid_cell_size;
        int y = offsetY + gridY * grid_cell_size;
        
        // Draw hover effect
        if (pos == m_hoveredPosition && pos != m_selectedPosition) {
             wxRect cellRect(x + 1, y + 1, grid_cell_size - 1, grid_cell_size - 1);
             dc.SetBrush(wxBrush(wxColour(240, 240, 255)));
             dc.SetPen(*wxTRANSPARENT_PEN);
             dc.DrawRectangle(cellRect);
        }

        // Highlight selected position -- more minimalist selection
        if (pos == m_selectedPosition) {
            dc.SetPen(wxPen(wxColour(0, 120, 215), 2));
            dc.SetBrush(wxBrush(wxColour(0, 120, 215, 30)));
            dc.DrawRectangle(x + 1, y + 1, grid_cell_size - 2, grid_cell_size - 2);
        }
        
        // Draw position label (centered small)
        // wxString label = wxstr(edgePositionToString(pos));
        // wxSize textSize = dc.GetTextExtent(label);
        // dc.DrawText(label, x + (grid_cell_size - textSize.GetWidth()) / 2, 
        //           y + grid_cell_size - textSize.GetHeight() - 2);
        
        // Draw sprite if available
        uint16_t itemId = GetItemId(pos);
        if (itemId > 0) {
            const ItemType& type = g_items.getItemType(itemId);
            if (type.id != 0) {
                Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
                if (sprite) {
                    // Draw centered 32x32
                    int spriteSize = 32;
                    int drawX = x + (grid_cell_size - spriteSize) / 2;
                    int drawY = y + (grid_cell_size - spriteSize) / 2;
                    sprite->DrawTo(&dc, SPRITE_SIZE_32x32, 
                                 drawX, drawY, 
                                 spriteSize, 
                                 spriteSize);
                }
            }
        } else {
             // Only draw label if no item, or draw it faintly over?
             // User requested "minimalist", maybe hide label if item is there?
             // Let's keep it for now as it aids navigation
             wxString label = wxstr(edgePositionToString(pos));
             wxSize textSize = dc.GetTextExtent(label);
             // Draw label at bottom right
             dc.DrawText(label, x + (grid_cell_size - textSize.GetWidth()) / 2, 
                        y + grid_cell_size - textSize.GetHeight() - 2);
        }
    };
    
    // Draw normal direction items
    drawItemAtPos(EDGE_N, 0, 0, normal_offset_x, normal_offset_y);
    drawItemAtPos(EDGE_E, 1, 0, normal_offset_x, normal_offset_y);
    drawItemAtPos(EDGE_S, 0, 1, normal_offset_x, normal_offset_y);
    drawItemAtPos(EDGE_W, 1, 1, normal_offset_x, normal_offset_y);
    
    // Draw corner items
    drawItemAtPos(EDGE_CNW, 0, 0, corner_offset_x, corner_offset_y);
    drawItemAtPos(EDGE_CNE, 1, 0, corner_offset_x, corner_offset_y);
    drawItemAtPos(EDGE_CSW, 0, 1, corner_offset_x, corner_offset_y);
    drawItemAtPos(EDGE_CSE, 1, 1, corner_offset_x, corner_offset_y);
    
    // Draw diagonal items
    drawItemAtPos(EDGE_DNW, 0, 0, diag_offset_x, diag_offset_y);
    drawItemAtPos(EDGE_DNE, 1, 0, diag_offset_x, diag_offset_y);
    drawItemAtPos(EDGE_DSW, 0, 1, diag_offset_x, diag_offset_y);
    drawItemAtPos(EDGE_DSE, 1, 1, diag_offset_x, diag_offset_y);
}

wxPoint BorderGridPanel::GetPositionCoordinates(BorderEdgePosition pos) const {
    switch (pos) {
        case EDGE_N:   return wxPoint(1, 0);
        case EDGE_E:   return wxPoint(2, 1);
        case EDGE_S:   return wxPoint(1, 2);
        case EDGE_W:   return wxPoint(0, 1);
        case EDGE_CNW: return wxPoint(0, 0);
        case EDGE_CNE: return wxPoint(2, 0);
        case EDGE_CSE: return wxPoint(2, 2);
        case EDGE_CSW: return wxPoint(0, 2);
        case EDGE_DNW: return wxPoint(0.5, 0.5);
        case EDGE_DNE: return wxPoint(1.5, 0.5);
        case EDGE_DSE: return wxPoint(1.5, 1.5);
        case EDGE_DSW: return wxPoint(0.5, 1.5);
        default:       return wxPoint(-1, -1);
    }
}

BorderEdgePosition BorderGridPanel::GetPositionFromCoordinates(int x, int y) const
{
    // Re-calculate offsets logic must match OnPaint exactly to be clickable
    wxSize size = GetClientSize();
    const int total_width = size.GetWidth();
    const int total_height = size.GetHeight();
    
    // MATCHING CONSTANTS WITH ONPAINT
    const int grid_cell_size = 48; // Updated to 48
    
    const int normal_grid_size = 2;
    const int normal_grid_width = normal_grid_size * grid_cell_size;
    const int normal_grid_height = normal_grid_size * grid_cell_size;

    const int corner_grid_size = 2;
    const int corner_grid_width = corner_grid_size * grid_cell_size;
    
    const int diag_grid_size = 2;
    const int diag_grid_width = diag_grid_size * grid_cell_size;
    
    // Dynamic spacing logic from OnPaint
    const int start_y = (total_height - normal_grid_height) / 2;
    const int spacing = (total_width - (3 * normal_grid_width)) / 4;
    const int safe_spacing = std::max(10, spacing);
    
    const int normal_offset_x = safe_spacing;
    const int normal_offset_y = start_y;
    
    const int corner_offset_x = normal_offset_x + normal_grid_width + safe_spacing;
    const int corner_offset_y = start_y;
    
    const int diag_offset_x = corner_offset_x + corner_grid_width + safe_spacing;
    const int diag_offset_y = start_y;
    
    // Check which grid section the click is in
    
    // Normal grid
    if (x >= normal_offset_x && x < normal_offset_x + normal_grid_width &&
        y >= normal_offset_y && y < normal_offset_y + normal_grid_height) {
        
        int gridX = (x - normal_offset_x) / grid_cell_size;
        int gridY = (y - normal_offset_y) / grid_cell_size;
        
        if (gridX == 0 && gridY == 0) return EDGE_N;
        if (gridX == 1 && gridY == 0) return EDGE_E;
        if (gridX == 0 && gridY == 1) return EDGE_S;
        if (gridX == 1 && gridY == 1) return EDGE_W;
    }
    
    // Corner grid
    if (x >= corner_offset_x && x < corner_offset_x + corner_grid_width &&
        y >= corner_offset_y && y < corner_offset_y + normal_grid_height) { // Use normal height as they are same
        
        int gridX = (x - corner_offset_x) / grid_cell_size;
        int gridY = (y - corner_offset_y) / grid_cell_size;
        
        if (gridX == 0 && gridY == 0) return EDGE_CNW;
        if (gridX == 1 && gridY == 0) return EDGE_CNE;
        if (gridX == 0 && gridY == 1) return EDGE_CSW;
        if (gridX == 1 && gridY == 1) return EDGE_CSE;
    }
    
    // Diagonal grid
    if (x >= diag_offset_x && x < diag_offset_x + diag_grid_width &&
        y >= diag_offset_y && y < diag_offset_y + normal_grid_height) {
        
        int gridX = (x - diag_offset_x) / grid_cell_size;
        int gridY = (y - diag_offset_y) / grid_cell_size;
        
        if (gridX == 0 && gridY == 0) return EDGE_DNW;
        if (gridX == 1 && gridY == 0) return EDGE_DNE;
        if (gridX == 0 && gridY == 1) return EDGE_DSW;
        if (gridX == 1 && gridY == 1) return EDGE_DSE;
    }
    
    return EDGE_NONE;
}

void BorderGridPanel::OnMouseClick(wxMouseEvent& event) {
    int x = event.GetX();
    int y = event.GetY();
    
    BorderEdgePosition pos = GetPositionFromCoordinates(x, y);
    if (pos != EDGE_NONE) {
        // Set the position as selected in the grid
        SetSelectedPosition(pos);
        
        // Direct interaction: Open FindItemDialog immediately
        FindItemDialog browseDialog(this, "Select Border Item");
        if (browseDialog.ShowModal() == wxID_OK) {
             uint16_t itemId = browseDialog.getResultID();
             
             if (itemId > 0) {
                 // Find parent and set item
                 wxWindow* parent = GetParent();
                 while (parent && !dynamic_cast<BorderEditorDialog*>(parent)) {
                    parent = parent->GetParent();
                 }
                 BorderEditorDialog* dialog = dynamic_cast<BorderEditorDialog*>(parent);
                 if (dialog) {
                     dialog->SetBorderItem(pos, itemId);
                 }
             }
         }
    }
}

void BorderGridPanel::OnMouseDown(wxMouseEvent& event) {
    BorderEdgePosition pos = GetPositionFromCoordinates(event.GetX(), event.GetY());
    SetSelectedPosition(pos);
    event.Skip();
}

void BorderGridPanel::OnRightDown(wxMouseEvent& event) {
    BorderEdgePosition pos = GetPositionFromCoordinates(event.GetX(), event.GetY());
    if (pos != EDGE_NONE) {
        wxWindow* parent = GetParent();
        while (parent && !dynamic_cast<BorderEditorDialog*>(parent)) {
            parent = parent->GetParent();
        }
        
        BorderEditorDialog* dialog = dynamic_cast<BorderEditorDialog*>(parent);
        if (dialog) {
             dialog->RemoveBorderItem(pos);
             SetSelectedPosition(pos);
        }
    }
}

void BorderGridPanel::OnLeftDClick(wxMouseEvent& event) {
     BorderEdgePosition pos = GetPositionFromCoordinates(event.GetX(), event.GetY());
     if (pos != EDGE_NONE) {
         SetSelectedPosition(pos);
         FindItemDialog browseDialog(this, "Select Border Item");
         if (browseDialog.ShowModal() == wxID_OK) {
             uint16_t itemId = browseDialog.getResultID();
             if (itemId > 0) {
                 wxWindow* parent = GetParent();
                 while (parent && !dynamic_cast<BorderEditorDialog*>(parent)) {
                    parent = parent->GetParent();
                 }
                 BorderEditorDialog* dialog = dynamic_cast<BorderEditorDialog*>(parent);
                 if (dialog) {
                     dialog->SetBorderItem(pos, itemId);
                 }
             }
         }
     }
}

void BorderGridPanel::OnMouseMove(wxMouseEvent& event) {
    BorderEdgePosition pos = GetPositionFromCoordinates(event.GetX(), event.GetY());
    if (pos != m_hoveredPosition) {
        m_hoveredPosition = pos;
        Refresh();
    }
}

void BorderGridPanel::OnLeaveWindow(wxMouseEvent& event) {
    if (m_hoveredPosition != EDGE_NONE) {
        m_hoveredPosition = EDGE_NONE;
        Refresh();
    }
}

// ============================================================================
// BorderPreviewPanel

BorderPreviewPanel::BorderPreviewPanel(wxWindow* parent, wxWindowID id) :
    wxPanel(parent, id, wxDefaultPosition, wxSize(BORDER_PREVIEW_SIZE, BORDER_PREVIEW_SIZE)) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(BORDER_PREVIEW_SIZE, BORDER_PREVIEW_SIZE));
}

BorderPreviewPanel::~BorderPreviewPanel() {
}

void BorderPreviewPanel::SetBorderItems(const std::vector<BorderItem>& items) {
    m_borderItems = items;
    Refresh();
}

void BorderPreviewPanel::Clear() {
    m_borderItems.clear();
    Refresh();
}

void BorderPreviewPanel::OnPaint(wxPaintEvent& event) {
    wxAutoBufferedPaintDC dc(this);
    
    wxRect rect = GetClientRect();
    dc.SetBackground(wxBrush(wxColour(240, 240, 240)));
    dc.Clear();
    
    const int GRID_SIZE = 5;
    const int preview_cell_size = BORDER_PREVIEW_SIZE / GRID_SIZE;
    
    // Center the grid in the panel
    int grid_total_size = GRID_SIZE * preview_cell_size;
    int offset_x = (rect.GetWidth() - grid_total_size) / 2;
    int offset_y = (rect.GetHeight() - grid_total_size) / 2;
    
    wxPen gridPen(wxColour(200, 200, 200));
    dc.SetPen(gridPen);
    for (int i = 0; i <= GRID_SIZE; i++) {
        dc.DrawLine(offset_x + i * preview_cell_size, offset_y, 
                    offset_x + i * preview_cell_size, offset_y + grid_total_size);
        dc.DrawLine(offset_x, offset_y + i * preview_cell_size, 
                    offset_x + grid_total_size, offset_y + i * preview_cell_size);
    }
    
    // Draw sample ground center
    dc.SetBrush(wxBrush(wxColour(120, 180, 100)));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(offset_x + (GRID_SIZE / 2) * preview_cell_size, 
                     offset_y + (GRID_SIZE / 2) * preview_cell_size, 
                     preview_cell_size, preview_cell_size);
    
    // Draw border items
    for (const BorderItem& item : m_borderItems) {
        wxPoint offset(0, 0);
        switch (item.position) {
            case EDGE_N:   offset = wxPoint(0, -1); break;
            case EDGE_E:   offset = wxPoint(1, 0); break;
            case EDGE_S:   offset = wxPoint(0, 1); break;
            case EDGE_W:   offset = wxPoint(-1, 0); break;
            case EDGE_CNW: offset = wxPoint(-1, -1); break;
            case EDGE_CNE: offset = wxPoint(1, -1); break;
            case EDGE_CSE: offset = wxPoint(1, 1); break;
            case EDGE_CSW: offset = wxPoint(-1, 1); break;
            case EDGE_DNW: offset = wxPoint(-1, -1); break; 
            case EDGE_DNE: offset = wxPoint(1, -1); break;
            case EDGE_DSE: offset = wxPoint(1, 1); break;
            case EDGE_DSW: offset = wxPoint(-1, 1); break;
            default: continue;
        }
        
        int x = offset_x + (GRID_SIZE / 2 + offset.x) * preview_cell_size;
        int y = offset_y + (GRID_SIZE / 2 + offset.y) * preview_cell_size;
        
        const ItemType& type = g_items.getItemType(item.itemId);
        if (type.id != 0) {
            Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
            if (sprite) {
                // Draw centered 32x32
                int drawX = x + (preview_cell_size - 32) / 2;
                int drawY = y + (preview_cell_size - 32) / 2;
                
                sprite->DrawTo(&dc, SPRITE_SIZE_32x32, drawX, drawY, 32, 32);
            }
        }
    }
}

void BorderEditorDialog::LoadExistingGroundBrushes() {
    m_existingGroundBrushesCombo->Clear();
    
    // Add "Create New" as the first option
    m_existingGroundBrushesCombo->Append("<Create New>");
    m_existingGroundBrushesCombo->SetSelection(0);
    
    // Find the grounds.xml file
    wxString groundsFile = GetGroundsFilePath();

    if (!wxFileExists(groundsFile)) {
        wxMessageBox("Cannot find grounds.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(groundsFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load grounds.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    // Look for all brush nodes
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid grounds.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    for (pugi::xml_node brushNode = materials.child("brush"); brushNode; brushNode = brushNode.next_sibling("brush")) {
        pugi::xml_attribute nameAttr = brushNode.attribute("name");
        pugi::xml_attribute serverLookIdAttr = brushNode.attribute("server_lookid");
        pugi::xml_attribute typeAttr = brushNode.attribute("type");
        
        // Only include ground brushes
        if (!typeAttr || std::string(typeAttr.as_string()) != "ground") {
            continue;
        }
        
        if (nameAttr && serverLookIdAttr) {
            wxString brushName = wxString(nameAttr.as_string());
            int serverId = serverLookIdAttr.as_int();
            
            // Add the brush name to the combo box with the serverId as client data
            wxStringClientData* data = new wxStringClientData(wxString::Format("%d", serverId));
            m_existingGroundBrushesCombo->Append(brushName, data);
        }
    }
}

void BorderEditorDialog::ClearGroundItems() {
    m_groundItems.clear();
    m_nameCtrl->SetValue("");
    m_idCtrl->SetValue(m_nextBorderId);
    m_serverLookIdCtrl->SetValue(0);
    m_zOrderCtrl->SetValue(0);
    m_groundItemIdCtrl->SetValue(0);
    m_groundItemChanceCtrl->SetValue(10);
    
    // Reset border alignment options
    m_borderAlignmentChoice->SetSelection(0); // Default to "outer"
    m_includeToNoneCheck->SetValue(true);     // Default to checked
    m_includeInnerCheck->SetValue(false);     // Default to unchecked
    
    UpdateGroundItemsList();
}

void BorderEditorDialog::UpdateGroundItemsList() {
    m_groundItemsList->Clear();
    
    for (const GroundItem& item : m_groundItems) {
        wxString itemText = wxString::Format("Item ID: %d, Chance: %d", item.itemId, item.chance);
        m_groundItemsList->Append(itemText);
    }
}

void BorderEditorDialog::OnPageChanged(wxBookCtrlEvent& event) {
    m_activeTab = event.GetSelection();
    
    // When switching to the ground tab, use the same border items for the ground brush
    if (m_activeTab == 1) {
        // Update the ground items preview (not implemented yet)
    }
}

void BorderEditorDialog::OnAddGroundItem(wxCommandEvent& event) {
    uint16_t itemId = m_groundItemIdCtrl->GetValue();
    int chance = m_groundItemChanceCtrl->GetValue();
    
    if (itemId > 0) {
        // Check if this item already exists, if so update its chance
        bool updated = false;
        for (size_t i = 0; i < m_groundItems.size(); i++) {
            if (m_groundItems[i].itemId == itemId) {
                m_groundItems[i].chance = chance;
                updated = true;
                break;
            }
        }
        
        if (!updated) {
            // Add new item
            m_groundItems.push_back(GroundItem(itemId, chance));
        }
        
        // Update the list
        UpdateGroundItemsList();
    }
}

void BorderEditorDialog::OnRemoveGroundItem(wxCommandEvent& event) {
    int selection = m_groundItemsList->GetSelection();
    if (selection != wxNOT_FOUND && selection < static_cast<int>(m_groundItems.size())) {
        m_groundItems.erase(m_groundItems.begin() + selection);
        UpdateGroundItemsList();
    }
}

void BorderEditorDialog::OnGroundBrowse(wxCommandEvent& event) {
    // Open the Find Item dialog to select a ground item
    FindItemDialog dialog(this, "Select Ground Item");
    
    if (dialog.ShowModal() == wxID_OK) {
        // Get the selected item ID
        uint16_t itemId = dialog.getResultID();
        
        // Set the ground item ID field
        if (itemId > 0) {
            m_groundItemIdCtrl->SetValue(itemId);
        }
    }
}

void BorderEditorDialog::OnLoadGroundBrush(wxCommandEvent& event) {
    int selection = m_existingGroundBrushesCombo->GetSelection();
    if (selection <= 0) {
        // Selected "Create New" or nothing
        ClearGroundItems();
        return;
    }
    
    wxStringClientData* data = static_cast<wxStringClientData*>(m_existingGroundBrushesCombo->GetClientObject(selection));
    if (!data) return;
    
    int serverLookId = wxAtoi(data->GetData());
    
    // Find the grounds.xml file
    wxString groundsFile = GetGroundsFilePath();

    if (!wxFileExists(groundsFile)) {
        wxMessageBox("Cannot find grounds.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }

    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(groundsFile).c_str());

    if (!result) {
        wxMessageBox("Failed to load grounds.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }

    // Clear existing items
    ClearGroundItems();
    
    // Find the brush with the specified server_lookid
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid grounds.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    for (pugi::xml_node brushNode = materials.child("brush"); brushNode; brushNode = brushNode.next_sibling("brush")) {
        pugi::xml_attribute serverLookIdAttr = brushNode.attribute("server_lookid");
        
        if (serverLookIdAttr && serverLookIdAttr.as_int() == serverLookId) {
            // Found the brush, load its properties
            pugi::xml_attribute nameAttr = brushNode.attribute("name");
            pugi::xml_attribute zOrderAttr = brushNode.attribute("z-order");
            
            if (nameAttr) {
                m_nameCtrl->SetValue(wxString(nameAttr.as_string()));
            }
            
            m_serverLookIdCtrl->SetValue(serverLookId);
            
            if (zOrderAttr) {
                m_zOrderCtrl->SetValue(zOrderAttr.as_int());
            }
            
            // Load all item nodes
            for (pugi::xml_node itemNode = brushNode.child("item"); itemNode; itemNode = itemNode.next_sibling("item")) {
                pugi::xml_attribute idAttr = itemNode.attribute("id");
                pugi::xml_attribute chanceAttr = itemNode.attribute("chance");
                
                if (idAttr) {
                    uint16_t itemId = idAttr.as_uint();
                    int chance = chanceAttr ? chanceAttr.as_int() : 10;
                    
                    m_groundItems.push_back(GroundItem(itemId, chance));
                }
            }
            
            // Load all border nodes and add to the border grid
            m_borderItems.clear();
            m_gridPanel->Clear();
            
            // Reset alignment options
            m_borderAlignmentChoice->SetSelection(0); // Default to "outer"
            m_includeToNoneCheck->SetValue(true);     // Default to checked
            m_includeInnerCheck->SetValue(false);     // Default to unchecked
            
            bool hasNormalBorder = false;
            bool hasToNoneBorder = false;
            bool hasInnerBorder = false;
            bool hasInnerToNoneBorder = false;
            wxString alignment = "outer"; // Default
            
            for (pugi::xml_node borderNode = brushNode.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
                pugi::xml_attribute alignAttr = borderNode.attribute("align");
                pugi::xml_attribute toAttr = borderNode.attribute("to");
                pugi::xml_attribute idAttr = borderNode.attribute("id");
                
                if (idAttr) {
                    int borderId = idAttr.as_int();
                    m_idCtrl->SetValue(borderId);
                    
                    // Check border type and attributes
                    if (alignAttr) {
                        wxString alignVal = wxString(alignAttr.as_string());
                        
                        if (alignVal == "outer") {
                            if (toAttr && wxString(toAttr.as_string()) == "none") {
                                hasToNoneBorder = true;
                            } else {
                                hasNormalBorder = true;
                                alignment = "outer";
                            }
                        } else if (alignVal == "inner") {
                            if (toAttr && wxString(toAttr.as_string()) == "none") {
                                hasInnerToNoneBorder = true;
                            } else {
                                hasInnerBorder = true;
                            }
                        }
                    }
                    
                    // Load the border details from borders.xml
                    wxString bordersFile = GetBordersFilePath();

                    if (!wxFileExists(bordersFile)) {
                        // Just skip if we can't find borders.xml
                        continue;
                    }
                    
                    pugi::xml_document bordersDoc;
                    pugi::xml_parse_result bordersResult = bordersDoc.load_file(nstr(bordersFile).c_str());
                    
                    if (!bordersResult) {
                        // Skip if we can't load borders.xml
                        continue;
                    }
                    
                    pugi::xml_node bordersMaterials = bordersDoc.child("materials");
                    if (!bordersMaterials) {
                        continue;
                    }
                    
                    for (pugi::xml_node targetBorder = bordersMaterials.child("border"); targetBorder; targetBorder = targetBorder.next_sibling("border")) {
                        pugi::xml_attribute targetIdAttr = targetBorder.attribute("id");
                        
                        if (targetIdAttr && targetIdAttr.as_int() == borderId) {
                            // Found the border, load its items
                            for (pugi::xml_node borderItemNode = targetBorder.child("borderitem"); borderItemNode; borderItemNode = borderItemNode.next_sibling("borderitem")) {
                                pugi::xml_attribute edgeAttr = borderItemNode.attribute("edge");
                                pugi::xml_attribute itemAttr = borderItemNode.attribute("item");
                                
                                if (!edgeAttr || !itemAttr) continue;
                                
                                BorderEdgePosition pos = edgeStringToPosition(edgeAttr.as_string());
                                uint16_t borderItemId = itemAttr.as_uint();
                                
                                if (pos != EDGE_NONE && borderItemId > 0) {
                                    m_borderItems.push_back(BorderItem(pos, borderItemId));
                                    m_gridPanel->SetItemId(pos, borderItemId);
                                }
                            }
                            
                            break;
                        }
                    }
                }
            }
            
            // Update the ground items list and border preview
            UpdateGroundItemsList();
            UpdatePreview();
            
            // Apply the loaded border alignment settings
            if (hasInnerBorder) {
                m_includeInnerCheck->SetValue(true);
            }
            
            if (!hasToNoneBorder) {
                m_includeToNoneCheck->SetValue(false);
            }
            
            int alignmentIndex = 0; // Default to outer
            if (alignment == "inner") {
                alignmentIndex = 1;
            }
            m_borderAlignmentChoice->SetSelection(alignmentIndex);
            
            break;
        }
    }
    
    // Keep selection
    m_existingGroundBrushesCombo->SetSelection(selection);
}

bool BorderEditorDialog::ValidateGroundBrush() {
    // Check for empty name
    if (m_nameCtrl->GetValue().IsEmpty()) {
        wxMessageBox("Please enter a name for the ground brush.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    if (m_groundItems.empty()) {
        wxMessageBox("The ground brush must have at least one item.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    if (m_serverLookIdCtrl->GetValue() <= 0) {
        wxMessageBox("You must specify a valid server look ID.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    // Check tileset selection
    if (m_tilesetChoice->GetSelection() == wxNOT_FOUND) {
        wxMessageBox("Please select a tileset for the ground brush.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    return true;
}

void BorderEditorDialog::SaveGroundBrush() {
    if (!ValidateGroundBrush()) {
        return;
    }
    
    // Get the ground brush properties
    wxString name = m_nameCtrl->GetValue();
    
    // Double check that we have a name (it's also checked in ValidateGroundBrush)
    if (name.IsEmpty()) {
        wxMessageBox("You must provide a name for the ground brush.", "Error", wxICON_ERROR);
        return;
    }
    
    int serverId = m_serverLookIdCtrl->GetValue();
    int zOrder = m_zOrderCtrl->GetValue();
    int borderId = m_idCtrl->GetValue();  // This should be taken from common properties
    
    // Get the selected tileset
    int tilesetSelection = m_tilesetChoice->GetSelection();
    if (tilesetSelection == wxNOT_FOUND) {
        wxMessageBox("Please select a tileset.", "Validation Error", wxICON_ERROR);
        return;
    }
    wxString tilesetName = m_tilesetChoice->GetString(tilesetSelection);
    
    // Find the grounds.xml file
    wxString groundsFile = GetGroundsFilePath();

    if (!wxFileExists(groundsFile)) {
        wxMessageBox("Cannot find grounds.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }

    // Make sure the border is saved first if we have border items
    if (!m_borderItems.empty()) {
        SaveBorder();
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(groundsFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load grounds.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid grounds.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    // Check if a brush with this name already exists
    bool brushExists = false;
    pugi::xml_node existingBrush;
    
    for (pugi::xml_node brushNode = materials.child("brush"); brushNode; brushNode = brushNode.next_sibling("brush")) {
        pugi::xml_attribute nameAttr = brushNode.attribute("name");
        
        if (nameAttr && wxString(nameAttr.as_string()) == name) {
            brushExists = true;
            existingBrush = brushNode;
            break;
        }
    }
    
    if (brushExists) {
        // Ask for confirmation to overwrite
        if (wxMessageBox("A ground brush with name '" + name + "' already exists. Do you want to overwrite it?", 
                        "Confirm Overwrite", wxYES_NO | wxICON_QUESTION) != wxYES) {
            return;
        }
        
        // Remove the existing brush
        materials.remove_child(existingBrush);
    }
    
    // Create the new brush node
    pugi::xml_node brushNode = materials.append_child("brush");
    brushNode.append_attribute("name").set_value(nstr(name).c_str());
    brushNode.append_attribute("type").set_value("ground");
    brushNode.append_attribute("server_lookid").set_value(serverId);
    brushNode.append_attribute("z-order").set_value(zOrder);
    
    // Add all ground items
    for (const GroundItem& item : m_groundItems) {
        pugi::xml_node itemNode = brushNode.append_child("item");
        itemNode.append_attribute("id").set_value(item.itemId);
        itemNode.append_attribute("chance").set_value(item.chance);
    }
    
    // Add border reference if we have border items, or if border ID is specified (even without items)
    if (!m_borderItems.empty() || borderId > 0) {
        // Get alignment type
        wxString alignmentType = m_borderAlignmentChoice->GetStringSelection();
        
        // Main border
        pugi::xml_node borderNode = brushNode.append_child("border");
        borderNode.append_attribute("align").set_value(nstr(alignmentType).c_str());
        borderNode.append_attribute("id").set_value(borderId);
        
        // "to none" border if checked
        if (m_includeToNoneCheck->IsChecked()) {
            pugi::xml_node borderToNoneNode = brushNode.append_child("border");
            borderToNoneNode.append_attribute("align").set_value(nstr(alignmentType).c_str());
            borderToNoneNode.append_attribute("to").set_value("none");
            borderToNoneNode.append_attribute("id").set_value(borderId);
        }
        
        // Inner border if checked
        if (m_includeInnerCheck->IsChecked()) {
            pugi::xml_node innerBorderNode = brushNode.append_child("border");
            innerBorderNode.append_attribute("align").set_value("inner");
            innerBorderNode.append_attribute("id").set_value(borderId);
            
            // Inner "to none" border if checked
            if (m_includeToNoneCheck->IsChecked()) {
                pugi::xml_node innerToNoneNode = brushNode.append_child("border");
                innerToNoneNode.append_attribute("align").set_value("inner");
                innerToNoneNode.append_attribute("to").set_value("none");
                innerToNoneNode.append_attribute("id").set_value(borderId);
            }
        }
    }
    
    // Save the file
    if (!doc.save_file(nstr(groundsFile).c_str())) {
        wxMessageBox("Failed to save changes to grounds.xml", "Error", wxICON_ERROR);
        return;
    }
    
    // Now also add this brush to the selected tileset
    wxString tilesetsFile = GetTilesetsFilePath();

    if (!wxFileExists(tilesetsFile)) {
        wxMessageBox("Cannot find tilesets.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the tilesets XML file
    pugi::xml_document tilesetsDoc;
    pugi::xml_parse_result tilesetsResult = tilesetsDoc.load_file(nstr(tilesetsFile).c_str());
    
    if (!tilesetsResult) {
        wxMessageBox("Failed to load tilesets.xml: " + wxString(tilesetsResult.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node tilesetsMaterials = tilesetsDoc.child("materials");
    if (!tilesetsMaterials) {
        wxMessageBox("Invalid tilesets.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    // Find the selected tileset
    bool tilesetFound = false;
    for (pugi::xml_node tilesetNode = tilesetsMaterials.child("tileset"); tilesetNode; tilesetNode = tilesetNode.next_sibling("tileset")) {
        pugi::xml_attribute nameAttr = tilesetNode.attribute("name");
        
        if (nameAttr && wxString(nameAttr.as_string()) == tilesetName) {
            // Find the terrain node
            pugi::xml_node terrainNode = tilesetNode.child("terrain");
            if (!terrainNode) {
                // Create terrain node if it doesn't exist
                terrainNode = tilesetNode.append_child("terrain");
            }
            
            // Check if the brush is already in this tileset
            bool brushFound = false;
            for (pugi::xml_node brushNode = terrainNode.child("brush"); brushNode; brushNode = brushNode.next_sibling("brush")) {
                pugi::xml_attribute brushNameAttr = brushNode.attribute("name");
                
                if (brushNameAttr && wxString(brushNameAttr.as_string()) == name) {
                    brushFound = true;
                    break;
                }
            }
            
            // If brush not found, add it
            if (!brushFound) {
                // Add a brush node directly under terrain - no empty attributes
                pugi::xml_node newBrushNode = terrainNode.append_child("brush");
                newBrushNode.append_attribute("name").set_value(nstr(name).c_str());
            }
            
            tilesetFound = true;
            break;
        }
    }
    
    if (!tilesetFound) {
        wxMessageBox("Selected tileset not found in tilesets.xml", "Error", wxICON_ERROR);
        return;
    }
    
    // Save the tilesets.xml file
    if (!tilesetsDoc.save_file(nstr(tilesetsFile).c_str())) {
        wxMessageBox("Failed to save changes to tilesets.xml", "Error", wxICON_ERROR);
        return;
    }
    
    wxMessageBox("Ground brush saved successfully and added to the " + tilesetName + " tileset.", "Success", wxICON_INFORMATION);
    
    // Reload the existing ground brushes list
    LoadExistingGroundBrushes();
}

void BorderEditorDialog::LoadTilesets() {
    // Clear the choice control
    m_tilesetChoice->Clear();
    m_tilesets.clear();
    
    // Find the tilesets.xml file
    wxString tilesetsFile = GetTilesetsFilePath();

    if (!wxFileExists(tilesetsFile)) {
        wxMessageBox("Cannot find tilesets.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(tilesetsFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load tilesets.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid tilesets.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    // Parse all tilesets
    wxArrayString tilesetNames; // Store in sorted order
    for (pugi::xml_node tilesetNode = materials.child("tileset"); tilesetNode; tilesetNode = tilesetNode.next_sibling("tileset")) {
        pugi::xml_attribute nameAttr = tilesetNode.attribute("name");
        
        if (nameAttr) {
            wxString tilesetName = wxString(nameAttr.as_string());
            
            // Add to our array of names
            tilesetNames.Add(tilesetName);
            
            // Add to the map for later use
            m_tilesets[tilesetName] = tilesetName;
        }
    }
    
    // Sort tileset names alphabetically
    tilesetNames.Sort();
    
    // Add sorted names to the choice control
    for (size_t i = 0; i < tilesetNames.GetCount(); ++i) {
        m_tilesetChoice->Append(tilesetNames[i]);
    }
    
    // Select the first tileset by default if any exist
    if (m_tilesetChoice->GetCount() > 0) {
        m_tilesetChoice->SetSelection(0);
    }
} 
