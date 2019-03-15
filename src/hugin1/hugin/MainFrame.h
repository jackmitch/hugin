// -*- c-basic-offset: 4 -*-
/** @file MainFrame.h
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id$
 *
 *  This is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _MAINFRAME_H
#define _MAINFRAME_H


#include <vector>
#include <set>
#include <map>

#include "panodata/Panorama.h"

#include "wx/docview.h"
#include "wx/help.h"
#if !wxUSE_HELP
#error wxWidgets needs to be compiled with help support (wxUSE_HELP not set)
#endif
#if defined __WXMSW__ && !(wxCHECK_VERSION(3,1,1))
#include "base_wx/wxPlatform.h"
#define wxHelpController HuginCHMHelpController
#endif

#include <appbase/ProgressDisplay.h>

#include "hugin/OptimizePanel.h"
#include "hugin/PreferencesDialog.h"

// Celeste header
#include "Celeste.h"
#include "GuiLevel.h"

// forward declarations, to save the #include statements
class CPEditorPanel;
class ImgPreview;
class ImagesPanel;
class MaskEditorPanel;
class OptimizePhotometricPanel;
class PanoPanel;
class PreviewFrame;
class GLPreviewFrame;
class CPListFrame;

/** simple class that forward the drop to the mainframe */
class PanoDropTarget : public wxFileDropTarget
{
public:
    PanoDropTarget(HuginBase::Panorama & p, bool imageOnly = false)
        : pano(p)
    { m_imageOnly = imageOnly; }

    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames);

private:
    HuginBase::Panorama & pano;
    bool m_imageOnly;
};

/** The main window frame.
 *
 *  It contains the menu & statusbar and a big notebook with
 *  the different tabs. It also holds the Panorama model.
 *
 *  it therefor also hold operations that determine the lifecycle
 *  of the panorama object (new, open, save, quit).
 */
class MainFrame : public wxFrame, public HuginBase::PanoramaObserver, public AppBase::ProgressDisplay
{
public:

    /** ctor.
     */
    MainFrame(wxWindow* parent, HuginBase::Panorama & pano);

    /** dtor.
     */
    virtual ~MainFrame();

    /** Enable or disable undo and redo.
     *  They should be enabled only when there is a command to act upon.
     */
    virtual void panoramaChanged(HuginBase::Panorama &pano);
    void panoramaImagesChanged(HuginBase::Panorama &pano, const HuginBase::UIntSet & imgNr);
    /** returns panorama object */
    const HuginBase::Panorama & getPanorama() { return pano; };

    // called when a control point in CPListFrame is selected
    void ShowCtrlPoint(unsigned int cpNr);

    /// get the path to the xrc directory
    const wxString & GetXRCPath();
    /** get the path to data directory */
    const wxString & GetDataPath();
    /** sets the status of the "optimize only active images" menu item */
    void SetOptimizeOnlyActiveImages(const bool onlyActive);
    const bool GetOptimizeOnlyActiveImages() const;
    /** sets the status of the "ignore line cp" menu item */
    void SetOptimizeIgnoreLineCp(const bool ignoreLineCP);
    const bool GetOptimizeIgnoreLineCp() const;

    /// hack.. kind of a pseudo singleton...
    static MainFrame * Get();

    // Used to display tip of the day after main interface is initialised and visible
	void OnTipOfDay(wxCommandEvent & e);

    // load a project
    void LoadProjectFile(const wxString & filename);
    void RunAssistant(wxWindow* mainWin, const wxString& userdefinedAssistant=wxEmptyString);

    /** disables all OpenGL related menu items and toobar buttons */
    void DisableOpenGLTools();
#ifdef __WXMAC__
    void MacOnOpenFile(const wxString & filename);
#endif
    enum CloseReason
    {
        CLOSE_PROGRAM,
        LOAD_NEW_PROJECT,
        NEW_PROJECT
    };
    bool CloseProject(bool cancelable, CloseReason reason);

    // TODO: create a nice generic optimisation & stitching function
    // instead of these gateway functions to the optimizer and pano panels.
    void OnOptimize(wxCommandEvent & e);
    void OnOnlyActiveImages(wxCommandEvent &e);
    void OnIgnoreLineCp(wxCommandEvent &e);
    void OnPhotometricOptimize(wxCommandEvent & e);
    void OnDoStitch(wxCommandEvent & e);
    void OnUserDefinedStitch(wxCommandEvent & e);
    void OnUserDefinedStitchSaved(wxCommandEvent & e);
    void OnTogglePreviewFrame(wxCommandEvent & e);
    void OnToggleGLPreviewFrame(wxCommandEvent & e);
    void OnAddImages(wxCommandEvent & e);
    void OnSaveProject(wxCommandEvent & e);
    void OnSetGuiSimple(wxCommandEvent & e);
    void OnSetGuiAdvanced(wxCommandEvent & e);
    void OnSetGuiExpert(wxCommandEvent & e);

    /** call help browser with given file */
    void DisplayHelp(wxString section = wxEmptyString);

    /** opens the control points tab with the both images selected */
    void ShowCtrlPointEditor(unsigned int img1, unsigned int img2);
    /** opens the mask/crop editor with the given image selected */
    void ShowMaskEditor(size_t imgNr);
    /** opens the stitcher tab */
    void ShowStitcherTab();

    void OnCPListFrameClosed();
    /** returns default cp detector setting */
    CPDetectorSetting& GetDefaultSetting();
    /** run the cp generator with the given setting on selected images */
    void RunCPGenerator(CPDetectorSetting &setting, const HuginBase::UIntSet& img);
    /** runs the currently selected cp generator on given images */
    void RunCPGenerator(const HuginBase::UIntSet& img);
    /** return the currently selected cp generator description */
    const wxString GetSelectedCPGenerator();

    wxString getProjectName();

    struct celeste::svm_model* GetSVMModel();
    
    GLPreviewFrame * getGLPreview();
    wxHelpController& GetHelpController() { return m_HelpController; }
    void SetGuiLevel(GuiLevel newLevel);
    const GuiLevel GetGuiLevel() const { return m_guiLevel; };

    wxFileHistory* GetFileHistory() { return &m_mruFiles; };

    /** returns the string which describes the current selected optimizer setting */
    wxString GetCurrentOptimizerString();

protected:
    // called when a progress message should be displayed
    /** receive notification about progress. Should not be called directly.
     *
     *  @param msg message text
     *  @param progress optional progress indicator (0-100)
     */
    void updateProgressDisplay();

private:

    // event handlers
    void OnExit(wxCloseEvent & e);
    void OnUserQuit(wxCommandEvent & e);
    void OnAbout(wxCommandEvent & e);
    void OnHelp(wxCommandEvent & e);
    void OnKeyboardHelp(wxCommandEvent & e);
    void OnFAQ(wxCommandEvent & e);
    void OnShowPrefs(wxCommandEvent &e);
#ifdef HUGIN_HSI
    void OnPythonScript(wxCommandEvent & e);
    void OnPlugin(wxCommandEvent& e);
#endif
    void OnUndo(wxCommandEvent & e);
    void OnRedo(wxCommandEvent & e);
    void OnSaveProjectAs(wxCommandEvent & e);
    void OnSavePTStitcherAs(wxCommandEvent & e);
    void OnLoadProject(wxCommandEvent & e);
    void OnNewProject(wxCommandEvent & e);
    void OnAddTimeImages(wxCommandEvent & e);
    void OnRunAssistant(wxCommandEvent & e);
    void OnRunAssistantUserdefined(wxCommandEvent & e);
    void OnFineTuneAll(wxCommandEvent & e);
    void OnRemoveCPinMasks(wxCommandEvent & e);
    void OnMergeProject(wxCommandEvent & e);
    void OnReadPapywizard(wxCommandEvent & e);
    void OnApplyTemplate(wxCommandEvent & e);
    void OnSendToBatch(wxCommandEvent & e);
    void OnSendToAssistantQueue(wxCommandEvent & e);
    void OnOpenPTBatcher(wxCommandEvent & e);
//    void OnToggleOptimizeFrame(wxCommandEvent & e);
    void OnShowCPFrame(wxCommandEvent & e);
    /** event handler for recently used files */
    void OnMRUFiles(wxCommandEvent &e);
    /** event handler for full screen */
    void OnFullScreen(wxCommandEvent &e);
    void UpdatePanels(wxCommandEvent & e);
    void OnSize(wxSizeEvent &e);
    void enableTools(bool option);
    /** adds the given files to the projects, with checking for invalid filenames */
    void AddImages(wxArrayString& filenameArray);

    void OnShowPanel(wxCommandEvent &e);
    /** event handler called when loading of image file failed */
    void OnLoadingFailed(wxCommandEvent& e);

    wxFileHistory m_mruFiles;
    wxNotebook * m_notebook;
    // tab panels
    ImagesPanel* images_panel;
    MaskEditorPanel* mask_panel;
    CPEditorPanel * cpe;
    OptimizePanel * opt_panel;
    bool m_show_opt_panel;
    OptimizePhotometricPanel * opt_photo_panel;
    bool m_show_opt_photo_panel;
    PanoPanel * pano_panel;
    struct celeste::svm_model* svmModel;

    // flying windows
    PreviewFrame * preview_frame;
    GLPreviewFrame * gl_preview_frame;
    CPListFrame * cp_frame;
    GuiLevel m_guiLevel;

    // Image Preview
    ImgPreview *canvas;

    // Preferences
    //PreferencesDialog * pref_dlg;

    // the model
    HuginBase::Panorama & pano;

    // filename of the current project
    wxString m_filename;

    // self
    static MainFrame* m_this;
    // file menu
    wxMenu* m_menu_file_simple;
    wxMenu* m_menu_file_advanced;
    wxMenuItem* m_outputUserMenu = nullptr;
    wxMenuItem* m_assistantUserMenu = nullptr;
    // sticky setting, to prevent reading to often
    bool m_optOnlyActiveImages;
    bool m_optIgnoreLineCp;
    // help controller
    wxHelpController m_HelpController;

#ifdef HUGIN_HSI
    // list associating the wxID in the menu with a python script
    std::map<int, wxFileName> m_plugins;
#endif
    // list for user-defined output sequences
    std::map<int, wxString> m_userOutput;
    std::map<int, wxString> m_userAssistant;

    DECLARE_EVENT_TABLE()
};

// event used to signal invalid or missing image files
#if defined _WIN32 && defined Hugin_shared
DECLARE_LOCAL_EVENT_TYPE(EVT_LOADING_FAILED, -1)
#else
DECLARE_EVENT_TYPE(EVT_LOADING_FAILED, -1)
#endif

#endif // _MAINFRAME_H
