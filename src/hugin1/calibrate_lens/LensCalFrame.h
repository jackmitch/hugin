// -*- c-basic-offset: 4 -*-

/** @file LensCalFrame.h
 *
 *  @brief declaration of LensCal main frame class
 *
 *  @author T. Modes
 *
 */

/* 
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

#ifndef LENSCALFRAME_H
#define LENSCALFRAME_H

#include <vector>
#include <set>

#include "panoinc_WX.h"
#include "panodata/Panorama.h"
#include "lines/FindLines.h"
#include "LensCalTypes.h"
#include "LensCalImageCtrl.h"

/** simple class that forward the drop to the mainframe */
class FileDropTarget : public wxFileDropTarget
{
public:
    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames);
};

// forward declaration, to prevent include of class
namespace wxGraphTools
{
class GraphPopupWindow;
};

/** The main window frame.
 *
 */
class LensCalFrame:public wxFrame, public AppBase::ProgressDisplay
{
public:

    /** constructor  */
    explicit LensCalFrame(wxWindow* parent);
    /** destructor */
    virtual ~LensCalFrame();

    /** get the path to the xrc directory */
    const wxString & GetXRCPath();
    void AddImages(wxArrayString files);
    void UpdateListString(unsigned int index);
    /** updates the list box with current values */
    void UpdateList(bool restoreSelection);

protected:
    /** called when a progress message should be displayed */
    void updateProgressDisplay();

private:
    // event handlers
    void OnExit(wxCommandEvent &e);
    void OnAddImage(wxCommandEvent &e);
    void OnRemoveImage(wxCommandEvent &e);
    void OnFindLines(wxCommandEvent &e);
    void OnOptimize(wxCommandEvent &e);
    /** show distortion graph */
    void OnShowDistortionGraph(wxCommandEvent &e);
    void SaveLensToIni();
    void OnSaveLens(wxCommandEvent &e);
    void OnSaveProject(wxCommandEvent &e);
    void OnImageSelected(wxCommandEvent &e);
    void OnSelectPreviewContent(wxCommandEvent &e);
    void OnReset(wxCommandEvent &e);
    void OnShowLines(wxCommandEvent &e);
    void OnRefresh(wxCommandEvent &e);
    /** reads all input values into internal values */
    bool ReadInputs(bool readFocalLength,bool readOptions,bool readLensParameter);
    /** do the optimization */
    void Optimize();
    /** update progress display */
    bool displayProgress();
    /** return panorama object with all images */
    HuginBase::Panorama GetPanorama();
    /** enable all buttons and menu items depending on number of active images*/
    void EnableButtons();
    /** set all parameter input wxTextField to internal values */
    void ParametersToDisplay();

    //link to some controls
    wxChoice* m_choice_projection;
    wxListBox* m_images_list;
    LensCalImageCtrl* m_preview;
    /** list of all detected lines */
    std::vector<ImageLineList*> m_images;
    //internal values of inputs
    HuginBase::SrcPanoImage::Projection m_projection;
    double m_focallength;
    double m_cropfactor;
    double m_edge_scale;
    double m_edge_threshold;
    unsigned int m_resize_dimension;
    double m_minlinelength;
    double m_a;
    double m_b;
    double m_c;
    double m_d;
    double m_e;

    // progress reporter
    double m_progressMax;
    double m_progress;
    wxString m_progressMsg;
    // popup window
    wxGraphTools::GraphPopupWindow* m_popup = nullptr;

    DECLARE_EVENT_TABLE()
};


#endif // LENSCALFRAME_H
