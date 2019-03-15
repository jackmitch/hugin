// -*- c-basic-offset: 4 -*-

/** @file MainFrame.cpp
 *
 *  @brief implementation of MainFrame Class
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id$
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

#include "hugin_config.h"

#include <wx/dir.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include "panoinc_WX.h"
#include "panoinc.h"

#include "base_wx/platform.h"
#include "base_wx/wxPlatform.h"

#include "vigra/imageinfo.hxx"
#include "vigra_ext/Correlation.h"

#include "hugin/config_defaults.h"
#include "hugin/PreferencesDialog.h"
#include "hugin/MainFrame.h"
#include "base_wx/wxPanoCommand.h"
#include "base_wx/CommandHistory.h"
#include "hugin/PanoPanel.h"
#include "hugin/ImagesPanel.h"
#include "hugin/MaskEditorPanel.h"
#include "hugin/OptimizePanel.h"
#include "hugin/OptimizePhotometricPanel.h"
#include "hugin/PreviewFrame.h"
#include "hugin/GLPreviewFrame.h"
#include "hugin/huginApp.h"
#include "hugin/CPEditorPanel.h"
#include "hugin/CPListFrame.h"
#include "hugin/LocalizedFileTipProvider.h"
#include "algorithms/control_points/CleanCP.h"
#include "hugin/PanoOperation.h"
#include "hugin/PapywizardImport.h"

#include "base_wx/MyProgressDialog.h"
#include "base_wx/RunStitchPanel.h"
#include "base_wx/wxImageCache.h"
#include "base_wx/PTWXDlg.h"
#include "base_wx/MyExternalCmdExecDialog.h"
#include "base_wx/AssistantExecutor.h"
#include "algorithms/optimizer/ImageGraph.h"

#include "base_wx/huginConfig.h"
#include "hugin/AboutDialog.h"

#if HUGIN_HSI
#include "PluginItems.h"
#endif

#ifdef __MINGW32__
// fixes for mingw compilation...
#undef FindWindow
#endif

/** class for showing splash screen
    the class wxSplashScreen from wxWidgets does not work correctly for our use case, it closes
    automatically the window if the user presses a key or does mouse clicks. These modified
    version does ignore these events. The window needs to be explicitly closed by the caller */
class HuginSplashScreen : public wxFrame
{
public:
    HuginSplashScreen() {};
    HuginSplashScreen(wxWindow* parent, wxBitmap bitmap) : 
        wxFrame(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP)
    {
        SetExtraStyle(GetExtraStyle() | wxWS_EX_TRANSIENT);
        wxSizer* topSizer=new wxBoxSizer(wxVERTICAL);
        wxStaticBitmap* staticBitmap=new wxStaticBitmap(this,wxID_ANY,bitmap);
        topSizer->Add(staticBitmap,1,wxEXPAND);
        SetSizerAndFit(topSizer);
        SetClientSize(bitmap.GetWidth(), bitmap.GetHeight());
        CenterOnScreen();
        Show(true);
        SetFocus();
#if defined(__WXMSW__) || defined(__WXMAC__)
        Update();
#elif defined(__WXGTK20__)
        //do nothing
#else
        wxYieldIfNeeded();
#endif
    };
    DECLARE_DYNAMIC_CLASS(HuginSplashScreen)
    DECLARE_EVENT_TABLE()
};

IMPLEMENT_DYNAMIC_CLASS(HuginSplashScreen, wxFrame)
BEGIN_EVENT_TABLE(HuginSplashScreen, wxFrame)
END_EVENT_TABLE()

/** file drag and drop handler method */
bool PanoDropTarget::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
{
    DEBUG_TRACE("OnDropFiles");
    MainFrame * mf = MainFrame::Get();
    if (!mf) return false;

    if (!m_imageOnly && filenames.GetCount() == 1) {
        wxFileName file(filenames[0]);
        if (file.GetExt().CmpNoCase(wxT("pto")) == 0 ||
            file.GetExt().CmpNoCase(wxT("ptp")) == 0 ||
            file.GetExt().CmpNoCase(wxT("pts")) == 0 )
        {
            // load project
            if (mf->CloseProject(true, MainFrame::LOAD_NEW_PROJECT))
            {
                mf->LoadProjectFile(file.GetFullPath());
                // remove old images from cache
                ImageCache::getInstance().flush();
            }
            return true;
        }
    }

    // try to add as images
    std::vector<std::string> filesv;
    wxArrayString invalidFiles;
    for (unsigned int i=0; i< filenames.GetCount(); i++) {
        wxFileName file(filenames[i]);

        if (file.GetExt().CmpNoCase(wxT("jpg")) == 0 ||
            file.GetExt().CmpNoCase(wxT("jpeg")) == 0 ||
            file.GetExt().CmpNoCase(wxT("tif")) == 0 ||
            file.GetExt().CmpNoCase(wxT("tiff")) == 0 ||
            file.GetExt().CmpNoCase(wxT("png")) == 0 ||
            file.GetExt().CmpNoCase(wxT("bmp")) == 0 ||
            file.GetExt().CmpNoCase(wxT("gif")) == 0 ||
            file.GetExt().CmpNoCase(wxT("pnm")) == 0 ||
            file.GetExt().CmpNoCase(wxT("sun")) == 0 ||
            file.GetExt().CmpNoCase(wxT("hdr")) == 0 ||
            file.GetExt().CmpNoCase(wxT("viff")) == 0 )
        {
            if(containsInvalidCharacters(filenames[i]))
            {
                invalidFiles.Add(file.GetFullPath());
            }
            else
            {
                filesv.push_back((const char *)filenames[i].mb_str(HUGIN_CONV_FILENAME));
            };
        }
    }
    // we got some images to add.
    if (filesv.size() > 0)
    {
        // use a Command to ensure proper undo and updating of GUI parts
        wxBusyCursor();
        if(pano.getNrOfCtrlPoints()>0)
        {
            PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::wxAddImagesCmd(pano, filesv));
        }
        else
        {
            std::vector<PanoCommand::PanoCommand*> cmds;
            cmds.push_back(new PanoCommand::wxAddImagesCmd(pano, filesv));
            cmds.push_back(new PanoCommand::DistributeImagesCmd(pano));
            cmds.push_back(new PanoCommand::CenterPanoCmd(pano));
            PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::CombinedPanoCommand(pano, cmds));
        };

    }
    if(invalidFiles.size()>0)
    {
        ShowFilenameWarning(mf, invalidFiles);
    }

    return true;
}


#if defined _WIN32 && defined Hugin_shared
DEFINE_LOCAL_EVENT_TYPE(EVT_LOADING_FAILED)
#else
DEFINE_EVENT_TYPE(EVT_LOADING_FAILED)
#endif

// event table. this frame will recieve mostly global commands.
BEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(XRCID("action_new_project"),  MainFrame::OnNewProject)
    EVT_MENU(XRCID("action_load_project"),  MainFrame::OnLoadProject)
    EVT_MENU(XRCID("action_save_project"),  MainFrame::OnSaveProject)
    EVT_MENU(XRCID("action_save_as_project"),  MainFrame::OnSaveProjectAs)
    EVT_MENU(XRCID("action_save_as_ptstitcher"),  MainFrame::OnSavePTStitcherAs)
    EVT_MENU(XRCID("action_open_batch_processor"),  MainFrame::OnOpenPTBatcher)
    EVT_MENU(XRCID("action_import_project"), MainFrame::OnMergeProject)
    EVT_MENU(XRCID("action_import_papywizard"), MainFrame::OnReadPapywizard)
    EVT_MENU(XRCID("action_apply_template"),  MainFrame::OnApplyTemplate)
    EVT_MENU(XRCID("action_exit_hugin"),  MainFrame::OnUserQuit)
    EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, MainFrame::OnMRUFiles)
    EVT_MENU(XRCID("action_show_about"),  MainFrame::OnAbout)
    EVT_MENU(XRCID("action_show_help"),  MainFrame::OnHelp)
    EVT_MENU(XRCID("action_show_tip"),  MainFrame::OnTipOfDay)
    EVT_MENU(XRCID("action_show_shortcuts"),  MainFrame::OnKeyboardHelp)
    EVT_MENU(XRCID("action_show_faq"),  MainFrame::OnFAQ)
    EVT_MENU(XRCID("action_show_prefs"), MainFrame::OnShowPrefs)
    EVT_MENU(XRCID("action_assistant"), MainFrame::OnRunAssistant)
    EVT_MENU(XRCID("action_batch_assistant"), MainFrame::OnSendToAssistantQueue)
    EVT_MENU(XRCID("action_gui_simple"), MainFrame::OnSetGuiSimple)
    EVT_MENU(XRCID("action_gui_advanced"), MainFrame::OnSetGuiAdvanced)
    EVT_MENU(XRCID("action_gui_expert"), MainFrame::OnSetGuiExpert)
#ifdef HUGIN_HSI
    EVT_MENU(XRCID("action_python_script"), MainFrame::OnPythonScript)
#endif
    EVT_MENU(XRCID("ID_EDITUNDO"), MainFrame::OnUndo)
    EVT_MENU(XRCID("ID_EDITREDO"), MainFrame::OnRedo)
    EVT_MENU(XRCID("ID_SHOW_FULL_SCREEN"), MainFrame::OnFullScreen)
    EVT_MENU(XRCID("ID_SHOW_PREVIEW_FRAME"), MainFrame::OnTogglePreviewFrame)
    EVT_MENU(XRCID("ID_SHOW_GL_PREVIEW_FRAME"), MainFrame::OnToggleGLPreviewFrame)
    EVT_BUTTON(XRCID("ID_SHOW_PREVIEW_FRAME"),MainFrame::OnTogglePreviewFrame)
    EVT_BUTTON(XRCID("ID_SHOW_GL_PREVIEW_FRAME"), MainFrame::OnToggleGLPreviewFrame)

    EVT_MENU(XRCID("action_optimize"),  MainFrame::OnOptimize)
    EVT_MENU(XRCID("action_optimize_only_active"), MainFrame::OnOnlyActiveImages)
    EVT_MENU(XRCID("action_optimize_ignore_line_cp"), MainFrame::OnIgnoreLineCp)
    EVT_BUTTON(XRCID("action_optimize"),  MainFrame::OnOptimize)
    EVT_MENU(XRCID("action_finetune_all_cp"), MainFrame::OnFineTuneAll)
//    EVT_BUTTON(XRCID("action_finetune_all_cp"), MainFrame::OnFineTuneAll)
    EVT_MENU(XRCID("action_remove_cp_in_masks"), MainFrame::OnRemoveCPinMasks)

    EVT_MENU(XRCID("ID_CP_TABLE"), MainFrame::OnShowCPFrame)
    EVT_BUTTON(XRCID("ID_CP_TABLE"),MainFrame::OnShowCPFrame)

    EVT_MENU(XRCID("ID_SHOW_PANEL_IMAGES"), MainFrame::OnShowPanel)
    EVT_MENU(XRCID("ID_SHOW_PANEL_MASK"), MainFrame::OnShowPanel)
    EVT_MENU(XRCID("ID_SHOW_PANEL_CP_EDITOR"), MainFrame::OnShowPanel)
    EVT_MENU(XRCID("ID_SHOW_PANEL_OPTIMIZER"), MainFrame::OnShowPanel)
    EVT_MENU(XRCID("ID_SHOW_PANEL_OPTIMIZER_PHOTOMETRIC"), MainFrame::OnShowPanel)
    EVT_MENU(XRCID("ID_SHOW_PANEL_PANORAMA"), MainFrame::OnShowPanel)
    EVT_MENU(XRCID("action_stitch"), MainFrame::OnDoStitch)
    EVT_MENU(XRCID("action_stitch_userdefined"), MainFrame::OnUserDefinedStitch)
    EVT_MENU(XRCID("action_add_images"),  MainFrame::OnAddImages)
    EVT_BUTTON(XRCID("action_add_images"),  MainFrame::OnAddImages)
    EVT_MENU(XRCID("action_add_time_images"),  MainFrame::OnAddTimeImages)
    EVT_BUTTON(XRCID("action_add_time_images"),  MainFrame::OnAddTimeImages)
    EVT_CLOSE(  MainFrame::OnExit)
    EVT_SIZE(MainFrame::OnSize)
    EVT_COMMAND(wxID_ANY, EVT_LOADING_FAILED, MainFrame::OnLoadingFailed)
END_EVENT_TABLE()

// change this variable definition
//wxTextCtrl *itemProjTextMemo;
// image preview
//wxBitmap *p_img = (wxBitmap *) NULL;
//WX_DEFINE_ARRAY()

enum
{
    wxIDPYTHONSCRIPTS = wxID_HIGHEST + 2000,
    wxIDUSEROUTPUTSEQUENCE = wxID_HIGHEST + 2500,
    wxIDUSERASSISTANT = wxID_HIGHEST + 3000
};

MainFrame::MainFrame(wxWindow* parent, HuginBase::Panorama & pano)
    : cp_frame(0), pano(pano)
{
    preview_frame = 0;
    svmModel=NULL;

    bool disableOpenGL=false;
    if(wxGetKeyState(WXK_COMMAND))
    {
        wxDialog dlg;
        wxXmlResource::Get()->LoadDialog(&dlg, NULL, wxT("disable_opengl_dlg"));
        long noOpenGL=wxConfigBase::Get()->Read(wxT("DisableOpenGL"), 0l);
        if(noOpenGL==1)
        {
            XRCCTRL(dlg, "disable_dont_ask_checkbox", wxCheckBox)->SetValue(true);
        };
        if(dlg.ShowModal()==wxID_OK)
        {
            if(XRCCTRL(dlg, "disable_dont_ask_checkbox", wxCheckBox)->IsChecked())
            {
                wxConfigBase::Get()->Write(wxT("DisableOpenGL"), 1l);
            }
            else
            {
                wxConfigBase::Get()->Write(wxT("DisableOpenGL"), 0l);
            };
            disableOpenGL=true;
        }
        else
        {
            wxConfigBase::Get()->Write(wxT("DisableOpenGL"), 0l);
        };
    }
    else
    {
        long noOpenGL=wxConfigBase::Get()->Read(wxT("DisableOpenGL"), 0l);
        disableOpenGL=(noOpenGL==1);
    };

    wxBitmap bitmap;
    HuginSplashScreen* splash = 0;
    wxYield();

    if (bitmap.LoadFile(huginApp::Get()->GetXRCPath() + wxT("data/splash.png"), wxBITMAP_TYPE_PNG))
    {
        // embed package version into string.
        {
            wxMemoryDC dc;
            dc.SelectObject(bitmap);
#ifdef __WXMAC__
            wxFont font(9, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
#else
            wxFont font(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
#endif
            dc.SetFont(font);
            dc.SetTextForeground(*wxBLACK);
            dc.SetTextBackground(*wxWHITE);
            int tw, th;
            wxString version;
            version.Printf(_("Version %s"), wxString(hugin_utils::GetHuginVersion().c_str(), wxConvLocal).c_str());
            dc.GetTextExtent(version, &tw, &th);
            // place text on bitmap.
            dc.DrawText(version, bitmap.GetWidth() - tw - 3, bitmap.GetHeight() - th - 3);
            dc.SelectObject(wxNullBitmap);
        }

        splash = new HuginSplashScreen(NULL, bitmap);
    } else {
        wxLogFatalError(_("Fatal installation error\nThe file data/splash.png was not found at:") + huginApp::Get()->GetXRCPath());
        abort();
    }
    //splash->Refresh();
    wxYield();

    // save our pointer
    m_this = this;

    DEBUG_TRACE("");
    // load our children. some children might need special
    // initialization. this will be done later.
    wxXmlResource::Get()->LoadFrame(this, parent, wxT("main_frame"));
    DEBUG_TRACE("");

    // load our menu bar
#ifdef __WXMAC__
    wxApp::s_macAboutMenuItemId = XRCID("action_show_about");
    wxApp::s_macPreferencesMenuItemId = XRCID("action_show_prefs");
    wxApp::s_macExitMenuItemId = XRCID("action_exit_hugin");
    wxApp::s_macHelpMenuTitleName = _("&Help");
#endif
    wxMenuBar* mainMenu=wxXmlResource::Get()->LoadMenuBar(this, wxT("main_menubar"));
    m_menu_file_simple=wxXmlResource::Get()->LoadMenu(wxT("file_menu_simple"));
    m_menu_file_advanced=wxXmlResource::Get()->LoadMenu(wxT("file_menu_advanced"));
    mainMenu->Insert(0, m_menu_file_simple, _("&File"));
    SetMenuBar(mainMenu);
    m_optOnlyActiveImages = (wxConfigBase::Get()->Read(wxT("/OptimizePanel/OnlyActiveImages"), 1l) != 0);
    m_optIgnoreLineCp = false;

#ifdef HUGIN_HSI
    wxMenuBar* menubar=GetMenuBar();
    // the plugin menu will be generated dynamically
    wxMenu *pluginMenu=new wxMenu();
    // search for all .py files in plugins directory
    wxDir dir(GetDataPath()+wxT("plugins"));
    if (dir.IsOpened())
    {
        wxString filename;
        bool cont = dir.GetFirst(&filename, wxT("*.py"), wxDIR_FILES | wxDIR_HIDDEN);
        PluginItems items;
        while (cont)
        {
            wxFileName file(dir.GetName(), filename);
            file.MakeAbsolute();
            PluginItem item(file);
            if (item.IsAPIValid())
            {
                items.push_back(item);
            };
            cont = dir.GetNext(&filename);
        };
        items.sort(comparePluginItem);

        int pluginID = wxIDPYTHONSCRIPTS;
        for (PluginItems::const_iterator it = items.begin(); it != items.end(); ++it)
        {
            PluginItem item = *it;
            int categoryID = pluginMenu->FindItem(item.GetCategory());
            wxMenu* categoryMenu;
            if (categoryID == wxNOT_FOUND)
            {
                categoryMenu = new wxMenu();
                pluginMenu->AppendSubMenu(categoryMenu, item.GetCategory());
            }
            else
            {
                categoryMenu = pluginMenu->FindItem(categoryID)->GetSubMenu();
            };
            categoryMenu->Append(pluginID, item.GetName(), item.GetDescription());
            m_plugins[pluginID] = item.GetFilename();
            Connect(pluginID, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainFrame::OnPlugin));
            pluginID++;
        };
        // show the new menu
        if (pluginMenu->GetMenuItemCount() > 0)
        {
            menubar->Insert(menubar->GetMenuCount() - 2, pluginMenu, _("&Actions"));
        };
    };
#else
    GetMenuBar()->Enable(XRCID("action_python_script"), false);
#endif

    // add saved user defined output sequences
    {
        wxArrayString files;
        // search all .executor files, do not follow links
        wxDir::GetAllFiles(GetDataPath(), &files, wxT("*.executor"), wxDIR_FILES | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
        const size_t nrAllUserSequences = files.size();
        wxDir::GetAllFiles(hugin_utils::GetUserAppDataDir(), &files, wxT("*.executor"), wxDIR_FILES | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
        if (!files.IsEmpty())
        {
            // we found some files
            long outputId = wxIDUSEROUTPUTSEQUENCE;
            int outputMenuId=mainMenu->FindMenu(_("&Output"));
            if (outputMenuId != wxNOT_FOUND)
            {
                wxMenu* outputSequencesMenu = new wxMenu;
                size_t i = 0;
                for (auto file : files)
                {
                    if (i > 0 && i == nrAllUserSequences && outputSequencesMenu->GetMenuItemCount() > 0)
                    {
                        outputSequencesMenu->AppendSeparator();
                    };
                    wxFileInputStream inputStream(file);
                    if (inputStream.IsOk())
                    {
                        // read descriptions from file
                        wxFileConfig executorFile(inputStream);
                        wxString desc = executorFile.Read(wxT("/General/Description"), wxEmptyString);
                        desc = desc.Trim(true).Trim(false);
                        wxString help = executorFile.Read(wxT("/General/Help"), wxEmptyString);
                        help = help.Trim(true).Trim(false);
                        if (help.IsEmpty())
                        {
                            help = wxString::Format(_("User defined sequence: %s"), file);
                        };
                        // add menu
                        if (!desc.IsEmpty())
                        {
                            outputSequencesMenu->Append(outputId, desc, help);
                            Bind(wxEVT_MENU, &MainFrame::OnUserDefinedStitchSaved, this, outputId);
                            m_userOutput[outputId] = file;
                            ++outputId;
                        };
                    };
                    i++;
                };
                if ((outputSequencesMenu->GetMenuItemCount() == 1 && !(outputSequencesMenu->FindItemByPosition(0)->IsSeparator())) ||
                    outputSequencesMenu->GetMenuItemCount() > 1)
                {
                    m_outputUserMenu = mainMenu->GetMenu(outputMenuId)->AppendSubMenu(outputSequencesMenu, _("User defined output sequences"));
                }
                else
                {
                    delete outputSequencesMenu;
                };
            };
        };
    };
    // add saved user defined assistants
    {
        wxArrayString files;
        // search all .assistant files, do not follow links
        wxDir::GetAllFiles(GetDataPath(), &files, wxT("*.assistant"), wxDIR_FILES | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
        const size_t nrAllUserSequences = files.size();
        wxDir::GetAllFiles(hugin_utils::GetUserAppDataDir(), &files, wxT("*.assistant"), wxDIR_FILES | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
        if (!files.IsEmpty())
        {
            // we found some files
            long assistantId = wxIDUSERASSISTANT;
            const int editMenuId = mainMenu->FindMenu(_("&Edit"));
            if (editMenuId != wxNOT_FOUND)
            {
                wxMenu* userAssistantMenu = new wxMenu;
                size_t i = 0;
                for (auto file : files)
                {
                    if (i > 0 && i == nrAllUserSequences && userAssistantMenu->GetMenuItemCount() > 0)
                    {
                        userAssistantMenu->AppendSeparator();
                    };
                    wxFileInputStream inputStream(file);
                    if (inputStream.IsOk())
                    {
                        // read descriptions from file
                        wxFileConfig assistantFile(inputStream);
                        wxString desc = assistantFile.Read(wxT("/General/Description"), wxEmptyString);
                        desc = desc.Trim(true).Trim(false);
                        wxString help = assistantFile.Read(wxT("/General/Help"), wxEmptyString);
                        help = help.Trim(true).Trim(false);
                        if (help.IsEmpty())
                        {
                            help = wxString::Format(_("User defined assistant: %s"), file);
                        };
                        // add menu
                        if (!desc.IsEmpty())
                        {
                            userAssistantMenu->Append(assistantId, desc, help);
                            Bind(wxEVT_MENU, &MainFrame::OnRunAssistantUserdefined, this, assistantId);
                            m_userAssistant[assistantId] = file;
                            ++assistantId;
                        };
                    };
                    i++;
                };
                if (userAssistantMenu->GetMenuItemCount() > 0)
                {
                    m_assistantUserMenu = mainMenu->GetMenu(editMenuId)->Insert(5, wxID_ANY, _("User defined assistant"), userAssistantMenu);
                }
                else
                {
                    delete userAssistantMenu;
                };
            };
        };
    };

    // create tool bar
    SetToolBar(wxXmlResource::Get()->LoadToolBar(this, wxT("main_toolbar")));

    // Disable tools by default
    enableTools(false);

    // put an "unknown" object in an xrc file and
    // take as wxObject (second argument) the return value of wxXmlResource::Get
    // finish the images_panel
    DEBUG_TRACE("");

    // image_panel
    images_panel = XRCCTRL(*this, "images_panel_unknown", ImagesPanel);
    assert(images_panel);
    images_panel->Init(&pano);
    DEBUG_TRACE("");

    m_notebook = XRCCTRL((*this), "controls_notebook", wxNotebook);
//    m_notebook = ((wxNotebook*) ((*this).FindWindow(XRCID("controls_notebook"))));
//    m_notebook = ((wxNotebook*) (FindWindow(XRCID("controls_notebook"))));
    DEBUG_ASSERT(m_notebook);

    // the mask panel
    mask_panel = XRCCTRL(*this, "mask_panel_unknown", MaskEditorPanel);
    assert(mask_panel);
    mask_panel->Init(&pano);

    // the pano_panel
    DEBUG_TRACE("");
    pano_panel = XRCCTRL(*this, "panorama_panel_unknown", PanoPanel);
    assert(pano_panel);
    pano_panel->Init(&pano);
    pano_panel->panoramaChanged (pano); // initialize from pano

    cpe = XRCCTRL(*this, "cp_editor_panel_unknown", CPEditorPanel);
    assert(cpe);
    cpe->Init(&pano);

    opt_panel = XRCCTRL(*this, "optimizer_panel_unknown", OptimizePanel);
    assert(opt_panel);
    opt_panel->Init(&pano);
    m_show_opt_panel=true;

    opt_photo_panel = XRCCTRL(*this, "optimizer_photometric_panel_unknown", OptimizePhotometricPanel);
    assert(opt_photo_panel);
    opt_photo_panel->Init(&pano);
    m_show_opt_photo_panel=true;

    // generate list of most recently uses files and add it to menu
    // needs to be initialized before the fast preview, there we call AddFilesToMenu()
    wxConfigBase * config=wxConfigBase::Get();
    m_mruFiles.Load(*config);
    m_mruFiles.UseMenu(m_menu_file_advanced->FindItem(XRCID("menu_mru"))->GetSubMenu());

    preview_frame = new PreviewFrame(this, pano);
    if(disableOpenGL)
    {
        gl_preview_frame=NULL;
        DisableOpenGLTools();
        m_mruFiles.AddFilesToMenu();
    }
    else
    {
        gl_preview_frame = new GLPreviewFrame(this, pano);
    };

    // set the minimize icon
#ifdef __WXMSW__
    wxIcon myIcon(GetXRCPath() + wxT("data/hugin.ico"),wxBITMAP_TYPE_ICO);
#else
    wxIcon myIcon(GetXRCPath() + wxT("data/hugin.png"),wxBITMAP_TYPE_PNG);
#endif
    SetIcon(myIcon);

    // create a new drop handler. wxwindows deletes the automaticall
    SetDropTarget(new PanoDropTarget(pano));
    DEBUG_TRACE("");

    PanoOperation::GeneratePanoOperationVector();

    // create a status bar
    const int fields (2);
    CreateStatusBar(fields);
    int widths[fields] = {-1, 85};
    SetStatusWidths( fields, &widths[0]);
    SetStatusText(_("Started"), 0);
    wxYield();
    DEBUG_TRACE("");

    // observe the panorama
    pano.addObserver(this);

    // Set sizing characteristics
    //set minumum size
#if defined __WXMAC__ || defined __WXMSW__
    // a minimum nice looking size; smaller than this would clutter the layout.
    SetSizeHints(900, 675);
#else
    // For ASUS eeePc
    SetSizeHints(780, 455); //set minumum size
#endif

    // set progress display for image cache.
    ImageCache::getInstance().setProgressDisplay(this);
#if defined __WXMSW__
    unsigned long long mem = HUGIN_IMGCACHE_UPPERBOUND;
    unsigned long mem_low = wxConfigBase::Get()->Read(wxT("/ImageCache/UpperBound"), HUGIN_IMGCACHE_UPPERBOUND);
    unsigned long mem_high = wxConfigBase::Get()->Read(wxT("/ImageCache/UpperBoundHigh"), (long) 0);
    if (mem_high > 0) {
      mem = ((unsigned long long) mem_high << 32) + mem_low;
    }
    else {
      mem = mem_low;
    }
    ImageCache::getInstance().SetUpperLimit(mem);
#else
    ImageCache::getInstance().SetUpperLimit(wxConfigBase::Get()->Read(wxT("/ImageCache/UpperBound"), HUGIN_IMGCACHE_UPPERBOUND));
#endif

    if(splash) {
        splash->Close();
        delete splash;
    }
    wxYield();

    // disable automatic Layout() calls, to it by hand
    SetAutoLayout(false);
    SetOptimizeOnlyActiveImages(m_optOnlyActiveImages);


#ifdef __WXMSW__
    // wxFrame does have a strange background color on Windows, copy color from a child widget
    this->SetBackgroundColour(images_panel->GetBackgroundColour());
#endif

// By using /SUBSYSTEM:CONSOLE /ENTRY:"WinMainCRTStartup" in the linker
// options for the debug build, a console window will be used for stdout
// and stderr. No need to redirect to a file. Better security since we can't
// guarantee that c: exists and writing a file to the root directory is
// never a good idea. release build still uses /SUBSYSTEM:WINDOWS

#if 0
#ifdef DEBUG
#ifdef __WXMSW__

    freopen("c:\\hugin_stdout.txt", "w", stdout);    // redirect stdout to file
    freopen("c:\\hugin_stderr.txt", "w", stderr);    // redirect stderr to file
#endif
#endif
#endif
    //reload gui level
    m_guiLevel=GUI_ADVANCED;
    long guiLevel=config->Read(wxT("/GuiLevel"),(long)0);
    guiLevel = std::max<long>(0, std::min<long>(2, guiLevel));
    if(guiLevel==GUI_SIMPLE && disableOpenGL)
    {
        guiLevel=GUI_ADVANCED;
    };
    SetGuiLevel(static_cast<GuiLevel>(guiLevel));

#ifndef __WXMSW__
    // check settings of help window and fix when needed
    FixHelpSettings();
#endif

    DEBUG_TRACE("");
}

MainFrame::~MainFrame()
{
    DEBUG_TRACE("dtor");
    if(m_guiLevel==GUI_SIMPLE)
    {
        delete m_menu_file_advanced;
    }
    else
    {
        delete m_menu_file_simple;
    };
    ImageCache::getInstance().setProgressDisplay(NULL);
	delete & ImageCache::getInstance();
    delete & PanoCommand::GlobalCmdHist::getInstance();
//    delete cpe;
//    delete images_panel;
    DEBUG_DEBUG("removing observer");
    pano.removeObserver(this);

    // get the global config object
    wxConfigBase* config = wxConfigBase::Get();

    StoreFramePosition(this, wxT("MainFrame"));

    //store most recently used files
    m_mruFiles.Save(*config);
    //store gui level
    config->Write(wxT("/GuiLevel"),(long)m_guiLevel);
    // store optimize only active images
    config->Write("/OptimizePanel/OnlyActiveImages", m_optOnlyActiveImages ? 1l : 0l);

    config->Flush();
    if(svmModel!=NULL)
    {
        celeste::destroySVMmodel(svmModel);
    };
    PanoOperation::CleanPanoOperationVector();

    DEBUG_TRACE("dtor end");
}

void MainFrame::panoramaChanged(HuginBase::Panorama &pano)
{
    wxToolBar* theToolBar = GetToolBar();
    wxMenuBar* theMenuBar = GetMenuBar();
    bool can_undo = PanoCommand::GlobalCmdHist::getInstance().canUndo();
    theMenuBar->Enable    (XRCID("ID_EDITUNDO"), can_undo);
    theToolBar->EnableTool(XRCID("ID_EDITUNDO"), can_undo);
    bool can_redo = PanoCommand::GlobalCmdHist::getInstance().canRedo();
    theMenuBar->Enable    (XRCID("ID_EDITREDO"), can_redo);
    theToolBar->EnableTool(XRCID("ID_EDITREDO"), can_redo);

    //show or hide optimizer and exposure optimizer tab depending on optimizer master switches
    if(pano.getOptimizerSwitch()==0 && !m_show_opt_panel)
    {
        m_notebook->InsertPage(3, opt_panel, _("Optimizer"));
        m_show_opt_panel=true;
    };
    if(pano.getOptimizerSwitch()!=0 && m_show_opt_panel)
    {
        m_notebook->RemovePage(3);
        m_show_opt_panel=false;
    };
    if(pano.getPhotometricOptimizerSwitch()==0 && !m_show_opt_photo_panel)
    {
        if(m_show_opt_panel)
        {
            m_notebook->InsertPage(4, opt_photo_panel, _("Exposure"));
        }
        else
        {
            m_notebook->InsertPage(3, opt_photo_panel, _("Exposure"));
        }
        m_show_opt_photo_panel=true;
    };
    if(pano.getPhotometricOptimizerSwitch()!=0 && m_show_opt_photo_panel)
    {
        if(m_show_opt_panel)
        {
            m_notebook->RemovePage(4);
        }
        else
        {
            m_notebook->RemovePage(3);
        };
        m_show_opt_photo_panel=false;
    };
    theMenuBar->Enable(XRCID("ID_SHOW_PANEL_OPTIMIZER"), m_show_opt_panel);
    theMenuBar->Enable(XRCID("ID_SHOW_PANEL_OPTIMIZER_PHOTOMETRIC"), m_show_opt_photo_panel);
}

//void MainFrame::panoramaChanged(HuginBase::Panorama &panorama)
void MainFrame::panoramaImagesChanged(HuginBase::Panorama &panorama, const HuginBase::UIntSet & changed)
{
    DEBUG_TRACE("");
    assert(&pano == &panorama);
    enableTools(pano.getNrOfImages() > 0);
}

void MainFrame::OnUserQuit(wxCommandEvent & e)
{
    Close();
}

bool MainFrame::CloseProject(bool cancelable, CloseReason reason)
{
    if (pano.isDirty()) {
        wxString messageString;
        switch (reason)
        {
            case LOAD_NEW_PROJECT:
                messageString = _("Save changes to the project file before opening another project?");
                break;
            case NEW_PROJECT:
                messageString = _("Save changes to the project file before starting a new project?");
                break;
            case CLOSE_PROGRAM:
            default:
                messageString = _("Save changes to the project file before closing?");
                break;
        };
                wxMessageDialog message(wxGetActiveWindow(), messageString,
#ifdef _WIN32
                                _("Hugin"),
#else
                                wxT(""),
#endif
                                wxICON_EXCLAMATION | wxYES_NO | (cancelable? (wxCANCEL):0));
        switch(reason)
        {
            case LOAD_NEW_PROJECT:
                message.SetExtendedMessage(_("If you load another project without saving, your changes since last save will be discarded."));
                break;
            case NEW_PROJECT:
                message.SetExtendedMessage(_("If you start a new project without saving, your changes since last save will be discarded."));
                break;
            case CLOSE_PROGRAM:
            default:
                message.SetExtendedMessage(_("If you close without saving, your changes since your last save will be discarded."));
                break;
        };
    #if defined __WXMAC__ || defined __WXMSW__
        // Apple human interface guidelines and Windows user experience interaction guidelines
        message.SetYesNoLabels(wxID_SAVE, _("Don't Save"));
    #else
        // Gnome human interface guidelines:
        message.SetYesNoLabels(wxID_SAVE, _("Close without saving"));
    #endif
        int answer = message.ShowModal();
        switch (answer){
            case wxID_YES:
            {
                wxCommandEvent dummy;
                OnSaveProject(dummy);
                return !pano.isDirty();
            }
            case wxID_CANCEL:
                return false;
            default: //no save
                return true;
        }
    }
    else return true;
}

void MainFrame::OnExit(wxCloseEvent & e)
{
    DEBUG_TRACE("");
    if(m_guiLevel!=GUI_SIMPLE)
    {
        if(!CloseProject(e.CanVeto(), CLOSE_PROGRAM))
        {
           if (e.CanVeto())
           {
                e.Veto();
                return;
           }
           wxLogError(_("forced close"));
        }
    }
    else
    {
        if(e.CanVeto())
        {
            Hide();
            e.Veto();
            return;
        };
    };

    if(preview_frame)
    {
       preview_frame->Close(true);
    }

    ImageCache::getInstance().flush();
    Destroy();
    DEBUG_TRACE("");
}

void MainFrame::OnSaveProject(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    try {
    wxFileName scriptName = m_filename;
    if (m_filename == wxT("")) {
        OnSaveProjectAs(e);
        scriptName = m_filename;
    } else {
        // the project file is just a PTOptimizer script...
        std::string path = hugin_utils::getPathPrefix(std::string(scriptName.GetFullPath().mb_str(HUGIN_CONV_FILENAME)));
        DEBUG_DEBUG("stripping " << path << " from image filenames");
        std::ofstream script(scriptName.GetFullPath().mb_str(HUGIN_CONV_FILENAME));
        script.exceptions ( std::ofstream::eofbit | std::ofstream::failbit | std::ofstream::badbit );
        HuginBase::UIntSet all;
        if (pano.getNrOfImages() > 0) {
           fill_set(all, 0, pano.getNrOfImages()-1);
        }
        pano.printPanoramaScript(script, pano.getOptimizeVector(), pano.getOptions(), all, false, path);
        script.close();

        SetStatusText(wxString::Format(_("saved project %s"), m_filename.c_str()),0);
        if(m_guiLevel==GUI_SIMPLE)
        {
            if(gl_preview_frame)
            {
                gl_preview_frame->SetTitle(scriptName.GetName() + wxT(".") + scriptName.GetExt() + wxT(" - ") + _("Hugin - Panorama Stitcher"));
            };
            SetTitle(scriptName.GetName() + wxT(".") + scriptName.GetExt() + wxT(" - ") + _("Panorama editor"));
        }
        else
        {
            SetTitle(scriptName.GetName() + wxT(".") + scriptName.GetExt() + wxT(" - ") + _("Hugin - Panorama Stitcher"));
        };

        pano.clearDirty();
    }
    } catch (std::exception & e) {
        wxString err(e.what(), wxConvLocal);
            wxMessageBox(wxString::Format(_("Could not save project file \"%s\".\nMaybe the file or the folder is read-only.\n\n(Error code: %s)"),m_filename.c_str(),err.c_str()),_("Error"),wxOK|wxICON_ERROR);
    }
}

void MainFrame::OnSaveProjectAs(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    wxFileName scriptName;
    if (m_filename.IsEmpty())
    {
        scriptName.Assign(getDefaultProjectName(pano) + wxT(".pto"));
    }
    else
    {
        scriptName=m_filename;
    };
    scriptName.Normalize();
    wxFileDialog dlg(wxGetActiveWindow(),
                     _("Save project file"),
                     scriptName.GetPath(), scriptName.GetFullName(),
                     _("Project files (*.pto)|*.pto|All files (*)|*"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);
    if (dlg.ShowModal() == wxID_OK) {
        wxConfig::Get()->Write(wxT("/actualPath"), dlg.GetDirectory());  // remember for later
        wxString fn = dlg.GetPath();
        if (fn.Right(4).CmpNoCase(wxT(".pto"))!=0)
        {
            fn.Append(wxT(".pto"));
            if (wxFile::Exists(fn)) {
                int d = wxMessageBox(wxString::Format(_("File %s exists. Overwrite?"), fn.c_str()),
                    _("Save project"), wxYES_NO | wxICON_QUESTION);
                if (d != wxYES) {
                    return;
                }
            }
        }
        m_filename = fn;
        m_mruFiles.AddFileToHistory(m_filename);
        OnSaveProject(e);
    }
}

void MainFrame::OnSavePTStitcherAs(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    wxString scriptName = m_filename;
    if (m_filename == wxT("")) {
        scriptName = getDefaultProjectName(pano);
    }
    wxFileName scriptNameFN(scriptName);
    wxString fn = scriptNameFN.GetName() + wxT(".txt");
    wxFileDialog dlg(wxGetActiveWindow(),
                     _("Save PTmender script file"),
                     wxConfigBase::Get()->Read(wxT("/actualPath"),wxT("")), fn,
                     _("PTmender files (*.txt)|*.txt"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);
    if (dlg.ShowModal() == wxID_OK) {
        wxString fname = dlg.GetPath();
        // the project file is just a PTStitcher script...
        wxFileName scriptName = fname;
        HuginBase::UIntSet all;
        if (pano.getNrOfImages() > 0) {
            fill_set(all, 0, pano.getNrOfImages()-1);
        }
        std::ofstream script(scriptName.GetFullPath().mb_str(HUGIN_CONV_FILENAME));
        pano.printStitcherScript(script, pano.getOptions(), all);
        script.close();
    }

}

void MainFrame::LoadProjectFile(const wxString & filename)
{
    DEBUG_TRACE("");

    // remove old images from cache
    // hmm probably not a good idea, if the project is reloaded..
    // ImageCache::getInstance().flush();

    SetStatusText( _("Open project:   ") + filename);

    wxFileName fname(filename);
    wxString path = fname.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
    if (fname.IsOk() && fname.FileExists())
    {
        m_filename = filename;
        wxBusyCursor wait;
        deregisterPTWXDlgFcn();
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::wxLoadPTProjectCmd(pano, (const char *)filename.mb_str(HUGIN_CONV_FILENAME), (const char *)path.mb_str(HUGIN_CONV_FILENAME), true)
           );
        PanoCommand::GlobalCmdHist::getInstance().clear();
        registerPTWXDlgFcn();
        DEBUG_DEBUG("project contains " << pano.getNrOfImages() << " after load");
        GuiLevel reqGuiLevel=GetMinimumGuiLevel(pano);
        if(reqGuiLevel>m_guiLevel)
        {
            SetGuiLevel(reqGuiLevel);
        };
        if (pano.getNrOfImages() > 0)
        {
            SetStatusText(_("Project opened"));
            m_mruFiles.AddFileToHistory(fname.GetFullPath());
            if (m_guiLevel == GUI_SIMPLE)
            {
                if (gl_preview_frame)
                {
                    gl_preview_frame->SetTitle(fname.GetName() + wxT(".") + fname.GetExt() + wxT(" - ") + _("Hugin - Panorama Stitcher"));
                };
                SetTitle(fname.GetName() + wxT(".") + fname.GetExt() + wxT(" - ") + _("Panorama editor"));
            }
            else
            {
                SetTitle(fname.GetName() + wxT(".") + fname.GetExt() + wxT(" - ") + _("Hugin - Panorama Stitcher"));
            };
        }
        else
        {
            SetStatusText(_("Loading canceled"));
            m_filename = "";
            if (m_guiLevel == GUI_SIMPLE)
            {
                if (gl_preview_frame)
                {
                    gl_preview_frame->SetTitle(_("Hugin - Panorama Stitcher"));
                };
                SetTitle(_("Panorama editor"));
            }
            else
            {
                SetTitle(_("Hugin - Panorama Stitcher"));
            };
        };
        if (! (fname.GetExt() == wxT("pto"))) {
            // do not remember filename if its not a hugin project
            // to avoid overwriting the original project with an
            // incompatible one
            m_filename = wxT("");
        }
        // get the global config object
        wxConfigBase* config = wxConfigBase::Get();
        config->Write(wxT("/actualPath"), path);  // remember for later
    } else {
        SetStatusText( _("Error opening project:   ") + filename);
        DEBUG_ERROR("Could not open file " << filename);
    }

    // force update of preview window
    if ( !(preview_frame->IsIconized() ||(! preview_frame->IsShown()) ) ) {
        wxCommandEvent dummy;
        preview_frame->OnUpdate(dummy);
    }
}

#ifdef __WXMAC__
void MainFrame::MacOnOpenFile(const wxString & filename)
{
    if(!CloseProject(true, LOAD_NEW_PROJECT)) return; //if closing old project is canceled do nothing.

    ImageCache::getInstance().flush();
    LoadProjectFile(filename);
}
#endif

void MainFrame::OnLoadProject(wxCommandEvent & e)
{
    DEBUG_TRACE("");

    if(CloseProject(true, LOAD_NEW_PROJECT)) //if closing old project is canceled do nothing.
    {
        // get the global config object
        wxConfigBase* config = wxConfigBase::Get();

        wxString defaultdir = config->Read(wxT("/actualPath"),wxT(""));
        wxFileDialog dlg(wxGetActiveWindow(),
                         _("Open project file"),
                         defaultdir, wxT(""),
                         _("Project files (*.pto)|*.pto|All files (*)|*"),
                         wxFD_OPEN, wxDefaultPosition);
        dlg.SetDirectory(defaultdir);
        if (dlg.ShowModal() == wxID_OK)
        {
            wxString filename = dlg.GetPath();
            if(vigra::isImage(filename.mb_str(HUGIN_CONV_FILENAME)))
            {
                if(wxMessageBox(wxString::Format(_("File %s is an image file and not a project file.\nThis file can't be open with File, Open.\nDo you want to add this image file to the current project?"),filename.c_str()),
#ifdef __WXMSW__
                    _("Hugin"),
#else
                    wxT(""),
#endif
                    wxYES_NO | wxICON_QUESTION)==wxYES)
                {
                    wxArrayString filenameArray;
                    filenameArray.Add(filename);
                    AddImages(filenameArray);
                };
                return;
            }
            else
            {
                // remove old images from cache
                ImageCache::getInstance().flush();

                LoadProjectFile(filename);
                return;
            };
        }
    }
    // do not close old project
    // nothing to open
    SetStatusText( _("Open project: cancel"));
}

void MainFrame::OnNewProject(wxCommandEvent & e)
{
    if(!CloseProject(true, NEW_PROJECT)) return; //if closing current project is canceled

    m_filename = wxT("");
    PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::wxNewProjectCmd(pano));
    PanoCommand::GlobalCmdHist::getInstance().clear();
    // remove old images from cache
    ImageCache::getInstance().flush();
    if(m_guiLevel==GUI_SIMPLE)
    {
        if(gl_preview_frame)
        {
            gl_preview_frame->SetTitle(_("Hugin - Panorama Stitcher"));
        };
        SetTitle(_("Panorama editor"));
    }
    else
    {
        SetTitle(_("Hugin - Panorama Stitcher"));
    };

    wxCommandEvent dummy;
    preview_frame->OnUpdate(dummy);
}

void MainFrame::OnAddImages( wxCommandEvent& event )
{
    DEBUG_TRACE("");
    PanoOperation::AddImageOperation addImage;
    HuginBase::UIntSet images;
    PanoCommand::PanoCommand* cmd = addImage.GetCommand(wxGetActiveWindow(), pano, images, m_guiLevel);
    if(cmd!=NULL)
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(cmd);
    }
    else
    {
        // nothing to open
        SetStatusText( _("Add Image: cancel"));
    }

    DEBUG_TRACE("");
}

void MainFrame::AddImages(wxArrayString& filenameArray)
{
    wxArrayString invalidFiles;
    for(unsigned int i=0;i<filenameArray.GetCount(); i++)
    {
        if(containsInvalidCharacters(filenameArray[i]))
        {
            invalidFiles.Add(filenameArray[i]);
        };
    };
    if(invalidFiles.size()>0)
    {
        ShowFilenameWarning(this, invalidFiles);
    }
    else
    {
        std::vector<std::string> filesv;
        for (unsigned int i=0; i< filenameArray.GetCount(); i++) {
            filesv.push_back((const char *)filenameArray[i].mb_str(HUGIN_CONV_FILENAME));
        }

        // we got some images to add.
        if (filesv.size() > 0) {
            // use a Command to ensure proper undo and updating of GUI
            // parts
            wxBusyCursor();
            PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::wxAddImagesCmd(pano, filesv)
                );
        };
    };
};

void MainFrame::OnAddTimeImages( wxCommandEvent& event )
{
    PanoOperation::AddImagesSeriesOperation imageSeriesOp;
    HuginBase::UIntSet images;
    PanoCommand::PanoCommand* cmd = imageSeriesOp.GetCommand(wxGetActiveWindow(), pano, images, m_guiLevel);
    if(cmd!=NULL)
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(cmd);
    };
};


void MainFrame::OnShowPanel(wxCommandEvent & e)
{
    if(e.GetId()==XRCID("ID_SHOW_PANEL_MASK"))
        m_notebook->SetSelection(1);
    else
        if(e.GetId()==XRCID("ID_SHOW_PANEL_CP_EDITOR"))
            m_notebook->SetSelection(2);
        else
            if(e.GetId()==XRCID("ID_SHOW_PANEL_OPTIMIZER"))
                m_notebook->SetSelection(3);
            else
                if(e.GetId()==XRCID("ID_SHOW_PANEL_OPTIMIZER_PHOTOMETRIC"))
                {
                    if(m_show_opt_panel)
                    {
                        m_notebook->SetSelection(4);
                    }
                    else
                    {
                        m_notebook->SetSelection(3);
                    };
                }
                else
                    if(e.GetId()==XRCID("ID_SHOW_PANEL_PANORAMA"))
                    {
                        if(m_show_opt_panel && m_show_opt_photo_panel)
                        {
                            m_notebook->SetSelection(5);
                        }
                        else
                        {
                            if(m_show_opt_panel || m_show_opt_photo_panel)
                            {
                                m_notebook->SetSelection(4);
                            }
                            else
                            {
                                m_notebook->SetSelection(3);
                            };
                        };
                    }
                    else
                        m_notebook->SetSelection(0);
}

void MainFrame::OnLoadingFailed(wxCommandEvent & e)
{
    // check if file exists
    if (wxFileExists(e.GetString()))
    {
        // file exists, but could not loaded
        wxMessageBox(wxString::Format(_("Could not load image \"%s\".\nThis file is not a valid image.\nThis file will be removed from the project."), e.GetString()),
#ifdef _WIN32
            _("Hugin"),
#else
            wxT(""),
#endif
            wxOK | wxICON_ERROR);
    }
    else
    {
        // file does not exists
        wxMessageBox(wxString::Format(_("Could not load image \"%s\".\nThis file was renamed, deleted or is on a non-accessible drive.\nThis file will be removed from the project."), e.GetString()),
#ifdef _WIN32
            _("Hugin"),
#else
            wxT(""),
#endif
            wxOK | wxICON_ERROR);
    };
    // now remove the file from the pano
    const std::string filename(e.GetString().mb_str(HUGIN_CONV_FILENAME));
    HuginBase::UIntSet imagesToRemove;
    for (size_t i = 0; i < pano.getNrOfImages(); ++i)
    {
        if (pano.getImage(i).getFilename() == filename)
        {
            imagesToRemove.insert(i);
        };
    };
    if (!imagesToRemove.empty())
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::RemoveImagesCmd(pano, imagesToRemove));
    };
}


void MainFrame::OnAbout(wxCommandEvent & e)
{
    AboutDialog dlg(wxGetActiveWindow());
    dlg.ShowModal();
}

/*
void MainFrame::OnAbout(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    wxDialog dlg;
	wxString strFile;
	wxString langCode;

    wxXmlResource::Get()->LoadDialog(&dlg, this, wxT("about_dlg"));

#if __WXMAC__ && defined MAC_SELF_CONTAINED_BUNDLE
    //rely on the system's locale choice
    strFile = MacGetPathToBundledResourceFile(CFSTR("about.htm"));
    if(strFile!=wxT("")) XRCCTRL(dlg,"about_html",wxHtmlWindow)->LoadPage(strFile);
#else
    //if the language is not default, load custom About file (if exists)
    langCode = huginApp::Get()->GetLocale().GetName().Left(2).Lower();
    DEBUG_INFO("Lang Code: " << langCode.mb_str(wxConvLocal));
    if(langCode != wxString(wxT("en")))
    {
        strFile = GetXRCPath() + wxT("data/about_") + langCode + wxT(".htm");
        if(wxFile::Exists(strFile))
        {
            DEBUG_TRACE("Using About: " << strFile.mb_str(wxConvLocal));
            XRCCTRL(dlg,"about_html",wxHtmlWindow)->LoadPage(strFile);
        }
    }
#endif
    dlg.ShowModal();
}
*/

void MainFrame::OnHelp(wxCommandEvent & e)
{
    DisplayHelp();
}

void MainFrame::OnKeyboardHelp(wxCommandEvent & e)
{
    DisplayHelp(wxT("Hugin_Keyboard_shortcuts.html"));
}

void MainFrame::OnFAQ(wxCommandEvent & e)
{
    DisplayHelp(wxT("Hugin_FAQ.html"));
}


void MainFrame::DisplayHelp(wxString section)
{
    if (section.IsEmpty())
    {
        GetHelpController().DisplayContents();
    }
    else
    {
#if defined __wxMSW__ && !(wxCHECK_VERSION(3,1,1))
        // wxWidgets 3.x has a bug, that prevents DisplaySection to work on Win8/10 64 bit
        // see: http://trac.wxwidgets.org/ticket/14888
        // so using DisplayContents() and our own implementation of HuginCHMHelpController
        GetHelpController().DisplayHelpPage(section);
#else
        GetHelpController().DisplaySection(section);
#endif
    };
}

void MainFrame::OnTipOfDay(wxCommandEvent& WXUNUSED(e))
{
    wxString strFile;
    bool bShowAtStartup;
// DGSW FIXME - Unreferenced
//	bool bTipsExist = false;
    int nValue;

    wxConfigBase * config = wxConfigBase::Get();
    nValue = config->Read(wxT("/MainFrame/ShowStartTip"),1l);

    //TODO: tips not localisable
    DEBUG_INFO("Tip index: " << nValue);
    strFile = GetXRCPath() + wxT("data/tips.txt");  //load default file

    DEBUG_INFO("Reading tips from " << strFile.mb_str(wxConvLocal));
    wxTipProvider *tipProvider = new LocalizedFileTipProvider(strFile, nValue);
    bShowAtStartup = wxShowTip(wxGetActiveWindow(), tipProvider, (nValue ? true : false));

    //store startup preferences
    nValue = (bShowAtStartup ? tipProvider->GetCurrentTip() : 0);
    DEBUG_INFO("Writing tip index: " << nValue);
    config->Write(wxT("/MainFrame/ShowStartTip"), nValue);
    delete tipProvider;
}


void MainFrame::OnShowPrefs(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    PreferencesDialog pref_dlg(wxGetActiveWindow());
    pref_dlg.ShowModal();
    //update image cache size
    wxConfigBase* cfg=wxConfigBase::Get();
#if defined __WXMSW__
    unsigned long long mem = HUGIN_IMGCACHE_UPPERBOUND;
    unsigned long mem_low = cfg->Read(wxT("/ImageCache/UpperBound"), HUGIN_IMGCACHE_UPPERBOUND);
    unsigned long mem_high = cfg->Read(wxT("/ImageCache/UpperBoundHigh"), (long) 0);
    if (mem_high > 0)
    {
      mem = ((unsigned long long) mem_high << 32) + mem_low;
    }
    else
    {
      mem = mem_low;
    }
    ImageCache::getInstance().SetUpperLimit(mem);
#else
    ImageCache::getInstance().SetUpperLimit(cfg->Read(wxT("/ImageCache/UpperBound"), HUGIN_IMGCACHE_UPPERBOUND));
#endif
    images_panel->ReloadCPDetectorSettings();
    if(gl_preview_frame)
    {
        gl_preview_frame->SetShowProjectionHints(cfg->Read(wxT("/GLPreviewFrame/ShowProjectionHints"),HUGIN_SHOW_PROJECTION_HINTS)!=0);
    };
}

void MainFrame::UpdatePanels( wxCommandEvent& WXUNUSED(event) )
{   // Maybe this can be invoced by the Panorama::Changed() or
    // something like this. So no everytime update would be needed.
    DEBUG_TRACE("");
}

void MainFrame::OnTogglePreviewFrame(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    if (preview_frame->IsIconized()) {
        preview_frame->Iconize(false);
    }
    preview_frame->Show();
    preview_frame->Raise();

	// we need to force an update since autoupdate fires
	// before the preview frame is shown
    wxCommandEvent dummy;
	preview_frame->OnUpdate(dummy);
}

void MainFrame::OnToggleGLPreviewFrame(wxCommandEvent & e)
{
    if(gl_preview_frame==NULL)
    {
        return;
    };
#if defined __WXMSW__ || defined __WXMAC__
    gl_preview_frame->InitPreviews();
#endif
    if (gl_preview_frame->IsIconized()) {
        gl_preview_frame->Iconize(false);
    }
    gl_preview_frame->Show();
#if defined __WXMSW__
    // on wxMSW Show() does not send OnShowEvent needed to update the
    // visibility state of the fast preview windows
    // so explicit calling this event handler
    wxShowEvent se;
    se.SetShow(true);
    gl_preview_frame->OnShowEvent(se);
#elif defined __WXGTK__
    gl_preview_frame->LoadOpenGLLayout();
#endif
    gl_preview_frame->Raise();
}

void MainFrame::OnShowCPFrame(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    if (cp_frame) {
        if (cp_frame->IsIconized()) {
            cp_frame->Iconize(false);
        }
        cp_frame->Show();
        cp_frame->Raise();
    } else {
        cp_frame = new CPListFrame(this, pano);
        cp_frame->Show();
    }
}

void MainFrame::OnCPListFrameClosed()
{
    cp_frame = 0;
}

void MainFrame::OnOptimize(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    wxCommandEvent dummy;
    opt_panel->OnOptimizeButton(dummy);
}

void MainFrame::OnOnlyActiveImages(wxCommandEvent &e)
{
    SetOptimizeOnlyActiveImages(GetMenuBar()->IsChecked(XRCID("action_optimize_only_active")));
};

void MainFrame::SetOptimizeOnlyActiveImages(const bool onlyActive)
{
    m_optOnlyActiveImages = onlyActive;
    wxMenuBar* menubar = GetMenuBar();
    if (menubar)
    {
        menubar->Check(XRCID("action_optimize_only_active"), onlyActive);
    };
    opt_panel->SetOnlyActiveImages(m_optOnlyActiveImages);
    opt_photo_panel->SetOnlyActiveImages(m_optOnlyActiveImages);
    // notify all observer so they can update their display
    pano.changeFinished();
};

const bool MainFrame::GetOptimizeOnlyActiveImages() const
{
    return m_optOnlyActiveImages;
};

void MainFrame::OnIgnoreLineCp(wxCommandEvent &e)
{
    m_optIgnoreLineCp = GetMenuBar()->IsChecked(XRCID("action_optimize_ignore_line_cp"));
    opt_panel->SetIgnoreLineCP(m_optIgnoreLineCp);
    // notify all observer so they can update their display
    pano.changeFinished();
};

void MainFrame::SetOptimizeIgnoreLineCp(const bool ignoreLineCP)
{
    m_optIgnoreLineCp = ignoreLineCP;
    wxMenuBar* menubar = GetMenuBar();
    if (menubar)
    {
        menubar->Check(XRCID("action_optimize_ignore_line_cp"), ignoreLineCP);
        // notify all observer so they can update their display
        pano.changeFinished();
    };
};

const bool MainFrame::GetOptimizeIgnoreLineCp() const
{
    return m_optIgnoreLineCp;
};

void MainFrame::OnPhotometricOptimize(wxCommandEvent & e)
{
    wxCommandEvent dummy;
    opt_photo_panel->OnOptimizeButton(dummy);
};

void MainFrame::OnDoStitch(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    wxCommandEvent cmdEvt(wxEVT_COMMAND_BUTTON_CLICKED,XRCID("pano_button_stitch"));
    pano_panel->GetEventHandler()->AddPendingEvent(cmdEvt);
}

void MainFrame::OnUserDefinedStitch(wxCommandEvent & e)
{
    pano_panel->DoUserDefinedStitch();
}

void MainFrame::OnUserDefinedStitchSaved(wxCommandEvent & e)
{
    auto filename = m_userOutput.find(e.GetId());
    if (filename != m_userOutput.end())
    {
        pano_panel->DoUserDefinedStitch(filename->second);
    }
    else
    {
        wxBell();
    };
}

void MainFrame::OnMergeProject(wxCommandEvent & e)
{
    // get the global config object
    wxConfigBase* config = wxConfigBase::Get();

    wxString defaultdir = config->Read(wxT("/actualPath"),wxT(""));
    wxFileDialog dlg(wxGetActiveWindow(),
                     _("Open project file"),
                     defaultdir, wxT(""),
                     _("Project files (*.pto)|*.pto|All files (*)|*"),
                     wxFD_OPEN, wxDefaultPosition);
    dlg.SetDirectory(defaultdir);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxString filename = dlg.GetPath();
        wxFileName fname(filename);
        wxString path = fname.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
        if (fname.IsOk() && fname.FileExists())
        {
            wxBusyCursor wait;
            HuginBase::PanoramaMemento newPano;
            std::ifstream in((const char *)fname.GetFullPath().mb_str(HUGIN_CONV_FILENAME));
            int ptoversion=0;
            if (newPano.loadPTScript(in, ptoversion, (const char *)path.mb_str(HUGIN_CONV_FILENAME)))
            {
                HuginBase::Panorama new_pano;
                new_pano.setMemento(newPano);
                PanoCommand::GlobalCmdHist::getInstance().addCommand(
                    new PanoCommand::MergePanoCmd(pano, new_pano)
                );
                m_mruFiles.AddFileToHistory(fname.GetFullPath());
                // force update of preview window
                if ( !(preview_frame->IsIconized() ||(! preview_frame->IsShown()) ) )
                {
                    wxCommandEvent dummy;
                    preview_frame->OnUpdate(dummy);
                };
            }
            else
            {
                wxMessageBox(wxString::Format(_("Could not read project file %s."),fname.GetFullPath().c_str()),_("Error"),wxOK|wxICON_ERROR);
            };
        };
    }
}

void MainFrame::OnReadPapywizard(wxCommandEvent & e)
{
    wxString currentDir = wxConfigBase::Get()->Read(wxT("/actualPath"), wxT(""));
    wxFileDialog dlg(wxGetActiveWindow(), _("Open Papywizard xml file"),
        currentDir, wxT(""), _("Papywizard xml files (*.xml)|*.xml|All files (*)|*"),
        wxFD_OPEN, wxDefaultPosition);
    dlg.SetDirectory(currentDir);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxConfigBase::Get()->Write(wxT("/actualPath"), dlg.GetDirectory());
        Papywizard::ImportPapywizardFile(dlg.GetPath(), pano);
    };
};

void MainFrame::OnApplyTemplate(wxCommandEvent & e)
{
    // get the global config object
    wxConfigBase* config = wxConfigBase::Get();

    wxFileDialog dlg(wxGetActiveWindow(),
                     _("Choose template project"),
                     config->Read(wxT("/templatePath"),wxT("")), wxT(""),
                     _("Project files (*.pto)|*.pto|All files (*)|*"),
                     wxFD_OPEN, wxDefaultPosition);
    dlg.SetDirectory(wxConfigBase::Get()->Read(wxT("/templatePath"),wxT("")));
    if (dlg.ShowModal() == wxID_OK) {
        wxString filename = dlg.GetPath();
        wxConfig::Get()->Write(wxT("/templatePath"), dlg.GetDirectory());  // remember for later

        std::ifstream file((const char *)filename.mb_str(HUGIN_CONV_FILENAME));

        PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::wxApplyTemplateCmd(pano, file));

    }
}

void MainFrame::OnOpenPTBatcher(wxCommandEvent & e)
{
#if defined __WXMAC__ && defined MAC_SELF_CONTAINED_BUNDLE
	// Original patch for OSX by Charlie Reiman dd. 18 June 2011
	// Slightly modified by HvdW. Errors in here are mine, not Charlie's. 
	FSRef appRef;
	FSRef actuallyLaunched;
	OSStatus err;
	FSRef documentArray[1]; // Don't really need an array if we only have 1 item
	LSLaunchFSRefSpec launchSpec;
	Boolean  isDir;
	
	err = LSFindApplicationForInfo(kLSUnknownCreator,
								   CFSTR("net.sourceforge.hugin.PTBatcherGUI"),
								   NULL,
								   &appRef,
								   NULL);
	if (err != noErr) {
		// error, can't find PTBatcherGUI
		wxMessageBox(wxString::Format(_("External program %s not found in the bundle, reverting to system path"), wxT("open")), _("Error"));
		// Possibly a silly attempt otherwise the previous would have worked as well, but just try it.
		wxExecute(_T("open -b net.sourceforge.hugin.PTBatcherGUI"));
	}
	else {
		wxExecute(_T("open -b net.sourceforge.hugin.PTBatcherGUI"));
	}	
#else
    const wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
	wxExecute(exePath.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + _T("PTBatcherGUI"));
#endif
}

void MainFrame::OnFineTuneAll(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    // fine-tune all points

    HuginBase::CPVector cps = pano.getCtrlPoints();

    // create a map of all control points.
    std::set<unsigned int> unoptimized;
    for (unsigned int i=0; i < cps.size(); i++) {
        // create all control points.
        unoptimized.insert(i);
    }

    unsigned int nGood=0;
    unsigned int nBad=0;

    wxConfigBase *cfg = wxConfigBase::Get();
    double corrThresh=HUGIN_FT_CORR_THRESHOLD;
    cfg->Read(wxT("/Finetune/CorrThreshold"), &corrThresh, HUGIN_FT_CORR_THRESHOLD);
    double curvThresh = HUGIN_FT_CURV_THRESHOLD;
    cfg->Read(wxT("/Finetune/CurvThreshold"),&curvThresh, HUGIN_FT_CURV_THRESHOLD);
    // load parameters
    const long templWidth = cfg->Read(wxT("/Finetune/TemplateSize"), HUGIN_FT_TEMPLATE_SIZE);
    const long sWidth = templWidth + cfg->Read(wxT("/Finetune/LocalSearchWidth"), HUGIN_FT_LOCAL_SEARCH_WIDTH);

    {
        ProgressReporterDialog progress(unoptimized.size(), _("Fine-tuning all points"), _("Fine-tuning"), wxGetActiveWindow());

    ImageCache & imgCache = ImageCache::getInstance();

    // do not process the control points in random order,
    // but walk from image to image, to reduce image reloading
    // in low mem situations.
    for (unsigned int imgNr = 0 ; imgNr < pano.getNrOfImages(); imgNr++) {
        std::set<unsigned int>::iterator it=unoptimized.begin();

        imgCache.softFlush();

        while (it != unoptimized.end()) {
            if (cps[*it].image1Nr == imgNr || cps[*it].image2Nr == imgNr) {
                if (!progress.updateDisplayValue())
                {
                    return;
                };
                if (cps[*it].mode == HuginBase::ControlPoint::X_Y) {
                    // finetune only normal points
                    DEBUG_DEBUG("fine tuning point: " << *it);
                    wxImage wxSearchImg;
                    ImageCache::ImageCacheRGB8Ptr searchImg = imgCache.getImage(
                        pano.getImage(cps[*it].image2Nr).getFilename())->get8BitImage();

                    ImageCache::ImageCacheRGB8Ptr templImg = imgCache.getImage(
                        pano.getImage(cps[*it].image1Nr).getFilename())->get8BitImage();

                    vigra_ext::CorrelationResult res;
                    vigra::Diff2D roundP1(hugin_utils::roundi(cps[*it].x1), hugin_utils::roundi(cps[*it].y1));
                    vigra::Diff2D roundP2(hugin_utils::roundi(cps[*it].x2), hugin_utils::roundi(cps[*it].y2));

                    res = PointFineTuneProjectionAware(pano.getImage(cps[*it].image1Nr), *templImg, roundP1, templWidth,
                        pano.getImage(cps[*it].image2Nr), *searchImg, roundP2, sWidth);

                    // invert curvature. we always assume its a maxima, the curvature there is negative
                    // however, we allow the user to specify a positive threshold, so we need to
                    // invert it
                    res.curv.x = - res.curv.x;
                    res.curv.y = - res.curv.y;

                    if (res.maxi < corrThresh || res.curv.x < curvThresh || res.curv.y < curvThresh ||
                        res.maxpos.x < 0 || res.maxpos.y < 0 || res.corrPos.x < 0 || res.corrPos.y < 0)
                    {
                        // Bad correlation result.
                        nBad++;
                        if (res.maxi >= corrThresh) {
                            cps[*it].error = 0;
                        }
                        cps[*it].error = res.maxi;
                        DEBUG_DEBUG("low correlation: " << res.maxi << " curv: " << res.curv);
                    } else {
                        nGood++;
                        // only update if a good correlation was found
                        cps[*it].x1 = res.corrPos.x;
                        cps[*it].y1 = res.corrPos.y;
                        cps[*it].x2 = res.maxpos.x;
                        cps[*it].y2 = res.maxpos.y;
                        cps[*it].error = res.maxi;
                    }
                }
                unsigned int rm = *it;
                ++it;
                unoptimized.erase(rm);
            } else {
                ++it;
            }
        }
    }
    }
    wxString result;
    result.Printf(_("%d points fine-tuned, %d points not updated due to low correlation\n\nHint: The errors of the fine-tuned points have been set to the correlation coefficient\nProblematic points can be spotted (just after fine-tune, before optimizing)\nby an error <= %.3f.\nThe error of points without a well defined peak (typically in regions with uniform color)\nwill be set to 0\n\nUse the Control Point list (F3) to see all points of the current project\n"),
                  nGood, nBad, corrThresh);
    wxMessageBox(result, _("Fine-tune result"), wxOK);
    // set newly optimized points
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::UpdateCPsCmd(pano, cps, false)
        );
}

void MainFrame::OnRemoveCPinMasks(wxCommandEvent & e)
{
    if(pano.getCtrlPoints().size()<2)
        return;
    HuginBase::UIntSet cps=getCPinMasks(pano);
    if(cps.size()>0)
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                    new PanoCommand::RemoveCtrlPointsCmd(pano,cps)
                    );
        wxMessageBox(wxString::Format(_("Removed %lu control points"), static_cast<unsigned long>(cps.size())),
                   _("Removing control points in masks"),wxOK|wxICON_INFORMATION);
    };
}

#ifdef HUGIN_HSI
void MainFrame::OnPythonScript(wxCommandEvent & e)
{
    wxString fname;
    wxFileDialog dlg(wxGetActiveWindow(),
            _("Select python script"),
            wxConfigBase::Get()->Read(wxT("/lensPath"),wxT("")), wxT(""),
            _("Python script (*.py)|*.py|All files (*.*)|*.*"),
            wxFD_OPEN, wxDefaultPosition);
    dlg.SetDirectory(wxConfigBase::Get()->Read(wxT("/pythonScriptPath"),wxT("")));

    if (dlg.ShowModal() == wxID_OK)
    {
        wxString filename = dlg.GetPath();
        wxConfig::Get()->Write(wxT("/pythonScriptPath"), dlg.GetDirectory());
        std::string scriptfile((const char *)filename.mb_str(HUGIN_CONV_FILENAME));
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
            new PanoCommand::PythonScriptPanoCmd(pano,scriptfile)
            );
    }
}

void MainFrame::OnPlugin(wxCommandEvent & e)
{
    wxFileName file=m_plugins[e.GetId()];
    if(file.FileExists())
    {
        std::string scriptfile((const char *)file.GetFullPath().mb_str(HUGIN_CONV_FILENAME));
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                                 new PanoCommand::PythonScriptPanoCmd(pano,scriptfile)
                                 );
    }
    else
    {
        wxMessageBox(wxString::Format(wxT("Python-Script %s not found.\nStopping processing."),file.GetFullPath().c_str()),_("Warning"),wxOK|wxICON_INFORMATION);
    };
}

#endif

void MainFrame::OnUndo(wxCommandEvent & e)
{
    DEBUG_TRACE("OnUndo");
    if (PanoCommand::GlobalCmdHist::getInstance().canUndo())
    {
        PanoCommand::GlobalCmdHist::getInstance().undo();
    }
    else
    {
        wxBell();
    };
}

void MainFrame::OnRedo(wxCommandEvent & e)
{
    DEBUG_TRACE("OnRedo");
    if (PanoCommand::GlobalCmdHist::getInstance().canRedo())
    {
        PanoCommand::GlobalCmdHist::getInstance().redo();
    };
}

void MainFrame::ShowCtrlPoint(unsigned int cpNr)
{
    DEBUG_DEBUG("Showing control point " << cpNr);
    m_notebook->SetSelection(2);
    cpe->ShowControlPoint(cpNr);
}

void MainFrame::ShowCtrlPointEditor(unsigned int img1, unsigned int img2)
{
    if(!IsShown())
    {
        Show();
        Raise();
    };
    m_notebook->SetSelection(2);
    cpe->setLeftImage(img1);
    cpe->setRightImage(img2);
}

void MainFrame::ShowMaskEditor(size_t imgNr)
{
    if(!IsShown())
    {
        Show();
        Raise();
    };
    m_notebook->SetSelection(1);
    mask_panel->setImage(imgNr, true);
};

void MainFrame::ShowStitcherTab()
{
    ///@todo Stop using magic numbers for the tabs.
    if(m_show_opt_panel && m_show_opt_photo_panel)
    {
        m_notebook->SetSelection(5);
    }
    else
    {
        if(m_show_opt_panel || m_show_opt_photo_panel)
        {
            m_notebook->SetSelection(4);
        }
        else
        {
            m_notebook->SetSelection(3);
        };
    };
}

/** update the display */
void MainFrame::updateProgressDisplay()
{
    wxString msg;
    if (!m_message.empty())
    {
        msg = wxGetTranslation(wxString(m_message.c_str(), wxConvLocal));
        if (!m_filename.empty())
        {
            msg.Append(wxT(" "));
            msg.Append(wxString(ProgressDisplay::m_filename.c_str(), HUGIN_CONV_FILENAME));
        };
    };
    GetStatusBar()->SetStatusText(msg, 0);

#ifdef __WXMSW__
    UpdateWindow(NULL);
#endif
}

void MainFrame::enableTools(bool option)
{
    wxToolBar* theToolBar = GetToolBar();
    theToolBar->EnableTool(XRCID("action_optimize"), option);
    theToolBar->EnableTool(XRCID("ID_SHOW_PREVIEW_FRAME"), option);
    //theToolBar->EnableTool(XRCID("ID_SHOW_GL_PREVIEW_FRAME"), option);
    wxMenuBar* theMenuBar = GetMenuBar();
    theMenuBar->Enable(XRCID("action_optimize"), option);
    theMenuBar->Enable(XRCID("action_finetune_all_cp"), option);
    theMenuBar->Enable(XRCID("action_remove_cp_in_masks"), option);
    theMenuBar->Enable(XRCID("ID_SHOW_PREVIEW_FRAME"), option);
    theMenuBar->Enable(XRCID("action_stitch"), option);
    theMenuBar->Enable(XRCID("action_stitch_userdefined"), option);
    if (m_outputUserMenu != nullptr)
    {
        m_outputUserMenu->Enable(option);
    };
    if (m_assistantUserMenu != nullptr)
    {
        m_assistantUserMenu->Enable(option);
    };
    theMenuBar->Enable(XRCID("action_assistant"), option);
    theMenuBar->Enable(XRCID("action_batch_assistant"), option);
    m_menu_file_advanced->Enable(XRCID("action_import_papywizard"), option);
    //theMenuBar->Enable(XRCID("ID_SHOW_GL_PREVIEW_FRAME"), option);
}


void MainFrame::OnSize(wxSizeEvent &e)
{
#ifdef DEBUG
    wxSize sz = this->GetSize();
    wxSize csz = this->GetClientSize();
    wxSize vsz = this->GetVirtualSize();
    DEBUG_TRACE(" size:" << sz.x << "," << sz.y <<
                " client: "<< csz.x << "," << csz.y <<
                " virtual: "<< vsz.x << "," << vsz.y);
#endif

    Layout();
    e.Skip();
}

CPDetectorSetting& MainFrame::GetDefaultSetting()
{
    return images_panel->GetDefaultSetting();
};

void MainFrame::RunCPGenerator(CPDetectorSetting &setting, const HuginBase::UIntSet& img)
{
    images_panel->RunCPGenerator(setting, img);
};

void MainFrame::RunCPGenerator(const HuginBase::UIntSet& img)
{
    images_panel->RunCPGenerator(img);
};

const wxString MainFrame::GetSelectedCPGenerator()
{
    return images_panel->GetSelectedCPGenerator();
};

const wxString & MainFrame::GetXRCPath()
{
     return huginApp::Get()->GetXRCPath();
};

const wxString & MainFrame::GetDataPath()
{
    return wxGetApp().GetDataPath();
};

/// hack.. kind of a pseudo singleton...
MainFrame * MainFrame::Get()
{
    if (m_this) {
        return m_this;
    } else {
        DEBUG_FATAL("MainFrame not yet created");
        DEBUG_ASSERT(m_this);
        return 0;
    }
}

wxString MainFrame::getProjectName()
{
    return m_filename;
}

void MainFrame::OnMRUFiles(wxCommandEvent &e)
{
    size_t index = e.GetId() - wxID_FILE1;
    wxString f(m_mruFiles.GetHistoryFile(index));
    if (!f.empty())
    {
        wxFileName fn(f);
        if (fn.FileExists())
        {
            // if closing old project was canceled do nothing
            if (CloseProject(true, LOAD_NEW_PROJECT))
            {
                // remove old images from cache
                ImageCache::getInstance().flush();
                // finally load project
                LoadProjectFile(f);
            };
        }
        else
        {
            m_mruFiles.RemoveFileFromHistory(index);
            wxMessageBox(wxString::Format(_("File \"%s\" not found.\nMaybe file was renamed, moved or deleted."),f.c_str()),
                _("Error!"),wxOK | wxICON_INFORMATION );
        };
    };
}

void MainFrame::OnFullScreen(wxCommandEvent & e)
{
    ShowFullScreen(!IsFullScreen(), wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION);
#ifdef __WXGTK__
    //workaround a wxGTK bug that also the toolbar is hidden, but not requested to hide
    GetToolBar()->Show(true);
#endif
};

struct celeste::svm_model* MainFrame::GetSVMModel()
{
    if(svmModel==NULL)
    {
        // determine file name of SVM model file
        // get XRC path from application
        wxString wxstrModelFileName = huginApp::Get()->GetDataPath() + wxT(HUGIN_CELESTE_MODEL);
        // convert wxString to string
        std::string strModelFileName(wxstrModelFileName.mb_str(HUGIN_CONV_FILENAME));

        // SVM model file
        if (! wxFile::Exists(wxstrModelFileName) ) {
            wxMessageBox(wxString::Format(_("Celeste model expected in %s not found, Hugin needs to be properly installed."),wxstrModelFileName.c_str()), _("Fatal Error"));
            return NULL;
        }
        if(!celeste::loadSVMmodel(svmModel,strModelFileName))
        {
            wxMessageBox(wxString::Format(_("Could not load Celeste model file %s"),wxstrModelFileName.c_str()),_("Error"));
            svmModel=NULL;
        };
    }
    return svmModel;
};

GLPreviewFrame * MainFrame::getGLPreview()
{
    return gl_preview_frame;
}

void MainFrame::SetGuiLevel(GuiLevel newLevel)
{
    if(gl_preview_frame==NULL && newLevel==GUI_SIMPLE)
    {
        SetGuiLevel(GUI_ADVANCED);
        return;
    };
    if(m_guiLevel==GUI_EXPERT && newLevel!=GUI_EXPERT && pano.getOptimizerSwitch()==0)
    {
        bool needsUpdateOptimizerVar=false;
        HuginBase::OptimizeVector optVec = pano.getOptimizeVector();
        for(size_t i=0; i<optVec.size(); i++)
        {
            bool hasTrX=optVec[i].erase("TrX")>0;
            bool hasTrY=optVec[i].erase("TrY")>0;
            bool hasTrZ=optVec[i].erase("TrZ")>0;
            bool hasTpy=optVec[i].erase("Tpy")>0;
            bool hasTpp=optVec[i].erase("Tpp")>0;
            bool hasg=optVec[i].erase("g")>0;
            bool hast=optVec[i].erase("t")>0;
            needsUpdateOptimizerVar=needsUpdateOptimizerVar || hasTrX || hasTrY || hasTrZ || hasTpy || hasTpp || hasg || hast;
        };
        if(needsUpdateOptimizerVar)
        {
            PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::UpdateOptimizeVectorCmd(pano, optVec)
            );
        };
    };
    if(newLevel==GUI_SIMPLE && pano.getPhotometricOptimizerSwitch()==0)
    {
        bool needsUpdateOptimizerVar=false;
        HuginBase::OptimizeVector optVec = pano.getOptimizeVector();
        for(size_t i=0; i<optVec.size(); i++)
        {
            bool hasVx=optVec[i].erase("Vx")>0;
            bool hasVy=optVec[i].erase("Vy")>0;
            needsUpdateOptimizerVar=needsUpdateOptimizerVar || hasVx || hasVy;
        };
        if(needsUpdateOptimizerVar)
        {
            PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::UpdateOptimizeVectorCmd(pano, optVec)
            );
        };
    };
    m_guiLevel=newLevel;
    images_panel->SetGuiLevel(m_guiLevel);
    opt_panel->SetGuiLevel(m_guiLevel);
    opt_photo_panel->SetGuiLevel(m_guiLevel);
    pano_panel->SetGuiLevel(m_guiLevel);
    if(gl_preview_frame)
    {
        gl_preview_frame->SetGuiLevel(m_guiLevel);
    };
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
    if(m_guiLevel==GUI_SIMPLE)
    {
        if(!gl_preview_frame->IsShown())
        {
            wxCommandEvent dummy;
            OnToggleGLPreviewFrame(dummy);
        };
        wxGetApp().SetTopWindow(gl_preview_frame);
        GetMenuBar()->Remove(0);
        GetMenuBar()->Insert(0, m_menu_file_simple, _("&File"));
        if(m_filename.IsEmpty())
        {
            gl_preview_frame->SetTitle(_("Hugin - Panorama Stitcher"));
            SetTitle(_("Panorama editor"));
        }
        else
        {
            wxFileName scriptName = m_filename;
            gl_preview_frame->SetTitle(scriptName.GetName() + wxT(".") + scriptName.GetExt() + wxT(" - ") + _("Hugin - Panorama Stitcher"));
            SetTitle(scriptName.GetName() + wxT(".") + scriptName.GetExt() + wxT(" - ") + _("Panorama editor"));
        };
        Hide();
    }
    else
    {
        wxGetApp().SetTopWindow(this);
        GetMenuBar()->Remove(0);
        GetMenuBar()->Insert(0, m_menu_file_advanced, _("&File"));
        if(m_filename.IsEmpty())
        {
            SetTitle(_("Hugin - Panorama Stitcher"));
        }
        else
        {
            wxFileName scriptName = m_filename;
            SetTitle(scriptName.GetName() + wxT(".") + scriptName.GetExt() + wxT(" - ") + _("Hugin - Panorama Stitcher"));
        };
        if(!IsShown())
        {
            Show();
        };
    };
};

void MainFrame::OnSetGuiSimple(wxCommandEvent & e)
{
    GuiLevel reqGuiLevel=GetMinimumGuiLevel(pano);
    if(reqGuiLevel<=GUI_SIMPLE)
    {
        SetGuiLevel(GUI_SIMPLE);
    }
    else
    {
        if(reqGuiLevel==GUI_ADVANCED)
        {
            wxMessageBox(_("Can't switch to simple interface. The project is using stacks and/or vignetting center shift.\nThese features are not supported in simple interface."),
#ifdef __WXMSW__
                         wxT("Hugin"),
#else
                         wxT(""),
#endif
                         wxOK | wxICON_INFORMATION);
        }
        else
        {
            wxMessageBox(_("Can't switch to simple interface. The project is using translation or shear parameters.\nThese parameters are not supported in simple interface."),
#ifdef __WXMSW__
                         wxT("Hugin"),
#else
                         wxT(""),
#endif
                         wxOK | wxICON_INFORMATION);
        }
        SetGuiLevel(m_guiLevel);
    };
};

void MainFrame::OnSetGuiAdvanced(wxCommandEvent & e)
{
    GuiLevel reqGuiLevel=GetMinimumGuiLevel(pano);
    if(reqGuiLevel<=GUI_ADVANCED)
    {
        SetGuiLevel(GUI_ADVANCED);
    }
    else
    {
        wxMessageBox(_("Can't switch to advanced interface. The project is using translation or shear parameters.\nThese parameters are not supported in advanced interface."),
#ifdef __WXMSW__
                     wxT("Hugin"),
#else
                     wxT(""),
#endif
                     wxOK | wxICON_INFORMATION);
        SetGuiLevel(GUI_EXPERT);
    };
};

void MainFrame::OnSetGuiExpert(wxCommandEvent & e)
{
    SetGuiLevel(GUI_EXPERT);
};

void MainFrame::DisableOpenGLTools()
{
    GetMenuBar()->Enable(XRCID("ID_SHOW_GL_PREVIEW_FRAME"), false);
    GetMenuBar()->Enable(XRCID("action_gui_simple"), false);
    GetToolBar()->EnableTool(XRCID("ID_SHOW_GL_PREVIEW_FRAME"), false); 
};

void MainFrame::RunAssistant(wxWindow* mainWin, const wxString& userdefinedAssistant)
{
    //save project into temp directory
    wxString tempDir= wxConfig::Get()->Read(wxT("tempDir"),wxT(""));
    if(!tempDir.IsEmpty())
    {
        if(tempDir.Last()!=wxFileName::GetPathSeparator())
        {
            tempDir.Append(wxFileName::GetPathSeparator());
        }
    };
    wxFileName scriptFileName(wxFileName::CreateTempFileName(tempDir+wxT("ha")));
    std::ofstream script(scriptFileName.GetFullPath().mb_str(HUGIN_CONV_FILENAME));
    script.exceptions ( std::ofstream::eofbit | std::ofstream::failbit | std::ofstream::badbit );
    HuginBase::UIntSet all;
    fill_set(all, 0, pano.getNrOfImages()-1);
    pano.printPanoramaScript(script, pano.getOptimizeVector(), pano.getOptions(), all, false);
    script.close();

    // get assistant queue
    const wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    HuginQueue::CommandQueue* commands;
    if (userdefinedAssistant.IsEmpty())
    {
        commands = HuginQueue::GetAssistantCommandQueue(pano, exePath.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR), scriptFileName.GetFullPath());
    }
    else
    {
        commands = HuginQueue::GetAssistantCommandQueueUserDefined(pano, exePath.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR), scriptFileName.GetFullPath(), userdefinedAssistant);
        if (commands->empty())
        {
            wxMessageBox(_("The assistant queue is empty. This indicates an error in the user defined assistant file."),
                _("Error"), wxOK | wxICON_ERROR, mainWin);
            //delete temporary files
            wxRemoveFile(scriptFileName.GetFullPath());
            return;
        };
    };
    //execute queue
    int ret = MyExecuteCommandQueue(commands, mainWin, _("Running assistant"));

    //read back panofile
    PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::wxLoadPTProjectCmd(pano,
        (const char *)scriptFileName.GetFullPath().mb_str(HUGIN_CONV_FILENAME), 
        (const char *)scriptFileName.GetPath(wxPATH_NATIVE | wxPATH_GET_SEPARATOR).mb_str(HUGIN_CONV_FILENAME), 
        ret==0, false));

    //delete temporary files
    wxRemoveFile(scriptFileName.GetFullPath());
    //if return value is non-zero, an error occurred in the assistant
    if(ret!=0)
    {
        if (userdefinedAssistant.IsEmpty())
        {
            if (pano.getNrOfImages() == 1)
            {
                wxMessageBox(_("The assistant could not find vertical lines. Please add vertical lines in the panorama editor and optimize project manually."),
                    _("Warning"), wxOK | wxICON_INFORMATION, mainWin);
            }
            else
            {
                //check for unconnected images
                HuginGraph::ImageGraph graph(pano);
                const HuginGraph::ImageGraph::Components comps = graph.GetComponents();
                if (comps.size() > 1)
                {
                    // switch to images panel.
                    unsigned i1 = *(comps[0].rbegin());
                    unsigned i2 = *(comps[1].begin());
                    ShowCtrlPointEditor(i1, i2);
                    // display message box with 
                    wxMessageBox(wxString::Format(_("Warning %d unconnected image groups found:"), static_cast<int>(comps.size())) + Components2Str(comps) + wxT("\n")
                        + _("Please create control points between unconnected images using the Control Points tab in the panorama editor.\n\nAfter adding the points, press the \"Align\" button again"), _("Error"), wxOK, mainWin);
                    return;
                };
                wxMessageBox(_("The assistant did not complete successfully. Please check the resulting project file."),
                    _("Warning"), wxOK | wxICON_INFORMATION, mainWin);
            };
        }
        else
        {
            wxMessageBox(_("The assistant did not complete successfully. Please check the resulting project file."),
                _("Warning"), wxOK | wxICON_INFORMATION, mainWin);
        };
    };
};

void MainFrame::OnRunAssistant(wxCommandEvent & e)
{
    RunAssistant(this);
};

void MainFrame::OnRunAssistantUserdefined(wxCommandEvent & e)
{
    const auto filename = m_userAssistant.find(e.GetId());
    if (filename != m_userAssistant.end())
    {
        RunAssistant(this, filename->second);
    }
    else
    {
        wxBell();
    };
};

void MainFrame::OnSendToAssistantQueue(wxCommandEvent &e)
{
    wxCommandEvent dummy;
    OnSaveProject(dummy);
    wxString projectFile = getProjectName();
    if(wxFileName::FileExists(projectFile))
    {
#if defined __WXMAC__ && defined MAC_SELF_CONTAINED_BUNDLE
        // Original patch for OSX by Charlie Reiman dd. 18 June 2011
        // Slightly modified by HvdW. Errors in here are mine, not Charlie's. 
        FSRef appRef;
        FSRef actuallyLaunched;
        OSStatus err;
        FSRef documentArray[1]; // Don't really need an array if we only have 1 item
        LSLaunchFSRefSpec launchSpec;
        Boolean  isDir;

        err = LSFindApplicationForInfo(kLSUnknownCreator,
                                       CFSTR("net.sourceforge.hugin.PTBatcherGUI"),
                                       NULL,
                                       &appRef,
                                       NULL);
        if (err != noErr)
        {
            // error, can't find PTBatcherGUI
            wxMessageBox(wxString::Format(_("External program %s not found in the bundle, reverting to system path"), wxT("open")), _("Error"));
            // Possibly a silly attempt otherwise the previous would have worked as well, but just try it.
            wxExecute(_T("open -b net.sourceforge.hugin.PTBatcherGUI ")+hugin_utils::wxQuoteFilename(projectFile));
            return;
        }

        wxCharBuffer projectFilebuffer=projectFile.ToUTF8();
        // Point to document
        err = FSPathMakeRef((unsigned char*) projectFilebuffer.data(), &documentArray[0], &isDir);
        if (err != noErr || isDir)
        {
            // Something went wrong.
            wxMessageBox(wxString::Format(_("Project file not found"), wxT("open")), _("Error"));
            return;
        }
        launchSpec.appRef = &appRef;
        launchSpec.numDocs = sizeof(documentArray)/sizeof(documentArray[0]);
        launchSpec.itemRefs = documentArray;
        launchSpec.passThruParams = NULL;
        launchSpec.launchFlags = kLSLaunchDontAddToRecents + kLSLaunchDontSwitch;
        launchSpec.asyncRefCon = NULL;

        err = LSOpenFromRefSpec(&launchSpec, &actuallyLaunched);
        if (err != noErr && err != kLSLaunchInProgressErr)
        {  
            // Should be ok if it's in progress... I think. 
            // Launch failed.
            wxMessageBox(wxString::Format(_("Can't launch PTBatcherGui"), wxT("open")), _("Error"));
            return;
        }

        // Should verify that actuallyLaunched and appRef are the same.
        if (FSCompareFSRefs(&appRef, &actuallyLaunched) != noErr)
        {
            // error, lauched the wrong thing.
            wxMessageBox(wxString::Format(_("Launched incorrect programme"), wxT("open")), _("Error"));
            return;
        }
#else
        const wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
        wxExecute(exePath.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + wxT("PTBatcherGUI -a ")+hugin_utils::wxQuoteFilename(projectFile));
#endif
    }
};

wxString MainFrame::GetCurrentOptimizerString()
{
    return images_panel->GetCurrentOptimizerString();
};

MainFrame * MainFrame::m_this = 0;
