// -*- c-basic-offset: 4 -*-

/** @file GLPreviewFrame.cpp
 *
 *  @brief implementation of GLPreviewFrame Class
 *
 *  @author James Legg and Pablo d'Angelo <pablo.dangelo@web.de>
 *  @author Darko Makreshanski
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <bitset>
#include <limits>
#include <iostream>

#include "hugin_config.h"

#include <GL/glew.h>

#include "panoinc_WX.h"
#include "panoinc.h"

#include "base_wx/platform.h"
#include "base_wx/wxPlatform.h"
#include "base_wx/LensTools.h"
#include "algorithms/optimizer/ImageGraph.h"
#include "algorithms/basic/LayerStacks.h"
#include <algorithms/basic/CalculateOptimalROI.h>
#include <algorithms/basic/CalculateCPStatistics.h>
#include <algorithms/nona/FitPanorama.h>
#include <algorithms/basic/CalculateMeanExposure.h>

#include "base_wx/MyProgressDialog.h"
#include "base_wx/wxPanoCommand.h"

#include "hugin/config_defaults.h"
#include "hugin/GLPreviewFrame.h"
#include "hugin/huginApp.h"
#include "hugin/MainFrame.h"
#include "hugin/ImagesPanel.h"
#include "base_wx/CommandHistory.h"
#include "hugin/GLViewer.h"
#include "hugin/TextKillFocusHandler.h"
#include "hugin/PanoOperation.h"
#include "hugin/PanoOutputDialog.h"
#include "base_wx/PTWXDlg.h"
#include "vigra_ext/InterestPoints.h"
#include "vigra_ext/Correlation.h"
#include "algorithms/control_points/CleanCP.h"
#include "hugin_utils/openmp_lock.h"

extern "C" {
#include <pano13/queryfeature.h>
}

#include "ToolHelper.h"
#include "Tool.h"
#include "DragTool.h"
#include "PreviewCropTool.h"
#include "PreviewIdentifyTool.h"
#include "PreviewDifferenceTool.h"
#include "PreviewPanoMaskTool.h"
#include "PreviewControlPointTool.h"
#include "PreviewLayoutLinesTool.h"
#include "PreviewColorPickerTool.h"
#include "PreviewGuideTool.h"
#include "PreviewEditCPTool.h"

#include "ProjectionGridTool.h"
#include "PanosphereSphereTool.h"

#include "OverviewCameraTool.h"
#include "OverviewOutlinesTool.h"

#include <wx/progdlg.h>
#include <wx/infobar.h>

// a random id, hope this doesn't break something..
enum {
    ID_TOGGLE_BUT = wxID_HIGHEST+500,
    PROJ_PARAM_NAMES_ID = wxID_HIGHEST+1300,
    PROJ_PARAM_VAL_ID = wxID_HIGHEST+1400,
    PROJ_PARAM_SLIDER_ID = wxID_HIGHEST+1500,
    PROJ_PARAM_RESET_ID = wxID_HIGHEST+1550,
    ID_TOGGLE_BUT_LEAVE = wxID_HIGHEST+1600,
    ID_FULL_SCREEN = wxID_HIGHEST+1710,
    ID_SHOW_ALL = wxID_HIGHEST+1711,
    ID_SHOW_NONE = wxID_HIGHEST+1712,
    ID_HIDE_HINTS = wxID_HIGHEST+1715
};

/** enum, which contains all different toolbar modes */
enum{
    mode_assistant=0,
    mode_preview,
    mode_layout,
    mode_projection,
    mode_drag,
    mode_crop
};

//------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(GLwxAuiFloatingFrame, wxAuiFloatingFrame)
    EVT_ACTIVATE(GLwxAuiFloatingFrame::OnActivate)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(GLPreviewFrame, wxFrame)
    EVT_CLOSE(GLPreviewFrame::OnClose)
    EVT_SHOW(GLPreviewFrame::OnShowEvent)
    //for some reason only key up is sent, key down event is not sent
//    EVT_KEY_DOWN(GLPreviewFrame::KeyDown)
//    EVT_KEY_UP(GLPreviewFrame::KeyUp)
    EVT_BUTTON(XRCID("preview_center_tool"), GLPreviewFrame::OnCenterHorizontally)
    EVT_BUTTON(XRCID("preview_fit_pano_tool"), GLPreviewFrame::OnFitPano)
    EVT_BUTTON(XRCID("preview_fit_pano_tool2"), GLPreviewFrame::OnFitPano)
    EVT_BUTTON(XRCID("preview_straighten_pano_tool"), GLPreviewFrame::OnStraighten)
    EVT_BUTTON(XRCID("apply_num_transform"), GLPreviewFrame::OnNumTransform)
    EVT_TEXT_ENTER(XRCID("input_yaw"), GLPreviewFrame::OnNumTransform)
    EVT_TEXT_ENTER(XRCID("input_pitch"), GLPreviewFrame::OnNumTransform)
    EVT_TEXT_ENTER(XRCID("input_roll"), GLPreviewFrame::OnNumTransform)
    EVT_TEXT_ENTER(XRCID("input_x"), GLPreviewFrame::OnNumTransform)
    EVT_TEXT_ENTER(XRCID("input_y"), GLPreviewFrame::OnNumTransform)
    EVT_TEXT_ENTER(XRCID("input_z"), GLPreviewFrame::OnNumTransform)
    EVT_BUTTON(ID_SHOW_ALL, GLPreviewFrame::OnShowAll)
    EVT_BUTTON(ID_SHOW_NONE, GLPreviewFrame::OnShowNone)
    EVT_CHECKBOX(XRCID("preview_photometric_tool"), GLPreviewFrame::OnPhotometric)
    EVT_TOGGLEBUTTON(XRCID("preview_identify_toggle_button"), GLPreviewFrame::OnIdentify)
    EVT_TOGGLEBUTTON(XRCID("preview_color_picker_toggle_button"), GLPreviewFrame::OnColorPicker)
    EVT_TOGGLEBUTTON(XRCID("preview_edit_cp_toggle_button"), GLPreviewFrame::OnEditCPTool)
    EVT_CHECKBOX(XRCID("preview_control_point_tool"), GLPreviewFrame::OnControlPoint)
    EVT_BUTTON(XRCID("preview_autocrop_tool"), GLPreviewFrame::OnAutocrop)
    EVT_BUTTON(XRCID("preview_stack_autocrop_tool"), GLPreviewFrame::OnStackAutocrop)
    EVT_NOTEBOOK_PAGE_CHANGED(XRCID("mode_toolbar_notebook"), GLPreviewFrame::OnSelectMode)
    EVT_NOTEBOOK_PAGE_CHANGING(XRCID("mode_toolbar_notebook"), GLPreviewFrame::OnToolModeChanging)
    EVT_BUTTON(ID_HIDE_HINTS, GLPreviewFrame::OnHideProjectionHints)
    EVT_BUTTON(XRCID("exposure_default_button"), GLPreviewFrame::OnDefaultExposure)
    EVT_SPIN_DOWN(XRCID("exposure_spin"), GLPreviewFrame::OnDecreaseExposure)
    EVT_SPIN_UP(XRCID("exposure_spin"), GLPreviewFrame::OnIncreaseExposure)
    EVT_CHOICE(XRCID("blend_mode_choice"), GLPreviewFrame::OnBlendChoice)
    EVT_CHOICE(XRCID("drag_mode_choice"), GLPreviewFrame::OnDragChoice)
    EVT_CHOICE(XRCID("projection_choice"), GLPreviewFrame::OnProjectionChoice)
    EVT_CHOICE(XRCID("overview_mode_choice"), GLPreviewFrame::OnOverviewModeChoice)
    EVT_CHOICE(XRCID("preview_guide_choice_crop"), GLPreviewFrame::OnGuideChanged)
    EVT_CHOICE(XRCID("preview_guide_choice_drag"), GLPreviewFrame::OnGuideChanged)
    EVT_CHOICE(XRCID("preview_guide_choice_proj"), GLPreviewFrame::OnGuideChanged)
    EVT_MENU(XRCID("action_show_overview"), GLPreviewFrame::OnOverviewToggle)
    EVT_MENU(XRCID("action_show_grid"), GLPreviewFrame::OnSwitchPreviewGrid)
    EVT_MENU(ID_CREATE_CP, GLPreviewFrame::OnCreateCP)
    EVT_MENU(ID_REMOVE_CP, GLPreviewFrame::OnRemoveCP)
    EVT_MENU_CLOSE(GLPreviewFrame::OnMenuClose)
#ifndef __WXMAC__
	EVT_COMMAND_SCROLL(XRCID("layout_scale_slider"), GLPreviewFrame::OnLayoutScaleChange)
	EVT_SCROLL_CHANGED(GLPreviewFrame::OnChangeFOV)
	EVT_COMMAND_SCROLL_CHANGED(XRCID("layout_scale_slider"), GLPreviewFrame::OnLayoutScaleChange)
#else
    EVT_SCROLL_THUMBRELEASE(GLPreviewFrame::OnChangeFOV)
    EVT_COMMAND_SCROLL(XRCID("layout_scale_slider"), GLPreviewFrame::OnLayoutScaleChange)
    EVT_SCROLL_CHANGED(GLPreviewFrame::OnChangeFOV)
    EVT_COMMAND_SCROLL_THUMBTRACK(XRCID("layout_scale_slider"), GLPreviewFrame::OnLayoutScaleChange)
#endif
	EVT_SCROLL_THUMBTRACK(GLPreviewFrame::OnTrackChangeFOV)
    EVT_TEXT_ENTER(XRCID("pano_text_hfov"), GLPreviewFrame::OnHFOVChanged )
    EVT_TEXT_ENTER(XRCID("pano_text_vfov"), GLPreviewFrame::OnVFOVChanged )
    EVT_TEXT_ENTER(XRCID("pano_val_roi_left"), GLPreviewFrame::OnROIChanged)
    EVT_TEXT_ENTER(XRCID("pano_val_roi_top"), GLPreviewFrame::OnROIChanged)
    EVT_TEXT_ENTER(XRCID("pano_val_roi_right"), GLPreviewFrame::OnROIChanged)
    EVT_TEXT_ENTER(XRCID("pano_val_roi_bottom"), GLPreviewFrame::OnROIChanged)
    EVT_BUTTON(XRCID("reset_crop_button"), GLPreviewFrame::OnResetCrop)
    EVT_TEXT_ENTER(XRCID("exposure_text"), GLPreviewFrame::OnExposureChanged)
    EVT_COMMAND_RANGE(PROJ_PARAM_VAL_ID,PROJ_PARAM_VAL_ID+PANO_PROJECTION_MAX_PARMS,wxEVT_COMMAND_TEXT_ENTER,GLPreviewFrame::OnProjParameterChanged)
    EVT_BUTTON(PROJ_PARAM_RESET_ID, GLPreviewFrame::OnProjParameterReset)
    EVT_TOOL(ID_FULL_SCREEN, GLPreviewFrame::OnFullScreen)
    EVT_COLOURPICKER_CHANGED(XRCID("preview_background"), GLPreviewFrame::OnPreviewBackgroundColorChanged)
    EVT_MENU(XRCID("ID_SHOW_FULL_SCREEN_PREVIEW"), GLPreviewFrame::OnFullScreen)
    EVT_MENU(XRCID("action_show_main_frame"), GLPreviewFrame::OnShowMainFrame)
    EVT_MENU(XRCID("action_exit_preview"), GLPreviewFrame::OnUserExit)
    EVT_CHOICE     ( XRCID("ass_lens_type"), GLPreviewFrame::OnLensTypeChanged)
    EVT_TEXT_ENTER ( XRCID("ass_focal_length"), GLPreviewFrame::OnFocalLengthChanged)
    EVT_TEXT_ENTER ( XRCID("ass_crop_factor"), GLPreviewFrame::OnCropFactorChanged)
    EVT_BUTTON     ( XRCID("ass_load_images_button"), GLPreviewFrame::OnLoadImages)
    EVT_BUTTON     ( XRCID("ass_align_button"), GLPreviewFrame::OnAlign)
    EVT_BUTTON     ( XRCID("ass_create_button"), GLPreviewFrame::OnCreate)
    // context menu of select all button
    EVT_MENU(XRCID("selectMenu_selectAll"), GLPreviewFrame::OnSelectAllMenu)
    EVT_MENU(XRCID("selectMenu_selectMedian"), GLPreviewFrame::OnSelectMedianMenu)
    EVT_MENU(XRCID("selectMenu_selectBrightest"), GLPreviewFrame::OnSelectDarkestMenu)
    EVT_MENU(XRCID("selectMenu_selectDarkest"), GLPreviewFrame::OnSelectBrightestMenu)
    EVT_MENU(XRCID("selectMenu_keepCurrentSelection"), GLPreviewFrame::OnSelectKeepSelection)
    EVT_MENU(XRCID("selectMenu_resetSelection"), GLPreviewFrame::OnSelectResetSelection)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(ImageToogleButtonEventHandler, wxEvtHandler)
    EVT_ENTER_WINDOW(ImageToogleButtonEventHandler::OnEnter)
    EVT_LEAVE_WINDOW(ImageToogleButtonEventHandler::OnLeave)
    EVT_TOGGLEBUTTON(-1, ImageToogleButtonEventHandler::OnChange)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(ImageGroupButtonEventHandler, wxEvtHandler)
    EVT_ENTER_WINDOW(ImageGroupButtonEventHandler::OnEnter)
    EVT_LEAVE_WINDOW(ImageGroupButtonEventHandler::OnLeave)
    EVT_CHECKBOX(-1, ImageGroupButtonEventHandler::OnChange)
END_EVENT_TABLE()

#define PF_STYLE (wxMAXIMIZE_BOX | wxMINIMIZE_BOX | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN)
GLwxAuiFloatingFrame* GLwxAuiManager::CreateFloatingFrame(wxWindow* parent, const wxAuiPaneInfo& p)
{
    DEBUG_DEBUG("CREATING FLOATING FRAME");
    frame->PauseResize();
    GLwxAuiFloatingFrame* fl_frame = new GLwxAuiFloatingFrame(parent, this, p);
    DEBUG_DEBUG("CREATED FLOATING FRAME");
    return fl_frame;
}

void GLwxAuiFloatingFrame::OnActivate(wxActivateEvent& evt)
{
    DEBUG_DEBUG("FRAME ACTIVATE");
    GLPreviewFrame * frame = ((GLwxAuiManager*) GetOwnerManager())->getPreviewFrame();
    frame->ContinueResize();
    evt.Skip();
}

void GLwxAuiFloatingFrame::OnMoveFinished()
{
    DEBUG_DEBUG("FRAME ON MOVE FINISHED");
    GLPreviewFrame * frame = ((GLwxAuiManager*) GetOwnerManager())->getPreviewFrame();
    frame->PauseResize();
    wxAuiFloatingFrame::OnMoveFinished();
    DEBUG_DEBUG("FRAME AFTER ON MOVE FINISHED");
}

void GLPreviewFrame::PauseResize()
{
    DEBUG_DEBUG("PAUSE RESIZE");
    GLresize = false;
}

void GLPreviewFrame::ContinueResize()
{
    GLresize = true;
    wxSizeEvent event = wxSizeEvent(wxSize());
    m_GLPreview->Resized(event);
    m_GLOverview->Resized(event);
}

#include <iostream>
GLPreviewFrame::GLPreviewFrame(wxFrame * frame, HuginBase::Panorama &pano)
    : wxFrame(frame,-1, _("Fast Panorama preview"), wxDefaultPosition, wxDefaultSize,
              PF_STYLE),
      m_pano(pano)
{

	DEBUG_TRACE("");

    // initialize pointer
    preview_helper = NULL;
    panosphere_overview_helper = NULL;
    plane_overview_helper = NULL;
    crop_tool = NULL;
    drag_tool = NULL;
    color_picker_tool = NULL;
    edit_cp_tool = NULL;
    overview_drag_tool = NULL;
    identify_tool = NULL ;
    panosphere_overview_identify_tool = NULL;
    plane_overview_identify_tool = NULL;
    difference_tool = NULL;
    plane_difference_tool = NULL;
    panosphere_difference_tool = NULL;
    pano_mask_tool = NULL;
    preview_guide_tool = NULL;
    m_guiLevel=GUI_SIMPLE;
#ifdef __WXGTK__
    loadedLayout=false;
#endif

    m_mode = -1;
    m_oldProjFormat = -1;
    // add a status bar
    CreateStatusBar(3);
    int widths[3] = {-3, 150, 150};
    SetStatusWidths(3, widths);
    SetStatusText(wxT(""),1);
    SetStatusText(wxT(""),2);
    wxConfigBase * cfg = wxConfigBase::Get();

    wxPanel *tool_panel = wxXmlResource::Get()->LoadPanel(this,wxT("mode_panel"));
    XRCCTRL(*this,"preview_center_tool",wxButton)->SetBitmapMargins(0,0);
    XRCCTRL(*this,"preview_fit_pano_tool",wxButton)->SetBitmapMargins(0,0);
    XRCCTRL(*this,"preview_straighten_pano_tool",wxButton)->SetBitmapMargins(0,0);
    XRCCTRL(*this,"preview_fit_pano_tool2",wxButton)->SetBitmapMargins(0,0);
    XRCCTRL(*this,"preview_autocrop_tool",wxButton)->SetBitmapMargins(0,0);
    XRCCTRL(*this,"preview_stack_autocrop_tool",wxButton)->SetBitmapMargins(0,0);

    m_tool_notebook = XRCCTRL(*this, "mode_toolbar_notebook", wxNotebook);
    m_identify_togglebutton = XRCCTRL(*this, "preview_identify_toggle_button", wxToggleButton);
    m_colorpicker_togglebutton = XRCCTRL(*this, "preview_color_picker_toggle_button", wxToggleButton);
    m_editCP_togglebutton = XRCCTRL(*this, "preview_edit_cp_toggle_button", wxToggleButton);
    wxBitmap bitmap;
#if !wxCHECK_VERSION(3,1,1)
    bitmap.LoadFile(huginApp::Get()->GetXRCPath() + wxT("data/identify_tool.png"), wxBITMAP_TYPE_PNG);
    m_identify_togglebutton->SetBitmap(bitmap, wxTOP);
    bitmap.LoadFile(huginApp::Get()->GetXRCPath() + wxT("data/preview_white_balance.png"), wxBITMAP_TYPE_PNG);
    m_colorpicker_togglebutton->SetBitmap(bitmap, wxTOP);
    bitmap.LoadFile(huginApp::Get()->GetXRCPath() + wxT("data/preview_control_point_tool.png"), wxBITMAP_TYPE_PNG);
    m_editCP_togglebutton->SetBitmap(bitmap, wxTOP);
#endif

    //build menu bar
#ifdef __WXMAC__
    wxApp::s_macExitMenuItemId = XRCID("action_exit_preview");
#endif
    wxMenuBar* simpleMenu=wxXmlResource::Get()->LoadMenuBar(this, wxT("preview_simple_menu"));
    m_filemenuSimple=wxXmlResource::Get()->LoadMenu(wxT("preview_file_menu"));
    m_filemenuAdvanced = wxXmlResource::Get()->LoadMenu(wxT("preview_file_menu_advanced"));
    MainFrame::Get()->GetFileHistory()->UseMenu(m_filemenuSimple->FindItem(XRCID("menu_mru_preview"))->GetSubMenu());
    MainFrame::Get()->GetFileHistory()->UseMenu(m_filemenuAdvanced->FindItem(XRCID("menu_mru_preview"))->GetSubMenu());
    MainFrame::Get()->GetFileHistory()->AddFilesToMenu();
    simpleMenu->Insert(0, m_filemenuSimple, _("&File"));
    SetMenuBar(simpleMenu);

    // initialize preview background color
    wxString c = cfg->Read(wxT("/GLPreviewFrame/PreviewBackground"),wxT(HUGIN_PREVIEW_BACKGROUND));
    m_preview_background_color = wxColour(c);
    XRCCTRL(*this, "preview_background", wxColourPickerCtrl)->SetColour(m_preview_background_color);
    XRCCTRL(*this, "preview_background", wxColourPickerCtrl)->Refresh();
    XRCCTRL(*this, "preview_background", wxColourPickerCtrl)->Update();
  
    m_topsizer = new wxBoxSizer( wxVERTICAL );

    wxPanel * toggle_panel = new wxPanel(this);

    bool overview_hidden;
    cfg->Read(wxT("/GLPreviewFrame/overview_hidden"), &overview_hidden, false);
    GetMenuBar()->FindItem(XRCID("action_show_overview"))->Check(!overview_hidden);

    m_ToggleButtonSizer = new wxStaticBoxSizer(
        new wxStaticBox(toggle_panel, -1, _("displayed images")),
    wxHORIZONTAL );
    toggle_panel->SetSizer(m_ToggleButtonSizer);

	m_ButtonPanel = new wxScrolledWindow(toggle_panel, -1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	//Horizontal scroll bars only
	m_ButtonPanel->SetScrollRate(10, 0);
    m_ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    m_ButtonPanel->SetAutoLayout(true);
	m_ButtonPanel->SetSizer(m_ButtonSizer);

    wxPanel *panel = new wxPanel(toggle_panel);
    bitmap.LoadFile(huginApp::Get()->GetXRCPath()+wxT("data/preview_show_all.png"),wxBITMAP_TYPE_PNG);
    wxString showAllLabel(_("All"));
    showAllLabel.Append(wxT("\u25bc"));
    m_selectAllButton = new wxButton(panel, ID_SHOW_ALL, showAllLabel, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_selectAllButton->SetBitmap(bitmap, wxLEFT);
    m_selectAllButton->SetBitmapMargins(0, 0);
    m_selectAllButton->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(GLPreviewFrame::OnSelectContextMenu), NULL, this);
    m_selectAllMenu = wxXmlResource::Get()->LoadMenu(wxT("preview_select_menu"));
    // read last used setting
    long mode = cfg->Read(wxT("/GLPreviewFrame/SelectAllMode"), 0l);
    m_selectAllMode = static_cast<SelectAllMode>(mode);
    switch (m_selectAllMode)
    {
        case SELECT_MEDIAN_IMAGES:
            m_selectAllMenu->Check(XRCID("selectMenu_selectMedian"), true);
            break;
        case SELECT_DARKEST_IMAGES:
            m_selectAllMenu->Check(XRCID("selectMenu_selectDarkest"), true);
            break;
        case SELECT_BRIGHTEST_IMAGES:
            m_selectAllMenu->Check(XRCID("selectMenu_selectBrightest"), true);
            break;
        case SELECT_ALL_IMAGES:
        default:
            m_selectAllMenu->Check(XRCID("selectMenu_selectAll"), true);
            break;
    };
    m_selectKeepSelection = (cfg->Read(wxT("/GLPreviewFrame/SelectAllKeepSelection"), 1l) == 1l);
    if (m_selectKeepSelection)
    {
        m_selectAllMenu->Check(XRCID("selectMenu_keepCurrentSelection"), true);
    }
    else
    {
        m_selectAllMenu->Check(XRCID("selectMenu_resetSelection"), true);
    };
    bitmap.LoadFile(huginApp::Get()->GetXRCPath()+wxT("data/preview_show_none.png"),wxBITMAP_TYPE_PNG);
    wxButton* select_none=new wxButton(panel,ID_SHOW_NONE,_("None"),wxDefaultPosition,wxDefaultSize,wxBU_EXACTFIT);
    select_none->SetBitmap(bitmap,wxLEFT);
    select_none->SetBitmapMargins(0,0);

    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_selectAllButton,0,wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM,5);
    sizer->Add(select_none,0,wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM,5);
    panel->SetSizer(sizer);
    m_ToggleButtonSizer->Add(panel, 0, wxALIGN_CENTER_VERTICAL);
    m_ToggleButtonSizer->Add(m_ButtonPanel, 1, wxALIGN_CENTER_VERTICAL, 0);

    m_topsizer->Add(tool_panel, 0, wxEXPAND | wxALL, 2);
    m_topsizer->Add(toggle_panel, 0, wxEXPAND | wxBOTTOM, 5);

    m_infoBar = new wxInfoBar(this);
    m_infoBar->AddButton(ID_HIDE_HINTS,_("Hide"));
    m_infoBar->Connect(ID_HIDE_HINTS,wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(GLPreviewFrame::OnHideProjectionHints),NULL,this);
    m_topsizer->Add(m_infoBar, 0, wxEXPAND);

    //create panel that will hold gl canvases
    wxPanel * vis_panel = new wxPanel(this);

    wxPanel * preview_panel = new wxPanel(vis_panel);
    wxPanel * overview_panel = new wxPanel(vis_panel);

    // create our Viewers
    int args[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, 0};
    m_GLPreview = new GLPreview(preview_panel, pano, args, this);
    m_GLOverview = new GLOverview(overview_panel, pano, args, this, m_GLPreview->GetContext());
    m_GLOverview->SetMode(GLOverview::PANOSPHERE);
      
#ifdef __WXGTK__
    // on wxGTK we can not create the OpenGL context with a hidden window
    // therefore we need to create the overview window open and later hide it
    overview_hidden=false;
#endif
    m_GLOverview->SetActive(!overview_hidden);

    // set the AUI manager to our panel
    m_mgr = new GLwxAuiManager(this, m_GLPreview, m_GLOverview);
    m_mgr->SetManagedWindow(vis_panel);
    vis_panel->SetMinSize(wxSize(200,150));
    
    //create the sizer for the preview
    wxFlexGridSizer * flexSizer = new wxFlexGridSizer(2,0,5,5);
    flexSizer->AddGrowableCol(0);
    flexSizer->AddGrowableRow(0);

    //overview sizer
    wxBoxSizer * overview_sizer = new wxBoxSizer(wxVERTICAL);


    flexSizer->Add(m_GLPreview,
                  1,        // not vertically stretchable
                  wxEXPAND | // horizontally stretchable
                  wxALL,    // draw border all around
                  5);       // border width

    m_VFOVSlider = new wxSlider(preview_panel, -1, 1,
                                1, 180,
                                wxDefaultPosition, wxDefaultSize,
                                wxSL_VERTICAL | wxSL_AUTOTICKS,
                                wxDefaultValidator,
                                _("VFOV"));
    m_VFOVSlider->SetLineSize(1);
    m_VFOVSlider->SetPageSize(10);
    m_VFOVSlider->SetTickFreq(5);
    m_VFOVSlider->SetToolTip(_("drag to change the vertical field of view"));

    flexSizer->Add(m_VFOVSlider, 0, wxEXPAND);

    m_HFOVSlider = new wxSlider(preview_panel, -1, 1,
                                1, 360,
                                wxDefaultPosition, wxDefaultSize,
                                wxSL_HORIZONTAL | wxSL_AUTOTICKS,
                                wxDefaultValidator,
                                _("HFOV"));
    m_HFOVSlider->SetPageSize(10);
    m_HFOVSlider->SetLineSize(1);
    m_HFOVSlider->SetTickFreq(5);

    m_HFOVSlider->SetToolTip(_("drag to change the horizontal field of view"));

    m_exposureText = XRCCTRL(*this, "exposure_text", wxTextCtrl);
    DEBUG_ASSERT(m_exposureText);
    m_exposureText->PushEventHandler(new TextKillFocusHandler(this));
    m_HFOVText = XRCCTRL(*this, "pano_text_hfov" ,wxTextCtrl);
    DEBUG_ASSERT(m_HFOVText);
    m_HFOVText->PushEventHandler(new TextKillFocusHandler(this));
    m_VFOVText = XRCCTRL(*this, "pano_text_vfov" ,wxTextCtrl);
    DEBUG_ASSERT(m_VFOVText);
    m_VFOVText->PushEventHandler(new TextKillFocusHandler(this));

    m_ROILeftTxt = XRCCTRL(*this, "pano_val_roi_left", wxTextCtrl);
    DEBUG_ASSERT(m_ROILeftTxt);
    m_ROILeftTxt->PushEventHandler(new TextKillFocusHandler(this));

    m_ROIRightTxt = XRCCTRL(*this, "pano_val_roi_right", wxTextCtrl);
    DEBUG_ASSERT(m_ROIRightTxt);
    m_ROIRightTxt->PushEventHandler(new TextKillFocusHandler(this));

    m_ROITopTxt = XRCCTRL(*this, "pano_val_roi_top", wxTextCtrl);
    DEBUG_ASSERT(m_ROITopTxt);
    m_ROITopTxt->PushEventHandler(new TextKillFocusHandler(this));

    m_ROIBottomTxt = XRCCTRL(*this, "pano_val_roi_bottom", wxTextCtrl);
    DEBUG_ASSERT(m_ROIBottomTxt);
    m_ROIBottomTxt->PushEventHandler(new TextKillFocusHandler(this));

    m_GuideChoiceCrop = XRCCTRL(*this, "preview_guide_choice_crop", wxChoice);
    m_GuideChoiceProj = XRCCTRL(*this, "preview_guide_choice_proj", wxChoice);
    m_GuideChoiceDrag = XRCCTRL(*this, "preview_guide_choice_drag", wxChoice);
    int guide=cfg->Read(wxT("/GLPreviewFrame/guide"),0l);
    m_GuideChoiceCrop->SetSelection(guide);
    m_GuideChoiceProj->SetSelection(guide);
    m_GuideChoiceDrag->SetSelection(guide);

    flexSizer->Add(m_HFOVSlider, 0, wxEXPAND);

    m_overviewCommandPanel = wxXmlResource::Get()->LoadPanel(overview_panel,wxT("overview_command_panel"));
    m_OverviewModeChoice = XRCCTRL(*this, "overview_mode_choice", wxChoice);
    m_overviewCommandPanel->SetSize(0,0,200,20,wxSIZE_AUTO_WIDTH);

    overview_sizer->Add(m_overviewCommandPanel, 0, wxEXPAND);
    overview_sizer->Add(m_GLOverview, 1, wxEXPAND);

    bool showGrid;
    cfg->Read(wxT("/GLPreviewFrame/showPreviewGrid"),&showGrid,true);
    GetMenuBar()->FindItem(XRCID("action_show_grid"))->Check(showGrid);

    preview_panel->SetSizer(flexSizer);
    overview_panel->SetSizer(overview_sizer);

    m_mgr->AddPane(preview_panel, 
        wxAuiPaneInfo(
            ).Name(wxT("preview")
            ).MinSize(300,200
            ).CloseButton(false
            ).CaptionVisible(false
            ).Caption(_("Preview")
            ).Floatable(false
            ).Dockable(false
            ).Center(
            )
        );

    m_mgr->AddPane(overview_panel, 
        wxAuiPaneInfo(
            ).Name(wxT("overview")
            ).MinSize(300,200
            ).CloseButton(false
            ).CaptionVisible(
            ).Caption(_("Overview")
            ).FloatingSize(100,100
            ).FloatingPosition(500,500
            ).Dockable(true
            ).PinButton(
            ).Left(
            ).Show(!overview_hidden
            )
        );


    m_topsizer->Add(vis_panel, 1, wxEXPAND);

    //assistant related controls
    m_imagesText = XRCCTRL(*this, "ass_load_images_text", wxStaticText);
    DEBUG_ASSERT(m_imagesText);

    m_lensTypeChoice = XRCCTRL(*this, "ass_lens_type", wxChoice);
    DEBUG_ASSERT(m_lensTypeChoice);
    FillLensProjectionList(m_lensTypeChoice);
    m_lensTypeChoice->SetSelection(0);

    m_focalLengthText = XRCCTRL(*this, "ass_focal_length", wxTextCtrl);
    DEBUG_ASSERT(m_focalLengthText);
    m_focalLengthText->PushEventHandler(new TextKillFocusHandler(this));

    m_cropFactorText = XRCCTRL(*this, "ass_crop_factor", wxTextCtrl);
    DEBUG_ASSERT(m_cropFactorText);
    m_cropFactorText->PushEventHandler(new TextKillFocusHandler(this));

    m_alignButton = XRCCTRL(*this, "ass_align_button", wxButton);
    DEBUG_ASSERT(m_alignButton);
    m_alignButton->Disable();

    m_createButton = XRCCTRL(*this, "ass_create_button", wxButton);
    DEBUG_ASSERT(m_createButton);
    m_createButton->Disable();

    m_ProjectionChoice = XRCCTRL(*this,"projection_choice",wxChoice);

    /* populate with all available projection types */
    int nP = panoProjectionFormatCount();
    for(int n=0; n < nP; n++) {
        pano_projection_features proj;
        if (panoProjectionFeaturesQuery(n, &proj)) {
            wxString str2(proj.name, wxConvLocal);
            m_ProjectionChoice->Append(wxGetTranslation(str2));
        }
    }
    m_ProjectionChoice->SetSelection(2);

    //////////////////////////////////////////////////////
    // Blend mode
    // remaining blend mode should be added after OpenGL context has been created
    // see FillBlendMode()
    m_differenceIndex = -1;
    // create choice item
    m_BlendModeChoice = XRCCTRL(*this,"blend_mode_choice",wxChoice);
    m_BlendModeChoice->Append(_("normal"));
    m_BlendModeChoice->SetSelection(0);

    m_DragModeChoice = XRCCTRL(*this, "drag_mode_choice", wxChoice);
    SetGuiLevel(GUI_SIMPLE);
    bool individualDrag;
    cfg->Read(wxT("/GLPreviewFrame/individualDragMode"), &individualDrag, false);
    if(individualDrag)
    {
        m_DragModeChoice->SetSelection(1);
    }
    else
    {
        m_DragModeChoice->SetSelection(0);
    };
    // default drag mode
    GLPreviewFrame::DragChoiceLayout(0);

    // TODO implement hdr display in OpenGL, if possible?
    // Disabled until someone can figure out HDR display in OpenGL.
    /*
    //////////////////////////////////////////////////////
    // LDR, HDR
    blendModeSizer->Add(new wxStaticText(this, -1, _("Output:")),
                        0,        // not vertically strechable
                        wxALL | wxALIGN_CENTER_VERTICAL, // draw border all around
                        5);       // border width

    m_choices[0] = _("LDR");
    m_choices[1] = _("HDR");
    m_outputModeChoice = new wxChoice(this, ID_OUTPUTMODE_CHOICE,
                                      wxDefaultPosition, wxDefaultSize,
                                      2, m_choices);
    m_outputModeChoice->SetSelection(0);
    blendModeSizer->Add(m_outputModeChoice,
                        0,
                        wxALL | wxALIGN_CENTER_VERTICAL,
                        5);
    */
    
    /////////////////////////////////////////////////////
    // exposure
    m_defaultExposureBut = XRCCTRL(*this, "exposure_default_button", wxBitmapButton);

    m_exposureTextCtrl = XRCCTRL(*this, "exposure_text", wxTextCtrl);
    m_exposureSpinBut = XRCCTRL(*this, "exposure_spin", wxSpinButton); 
    m_exposureSpinBut->SetValue(0);

    m_projection_panel = XRCCTRL(*this, "projection_panel", wxPanel);
    m_projParamSizer = new wxBoxSizer(wxHORIZONTAL);

    wxBitmapButton * resetProjButton=new wxBitmapButton(m_projection_panel, PROJ_PARAM_RESET_ID, 
        wxArtProvider::GetBitmap(wxART_REDO));
    resetProjButton->SetToolTip(_("Resets the projection's parameters to their default values."));
    m_projParamSizer->Add(resetProjButton, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);

    m_projParamNamesLabel.resize(PANO_PROJECTION_MAX_PARMS);
    m_projParamTextCtrl.resize(PANO_PROJECTION_MAX_PARMS);
    m_projParamSlider.resize(PANO_PROJECTION_MAX_PARMS);

    for (int i=0; i < PANO_PROJECTION_MAX_PARMS; i++) {

        wxBoxSizer* paramBoxSizer = new wxBoxSizer(wxVERTICAL);
        m_projParamNamesLabel[i] = new wxStaticText(m_projection_panel, PROJ_PARAM_NAMES_ID+i, _("param:"));
        paramBoxSizer->Add(m_projParamNamesLabel[i],
                        0,        // not vertically strechable
                        wxLEFT | wxRIGHT, // draw border all around
                        5);       // border width
        m_projParamTextCtrl[i] = new wxTextCtrl(m_projection_panel, PROJ_PARAM_VAL_ID+i, wxT("0"),
                                    wxDefaultPosition, wxSize(35,-1), wxTE_PROCESS_ENTER);
        m_projParamTextCtrl[i]->PushEventHandler(new TextKillFocusHandler(this));
        paramBoxSizer->Add(m_projParamTextCtrl[i],
                        0,        // not vertically strechable
                        wxLEFT | wxRIGHT, // draw border all around
                        5);       // border width

        m_projParamSizer->Add(paramBoxSizer);
        m_projParamSlider[i] = new wxSlider(m_projection_panel, PROJ_PARAM_SLIDER_ID+i, 0, -90, 90);
        m_projParamSizer->Add(m_projParamSlider[i],
                        1,        // not vertically strechable
                        wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL , // draw border all around
                        5);       // border width
    }

    m_projection_panel->GetSizer()->Add(m_projParamSizer, 1, wxALIGN_CENTER_VERTICAL);

    // do not show projection param sizer
    m_projection_panel->GetSizer()->Show(m_projParamSizer, false, true);

    // the initial size as calculated by the sizers
    this->SetSizer( m_topsizer );
    m_topsizer->SetSizeHints( this );

    // set the minimize icon
#ifdef __WXMSW__
    wxIcon myIcon(huginApp::Get()->GetXRCPath() + wxT("data/hugin.ico"),wxBITMAP_TYPE_ICO);
#else
    wxIcon myIcon(huginApp::Get()->GetXRCPath() + wxT("data/hugin.png"),wxBITMAP_TYPE_PNG);
#endif
    SetIcon(myIcon);

    m_pano.addObserver(this);

    RestoreFramePosition(this, wxT("GLPreviewFrame"));
    
#ifdef __WXMSW__
    // wxFrame does have a strange background color on Windows..
    this->SetBackgroundColour(m_GLPreview->GetBackgroundColour());
#endif

    m_showProjectionHints = cfg->Read(wxT("/GLPreviewFrame/ShowProjectionHints"), HUGIN_SHOW_PROJECTION_HINTS) == 1;
    m_degDigits = wxConfigBase::Get()->Read(wxT("/General/DegreeFractionalDigitsEdit"),3);

     // tell the manager to "commit" all the changes just made
    m_mgr->Update();

    if (cfg->Read(wxT("/GLPreviewFrame/isShown"), 0l) != 0)
    {
#if defined __WXMSW__ || defined __WXMAC__
        InitPreviews();
        Show();
#else
        Show();
        LoadOpenGLLayout();
#endif
    }
    SetDropTarget(new PanoDropTarget(m_pano, true));
#if defined __WXMAC__
    Layout();
    Update();
#endif
}

void GLPreviewFrame::LoadOpenGLLayout()
{
#ifdef __WXGTK__
    if(loadedLayout)
    {
        return;
    };
    loadedLayout=true;
#endif
    wxString OpenGLLayout=wxConfig::Get()->Read(wxT("/GLPreviewFrame/OpenGLLayout"));
    if(!OpenGLLayout.IsEmpty())
    {
        m_mgr->LoadPerspective(OpenGLLayout,true);
#ifdef __WXGTK__
        if(!GetMenuBar()->FindItem(XRCID("action_show_overview"))->IsChecked())
        {
            wxAuiPaneInfo &inf = m_mgr->GetPane(wxT("overview"));
            if (inf.IsOk())
            {
                inf.Hide();
            }
            m_GLOverview->SetActive(false);
        };
        m_mgr->Update();
        wxShowEvent dummy;
        dummy.SetShow(true);
        OnShowEvent(dummy);
#endif
    };
    FillBlendChoice();
};

GLPreviewFrame::~GLPreviewFrame()
{
    DEBUG_TRACE("dtor writing config");
    wxConfigBase * cfg = wxConfigBase::Get();

    StoreFramePosition(this, wxT("GLPreviewFrame"));

    if ( (!this->IsIconized()) && (! this->IsMaximized()) && this->IsShown()) {
        cfg->Write(wxT("/GLPreviewFrame/isShown"), 1l);
    } else {
        cfg->Write(wxT("/GLPreviewFrame/isShown"), 0l);
    }

    cfg->Write(wxT("/GLPreviewFrame/blendMode"), m_BlendModeChoice->GetSelection());
    cfg->Write(wxT("/GLPreviewFrame/OpenGLLayout"), m_mgr->SavePerspective());
    cfg->Write(wxT("/GLPreviewFrame/overview_hidden"), !(GetMenuBar()->FindItem(XRCID("action_show_overview"))->IsChecked()));
    cfg->Write(wxT("/GLPreviewFrame/showPreviewGrid"), GetMenuBar()->FindItem(XRCID("action_show_grid"))->IsChecked());
    cfg->Write(wxT("/GLPreviewFrame/individualDragMode"), individualDragging());
    cfg->Write(wxT("/GLPreviewFrame/guide"),m_GuideChoiceProj->GetSelection());
    
    // delete all of the tools. When the preview is never used we never get an
    // OpenGL context and therefore don't create the tools.
    if (crop_tool)
    {
        preview_helper->DeactivateTool(crop_tool); delete crop_tool;
        preview_helper->DeactivateTool(drag_tool); delete drag_tool;
        preview_helper->DeactivateTool(color_picker_tool); delete color_picker_tool;
        preview_helper->DeactivateTool(edit_cp_tool); delete edit_cp_tool;
        preview_helper->DeactivateTool(identify_tool); delete identify_tool;
        preview_helper->DeactivateTool(difference_tool); delete difference_tool;
        preview_helper->DeactivateTool(pano_mask_tool); delete pano_mask_tool;
        preview_helper->DeactivateTool(preview_control_point_tool); delete preview_control_point_tool;
        preview_helper->DeactivateTool(m_preview_layoutLinesTool); delete m_preview_layoutLinesTool;
        preview_helper->DeactivateTool(preview_projection_grid); delete preview_projection_grid;
        preview_helper->DeactivateTool(preview_guide_tool); delete preview_guide_tool;
    }
    if (panosphere_overview_identify_tool) {
        panosphere_overview_helper->DeactivateTool(overview_drag_tool); delete overview_drag_tool;
        panosphere_overview_helper->DeactivateTool(panosphere_overview_camera_tool); delete panosphere_overview_camera_tool;
        panosphere_overview_helper->DeactivateTool(panosphere_overview_identify_tool); delete panosphere_overview_identify_tool;
        panosphere_overview_helper->DeactivateTool(panosphere_sphere_tool); delete panosphere_sphere_tool;
        panosphere_overview_helper->DeactivateTool(overview_projection_grid); delete overview_projection_grid;
        panosphere_overview_helper->DeactivateTool(overview_outlines_tool); delete overview_outlines_tool;
        panosphere_overview_helper->DeactivateTool(panosphere_difference_tool); delete panosphere_difference_tool;
        panosphere_overview_helper->DeactivateTool(m_panosphere_layoutLinesTool); delete m_panosphere_layoutLinesTool;
        panosphere_overview_helper->DeactivateTool(panosphere_control_point_tool); delete panosphere_control_point_tool;
    }
    if (plane_overview_identify_tool) {
        plane_overview_helper->DeactivateTool(plane_overview_identify_tool); delete plane_overview_identify_tool;
        plane_overview_helper->DeactivateTool(plane_overview_camera_tool); delete plane_overview_camera_tool;
        plane_overview_helper->DeactivateTool(plane_difference_tool); delete plane_difference_tool;
        plane_overview_helper->DeactivateTool(plane_overview_outlines_tool); delete plane_overview_outlines_tool;
        plane_overview_helper->DeactivateTool(m_plane_layoutLinesTool); delete m_plane_layoutLinesTool;
        plane_overview_helper->DeactivateTool(plane_control_point_tool); delete plane_control_point_tool;
    }
    m_focalLengthText->PopEventHandler(true);
    m_cropFactorText->PopEventHandler(true);
    m_exposureText->PopEventHandler(true);
    m_HFOVText->PopEventHandler(true);
    m_VFOVText->PopEventHandler(true);
    m_ROILeftTxt->PopEventHandler(true);
    m_ROIRightTxt->PopEventHandler(true);
    m_ROITopTxt->PopEventHandler(true);
    m_ROIBottomTxt->PopEventHandler(true);
    for (int i=0; i < m_ToggleButtons.size(); i++)
    {
        m_ToggleButtons[i]->PopEventHandler(true);
        m_GroupToggleButtons[i]->PopEventHandler(true);
    }
    for (int i=0; i < PANO_PROJECTION_MAX_PARMS; i++)
    {
        m_projParamTextCtrl[i]->PopEventHandler(true);
    };
    m_pano.removeObserver(this);

     // deinitialize the frame manager
     m_mgr->UnInit();
     if (m_mgr) {
        delete m_mgr;
     }
     if(m_guiLevel!=GUI_SIMPLE)
     {
         delete m_filemenuSimple;
     }
     else
     {
         delete m_filemenuAdvanced;
     };

    DEBUG_TRACE("dtor end");
}

void GLPreviewFrame::InitPreviews()
{
    if(preview_helper==NULL || panosphere_overview_helper==NULL || plane_overview_helper==NULL)
    {
        m_GLPreview->SetUpContext();
        m_GLOverview->SetUpContext();
        LoadOpenGLLayout();
    };
};

/**
* Update tools and GUI elements according to blend mode choice
*/
void GLPreviewFrame::updateBlendMode()
{
    if (m_BlendModeChoice != NULL)
    {
        int index=m_BlendModeChoice->GetSelection();
        if(index==0)
        {
            // normal mode
            if (preview_helper != NULL 
                && difference_tool != NULL)
            {
                preview_helper->DeactivateTool(difference_tool);
            };

            if (panosphere_overview_helper != NULL 
                && panosphere_difference_tool != NULL)
            {
                panosphere_overview_helper->DeactivateTool(panosphere_difference_tool);
            };

            if (plane_overview_helper != NULL 
                && plane_difference_tool != NULL)
            {
                plane_overview_helper->DeactivateTool(plane_difference_tool);
            };


        }
        else
        {
            if(index==m_differenceIndex)
            {
                // difference mode
                if (preview_helper != NULL 
                    && identify_tool != NULL 
                    && difference_tool != NULL )
                {
                    identify_tool->setConstantOn(false);
//                    preview_helper->DeactivateTool(identify_tool);
                    m_identify_togglebutton->SetValue(false);
                    preview_helper->ActivateTool(difference_tool);
                    CleanButtonColours();
                };

                // difference mode
                if (panosphere_overview_helper != NULL 
                    && panosphere_overview_identify_tool != NULL 
                    && panosphere_difference_tool != NULL)
                {
//                    panosphere_overview_helper->DeactivateTool(panosphere_overview_identify_tool);
                    panosphere_overview_identify_tool->setConstantOn(false);
                    panosphere_overview_helper->ActivateTool(panosphere_difference_tool);
                };

                // difference mode
                if (plane_overview_helper != NULL 
                    && plane_overview_identify_tool != NULL 
                    && plane_difference_tool != NULL)
                {
//                    plane_overview_helper->DeactivateTool(plane_overview_identify_tool);
                    plane_overview_identify_tool->setConstantOn(false);
                    plane_overview_helper->ActivateTool(plane_difference_tool);
                };

            }
            else
            {
                DEBUG_WARN("Unknown blend mode selected");
            };
        }
    }
}

void GLPreviewFrame::UpdateRoiDisplay(const HuginBase::PanoramaOptions opts)
{
    m_ROILeftTxt->ChangeValue(wxString::Format(wxT("%d"), opts.getROI().left() ));
    m_ROIRightTxt->ChangeValue(wxString::Format(wxT("%d"), opts.getROI().right() ));
    m_ROITopTxt->ChangeValue(wxString::Format(wxT("%d"), opts.getROI().top() ));
    m_ROIBottomTxt->ChangeValue(wxString::Format(wxT("%d"), opts.getROI().bottom() ));
};

void GLPreviewFrame::panoramaChanged(HuginBase::Panorama &pano)
{
    m_lensTypeChoice->Enable(pano.getNrOfImages()>0);
    m_focalLengthText->Enable(pano.getNrOfImages()>0);
    m_cropFactorText->Enable(pano.getNrOfImages()>0);
    m_alignButton->Enable(pano.getNrOfImages()>0);

    if(pano.getNrOfImages()==0)
    {
        m_createButton->Disable();
        m_imagesText->SetLabel(_("No images loaded."));
    }
    else
    {
        bool enableCreate = false;
        // check if images are at position 0
        for (size_t i = 0; i < pano.getNrOfImages(); ++i)
        {
            const HuginBase::SrcPanoImage& img = pano.getImage(i);
            if (img.getYaw() != 0.0 || img.getPitch() != 0.0 || img.getRoll() != 0.0)
            {
                enableCreate = true;
                break;
            };
        };
        if (!enableCreate && pano.getNrOfImages() == 1)
        {
            // some more checks for single image projects
            if (pano.getOptions().getProjection() != HuginBase::PanoramaOptions::EQUIRECTANGULAR)
            {
                enableCreate = true;
            };
            if (pano.getOptions().getROI() != vigra::Rect2D(pano.getOptions().getSize()))
            {
                enableCreate = true;
            };
        };
        // disable create button after loading images
        const std::string lastCmd=PanoCommand::GlobalCmdHist::getInstance().getLastCommandName();
        if (lastCmd == "add images" || lastCmd== "add and distribute images")
        {
            enableCreate = false;
        }
        m_createButton->Enable(enableCreate);

        // in wxWidgets 2.9, format must have types that exactly match.
        // However std::size_t could be any unsiged integer, so we cast it to
        // unsigned long.int to be on the safe side.
        wxString imgMsg = wxString::Format(_("%lu images loaded."), (unsigned long int) pano.getNrOfImages());
        m_imagesText->SetLabel(imgMsg);

        // update data in lens display
        const HuginBase::SrcPanoImage& img = pano.getImage(0);
        SelectListValue(m_lensTypeChoice, img.getProjection());
        double focal_length = HuginBase::SrcPanoImage::calcFocalLength(img.getProjection(), img.getHFOV(), img.getCropFactor(), img.getSize());
        // use ChangeValue explicit, SetValue would create EVT_TEXT event which collides with our TextKillFocusHandler
        m_focalLengthText->ChangeValue(hugin_utils::doubleTowxString(focal_length,m_degDigits));
        m_cropFactorText->ChangeValue(hugin_utils::doubleTowxString(img.getCropFactor(),m_degDigits));
    }

    if (pano.getNrOfImages() > 1)
    {
        // in wxWidgets 2.9, format must have types that exactly match.
        // However std::size_t could be any unsiged integer, so we cast it to
        // unsigned long.int to be on the safe side.
        wxString alignMsg = wxString::Format(_("Images are connected by %lu control points.\n"), (unsigned long int) pano.getCtrlPoints().size());

        if (m_pano.getNrOfCtrlPoints() > 0)
        {
            // find components..
            HuginGraph::ImageGraph graph(m_pano);
            const HuginGraph::ImageGraph::Components comps = graph.GetComponents();
            if (comps.size() > 1)
            {
                alignMsg += wxString::Format(_("%lu unconnected image groups found: %s\n"), static_cast<unsigned long int>(comps.size()), Components2Str(comps).c_str());
            }
            else
            {
                if (m_pano.needsOptimization())
                {
                    alignMsg += _("Images or control points have changed, new alignment is needed.");
                }
                else
                {
                    HuginBase::CalculateCPStatisticsError calcStats(m_pano, MainFrame::Get()->GetOptimizeOnlyActiveImages(), MainFrame::Get()->GetOptimizeIgnoreLineCp());
                    calcStats.run();
                    const double max = calcStats.getResultMax();
                    const double mean = calcStats.getResultMean();

                    if (max != 0.0)
                    {
                        alignMsg = alignMsg + wxString::Format(_("Mean error after optimization: %.1f pixel, max: %.1f"), mean, max); 
                    }
                }
            }
        }
        XRCCTRL(*this, "ass_status_text", wxStaticText)->SetLabel(alignMsg);
        m_tool_notebook->GetPage(0)->Layout();
        Refresh();
    }
    else
    {
        XRCCTRL(*this, "ass_status_text", wxStaticText)->SetLabel(wxT(""));
    };

    GetMenuBar()->Enable(XRCID("ID_EDITUNDO"), PanoCommand::GlobalCmdHist::getInstance().canUndo());
    GetMenuBar()->Enable(XRCID("ID_EDITREDO"), PanoCommand::GlobalCmdHist::getInstance().canRedo());

    // TODO: update meaningful help text and dynamic links to relevant tabs

    const HuginBase::PanoramaOptions & opts = pano.getOptions();

    wxString projection;
    m_ProjectionChoice->SetSelection(opts.getProjection());
    m_VFOVSlider->Enable( opts.fovCalcSupported(opts.getProjection()) );
    
    // No HDR display yet.
    /*
    m_outputModeChoice->SetSelection(opts.outputMode);
    if (opts.outputMode == PanoramaOptions::OUTPUT_HDR) {
        m_exposureTextCtrl->Hide();
        m_defaultExposureBut->Hide();
        m_decExposureBut->Hide();
        m_incExposureBut->Hide();
    } else {
        m_exposureTextCtrl->Show();
        m_defaultExposureBut->Show();
        m_decExposureBut->Show();
        m_incExposureBut->Show();
    }*/
    m_exposureTextCtrl->ChangeValue(wxString(hugin_utils::doubleToString(opts.outputExposureValue,2).c_str(), wxConvLocal));

    bool activeImgs = pano.getActiveImages().size() > 0;

    // TODO: enable display of parameters and set their limits, if projection has some.

    int nParam = opts.m_projFeatures.numberOfParameters;
    bool relayout = false;
    // if the projection format has changed
    if (opts.getProjection() != m_oldProjFormat) {
        DEBUG_DEBUG("Projection format changed");
        if (nParam) {
            // show parameters and update labels.
            m_projection_panel->GetSizer()->Show(m_projParamSizer, true, true);
            int i;
            for (i=0; i < nParam; i++) {
                const pano_projection_parameter * pp = & (opts.m_projFeatures.parm[i]);
                wxString str2(pp->name, wxConvLocal);
                str2 = wxGetTranslation(str2);
                m_projParamNamesLabel[i]->SetLabel(str2);
                m_projParamSlider[i]->SetRange(hugin_utils::roundi(pp->minValue), hugin_utils::roundi(pp->maxValue));
            }
            for(;i < PANO_PROJECTION_MAX_PARMS; i++) {
                m_projParamNamesLabel[i]->Hide();
                m_projParamSlider[i]->Hide();
                m_projParamTextCtrl[i]->Hide();
            }
            relayout = true;
        } else {
            m_projection_panel->GetSizer()->Show(m_projParamSizer, false, true);
            relayout = true;
        }
    }
    if (nParam) {
        // display new values
        std::vector<double> params = opts.getProjectionParameters();
        assert((int) params.size() == nParam);
        for (int i=0; i < nParam; i++) {
            wxString val = wxString(hugin_utils::doubleToString(params[i],1).c_str(), wxConvLocal);
            m_projParamTextCtrl[i]->ChangeValue(wxString(val.wc_str(), wxConvLocal));
            m_projParamSlider[i]->SetValue(hugin_utils::roundi(params[i]));
        }
    }
    if (relayout) {
        m_projection_panel->Layout();
        Refresh();
    }
    SetStatusText(wxString::Format(wxT("%.1f x %.1f"), opts.getHFOV(), opts.getVFOV()),2);
    m_HFOVSlider->SetValue(hugin_utils::roundi(opts.getHFOV()));
    m_VFOVSlider->SetValue(hugin_utils::roundi(opts.getVFOV()));
    std::string val;
    val = hugin_utils::doubleToString(opts.getHFOV(),1);
    m_HFOVText->ChangeValue(wxString(val.c_str(), wxConvLocal));
    val = hugin_utils::doubleToString(opts.getVFOV(),1);
    m_VFOVText->ChangeValue(wxString(val.c_str(), wxConvLocal));
    m_VFOVText->Enable(opts.fovCalcSupported(opts.getProjection()));

    m_oldProjFormat = opts.getProjection();

    XRCCTRL(*this,"preview_autocrop_tool",wxButton)->Enable(activeImgs);
    XRCCTRL(*this,"preview_stack_autocrop_tool",wxButton)->Enable(activeImgs);
    UpdateRoiDisplay(opts);
    
    if(m_showProjectionHints)
    {
        ShowProjectionWarnings();
    };
    redrawPreview();
}

void GLPreviewFrame::panoramaImagesChanged(HuginBase::Panorama &pano, const HuginBase::UIntSet &changed)
{
    DEBUG_TRACE("");

    bool dirty = false;

    unsigned int nrImages = pano.getNrOfImages();
    unsigned int nrButtons = m_ToggleButtons.size();
    
    GetMenuBar()->Enable(XRCID("action_optimize"), nrImages > 0);

//    m_displayedImgs.clear();

    // remove items for nonexisting images
    for (int i=nrButtons-1; i>=(int)nrImages; i--)
    {
        m_ButtonSizer->Detach(m_ToggleButtonPanel[i]);
        // Image toggle buttons have a event handler on the stack which
        // must be removed before the buttons get destroyed.
        m_ToggleButtons[i]->PopEventHandler();
        m_GroupToggleButtons[i]->PopEventHandler();
        delete m_ToggleButtons[i];
        delete m_GroupToggleButtons[i];
        delete m_ToggleButtonPanel[i];
        m_ToggleButtons.pop_back();
        m_GroupToggleButtons.pop_back();
        m_ToggleButtonPanel.pop_back();
        delete toogle_button_event_handlers[i];
        toogle_button_event_handlers.pop_back();
        delete toggle_group_button_event_handlers[i];
        toggle_group_button_event_handlers.pop_back();
        dirty = true;
    }

    //change overview mode to panosphere if the translation plane parameter are non zero
    if (m_GLOverview->GetMode() == GLOverview::PLANE)
    {
        if (HasNonZeroTranslationPlaneParameters())
        {
            m_GLOverview->SetMode(GLOverview::PANOSPHERE);
            m_OverviewModeChoice->SetSelection(0);
            //check if drag mode is mosaic, if so reset to normal
            if(drag_tool)
            {
                if(drag_tool->getDragMode()==DragTool::drag_mode_mosaic)
                {
                    m_DragModeChoice->SetSelection(m_DragModeChoice->GetSelection()-2);
                    drag_tool->setDragMode(DragTool::drag_mode_normal);
                    EnableGroupCheckboxes(individualDragging());
                    // adjust the layout
                    DragChoiceLayout(m_DragModeChoice->GetSelection());
                };
            };
        }
    }

    // add buttons
    if ( nrImages >= nrButtons ) {
        for(HuginBase::UIntSet::const_iterator it = changed.begin(); it != changed.end(); ++it){
            if (*it >= nrButtons) {
                // create new item.
//                wxImage * bmp = new wxImage(sz.GetWidth(), sz.GetHeight());
                //put wxToggleButton in a wxPanel because 
                //on Windows the background colour of wxToggleButton can't be changed
                wxPanel *pan = new wxPanel(m_ButtonPanel);
                wxBoxSizer * siz = new wxBoxSizer(wxVERTICAL);
                pan->SetSizer(siz);
                wxToggleButton * but = new wxToggleButton(pan,
                                                          ID_TOGGLE_BUT + *it,
                                                          wxString::Format(wxT("%d"),*it),
                                                          wxDefaultPosition, wxDefaultSize,
                                                          wxBU_EXACTFIT);
                
                wxCheckBox *butcheck = new wxCheckBox(pan, wxID_ANY, wxT(""));

#if defined __WXMSW__ || defined __WXMAC__
                //we need a border around the button to see the colored panel
                //because changing backgroundcolor of wxToggleButton does not work in wxMSW
                siz->AddSpacer(5);
                siz->Add(butcheck, 0, wxALIGN_CENTRE_HORIZONTAL | wxFIXED_MINSIZE, 0);
                siz->Add(but,0,wxLEFT | wxRIGHT | wxBOTTOM , 5);
#else
                siz->Add(but,0,wxALL ,0);
                siz->Add(butcheck, 0, wxALL | wxFIXED_MINSIZE, 0);
#endif
                // for the identification tool to work, we need to find when the
                // mouse enters and exits the button. We use a custom event
                // handler, which will also toggle the images:
                ImageToogleButtonEventHandler * event_handler = new ImageToogleButtonEventHandler(*it, m_identify_togglebutton, &m_pano);
                event_handler->AddIdentifyTool(&identify_tool);
                event_handler->AddIdentifyTool(&panosphere_overview_identify_tool);
                event_handler->AddIdentifyTool(&plane_overview_identify_tool);
                toogle_button_event_handlers.push_back(event_handler);
                but->PushEventHandler(event_handler);

                ImageGroupButtonEventHandler * group_event_handler = new 
                    ImageGroupButtonEventHandler(*it, this, &m_pano);
                group_event_handler->AddDragTool((DragTool**) &drag_tool);
                group_event_handler->AddDragTool((DragTool**) &overview_drag_tool);
                group_event_handler->AddIdentifyTool(&identify_tool);
                group_event_handler->AddIdentifyTool(&panosphere_overview_identify_tool);
                group_event_handler->AddIdentifyTool(&plane_overview_identify_tool);
                toggle_group_button_event_handlers.push_back(group_event_handler);
                butcheck->PushEventHandler(group_event_handler);
                butcheck->Show(individualDragging() && m_mode==mode_drag);

                but->SetValue(true);
                m_ButtonSizer->Add(pan,
                                   0,
                                   wxLEFT | wxTOP,
                                   0);
                m_ToggleButtons.push_back(but);
                m_GroupToggleButtons.push_back(butcheck);
                m_ToggleButtonPanel.push_back(pan);
                dirty = true;
            }
        }
    }

    // update existing items
    HuginBase::UIntSet displayedImages = m_pano.getActiveImages();
    for (unsigned i=0; i < nrImages; i++) {
        m_ToggleButtons[i]->SetValue(set_contains(displayedImages, i));
        wxFileName tFilename(wxString (pano.getImage(i).getFilename().c_str(), HUGIN_CONV_FILENAME));
        m_ToggleButtons[i]->SetToolTip(tFilename.GetFullName());
    }

    if (dirty) {
        m_ButtonSizer->FitInside(m_ButtonPanel);
		Layout();
		DEBUG_INFO("New m_ButtonPanel width: " << (m_ButtonPanel->GetSize()).GetWidth());
		DEBUG_INFO("New m_ButtonPanel Height: " << (m_ButtonPanel->GetSize()).GetHeight());
    }
    if(nrImages==0)
    {
        SetMode(mode_assistant);
        m_tool_notebook->ChangeSelection(mode_assistant);
    };
    for(size_t i=2; i<m_tool_notebook->GetPageCount();i++)
    {
        m_tool_notebook->GetPage(i)->Enable(nrImages!=0);
    };
    redrawPreview();
}

void GLPreviewFrame::redrawPreview()
{
    m_GLPreview->Refresh();
    m_GLOverview->Refresh();
}

void GLPreviewFrame::OnShowEvent(wxShowEvent& e)
{

    DEBUG_TRACE("OnShow");
    bool toggle_on = GetMenuBar()->FindItem(XRCID("action_show_overview"))->IsChecked();
    wxAuiPaneInfo &inf = m_mgr->GetPane(wxT("overview"));
    if (inf.IsOk()) {
        if (e.IsShown()) {
            if (!inf.IsShown() && toggle_on ) {
                inf.Show();
                m_mgr->Update();
            }
        } else {
            if (inf.IsFloating() && inf.IsShown()) {
                DEBUG_DEBUG("hiding overview float");
                inf.Hide();
                m_mgr->Update();
            }
        }
    }

}

//the following methods are to substitude the GLViewer KeyUp and KeyDown methods
//so that tools use key events that happen globally to the preview frame
//however only key up event is sent, and not key down
//so until this is resolved they will remain to be handled by GLViewer
void GLPreviewFrame::KeyDown(wxKeyEvent& e)
{
    if (preview_helper) {
        preview_helper->KeypressEvent(e.GetKeyCode(), e.GetModifiers(), true);
    }
    if (GetMenuBar()->FindItem(XRCID("action_show_overview"))->IsChecked()) {
        if(m_GLOverview->GetMode() == GLOverview::PLANE) {
            if (plane_overview_helper) {
                plane_overview_helper->KeypressEvent(e.GetKeyCode(), e.GetModifiers(), true);
            }
        } else if (m_GLOverview->GetMode() == GLOverview::PANOSPHERE) {
            if (panosphere_overview_helper) {
                panosphere_overview_helper->KeypressEvent(e.GetKeyCode(), e.GetModifiers(), true);
            }
        }
    
    }
    e.Skip();
}

void GLPreviewFrame::KeyUp(wxKeyEvent& e)
{
    if (preview_helper) {
        preview_helper->KeypressEvent(e.GetKeyCode(), e.GetModifiers(), false);
    }
    if (GetMenuBar()->FindItem(XRCID("action_show_overview"))->IsChecked()) {
        if(m_GLOverview->GetMode() == GLOverview::PLANE) {
            if (plane_overview_helper) {
                plane_overview_helper->KeypressEvent(e.GetKeyCode(), e.GetModifiers(), false);
            }
        } else if (m_GLOverview->GetMode() == GLOverview::PANOSPHERE) {
            if (panosphere_overview_helper) {
                panosphere_overview_helper->KeypressEvent(e.GetKeyCode(), e.GetModifiers(), false);
            }
        }
    
    }
    e.Skip();
}



void GLPreviewFrame::OnOverviewToggle(wxCommandEvent& e)
{
    DEBUG_TRACE("overview toggle");
    bool toggle_on = GetMenuBar()->FindItem(XRCID("action_show_overview"))->IsChecked();
    wxAuiPaneInfo &inf = m_mgr->GetPane(wxT("overview"));
    if (inf.IsOk()) {
        if (inf.IsShown() && !toggle_on) {
            inf.Hide();
            m_GLOverview->SetActive(false);
            m_mgr->Update();
        } else if (!(inf.IsShown() && toggle_on)) {
            inf.Show();
#if defined __WXMSW__ || defined __WXMAC__
            m_GLOverview->SetActive(true);
            m_mgr->Update();
#else
            m_mgr->Update();
            m_GLOverview->SetActive(true);
#endif
        }
    }
}

void GLPreviewFrame::OnSwitchPreviewGrid(wxCommandEvent & e)
{
    if(GetMenuBar()->FindItem(XRCID("action_show_grid"))->IsChecked())
    {
        preview_helper->ActivateTool(preview_projection_grid);
        panosphere_overview_helper->ActivateTool(overview_projection_grid);
    }
    else
    {
        preview_helper->DeactivateTool(preview_projection_grid);
        panosphere_overview_helper->DeactivateTool(overview_projection_grid);
    }
    m_GLPreview->Refresh();
    m_GLOverview->Refresh();
}

void GLPreviewFrame::OnClose(wxCloseEvent& event)
{
    DEBUG_TRACE("OnClose")
    // do not close, just hide if we're not forced
    if(m_guiLevel==GUI_SIMPLE)
    {
        if(!MainFrame::Get()->CloseProject(event.CanVeto(), MainFrame::CLOSE_PROGRAM))
        {
           if (event.CanVeto())
           {
               event.Veto();
               return;
           };
        };
        MainFrame::Get()->Close(true);
    }
    else
    {
        if (event.CanVeto())
        {
            event.Veto();
            Hide();
            DEBUG_DEBUG("hiding");
        }
        else
        {
            DEBUG_DEBUG("closing");
            this->Destroy();
        }
    };
}

#if 0
// need to add the wxChoice somewhere
void PreviewFrame::OnProjectionChanged()
{
    PanoramaOptions opt = m_pano.getOptions();
    int lt = m_ProjectionChoice->GetSelection();
    wxString Ip;
    switch ( lt ) {
    case PanoramaOptions::RECTILINEAR:       Ip = _("Rectilinear"); break;
    case PanoramaOptions::CYLINDRICAL:       Ip = _("Cylindrical"); break;
    case PanoramaOptions::EQUIRECTANGULAR:   Ip = _("Equirectangular"); break;
    }
    opt.projectionFormat = (PanoramaOptions::ProjectionFormat) lt;
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetPanoOptionsCmd( pano, opt )
        );
    DEBUG_DEBUG ("Projection changed: "  << lt << ":" << Ip )


}
#endif

void GLPreviewFrame::OnCenterHorizontally(wxCommandEvent & e)
{
    if (m_pano.getActiveImages().size() == 0) return;

    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::CenterPanoCmd(m_pano)
        );
}

void GLPreviewFrame::OnStraighten(wxCommandEvent & e)
{
    if (m_pano.getNrOfImages() == 0) return;

    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::StraightenPanoCmd(m_pano)
        );
}

void GLPreviewFrame::OnFitPano(wxCommandEvent & e)
{
    if (m_pano.getActiveImages().size() == 0) return;

    DEBUG_TRACE("");
    HuginBase::PanoramaOptions opt = m_pano.getOptions();
    HuginBase::CalculateFitPanorama fitPano(m_pano);
    fitPano.run();
    opt.setHFOV(fitPano.getResultHorizontalFOV());
    opt.setHeight(hugin_utils::roundi(fitPano.getResultHeight()));

    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
        );

    DEBUG_INFO ( "new fov: [" << opt.getHFOV() << " "<< opt.getVFOV() << "] => height: " << opt.getHeight() );
}

void GLPreviewFrame::OnShowAll(wxCommandEvent & e)
{
    if (m_pano.getNrOfImages() == 0) return;

    HuginBase::UIntSet displayedImgs;
    if (m_selectAllMode == SELECT_ALL_IMAGES)
    {
        fill_set(displayedImgs, 0, m_pano.getNrOfImages() - 1);
    }
    else
    {
        if (m_selectKeepSelection)
        {
            displayedImgs = m_pano.getActiveImages();
        };
        std::vector<HuginBase::UIntVector> stackedImg = HuginBase::getSortedStacks(&m_pano);
        for (size_t i = 0; i < stackedImg.size(); ++i)
        {
            switch (m_selectAllMode)
            {
                case SELECT_BRIGHTEST_IMAGES:
                    displayedImgs.insert(*(stackedImg[i].rbegin()));
                    break;
                case SELECT_DARKEST_IMAGES:
                    displayedImgs.insert(*(stackedImg[i].begin()));
                    break;
                case SELECT_MEDIAN_IMAGES:
                default:
                    displayedImgs.insert(stackedImg[i][stackedImg[i].size() / 2]);
                    break;
            };
        };
    };
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetActiveImagesCmd(m_pano, displayedImgs)
        );
}

void GLPreviewFrame::OnShowNone(wxCommandEvent & e)
{
    if (m_pano.getNrOfImages() == 0) return;

    DEBUG_ASSERT(m_pano.getNrOfImages() == m_ToggleButtons.size());
    for (unsigned int i=0; i < m_pano.getNrOfImages(); i++) {
        m_ToggleButtons[i]->SetValue(false);
    }
    HuginBase::UIntSet displayedImgs;
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetActiveImagesCmd(m_pano, displayedImgs)
        );
}

void GLPreviewFrame::OnNumTransform(wxCommandEvent & e)
{
    if (m_pano.getNrOfImages() == 0) return;

    wxString text;
    double y;
    double p;
    double r;
    double x;
    double z;

    int index = m_DragModeChoice->GetSelection();
    switch (index) {
        case 0: //normal
        case 1: //normal, individual
            //@TODO limit numeric transform to selected images
            text = XRCCTRL(*this,"input_yaw",wxTextCtrl)->GetValue();
            if(!hugin_utils::stringToDouble(std::string(text.mb_str(wxConvLocal)), y))
            {
                wxBell();
                wxMessageBox(_("Yaw value must be numeric."),_("Warning"),wxOK | wxICON_ERROR,this);
                return;
            }
            text = XRCCTRL(*this,"input_pitch",wxTextCtrl)->GetValue();
            if (!hugin_utils::stringToDouble(std::string(text.mb_str(wxConvLocal)), p))
            {
                wxBell();
                wxMessageBox(_("Pitch value must be numeric."),_("Warning"),wxOK | wxICON_ERROR,this);
                return;
            }
            text = XRCCTRL(*this,"input_roll",wxTextCtrl)->GetValue();
            if (!hugin_utils::stringToDouble(std::string(text.mb_str(wxConvLocal)), r))
            {
                wxBell();
                wxMessageBox(_("Roll value must be numeric."),_("Warning"),wxOK | wxICON_ERROR,this);
                return;
            }
            PanoCommand::GlobalCmdHist::getInstance().addCommand(
                    new PanoCommand::RotatePanoCmd(m_pano, y, p, r)
                );
            break;
        case 2: //mosaic
        case 3: //mosaic, individual
            //@TODO limit numeric transform to selected images
            text = XRCCTRL(*this,"input_x",wxTextCtrl)->GetValue();
            if (!hugin_utils::stringToDouble(std::string(text.mb_str(wxConvLocal)), x))
            {
                wxBell();
                wxMessageBox(_("X value must be numeric."),_("Warning"),wxOK | wxICON_ERROR,this);
                return;
            }
            text = XRCCTRL(*this,"input_y",wxTextCtrl)->GetValue();
            if (!hugin_utils::stringToDouble(std::string(text.mb_str(wxConvLocal)), y))
            {
                wxBell();
                wxMessageBox(_("Y value must be numeric."),_("Warning"),wxOK | wxICON_ERROR,this);
                return;
            }
            text = XRCCTRL(*this,"input_z",wxTextCtrl)->GetValue();
            if(!hugin_utils::stringToDouble(std::string(text.mb_str(wxConvLocal)), z))
            {
                wxBell();
                wxMessageBox(_("Z value must be numeric."),_("Warning"),wxOK | wxICON_ERROR,this);
                return;
            }
            PanoCommand::GlobalCmdHist::getInstance().addCommand(
                    new PanoCommand::TranslatePanoCmd(m_pano, x, y, z)
                );
            break;
    }
}

void GLPreviewFrame::OnExposureChanged(wxCommandEvent & e)
{
    HuginBase::PanoramaOptions opts = m_pano.getOptions();
    // exposure
    wxString text = m_exposureTextCtrl->GetValue();
    DEBUG_INFO ("target exposure = " << text.mb_str(wxConvLocal) );
    double p = 0;
    if (text != wxT("")) {
        if (!hugin_utils::str2double(text, p)) {
            wxLogError(_("Value must be numeric."));
            return;
        }
    }
    opts.outputExposureValue = p;
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetPanoOptionsCmd( m_pano, opts )
                                           );
}

void GLPreviewFrame::OnProjParameterChanged(wxCommandEvent & e)
{
    HuginBase::PanoramaOptions opts = m_pano.getOptions();
    int nParam = opts.m_projFeatures.numberOfParameters;
    std::vector<double> para = opts.getProjectionParameters();
    for (int i = 0; i < nParam; i++) {
        if (e.GetEventObject() == m_projParamTextCtrl[i]) {
            wxString text = m_projParamTextCtrl[i]->GetValue();
            DEBUG_INFO ("param " << i << ":  = " << text.mb_str(wxConvLocal) );
            double p = 0;
            if (text != wxT("")) {
                if (!hugin_utils::str2double(text, p)) {
                    wxLogError(_("Value must be numeric."));
                    return;
                }
            }
            para[i] = p;
        }
    }
    opts.setProjectionParameters(para);
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetPanoOptionsCmd( m_pano, opts )
                                           );
}

void GLPreviewFrame::OnProjParameterReset(wxCommandEvent &e)
{
    HuginBase::PanoramaOptions opts = m_pano.getOptions();
    opts.resetProjectionParameters();
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetPanoOptionsCmd(m_pano, opts)
        );
};

void GLPreviewFrame::OnChangeFOV(wxScrollEvent & e)
{
    DEBUG_TRACE("");

    HuginBase::PanoramaOptions opt = m_pano.getOptions();

    if (e.GetEventObject() == m_HFOVSlider) {
        DEBUG_DEBUG("HFOV changed (slider): " << e.GetInt() << " == " << m_HFOVSlider->GetValue());
        opt.setHFOV(e.GetInt());
    } else if (e.GetEventObject() == m_VFOVSlider) {
        DEBUG_DEBUG("VFOV changed (slider): " << e.GetInt());
        opt.setVFOV(e.GetInt());
    } else if (e.GetEventObject() == XRCCTRL(*this,"layout_scale_slider",wxSlider)) {
        DEBUG_DEBUG("Layout scale changed (slider): " << e.GetInt());
        GLPreviewFrame::OnLayoutScaleChange(e);
    } else {
        int nParam = opt.m_projFeatures.numberOfParameters;
        std::vector<double> para = opt.getProjectionParameters();
        for (int i = 0; i < nParam; i++) {
            if (e.GetEventObject() == m_projParamSlider[i]) {
                // update
                para[i] = e.GetInt();
            }
        }
        opt.setProjectionParameters(para);
		opt.setHFOV(m_HFOVSlider->GetValue());
		opt.setVFOV(m_VFOVSlider->GetValue());
    }

    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
        );    
}

void GLPreviewFrame::OnTrackChangeFOV(wxScrollEvent & e)
{
    DEBUG_TRACE("");
    DEBUG_TRACE("fov change " << e.GetInt());
    HuginBase::PanoramaOptions opt = m_pano.getOptions();

    if (e.GetEventObject() == m_HFOVSlider) {
        opt.setHFOV(e.GetInt());
    } else if (e.GetEventObject() == m_VFOVSlider) {
        opt.setVFOV(e.GetInt());
    } else {
        int nParam = opt.m_projFeatures.numberOfParameters;
        std::vector<double> para = opt.getProjectionParameters();
        for (int i = 0; i < nParam; i++) {
            if (e.GetEventObject() == m_projParamSlider[i]) {
                // update
                para[i] = e.GetInt();
            }
        }
        opt.setProjectionParameters(para);
    }
    // we only actually update the panorama fully when the mouse is released.
    // As we are dragging it we don't want to create undo events, but we would
    // like to update the display, so we change the GLViewer's ViewState and
    // request a redraw.
    m_GLPreview->m_view_state->SetOptions(&opt);
    m_GLPreview->Refresh();
}

void GLPreviewFrame::OnBlendChoice(wxCommandEvent & e)
{
    if (e.GetEventObject() == m_BlendModeChoice)
    {
        updateBlendMode();
    }
    else
    {
        // FIXME DEBUG_WARN("wxChoice event from unknown object received");
    }
}

void GLPreviewFrame::OnDragChoice(wxCommandEvent & e)
{
    if (drag_tool)
    {
        DragTool::DragMode newDragMode=DragTool::drag_mode_normal;
        int index = m_DragModeChoice->GetSelection();
        switch (index) {
            case 0: //normal
            case 1:
                newDragMode=DragTool::drag_mode_normal;
                break;
            case 2: //mosaic
            case 3:
                newDragMode=DragTool::drag_mode_mosaic;
                break;
        }
        if(newDragMode==DragTool::drag_mode_mosaic)
        {
            if(HasNonZeroTranslationPlaneParameters())
            {
                if(wxMessageBox(_("The mosaic/plane mode works only correct for a remapping plane of yaw=0 and pitch=0.\nBut your project has non-zero Tpy and Tpp parameters.\nShould the Tpy and Tpp parameters reset to zero?"),
#ifdef __WXMSW__
                    _("Hugin"),
#else
                    wxEmptyString,
#endif
                    wxYES_NO | wxICON_QUESTION, this) == wxYES)
                {
                    ResetTranslationPlaneParameters();
                }
                else
                {
                    m_DragModeChoice->SetSelection(index-2);
                    return;
                };
            };
            //switch overview mode to plane
            UpdateOverviewMode(1);
            m_OverviewModeChoice->SetSelection(1);
        }
        else
        {
            //new mode is normal
            //set overview back to panosphere mode
            UpdateOverviewMode(0);
            m_OverviewModeChoice->SetSelection(0);
            m_GLOverview->m_visualization_state->ForceRequireRedraw();
            m_GLOverview->m_visualization_state->SetDirtyViewport();
        };
        //update drag mode
        drag_tool->setDragMode(newDragMode);
        EnableGroupCheckboxes(individualDragging());
        // adjust the layout
        DragChoiceLayout(index);
    };
};

bool GLPreviewFrame::HasNonZeroTranslationPlaneParameters()
{
    size_t nr = m_pano.getNrOfImages();
    for (size_t i = 0 ; i < nr; i++)
    {
        if (m_pano.getSrcImage(i).getTranslationPlaneYaw() != 0 ||
            m_pano.getSrcImage(i).getTranslationPlanePitch() != 0)
        {
            return true;
        };
    }
    return false;
};

void GLPreviewFrame::ResetTranslationPlaneParameters()
{
    HuginBase::UIntSet imgs;
    HuginBase::Panorama newPan = m_pano.duplicate();
    size_t nr = newPan.getNrOfImages();
    for (size_t i = 0 ; i < nr ; i++)
    {
        HuginBase::SrcPanoImage img = newPan.getSrcImage(i);
        img.setTranslationPlaneYaw(0);
        img.setTranslationPlanePitch(0);
        newPan.setSrcImage(i,img);
        imgs.insert(i);
    }
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::UpdateImagesVariablesCmd(m_pano, imgs, newPan.getVariables())
    );
};

bool GLPreviewFrame::UpdateOverviewMode(int newMode)
{
    GLOverview::OverviewMode newOverviewMode=GLOverview::PANOSPHERE;
    if(newMode==1)
    {
        newOverviewMode=GLOverview::PLANE;
    };
    if(m_GLOverview->GetMode()==newOverviewMode)
    {
        return true;
    };
    if (m_GLOverview->GetMode() == GLOverview::PANOSPHERE)
    {
        if (!HasNonZeroTranslationPlaneParameters())
        {
            m_GLOverview->SetMode(GLOverview::PLANE);
            return true;
        }
        else
        {
            if(wxMessageBox(_("The mosaic/plane mode works only correct for a remapping plane of yaw=0 and pitch=0.\nBut your project has non-zero Tpy and Tpp parameters.\nShould the Tpy and Tpp parameters reset to zero?"),
#ifdef __WXMSW__
                _("Hugin"),
#else
                wxEmptyString,
#endif
                wxYES_NO | wxICON_QUESTION, this) == wxYES)
            {
                ResetTranslationPlaneParameters();
                m_GLOverview->SetMode(GLOverview::PLANE);
                return true;
            }
            else
            {
                return false;
            };
        };
    }
    else
    {
        m_GLOverview->SetMode(GLOverview::PANOSPHERE);
        return true;
    }
};

void GLPreviewFrame::OnOverviewModeChoice( wxCommandEvent & e)
{
    if(UpdateOverviewMode(m_OverviewModeChoice->GetSelection()))
    {
        m_GLOverview->m_visualization_state->ForceRequireRedraw();
        m_GLOverview->m_visualization_state->SetDirtyViewport();
        //set drag mode to normal if new mode is panosphere mode
        if(m_GLOverview->GetMode()==GLOverview::PANOSPHERE && m_DragModeChoice->GetSelection()>1)
        {
            m_DragModeChoice->SetSelection(m_DragModeChoice->GetSelection()-2);
            OnDragChoice(e);
        };
    }
    else
    {
        //change mode was not successful or canceled by user, set mode choice back
        if(m_GLOverview->GetMode()==GLOverview::PANOSPHERE)
        {
            m_OverviewModeChoice->SetSelection(0);
        }
        else
        {
            m_OverviewModeChoice->SetSelection(1);
        };
    };
};

void GLPreviewFrame::DragChoiceLayout( int index )
{
    // visibility of controls based on selected drag mode
    bool normalMode=index==0 || index==1;
    XRCCTRL(*this,"label_yaw",wxStaticText)->Show(normalMode);
    XRCCTRL(*this,"input_yaw",wxTextCtrl)->Show(normalMode);
    XRCCTRL(*this,"label_pitch",wxStaticText)->Show(normalMode);
    XRCCTRL(*this,"input_pitch",wxTextCtrl)->Show(normalMode);
    XRCCTRL(*this,"label_roll",wxStaticText)->Show(normalMode);
    XRCCTRL(*this,"input_roll",wxTextCtrl)->Show(normalMode);
    XRCCTRL(*this,"label_x",wxStaticText)->Show(!normalMode);
    XRCCTRL(*this,"input_x",wxTextCtrl)->Show(!normalMode);
    XRCCTRL(*this,"label_y",wxStaticText)->Show(!normalMode);
    XRCCTRL(*this,"input_y",wxTextCtrl)->Show(!normalMode);
    XRCCTRL(*this,"label_z",wxStaticText)->Show(!normalMode);
    XRCCTRL(*this,"input_z",wxTextCtrl)->Show(!normalMode);
    // redraw layout to compress empty space
    XRCCTRL(*this,"apply_num_transform",wxButton)->GetParent()->Layout();
}

void GLPreviewFrame::OnDefaultExposure( wxCommandEvent & e )
{
    if (m_pano.getNrOfImages() > 0) {
        HuginBase::PanoramaOptions opt = m_pano.getOptions();
        opt.outputExposureValue = HuginBase::CalculateMeanExposure::calcMeanExposure(m_pano);
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
                                               );
    }
}

void GLPreviewFrame::OnIncreaseExposure( wxSpinEvent & e )
{
    HuginBase::PanoramaOptions opt = m_pano.getOptions();
    opt.outputExposureValue = opt.outputExposureValue + 1.0/3;
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
                                            );
}

void GLPreviewFrame::OnDecreaseExposure( wxSpinEvent & e )
{
    HuginBase::PanoramaOptions opt = m_pano.getOptions();
    opt.outputExposureValue = opt.outputExposureValue - 1.0/3;
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
                                           );
}

void GLPreviewFrame::OnProjectionChoice( wxCommandEvent & e )
{
    if (e.GetEventObject() == m_ProjectionChoice) {
        HuginBase::PanoramaOptions opt = m_pano.getOptions();
        int lt = m_ProjectionChoice->GetSelection();
        wxString Ip;
        opt.setProjection((HuginBase::PanoramaOptions::ProjectionFormat) lt);
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
                                            );
        DEBUG_DEBUG ("Projection changed: "  << lt);
        m_projection_panel->Layout();
        Refresh();
    } else {
        // FIXME DEBUG_WARN("wxChoice event from unknown object received");
    }
}

/* We don't have an OpenGL hdr display yet
void GLPreviewFrame::OnOutputChoice( wxCommandEvent & e)
{
    if (e.GetEventObject() == m_outputModeChoice) {
        PanoramaOptions opt = m_pano.getOptions();
        int lt = m_outputModeChoice->GetSelection();
        wxString Ip;
        opt.outputMode = ( (PanoramaOptions::OutputMode) lt );
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
                                               );

    } else {
        // FIXME DEBUG_WARN("wxChoice event from unknown object received");
    }
}
*/

void GLPreviewFrame::SetStatusMessage(wxString message)
{
    SetStatusText(message, 0);
}

void GLPreviewFrame::OnPhotometric(wxCommandEvent & e)
{
    m_GLPreview->SetPhotometricCorrect(e.IsChecked());
}

void GLPreviewFrame::MakePreviewTools(PreviewToolHelper *preview_helper_in)
{
    // create the tool objects.
    // we delay this until we have an OpenGL context so that they are free to
    // create texture objects and display lists before they are used.
    preview_helper = preview_helper_in;
    crop_tool = new PreviewCropTool(preview_helper);
    drag_tool = new PreviewDragTool(preview_helper);
    color_picker_tool = new PreviewColorPickerTool(preview_helper);
    edit_cp_tool = new PreviewEditCPTool(preview_helper);
    identify_tool = new PreviewIdentifyTool(preview_helper, this, true);
    preview_helper->ActivateTool(identify_tool);
    difference_tool = new PreviewDifferenceTool(preview_helper);
    pano_mask_tool = new PreviewPanoMaskTool(preview_helper);
    preview_control_point_tool = new PreviewControlPointTool(preview_helper);
    m_preview_layoutLinesTool = new PreviewLayoutLinesTool(preview_helper);

    preview_projection_grid = new PreviewProjectionGridTool(preview_helper);
    if(GetMenuBar()->FindItem(XRCID("action_show_grid"))->IsChecked())
    {
        preview_helper->ActivateTool(preview_projection_grid);
    };
    preview_guide_tool = new PreviewGuideTool(preview_helper);
    preview_guide_tool->SetGuideStyle((PreviewGuideTool::Guides)m_GuideChoiceProj->GetSelection());

    // activate tools that are always active.
    preview_helper->ActivateTool(pano_mask_tool);
    // update the blend mode which activates some tools
    updateBlendMode();
    m_GLPreview->SetPhotometricCorrect(true);
    // update toolbar
    SetMode(mode_assistant);
}

void GLPreviewFrame::MakePanosphereOverviewTools(PanosphereOverviewToolHelper *panosphere_overview_helper_in)
{
    panosphere_overview_helper = panosphere_overview_helper_in;
    overview_drag_tool = new OverviewDragTool(panosphere_overview_helper);
    panosphere_overview_camera_tool = new PanosphereOverviewCameraTool(panosphere_overview_helper);
    panosphere_overview_helper->ActivateTool(panosphere_overview_camera_tool);
    panosphere_overview_identify_tool = new PreviewIdentifyTool(panosphere_overview_helper, this, false);
    panosphere_overview_helper->ActivateTool(panosphere_overview_identify_tool);

    panosphere_sphere_tool = new PanosphereSphereTool(panosphere_overview_helper, GetPreviewBackgroundColor());
    panosphere_overview_helper->ActivateTool(panosphere_sphere_tool);
    
    overview_projection_grid = new PanosphereOverviewProjectionGridTool(panosphere_overview_helper);
    if(GetMenuBar()->FindItem(XRCID("action_show_grid"))->IsChecked())
    {
        panosphere_overview_helper->ActivateTool(overview_projection_grid);
    }
    
    
    overview_outlines_tool = new PanosphereOverviewOutlinesTool(panosphere_overview_helper, m_GLPreview);
    panosphere_overview_helper->ActivateTool(overview_outlines_tool);
    panosphere_difference_tool = new PreviewDifferenceTool(panosphere_overview_helper);

    m_panosphere_layoutLinesTool = new PreviewLayoutLinesTool(panosphere_overview_helper);
    panosphere_control_point_tool = new PreviewControlPointTool(panosphere_overview_helper);



}

void GLPreviewFrame::MakePlaneOverviewTools(PlaneOverviewToolHelper *plane_overview_helper_in)
{
    plane_overview_helper = plane_overview_helper_in;
    plane_overview_identify_tool = new PreviewIdentifyTool(plane_overview_helper, this, false);
    plane_overview_helper->ActivateTool(plane_overview_identify_tool);
    
    plane_overview_camera_tool = new PlaneOverviewCameraTool(plane_overview_helper);
    plane_overview_helper->ActivateTool(plane_overview_camera_tool);
    plane_difference_tool = new PreviewDifferenceTool(plane_overview_helper);

    plane_overview_outlines_tool = new PlaneOverviewOutlinesTool(plane_overview_helper, m_GLPreview);
    plane_overview_helper->ActivateTool(plane_overview_outlines_tool);

    m_plane_layoutLinesTool = new PreviewLayoutLinesTool(plane_overview_helper);
    plane_control_point_tool = new PreviewControlPointTool(plane_overview_helper);

}

void GLPreviewFrame::OnIdentify(wxCommandEvent & e)
{
    SetStatusText(wxT(""), 0); // blank status text as it refers to an old tool.
    if (e.IsChecked())
    {
        m_BlendModeChoice->SetSelection(0);
        preview_helper->DeactivateTool(difference_tool);
        panosphere_overview_helper->DeactivateTool(panosphere_difference_tool);
        plane_overview_helper->DeactivateTool(plane_difference_tool);
        identify_tool->setConstantOn(true);
        panosphere_overview_identify_tool->setConstantOn(true);
        plane_overview_identify_tool->setConstantOn(true);
//        TurnOffTools(preview_helper->ActivateTool(identify_tool));
//        TurnOffTools(panosphere_overview_helper->ActivateTool(panosphere_overview_identify_tool));
//        TurnOffTools(plane_overview_helper->ActivateTool(plane_overview_identify_tool));
    } else {
        identify_tool->setConstantOn(false);
        panosphere_overview_identify_tool->setConstantOn(false);
        plane_overview_identify_tool->setConstantOn(false);
//        preview_helper->DeactivateTool(identify_tool);
//        panosphere_overview_helper->DeactivateTool(panosphere_overview_identify_tool);
//        plane_overview_helper->DeactivateTool(plane_overview_identify_tool);
        CleanButtonColours();
    }
    m_GLPreview->Refresh();
    m_GLOverview->Refresh();
}

void GLPreviewFrame::OnControlPoint(wxCommandEvent & e)
{
    if (!m_editCP_togglebutton->GetValue())
    {
        //process event only if edit cp tool is disabled
        SetStatusText(wxT(""), 0); // blank status text as it refers to an old tool.
        if (e.IsChecked())
        {
            TurnOffTools(preview_helper->ActivateTool(preview_control_point_tool));
            TurnOffTools(panosphere_overview_helper->ActivateTool(panosphere_control_point_tool));
            TurnOffTools(plane_overview_helper->ActivateTool(plane_control_point_tool));
        }
        else {
            preview_helper->DeactivateTool(preview_control_point_tool);
            panosphere_overview_helper->DeactivateTool(panosphere_control_point_tool);
            plane_overview_helper->DeactivateTool(plane_control_point_tool);
        }
        m_GLPreview->Refresh();
        m_GLOverview->Refresh();
    };
}

void GLPreviewFrame::TurnOffTools(std::set<Tool*> tools)
{
    std::set<Tool*>::iterator i;
    for (i = tools.begin(); i != tools.end(); ++i)
    {
        if (*i == crop_tool)
        {
            // cover up the guidelines
            m_GLPreview->Refresh();
        } else if (*i == drag_tool)
        {
            // cover up its boxes
            m_GLPreview->Refresh();
        } else if (*i == identify_tool)
        {
            // disabled the identify tool, toggle its button off.
            m_identify_togglebutton->SetValue(false);
            // cover up its indicators and restore normal button colours.
            m_GLPreview->Refresh();
            m_GLOverview->Refresh();
            CleanButtonColours();
        } else if (*i == preview_control_point_tool)
        {
            // disabled the control point tool.
            XRCCTRL(*this,"preview_control_point_tool",wxCheckBox)->SetValue(false);
            // cover up the control point lines.
            m_GLPreview->Refresh();
            m_GLOverview->Refresh();
        }
    }
}

void GLPreviewFrame::SetImageButtonColour(unsigned int image_nr,
                                          unsigned char red,
                                          unsigned char green,
                                          unsigned char blue)
{
    // 0, 0, 0 indicates we want to go back to the system colour.
    // TODO: Maybe we should test this better on different themes.
#if defined __WXMSW__ || defined __WXMAC__
    if (red || green || blue)
    {
        // the identify tool wants us to highlight an image button in the given
        // colour, to match up with the display in the preview.
        // on windows change the color of the surhugin_utils::rounding wxPanel
        m_ToggleButtonPanel[image_nr]->SetBackgroundColour(wxColour(red, green, blue));
    }
    else
    {
        // return to the normal colour
        m_ToggleButtonPanel[image_nr]->SetBackgroundColour(m_ToggleButtonPanel[image_nr]->GetParent()->GetBackgroundColour());
    }
    m_ToggleButtonPanel[image_nr]->Refresh();
#else
    if (red || green || blue)
    {
        // change the color of the wxToggleButton 
        m_ToggleButtons[image_nr]->SetBackgroundStyle(wxBG_STYLE_COLOUR);
        m_ToggleButtons[image_nr]->SetBackgroundColour(wxColour(red, green, blue));
        // black should be visible on the button's vibrant colours.
        m_ToggleButtons[image_nr]->SetForegroundColour(wxColour(0, 0, 0));
    }
    else
    {
        // return to the normal colour
        m_ToggleButtons[image_nr]->SetBackgroundStyle(wxBG_STYLE_SYSTEM);
        m_ToggleButtons[image_nr]->SetBackgroundColour(wxNullColour);
        m_ToggleButtons[image_nr]->SetForegroundColour(wxNullColour);
    };
    m_ToggleButtons[image_nr]->Refresh();
#endif
}

void GLPreviewFrame::CleanButtonColours()
{
    // when we turn off the identification tool, any buttons that were coloured
    // to match the image in the preview should be given back the system themed
    // colours.
    unsigned int nr_images = m_pano.getNrOfImages();
    for (unsigned image = 0; image < nr_images; image++)
    {
#if defined __WXMSW__ || defined __WXMAC__
        m_ToggleButtonPanel[image]->SetBackgroundColour(m_ToggleButtonPanel[image]->GetParent()->GetBackgroundColour());
        m_ToggleButtonPanel[image]->Refresh();
#else
        m_ToggleButtons[image]->SetBackgroundStyle(wxBG_STYLE_SYSTEM);
        m_ToggleButtons[image]->SetBackgroundColour(wxNullColour);
        m_ToggleButtons[image]->SetForegroundColour(wxNullColour);
        m_ToggleButtons[image]->Refresh();
#endif
    }
}

void GLPreviewFrame::OnColorPicker(wxCommandEvent &e)
{
    // blank status text as it refers to an old tool.
    SetStatusText(wxT(""), 0); 
    if (e.IsChecked())
    {
        // deactivate delete cp tool if active
        preview_helper->DeactivateTool(edit_cp_tool);
        m_editCP_togglebutton->SetValue(false);
        preview_helper->ActivateTool(color_picker_tool);
    }
    else
    {
        preview_helper->DeactivateTool(color_picker_tool);
    };
    m_GLPreview->Refresh();
};

void GLPreviewFrame::UpdateGlobalWhiteBalance(double redFactor, double blueFactor)
{
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::UpdateWhiteBalance(m_pano, redFactor, blueFactor)
        );
    //now toggle button and deactivate tool
    m_colorpicker_togglebutton->SetValue(false);
    //direct deactivation of tool does not work because this function is called by the tool itself
    //so we are send an event to deactivate the tool
    wxCommandEvent e(wxEVT_COMMAND_TOOL_CLICKED, XRCID("preview_color_picker_tool"));
    e.SetInt(0);
    GetEventHandler()->AddPendingEvent(e);
};

void GLPreviewFrame::OnEditCPTool(wxCommandEvent &e)
{
    // blank status text as it refers to an old tool.
    SetStatusText(wxT(""), 0);
    if (e.IsChecked())
    {
        // deactivate color picker tool
        preview_helper->DeactivateTool(color_picker_tool);
        m_colorpicker_togglebutton->SetValue(false);
        // show automatically all cp
        preview_helper->ActivateTool(preview_control_point_tool);
        preview_helper->ActivateTool(edit_cp_tool);
    }
    else
    {
        if (!XRCCTRL(*this, "preview_control_point_tool", wxCheckBox)->GetValue())
        {
            preview_helper->DeactivateTool(preview_control_point_tool);
        };
        preview_helper->DeactivateTool(edit_cp_tool);
    };
    m_GLPreview->Refresh();
};

ImageToogleButtonEventHandler::ImageToogleButtonEventHandler(
                                  unsigned int image_number_in,
                                  wxToggleButton* identify_button_in,
                                  HuginBase::Panorama * m_pano_in)
{
    image_number = image_number_in;
    m_identify_button = identify_button_in;
    m_pano = m_pano_in;
}

void ImageToogleButtonEventHandler::OnEnter(wxMouseEvent & e)
{
    // When using the identify tool, we want to identify image locations when
    // the user moves the mouse over the image buttons, but only if the image
    // is being shown.
    if ( 
        m_identify_button->GetValue() && 
        m_pano->getActiveImages().count(image_number))
    {
        std::vector<PreviewIdentifyTool**>::iterator it;
        for(it = identify_tools.begin() ; it != identify_tools.end() ; ++it) {
            (*(*it))->ShowImageNumber(image_number);
        }
    }
    e.Skip();
}

void ImageToogleButtonEventHandler::OnLeave(wxMouseEvent & e)
{
    // if the mouse left one of the image toggle buttons with the identification
    // tool active, we should stop showing the image indicator for that button.
    if ( 
        m_identify_button->GetValue() && 
        m_pano->getActiveImages().count(image_number))
    {
        std::vector<PreviewIdentifyTool**>::iterator it;
        for(it = identify_tools.begin() ; it != identify_tools.end() ; ++it) {
            (*(*it))->StopShowingImages();
        }
    }
    e.Skip();
}

void ImageToogleButtonEventHandler::OnChange(wxCommandEvent & e)
{
    // the user is turning on or off an image using its button. We want to turn
    // the indicators on and off if appropriate correctly to. We use OnEnter
    // and OnLeave for the indicators, but these only work when the image is
    // showing, so we are carefull of the order:
    HuginBase::UIntSet activeImages = m_pano->getActiveImages();
    wxMouseEvent null_event;
    if (e.IsChecked()) {
        activeImages.insert(image_number);
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetActiveImagesCmd(*m_pano, activeImages)
        );
        OnEnter(null_event);
    } else {
        OnLeave(null_event);
        activeImages.erase(image_number);
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetActiveImagesCmd(*m_pano, activeImages)
        );
    }
}

void ImageToogleButtonEventHandler::AddIdentifyTool(PreviewIdentifyTool** identify_tool_in) {
    identify_tools.push_back(identify_tool_in);
}

ImageGroupButtonEventHandler::ImageGroupButtonEventHandler(unsigned int image_number, GLPreviewFrame* frame_in, HuginBase::Panorama* m_pano)
    : image_number(image_number), frame(frame_in), m_pano(m_pano) {}

void ImageGroupButtonEventHandler::AddIdentifyTool(PreviewIdentifyTool** identify_tool_in) {
    identify_tools.push_back(identify_tool_in);
}


void ImageGroupButtonEventHandler::OnEnter(wxMouseEvent & e)
{
    //mark the image
    if (m_pano->getActiveImages().count(image_number))
    {
        std::vector<PreviewIdentifyTool**>::iterator it;
        for(it = identify_tools.begin() ; it != identify_tools.end() ; ++it) {
            (*(*it))->ShowImageNumber(image_number);
        }
    }
    e.Skip();
}

void ImageGroupButtonEventHandler::OnLeave(wxMouseEvent & e)
{
    //unmark the image
    if (m_pano->getActiveImages().count(image_number))
    {
        std::vector<PreviewIdentifyTool**>::iterator it;
        for(it = identify_tools.begin() ; it != identify_tools.end() ; ++it) {
            (*(*it))->StopShowingImages();
        }
    }
    e.Skip();
}

void ImageGroupButtonEventHandler::OnChange(wxCommandEvent & e)
{
    wxMouseEvent null_event;
    if (e.IsChecked()) {
        frame->AddImageToDragGroup(image_number,false);
        OnEnter(null_event);
    } else {
        OnLeave(null_event);
        frame->RemoveImageFromDragGroup(image_number,false);
    }
}

void ImageGroupButtonEventHandler::AddDragTool(DragTool** drag_tool_in) {
    drag_tools.push_back(drag_tool_in);
}

bool GLPreviewFrame::individualDragging()
{
    return m_DragModeChoice->GetSelection()==1 || 
           m_DragModeChoice->GetSelection()==3;
}

void GLPreviewFrame::ToggleImageInDragGroup(unsigned int image_nr, bool update_check_box) {
    if (imageDragGroup.count(image_nr) == 0) {
        this->AddImageToDragGroup(image_nr, update_check_box);
    } else {
        this->RemoveImageFromDragGroup(image_nr, update_check_box);
    }
}
void GLPreviewFrame::RemoveImageFromDragGroup(unsigned int image_nr, bool update_check_box) {
    imageDragGroup.erase(image_nr);
    if (update_check_box) {
        m_GroupToggleButtons[image_nr]->SetValue(false);
    }
}
void GLPreviewFrame::AddImageToDragGroup(unsigned int image_nr, bool update_check_box) {
    imageDragGroup.insert(image_nr);
    if (update_check_box) {
        m_GroupToggleButtons[image_nr]->SetValue(true);
    }
}
void GLPreviewFrame::SetDragGroupImages(HuginBase::UIntSet imageDragGroup_in, bool update_check_box) {
    imageDragGroup.swap(imageDragGroup_in);
    std::vector<wxCheckBox*>::iterator it;
    unsigned int nr = 0;
    for(it = m_GroupToggleButtons.begin() ; it != m_GroupToggleButtons.end() ; ++it) {
        (*it)->SetValue(imageDragGroup.count(nr++)>0);
    }
}
HuginBase::UIntSet GLPreviewFrame::GetDragGroupImages() {
    return imageDragGroup;
}
void GLPreviewFrame::ClearDragGroupImages(bool update_check_box) {
    imageDragGroup.clear();
    std::vector<wxCheckBox*>::iterator it;
    for(it = m_GroupToggleButtons.begin() ; it != m_GroupToggleButtons.end() ; ++it) {
        (*it)->SetValue(false);
    }
}

void GLPreviewFrame::EnableGroupCheckboxes(bool isShown)
{
    std::vector<wxCheckBox*>::iterator it;
    for(it = m_GroupToggleButtons.begin() ; it != m_GroupToggleButtons.end() ; ++it)
    {
        (*it)->Show(isShown);
    }
    Layout();
};

/** call this method only with existing OpenGL context */
void GLPreviewFrame::FillBlendChoice()
{
    if(PreviewDifferenceTool::CheckOpenGLCanDifference())
        m_differenceIndex=m_BlendModeChoice->Append(_("difference"));
    // update size
    m_BlendModeChoice->InvalidateBestSize();
    m_BlendModeChoice->GetParent()->Layout();
    Refresh();
    // get blend mode last state
    unsigned int oldMode = wxConfigBase::Get()->Read(wxT("/GLPreviewFrame/blendMode"), 0l);
    // limit old state to max available states
    if (oldMode >= m_BlendModeChoice->GetCount())
    {
        oldMode = 0;
    }
    m_BlendModeChoice->SetSelection(oldMode);
    updateBlendMode();
};

void GLPreviewFrame::OnAutocrop(wxCommandEvent &e)
{
    DEBUG_INFO("Dirty ROI Calc\n");
    if (m_pano.getActiveImages().size() == 0)
    {
        return;
    };

    vigra::Rect2D newROI;
    {
        ProgressReporterDialog progress(0, _("Autocrop"), _("Calculating optimal crop"), this);
        HuginBase::CalculateOptimalROI cropPano(m_pano, &progress);
        cropPano.run();
        if (cropPano.hasRunSuccessfully())
        {
            newROI = cropPano.getResultOptimalROI();
        };
    };

    //set the ROI - fail if the right/bottom is zero, meaning all zero
    if(!newROI.isEmpty())
    {
        HuginBase::PanoramaOptions opt = m_pano.getOptions();
        opt.setROI(newROI);
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetPanoOptionsCmd(m_pano, opt )
            );
    }
}

void GLPreviewFrame::OnStackAutocrop(wxCommandEvent &e)
{
    DEBUG_INFO("Dirty ROI Calc\n");
    if (m_pano.getActiveImages().size() == 0)
    {
        return;
    };

    vigra::Rect2D newROI;
    {
        ProgressReporterDialog progress(0, _("Autocrop"), _("Calculating optimal crop"), this);
        HuginBase::UIntSet activeImages = m_pano.getActiveImages();
        std::vector<HuginBase::UIntSet> stackImgs = getHDRStacks(m_pano, activeImages, m_pano.getOptions());
        HuginBase::CalculateOptimalROI cropPano(m_pano, &progress);
        //only use hdr autocrop for projects with stacks
        //otherwise fall back to "normal" autocrop
        if (stackImgs.size()<activeImages.size())
        {
            cropPano.setStacks(stackImgs);
        }
        cropPano.run();
        if (cropPano.hasRunSuccessfully())
        {
            newROI = cropPano.getResultOptimalROI();
        };
    };

    //set the ROI - fail if the right/bottom is zero, meaning all zero
    if(!newROI.isEmpty())
    {
        HuginBase::PanoramaOptions opt = m_pano.getOptions();
        opt.setROI(newROI);
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetPanoOptionsCmd(m_pano, opt )
            );
    }
}

void GLPreviewFrame::OnFullScreen(wxCommandEvent & e)
{
    ShowFullScreen(!IsFullScreen(), wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION);
};

void GLPreviewFrame::SetMode(int newMode)
{
    if(m_mode==newMode)
        return;
    SetStatusText(wxT(""), 0); // blank status text as it refers to an old tool.
    switch(m_mode)
    {
        case mode_assistant:
        case mode_preview:
            // switch off identify and show cp tool
            identify_tool->setConstantOn(false);
            panosphere_overview_identify_tool->setConstantOn(false);
            plane_overview_identify_tool->setConstantOn(false);
            preview_helper->DeactivateTool(color_picker_tool);
            m_colorpicker_togglebutton->SetValue(false);
//            preview_helper->DeactivateTool(identify_tool);
//            panosphere_overview_helper->DeactivateTool(panosphere_overview_identify_tool);
//            plane_overview_helper->DeactivateTool(plane_overview_identify_tool);
            preview_helper->DeactivateTool(edit_cp_tool);
            m_editCP_togglebutton->SetValue(false);

            CleanButtonColours();
            m_identify_togglebutton->SetValue(false);
            preview_helper->DeactivateTool(preview_control_point_tool);
            panosphere_overview_helper->DeactivateTool(panosphere_control_point_tool);
            plane_overview_helper->DeactivateTool(plane_control_point_tool);
            XRCCTRL(*this,"preview_control_point_tool",wxCheckBox)->SetValue(false);
            break;
        case mode_layout:
            // disable layout mode.
            preview_helper->DeactivateTool(m_preview_layoutLinesTool);
            panosphere_overview_helper->DeactivateTool(m_panosphere_layoutLinesTool);
            plane_overview_helper->DeactivateTool(m_plane_layoutLinesTool);
            // reactivate identify tool when leaving layout mode
            preview_helper->ActivateTool(identify_tool);
            panosphere_overview_helper->ActivateTool(panosphere_overview_identify_tool);
            plane_overview_helper->ActivateTool(plane_overview_identify_tool);
            m_GLPreview->SetLayoutMode(false);
            m_GLOverview->SetLayoutMode(false);
            // Switch the panorama mask back on.
            preview_helper->ActivateTool(pano_mask_tool);
            //restore blend mode
            m_BlendModeChoice->SetSelection(non_layout_blend_mode);
            updateBlendMode();
            break;
        case mode_projection:
            preview_helper->DeactivateTool(preview_guide_tool);
            break;
        case mode_drag:
            preview_helper->DeactivateTool(preview_guide_tool);
            preview_helper->DeactivateTool(drag_tool);
            panosphere_overview_helper->DeactivateTool(overview_drag_tool);
            if (individualDragging()) {
                std::vector<wxCheckBox*>::iterator it;
                for(it = m_GroupToggleButtons.begin() ; it != m_GroupToggleButtons.end() ; ++it) {
                    (*it)->Show(false);
                }
            }
            break;
        case mode_crop:
            preview_helper->DeactivateTool(preview_guide_tool);
            preview_helper->DeactivateTool(crop_tool);
            break;
    };
    m_mode=newMode;
    wxScrollEvent dummy;
    switch(m_mode)
    {
        case mode_assistant:
        case mode_preview:
            break;
        case mode_layout:
            //save blend mode setting, set to normal for layout mode
            non_layout_blend_mode=m_BlendModeChoice->GetSelection();
            m_BlendModeChoice->SetSelection(0);
            updateBlendMode();
            // turn off things not used in layout mode.
            preview_helper->DeactivateTool(pano_mask_tool);
            // deactivate identify tool in layout mode
            preview_helper->DeactivateTool(identify_tool);
            panosphere_overview_helper->DeactivateTool(panosphere_overview_identify_tool);
            plane_overview_helper->DeactivateTool(plane_overview_identify_tool);
            m_GLPreview->SetLayoutMode(true);
            m_GLOverview->SetLayoutMode(true);
            preview_helper->ActivateTool(m_preview_layoutLinesTool);
            panosphere_overview_helper->ActivateTool(m_panosphere_layoutLinesTool);
            plane_overview_helper->ActivateTool(m_plane_layoutLinesTool);
            // we need to update the meshes after switch to layout mode
            // otherwise the following update of scale has no meshes to scale
            m_GLPreview->Update();
            m_GLOverview->Update();
            OnLayoutScaleChange(dummy);
            break;
        case mode_projection:
            preview_helper->ActivateTool(preview_guide_tool);
            break;
        case mode_drag:
            preview_helper->ActivateTool(preview_guide_tool);
            TurnOffTools(preview_helper->ActivateTool(drag_tool));
            TurnOffTools(panosphere_overview_helper->ActivateTool(overview_drag_tool));
            if (individualDragging()) {
                std::vector<wxCheckBox*>::iterator it;
                for(it = m_GroupToggleButtons.begin() ; it != m_GroupToggleButtons.end() ; ++it) {
                    (*it)->Show(true);
                }
            }
            break;
        case mode_crop:
            TurnOffTools(preview_helper->ActivateTool(crop_tool));
            preview_helper->ActivateTool(preview_guide_tool);
            break;
    };
    //enable group checkboxes only for drag mode tab
    EnableGroupCheckboxes(m_mode==mode_drag && individualDragging());
    m_GLPreview->Refresh();
};

void GLPreviewFrame::OnSelectMode(wxNotebookEvent &e)
{
    if(m_mode!=-1)
        SetMode(e.GetSelection());
};

void GLPreviewFrame::OnToolModeChanging(wxNotebookEvent &e)
{
    if(m_pano.getNrOfImages()==0 && e.GetOldSelection()==0)
    {
        wxBell();
        e.Veto();
    };
};

void GLPreviewFrame::OnROIChanged ( wxCommandEvent & e )
{
    HuginBase::PanoramaOptions opt = m_pano.getOptions();
    long left, right, top, bottom;
    if (!m_ROITopTxt->GetValue().ToLong(&top)) {
        wxLogError(_("Top needs to be an integer bigger than 0"));
        return;
    }
    if (!m_ROILeftTxt->GetValue().ToLong(&left)) {
        wxLogError(_("left needs to be an integer bigger than 0"));
        return;
    }
    if (!m_ROIRightTxt->GetValue().ToLong(&right)) {
        wxLogError(_("right needs to be an integer bigger than 0"));
        return;
    }
    if (!m_ROIBottomTxt->GetValue().ToLong(&bottom)) {
        wxLogError(_("bottom needs to be an integer bigger than 0"));
        return;
    }
    opt.setROI(vigra::Rect2D(left, top, right, bottom));
    // make sure that left is really to the left of right
    if(opt.getROI().width()<1) {
        wxLogError(_("left boundary must be smaller than right"));
        UpdateRoiDisplay(m_pano.getOptions());
        return;
    }
    // make sure that top is really higher than bottom
    if(opt.getROI().height()<1) {
        wxLogError(_("top boundary must be smaller than bottom"));
        UpdateRoiDisplay(m_pano.getOptions());
        return;
    }

    PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
                                           );
};

void GLPreviewFrame::OnResetCrop(wxCommandEvent &e)
{
    HuginBase::PanoramaOptions opt = m_pano.getOptions();
    opt.setROI(vigra::Rect2D(0,0,opt.getWidth(),opt.getHeight()));
    PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::SetPanoOptionsCmd(m_pano, opt));
};

void GLPreviewFrame::OnHFOVChanged ( wxCommandEvent & e )
{
    HuginBase::PanoramaOptions opt = m_pano.getOptions();


    wxString text = m_HFOVText->GetValue();
    DEBUG_INFO ("HFOV = " << text.mb_str(wxConvLocal) );
    if (text == wxT("")) {
        return;
    }

    double hfov;
    if (!hugin_utils::str2double(text, hfov)) {
        wxLogError(_("Value must be numeric."));
        return;
    }

    if ( hfov <=0 || hfov > opt.getMaxHFOV()) {
        wxLogError(wxString::Format(
            _("Invalid HFOV value. Maximum HFOV for this projection is %lf."),
            opt.getMaxHFOV()));
        hfov=opt.getMaxHFOV();
    }
    opt.setHFOV(hfov);
    // recalculate panorama height...
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
        );

    DEBUG_INFO ( "new hfov: " << hfov )
};

void GLPreviewFrame::OnVFOVChanged ( wxCommandEvent & e )
{
    HuginBase::PanoramaOptions opt = m_pano.getOptions();

    wxString text = m_VFOVText->GetValue();
    DEBUG_INFO ("VFOV = " << text.mb_str(wxConvLocal) );
    if (text == wxT("")) {
        return;
    }

    double vfov;
    if (!hugin_utils::str2double(text, vfov)) {
        wxLogError(_("Value must be numeric."));
        return;
    }

    if ( vfov <=0 || vfov > opt.getMaxVFOV()) {
        wxLogError(wxString::Format(
            _("Invalid VFOV value. Maximum VFOV for this projection is %lf."),
            opt.getMaxVFOV()));
        vfov = opt.getMaxVFOV();
    }
    opt.setVFOV(vfov);
    // recalculate panorama height...
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetPanoOptionsCmd( m_pano, opt )
        );

    DEBUG_INFO ( "new vfov: " << vfov )
};

void GLPreviewFrame::OnLayoutScaleChange(wxScrollEvent &e)
{
    if(m_mode==mode_layout)
    {
        double scale_factor=XRCCTRL(*this,"layout_scale_slider",wxSlider)->GetValue();
        m_GLPreview->SetLayoutScale(10.0 - sqrt(scale_factor));
        m_GLOverview->SetLayoutScale(10.0 - sqrt(scale_factor));
        m_GLPreview->Refresh();
        m_GLOverview->Refresh();
    };
};

void GLPreviewFrame::ShowProjectionWarnings()
{
    HuginBase::PanoramaOptions opts = m_pano.getOptions();
    double hfov = opts.getHFOV();
    double vfov = opts.getVFOV();
    double maxfov = hfov > vfov ? hfov : vfov;
    wxString message;
    // If this is set to true, offer rectilinear as an alternative if it fits.
    bool rectilinear_option = false;
    switch (opts.getProjection()) {
        case HuginBase::PanoramaOptions::RECTILINEAR:
            if (maxfov > 120.0) {
                            // wide rectilinear image
                message = _("With a wide field of view, panoramas with rectilinear projection get very stretched towards the edges.\n");
                if (vfov < 110) {
                    message += _("Since the field of view is only very wide in the horizontal direction, try a cylindrical projection instead.");
                } else {
                    message += _("For a very wide panorama, try equirectangular projection instead.");
                }
                message += _(" You could also try Panini projection.");
            }
            break;
        case HuginBase::PanoramaOptions::CYLINDRICAL:
            if (vfov > 120.0) {
                message = _("With a wide vertical field of view, panoramas with cylindrical projection get very stretched at the top and bottom.\nAn equirectangular projection would fit the same content in less vertical space.");
            } else rectilinear_option = true;
            break;
        case HuginBase::PanoramaOptions::EQUIRECTANGULAR:
            if (vfov < 110.0 && hfov > 120.0)
            {
                message = _("Since the vertical field of view is not too wide, you could try setting the panorama projection to cylindrical.\nCylindrical projection preserves vertical lines, unlike equirectangular.");
            } else rectilinear_option = true;
            break;
        case HuginBase::PanoramaOptions::FULL_FRAME_FISHEYE:
            if (maxfov < 280.0) {
                rectilinear_option = true;
                message = _("Stereographic projection is conformal, unlike this Fisheye panorama projection.\nA conformal projection preserves angles around a point, which often makes it easier on the eye.");
            }
            break;
        case HuginBase::PanoramaOptions::STEREOGRAPHIC:
            if (maxfov > 300.0) {
                message = _("Panoramas with stereographic projection and a very wide field of view stretch the image around the edges a lot.\nThe Fisheye panorama projection compresses it, so you can fit in a wide field of view and still have a reasonable coverage of the middle.");
            } else rectilinear_option = true;
            break;
        default:
            rectilinear_option = true;
    }
    if (rectilinear_option && maxfov < 110.0) {
        message = _("Setting the panorama to rectilinear projection would keep the straight lines straight.");
    }
    if (message.IsEmpty()) {
        // no message needed.
        m_infoBar->Dismiss();
    } else {
        m_infoBar->ShowMessage(message, wxICON_INFORMATION);
    }
};

void GLPreviewFrame::SetShowProjectionHints(bool new_value)
{
    m_showProjectionHints=new_value;
    if(!m_showProjectionHints)
    {
        m_infoBar->Dismiss();
    };
};

void GLPreviewFrame::OnHideProjectionHints(wxCommandEvent &e)
{
    wxMessageBox(_("You have hidden the infobar, which shows hints about selection of projection.\nIf you want to see the bar again, activate the bar in the preferences again."),
#ifdef __WXMSW__
        _("Hugin"),
#else
        wxT(""),
#endif
        wxOK | wxICON_INFORMATION, this);

    wxConfigBase* cfg=wxConfigBase::Get();
    cfg->Write(wxT("/GLPreviewFrame/ShowProjectionHints"), false);
    m_showProjectionHints=false;
    cfg->Flush();
    e.Skip();
};

void GLPreviewFrame::UpdateIdentifyTools(std::set<unsigned int> new_image_set)
{
    if(identify_tool)
    {
        identify_tool->UpdateWithNewImageSet(new_image_set);
    };
    if(panosphere_overview_identify_tool)
    {
        panosphere_overview_identify_tool->UpdateWithNewImageSet(new_image_set);
    };
    if(plane_overview_identify_tool)
    {
        plane_overview_identify_tool->UpdateWithNewImageSet(new_image_set);
    };
}

void GLPreviewFrame::OnPreviewBackgroundColorChanged(wxColourPickerEvent & e) {
    m_preview_background_color = XRCCTRL(*this, "preview_background", wxColourPickerCtrl)->GetColour();
    wxString c = m_preview_background_color.GetAsString(wxC2S_HTML_SYNTAX);
    wxConfigBase* cfg=wxConfigBase::Get();
    cfg->Write(wxT("/GLPreviewFrame/PreviewBackground"), c);
    cfg->Flush();
    panosphere_sphere_tool->SetPreviewBackgroundColor(m_preview_background_color);
    m_GLPreview->SetViewerBackground(m_preview_background_color);
    m_GLOverview->SetViewerBackground(m_preview_background_color);
    redrawPreview();
}

wxColor GLPreviewFrame::GetPreviewBackgroundColor() {
  return m_preview_background_color;
}

void GLPreviewFrame::OnGuideChanged(wxCommandEvent &e)
{
    if(preview_guide_tool)
    {
        int selection=e.GetSelection();
        preview_guide_tool->SetGuideStyle((PreviewGuideTool::Guides)selection);
        //synchronize wxChoice in projection and crop tab
        m_GuideChoiceCrop->SetSelection(selection);
        m_GuideChoiceProj->SetSelection(selection);
        m_GuideChoiceDrag->SetSelection(selection);
        redrawPreview();
    };
};

void GLPreviewFrame::SetGuiLevel(GuiLevel newLevel)
{
    int old_selection=m_DragModeChoice->GetSelection();
    if(old_selection==wxNOT_FOUND)
    {
        old_selection=0;
    };
    m_DragModeChoice->Clear();
    m_DragModeChoice->Append(_("normal"));
    m_DragModeChoice->Append(_("normal, individual"));
    if(newLevel==GUI_EXPERT)
    {
        m_DragModeChoice->Append(_("mosaic"));
        m_DragModeChoice->Append(_("mosaic, individual"));
        m_DragModeChoice->SetSelection(old_selection);
    }
    else
    {
        if(old_selection>1)
        {
            m_DragModeChoice->SetSelection(old_selection-2);
        }
        else
        {
            m_DragModeChoice->SetSelection(old_selection);
        };
    };
    DragChoiceLayout(m_DragModeChoice->GetSelection());
    wxCommandEvent dummy;
    OnDragChoice(dummy);

    old_selection=m_OverviewModeChoice->GetSelection();
    m_OverviewModeChoice->Clear();
    m_OverviewModeChoice->Append(_("Panosphere"));
    if(newLevel==GUI_EXPERT)
    {
        m_OverviewModeChoice->Append(_("Mosaic plane"));
    };
    if(newLevel==GUI_EXPERT && old_selection==1)
    {
        m_OverviewModeChoice->SetSelection(1);
    }
    else
    {
        m_GLOverview->SetMode(GLOverview::PANOSPHERE);
        m_OverviewModeChoice->SetSelection(0);
    };
    m_overviewCommandPanel->Show(newLevel==GUI_EXPERT);
    m_overviewCommandPanel->GetParent()->Layout();
    if(newLevel==GUI_SIMPLE)
    {
#ifdef __WXMAC__
        wxApp::s_macExitMenuItemId = XRCID("action_exit_preview");
#endif
        if(m_guiLevel!=GUI_SIMPLE)
        {
            GetMenuBar()->Remove(0);
            GetMenuBar()->Insert(0, m_filemenuSimple, _("&File"));
        };
        SetTitle(MainFrame::Get()->GetTitle());
    }
    else
    {
#ifdef __WXMAC__
        wxApp::s_macExitMenuItemId = XRCID("action_exit_hugin");
#endif
        if(m_guiLevel==GUI_SIMPLE)
        {
            GetMenuBar()->Remove(0);
            GetMenuBar()->Insert(0, m_filemenuAdvanced, _("&File"));
        };
        SetTitle(_("Fast Panorama preview"));
    };
    m_guiLevel=newLevel;
    // update menu items
    switch(m_guiLevel)
    {
        case GUI_SIMPLE:
            GetMenuBar()->FindItem(XRCID("action_gui_simple"))->Check();
            break;
        case GUI_ADVANCED:
            GetMenuBar()->FindItem(XRCID("action_gui_advanced"))->Check();
            break;
        case GUI_EXPERT:
            GetMenuBar()->FindItem(XRCID("action_gui_expert"))->Check();
            break;
    };
};

void GLPreviewFrame::OnShowMainFrame(wxCommandEvent &e)
{
    MainFrame::Get()->Show();
    MainFrame::Get()->Raise();
};

void GLPreviewFrame::OnUserExit(wxCommandEvent &e)
{
    Close();
};

void GLPreviewFrame::OnLoadImages( wxCommandEvent & e )
{
    // load the images.
    PanoOperation::AddImageOperation addImage;
    HuginBase::UIntSet images;
    PanoCommand::PanoCommand* cmd=addImage.GetCommand(wxGetActiveWindow(), m_pano, images, m_guiLevel);
    if(cmd==NULL)
    {
        return;
    }
    //distribute images only if the existing images are not connected
    //otherwise it would destruct the existing image pattern
    bool distributeImages=m_pano.getNrOfCtrlPoints()==0;

    long autoAlign = wxConfigBase::Get()->Read(wxT("/Assistant/autoAlign"), HUGIN_ASS_AUTO_ALIGN); 
    if (autoAlign)
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(cmd);
        wxCommandEvent dummy;
        OnAlign(dummy);
    }
    else
    {
        std::vector<PanoCommand::PanoCommand*> cmds;
        cmds.push_back(cmd);
        if(distributeImages)
        {
            cmds.push_back(new PanoCommand::DistributeImagesCmd(m_pano));
            cmds.push_back(new PanoCommand::CenterPanoCmd(m_pano));
        };
        PanoCommand::CombinedPanoCommand* combinedCmd = new PanoCommand::CombinedPanoCommand(m_pano, cmds);
        combinedCmd->setName("add and distribute images");
        PanoCommand::GlobalCmdHist::getInstance().addCommand(combinedCmd);
    };
}

void GLPreviewFrame::OnAlign( wxCommandEvent & e )
{
    MainFrame::Get()->RunAssistant(this);
}

void GLPreviewFrame::OnCreate( wxCommandEvent & e )
{
    PanoOutputDialog dlg(this, m_pano, m_guiLevel);
    if(dlg.ShowModal()==wxID_OK)
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::SetPanoOptionsCmd(m_pano, dlg.GetNewPanoramaOptions())
            );
        wxCommandEvent dummy;
        MainFrame::Get()->OnDoStitch(dummy);
    };
}

void GLPreviewFrame::OnLensTypeChanged (wxCommandEvent & e)
{
    // uses enum Lens::LensProjectionFormat from PanoramaMemento.h
    size_t var = GetSelectedValue(m_lensTypeChoice);
    const HuginBase::SrcPanoImage& img=m_pano.getImage(0);
    if (img.getProjection() != (HuginBase::Lens::LensProjectionFormat) var)
    {
        double fl = HuginBase::SrcPanoImage::calcFocalLength(img.getProjection(), img.getHFOV(), img.getCropFactor(), img.getSize());
        HuginBase::UIntSet imgs;
        imgs.insert(0);
        std::vector<PanoCommand::PanoCommand*> commands;
        commands.push_back(
                new PanoCommand::ChangeImageProjectionCmd(
                                    m_pano,
                                    imgs,
                                    (HuginBase::SrcPanoImage::Projection) var
                                )
            );
        
        commands.push_back(new PanoCommand::UpdateFocalLengthCmd(m_pano, imgs, fl));
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::CombinedPanoCommand(m_pano, commands));
    }
}

void GLPreviewFrame::OnFocalLengthChanged(wxCommandEvent & e)
{
    if (m_pano.getNrOfImages() == 0)
    {
        return;
    };

    // always change first lens
    wxString text = m_focalLengthText->GetValue();
    DEBUG_INFO("focal length: " << text.mb_str(wxConvLocal));
    double val;
    if (!hugin_utils::str2double(text, val))
    {
        return;
    }
    //no negative values, no zero input please
    if (val<0.1)
    {
        wxBell();
        return;
    };    

    // always change first lens...
    HuginBase::UIntSet images0;
    images0.insert(0);
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::UpdateFocalLengthCmd(m_pano, images0, val)
    );
}

void GLPreviewFrame::OnCropFactorChanged(wxCommandEvent & e)
{
    if (m_pano.getNrOfImages() == 0)
    {
        return;
    };

    wxString text = m_cropFactorText->GetValue();
    DEBUG_INFO("crop factor: " << text.mb_str(wxConvLocal));
    double val;
    if (!hugin_utils::str2double(text, val))
    {
        return;
    }
    //no negative values, no zero input please
    if (val<0.1)
    {
        wxBell();
        return;
    };    

    HuginBase::UIntSet images;
    images.insert(0);
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::UpdateCropFactorCmd(m_pano,images,val)
    );
}

// remove cp, relatively easy, we get the selected cp from the edit cp tool
void GLPreviewFrame::OnRemoveCP(wxCommandEvent & e)
{
    edit_cp_tool->SetMenuProcessed();
    PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::RemoveCtrlPointsCmd(m_pano, edit_cp_tool->GetFoundCPs()));
    // ask user, if pano should be optimized
    long afterEditCPAction = wxConfig::Get()->Read(wxT("/EditCPAfterAction"), 0l);
    bool optimize = false;
    if (afterEditCPAction == 0)
    {
        wxDialog dlg;
        wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("edit_cp_optimize_dialog"));
        XRCCTRL(dlg, "edit_cp_text1", wxStaticText)->SetLabel(wxString::Format(_("%lu control points were removed from the panorama.\n\nShould the panorama now be re-optimized?"), static_cast<unsigned long int>(edit_cp_tool->GetFoundCPs().size())));
        XRCCTRL(dlg, "edit_cp_text2", wxStaticText)->SetLabel(wxString::Format(_("Current selected optimizer strategy is \"%s\"."), MainFrame::Get()->GetCurrentOptimizerString().c_str()));
        dlg.Fit();
        optimize = (dlg.ShowModal() == wxID_OK);
        if (XRCCTRL(dlg, "edit_cp_dont_show_again_checkbox", wxCheckBox)->GetValue())
        {
            if (optimize)
            {
                wxConfig::Get()->Write(wxT("/EditCPAfterAction"), 1l);
            }
            else
            {
                wxConfig::Get()->Write(wxT("/EditCPAfterAction"), 2l);
            };
        };
    }
    else
    {
        optimize = (afterEditCPAction == 1);
    }
    if (optimize)
    {
        // invoke optimization routine
        wxCommandEvent ev(wxEVT_COMMAND_BUTTON_CLICKED, XRCID("action_optimize"));
        MainFrame::Get()->GetEventHandler()->AddPendingEvent(ev);
    };
};

// some helper for cp generation
// maximal width for remapping for cp generating
#define MAX_DIMENSION 1600
struct FindStruct
{
    size_t imgNr;
    vigra::BRGBImage image;
    vigra::BImage mask;
};

typedef std::vector<FindStruct> FindVector;
typedef std::multimap<double, vigra::Diff2D> MapPoints;

static hugin_omp::Lock cpLock;

void GLPreviewFrame::OnCreateCP(wxCommandEvent & e)
{
    edit_cp_tool->SetMenuProcessed();
    vigra::Rect2D roi = edit_cp_tool->GetSelectedROI();
    HuginBase::UIntSet imgs = HuginBase::getImagesinROI(m_pano, m_pano.getActiveImages(), roi);
    // some checking of conditions
    if (imgs.empty())
    {
        wxMessageBox(_("The selected region contains no active image.\nPlease select a region which is covered by at least 2 images."),
#ifdef __WXMSW__
            _("Hugin"),
#else
            wxT(""),
#endif
            wxOK | wxICON_INFORMATION, this);
        return;
    };
    if (imgs.size() < 2)
    {
        wxMessageBox(_("The selected region is only covered by a single image.\nCan't create control points for a single image."),
#ifdef __WXMSW__
            _("Hugin"),
#else
            wxT(""),
#endif
            wxOK | wxICON_INFORMATION, this);
        return;
    };
    if (roi.width() > 0.25 * m_pano.getOptions().getWidth())
    {
        if(wxMessageBox(_("The selected rectangle is very big.\nThis function is only intended for smaller areas. Otherwise unwanted side effect can appear.\n\nProceed anyway?"),
#ifdef __WXMSW__
            _("Hugin"),
#else
            wxT(""),
#endif
            wxYES_NO | wxICON_INFORMATION, this) == wxNO)
        {
            return;
        };
    }
    HuginBase::PanoramaOptions opts = m_pano.getOptions();
    opts.setROI(roi);
    // don't correct exposure
    opts.outputExposureValue = 0;
    // don't use GPU for remapping, this interfere with the fast preview window
    opts.remapUsingGPU = false;
    // rescale if size is too big
    if (roi.width() > MAX_DIMENSION)
    {
        opts.setWidth(opts.getWidth() * MAX_DIMENSION / roi.width(), true);
        roi = opts.getROI();
    };
    HuginBase::CPVector cps;
    {
        ProgressReporterDialog progress(2*imgs.size()+1, _("Searching control points"), _("Processing"), this);
        // remap all images to panorama projection
        FindVector cpInfos;
        HuginBase::CPVector tempCps;
        for (HuginBase::UIntSet::const_iterator it = imgs.begin(); it != imgs.end(); ++it)
        {
            const size_t imgNr = *it;
            if (!progress.updateDisplayValue(_("Remap image to panorama projection...")))
            {
                return;
            };
            FindStruct findStruct;
            findStruct.imgNr = imgNr;
            // remap image to panorama projection
            ImageCache::ImageCacheRGB8Ptr CachedImg = ImageCache::getInstance().getImage(m_pano.getImage(imgNr).getFilename())->get8BitImage();

            HuginBase::Nona::RemappedPanoImage<vigra::BRGBImage, vigra::BImage>* remapped = new HuginBase::Nona::RemappedPanoImage<vigra::BRGBImage, vigra::BImage>;
            HuginBase::SrcPanoImage srcImg = m_pano.getSrcImage(imgNr);
            // don't correct exposure
            srcImg.setExposureValue(0);
            remapped->setPanoImage(srcImg, opts, roi);
            remapped->remapImage(vigra::srcImageRange(*CachedImg), vigra_ext::INTERP_CUBIC, &progress);
            if (!progress.updateDisplay())
            {
                delete remapped;
                return;
            };
            findStruct.image = remapped->m_image;
            findStruct.mask = remapped->m_mask;
            delete remapped;
            cpInfos.push_back(findStruct);
        };
        if (cpInfos.size() > 1)
        {
            // match keypoints in all image pairs
            // select a sensible grid size depending on ratio of selected region, maximal should it be 25 regions
            unsigned gridx = hugin_utils::roundi(sqrt((double)roi.width() / (double)roi.height() * 25));
            if (gridx < 1)
            {
                gridx = 1;
            }
            unsigned gridy = hugin_utils::roundi(25 / gridx);
            if (gridy < 1)
            {
                gridy = 1;
            }
            while (roi.width() / gridx < 20 && gridx > 1)
            {
                --gridx;
            };
            while (roi.height() / gridy < 20 && gridy > 1)
            {
                --gridy;
            };
            // template width
            const long templWidth = 20;
            // search width
            const long sWidth = 100;
            // match all images with all
            for (size_t img1 = 0; img1 < cpInfos.size() - 1; ++img1)
            {
                if (!progress.updateDisplayValue(_("Matching interest points...")))
                {
                    return;
                };
                vigra::Size2D size(cpInfos[img1].image.width(), cpInfos[img1].image.height());
                // create a number of sub-regions
                std::vector<vigra::Rect2D> rects;
                for (unsigned party = 0; party < gridy; party++)
                {
                    for (unsigned partx = 0; partx < gridx; partx++)
                    {
                        vigra::Rect2D rect(partx*size.x / gridx, party*size.y / gridy,
                            (partx + 1)*size.x / gridx, (party + 1)*size.y / gridy);
                        rect &= vigra::Rect2D(size);
                        if (rect.width()>0 && rect.height()>0)
                        {
                            rects.push_back(rect);
                        };
                    };
                };

                if (!progress.updateDisplay())
                {
                    return;
                };

#pragma omp parallel for schedule(dynamic)
                for (int i = 0; i < rects.size(); ++i)
                {
                    MapPoints points;
                    vigra::Rect2D rect(rects[i]);
                    // run interest point detection in sub-region
                    vigra_ext::findInterestPointsPartial(srcImageRange(cpInfos[img1].image, vigra::RGBToGrayAccessor<vigra::RGBValue<vigra::UInt8> >()), rect, 2, 5 * 8, points);
                    //check if all points are inside the given image
                    MapPoints validPoints;
                    for (MapPoints::const_iterator it = points.begin(); it != points.end(); ++it)
                    {
                        if (cpInfos[img1].mask(it->second.x, it->second.y)>0)
                        {
                            validPoints.insert(*it);
                        };
                    };

                    if (!validPoints.empty())
                    {
                        // now fine-tune the interest points with all other images
                        for (size_t img2 = img1 + 1; img2 < cpInfos.size(); ++img2)
                        {
                            unsigned nGood = 0;
                            // loop over all points, starting with the highest corner score
                            for (MapPoints::const_reverse_iterator it = validPoints.rbegin(); it != validPoints.rend(); ++it)
                            {
                                if (nGood >= 2)
                                {
                                    // we have enough points, stop
                                    break;
                                }
                                //check if point is covered by second image
                                if (cpInfos[img2].mask(it->second.x, it->second.y) == 0)
                                {
                                    continue;
                                };
                                // finally fine-tune point
                                vigra_ext::CorrelationResult res = vigra_ext::PointFineTune(cpInfos[img1].image, vigra::RGBToGrayAccessor<vigra::RGBValue<vigra::UInt8> >(), it->second, templWidth,
                                    cpInfos[img2].image, vigra::RGBToGrayAccessor<vigra::RGBValue<vigra::UInt8> >(), it->second, sWidth);
                                if (res.maxi < 0.9)
                                {
                                    continue;
                                }
                                nGood++;
                                // add control point
                                {
                                    hugin_omp::ScopedLock sl(cpLock);
                                    tempCps.push_back(HuginBase::ControlPoint(cpInfos[img1].imgNr, it->second.x, it->second.y,
                                        cpInfos[img2].imgNr, res.maxpos.x, res.maxpos.y, HuginBase::ControlPoint::X_Y));
                                };
                            };
                        };
                    };
                };
                // free memory
                cpInfos[img1].image.resize(0, 0);
                cpInfos[img1].mask.resize(0, 0);
            };

            // transform coordinates back to image space
            for (size_t i = 0; i < tempCps.size(); ++i)
            {
                HuginBase::ControlPoint cp = tempCps[i];
                hugin_utils::FDiff2D p1(cp.x1 + roi.left(), cp.y1 + roi.top());
                hugin_utils::FDiff2D p1Img;
                HuginBase::PTools::Transform transform;
                transform.createTransform(m_pano.getImage(cp.image1Nr), opts);
                if (transform.transformImgCoord(p1Img, p1))
                {
                    hugin_utils::FDiff2D p2(cp.x2 + roi.left(), cp.y2 + roi.top());
                    hugin_utils::FDiff2D p2Img;
                    transform.createTransform(m_pano.getImage(cp.image2Nr), opts);
                    if (transform.transformImgCoord(p2Img, p2))
                    {
                        cp.x1 = p1Img.x;
                        cp.y1 = p1Img.y;
                        cp.x2 = p2Img.x;
                        cp.y2 = p2Img.y;
                        cps.push_back(cp);
                    };
                };
            };

            if (!cps.empty())
            {
                // check newly found control points
                // create copy
                HuginBase::Panorama copyPano=m_pano.duplicate();
                // remove all cps and set only the new found cp
                copyPano.setCtrlPoints(cps);
                // now create a subpano with only the selected images
                HuginBase::Panorama subPano = copyPano.getSubset(imgs);
                // clean control points
                if (!progress.updateDisplayValue(_("Checking results...")))
                {
                    return;
                };
                deregisterPTWXDlgFcn();
                HuginBase::UIntSet invalidCP = HuginBase::getCPoutsideLimit(subPano);
                registerPTWXDlgFcn();
                if (!invalidCP.empty())
                {
                    for (HuginBase::UIntSet::const_reverse_iterator it = invalidCP.rbegin(); it != invalidCP.rend(); ++it)
                    {
                        cps.erase(cps.begin() + *it);
                    };
                }
                // force closing progress dialog
                if (!progress.updateDisplayValue())
                {
                    return;
                };
                PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::AddCtrlPointsCmd(m_pano, cps));
                // ask user, if pano should be optimized
                long afterEditCPAction = wxConfig::Get()->Read(wxT("/EditCPAfterAction"), 0l);
                bool optimize = false;
                if (afterEditCPAction == 0)
                {
                    // close progress window, otherwise the dialog don't autoamtically get the focus
                    wxYield();
                    wxDialog dlg;
                    wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("edit_cp_optimize_dialog"));
                    XRCCTRL(dlg, "edit_cp_text1", wxStaticText)->SetLabel(wxString::Format(_("%lu control points were added to the panorama.\n\nShould the panorama now be re-optimized?"), static_cast<unsigned long int>(cps.size())));
                    XRCCTRL(dlg, "edit_cp_text2", wxStaticText)->SetLabel(wxString::Format(_("Current selected optimizer strategy is \"%s\"."), MainFrame::Get()->GetCurrentOptimizerString().c_str()));
                    dlg.Fit();
                    optimize = (dlg.ShowModal() == wxID_OK);
                    if (XRCCTRL(dlg, "edit_cp_dont_show_again_checkbox", wxCheckBox)->GetValue())
                    {
                        if (optimize)
                        {
                            wxConfig::Get()->Write(wxT("/EditCPAfterAction"), 1l);
                        }
                        else
                        {
                            wxConfig::Get()->Write(wxT("/EditCPAfterAction"), 2l);
                        };
                    };
                }
                else
                {
                    optimize = (afterEditCPAction == 1);
                }
                if (optimize)
                {
                    // invoke optimization routine
                    wxCommandEvent ev(wxEVT_COMMAND_BUTTON_CLICKED, XRCID("action_optimize"));
                    MainFrame::Get()->GetEventHandler()->AddPendingEvent(ev);
                };
            };
        };
    };
    // finally redraw
    m_GLPreview->Update();
    m_GLPreview->Refresh();
};

// handle menu close event to redraw preview, so that selection rectangle is hidden
void GLPreviewFrame::OnMenuClose(wxMenuEvent & e)
{
    m_GLPreview->Refresh();
    e.Skip();
};

void GLPreviewFrame::OnSelectContextMenu(wxContextMenuEvent& e)
{
    wxPoint point = e.GetPosition();
    // If from keyboard
    if (point.x == -1 && point.y == -1)
    {
        point = m_selectAllButton->GetPosition();
    }
    else
    {
        point = ScreenToClient(point);
    }
    PopupMenu(m_selectAllMenu, point);
};

void GLPreviewFrame::OnSelectAllMenu(wxCommandEvent& e)
{
    wxConfig::Get()->Write(wxT("/GLPreviewFrame/SelectAllMode"), 0l);
    m_selectAllMode = SELECT_ALL_IMAGES;
};

void GLPreviewFrame::OnSelectMedianMenu(wxCommandEvent& e)
{
    wxConfig::Get()->Write(wxT("/GLPreviewFrame/SelectAllMode"), 1l);
    m_selectAllMode = SELECT_MEDIAN_IMAGES;
};

void GLPreviewFrame::OnSelectBrightestMenu(wxCommandEvent& e)
{
    wxConfig::Get()->Write(wxT("/GLPreviewFrame/SelectAllMode"), 2l);
    m_selectAllMode = SELECT_BRIGHTEST_IMAGES;
};

void GLPreviewFrame::OnSelectDarkestMenu(wxCommandEvent& e)
{
    wxConfig::Get()->Write(wxT("/GLPreviewFrame/SelectAllMode"), 3l);
    m_selectAllMode = SELECT_DARKEST_IMAGES;
};

void GLPreviewFrame::OnSelectKeepSelection(wxCommandEvent& e)
{
    wxConfig::Get()->Write(wxT("/GLPreviewFrame/SelectAllKeepSelection"), true);
    m_selectKeepSelection = true;
};

void GLPreviewFrame::OnSelectResetSelection(wxCommandEvent& e)
{
    wxConfig::Get()->Write(wxT("/GLPreviewFrame/SelectAllKeepSelection"), false);
    m_selectKeepSelection = false;
};
