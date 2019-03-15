// -*- c-basic-offset: 4 -*-
/**  @file ImageVariableDialog.h
 *
 *  @brief Definition of dialog to edit image variables
 *
 *  @author T. Modes
 *
 */

/*  This is free software; you can redistribute it and/or
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

#ifndef _IMAGEVARIABLEDIALOG_H
#define _IMAGEVARIABLEDIALOG_H

#include "panoinc_WX.h"
#include "panoinc.h"
#include "GuiLevel.h"

// forward declaration
namespace wxGraphTools
{
class GraphPopupWindow;
};

/** Dialog for editing image variables */
class ImageVariableDialog : public wxDialog
{
public:
    /** Constructor, read from xrc ressource; restore last uses settings and position */
    ImageVariableDialog(wxWindow *parent, HuginBase::Panorama* pano, HuginBase::UIntSet imgs);
    /** destructor, saves position */
    ~ImageVariableDialog();
    /** sets the GuiLevel */
    void SetGuiLevel(GuiLevel newLevel);
    /** selects the tab with index i */
    void SelectTab(size_t i);

protected:
    /** Saves current state of all checkboxes when closing dialog with Ok */
    void OnOk(wxCommandEvent & e);
    /** shows the help */
    void OnHelp(wxCommandEvent & e);
    /** shows a popup with distortion graph */
    void OnShowDistortionGraph(wxCommandEvent & e);
    /** shows a popup with vignetting graph */
    void OnShowVignettingGraph(wxCommandEvent & e);
    /** shows a popup with response graph */
    void OnShowResponseGraph(wxCommandEvent & e);

private:
    HuginBase::Panorama* m_pano;
    HuginBase::UIntSet m_images;
    static const char *m_varNames[];
    wxGraphTools::GraphPopupWindow* m_popup;
    /** copy the variables from Panorama to dialog */
    void InitValues();
    /** applies the changed variables to the Panorama class, using CommandHistory */
    bool ApplyNewVariables();

    DECLARE_EVENT_TABLE()
};

#endif //_IMAGEVARIABLEDIALOG_H
