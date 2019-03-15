// -*- c-basic-offset: 4 -*-
/** @file PreferencesDialog.h
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

#ifndef _PREFERENCESDIALOG_H
#define _PREFERENCESDIALOG_H

#include "panoinc.h"
#include "panoinc_WX.h"

#include "icpfind/CPDetectorConfig.h"

/** hugin preferences dialog
 *
 *  A simple preferences dialog, used
 *  to inspect and set the various prefs stored
 *  in the wxConfig object
 */
class PreferencesDialog : public wxDialog
{
public:

    /** ctor.
     */
    PreferencesDialog(wxWindow *parent);

    /** dtor.
     */
    virtual ~PreferencesDialog();

    /** Config to Window
     *  @param panel to update (index starts with 1), use 0 to update all panels
     */
    void UpdateDisplayData(int panel);

    /** Window to Config */
    void UpdateConfigData();

protected:
    void OnOk(wxCommandEvent & e);
    void OnHelp(wxCommandEvent & e);
    void OnCancel(wxCommandEvent & e);
    void OnRotationCheckBox(wxCommandEvent & e);
    void OnEnblendExe(wxCommandEvent & e);
    void OnEnfuseExe(wxCommandEvent & e);
    void OnExifTool(wxCommandEvent & e);
    void OnExifArgfileChoose(wxCommandEvent & e);
    void OnExifArgfileEdit(wxCommandEvent & e);
    void OnExifArgfile2Choose(wxCommandEvent & e);
    void OnExifArgfile2Edit(wxCommandEvent & e);
    void OnRestoreDefaults(wxCommandEvent & e);
    void OnCustomEnblend(wxCommandEvent & e);
    void OnCustomEnfuse(wxCommandEvent & e);
    void OnCPDetectorAdd(wxCommandEvent & e);
    void OnCPDetectorEdit(wxCommandEvent & e);
    void OnCPDetectorDelete(wxCommandEvent & e);
    void OnCPDetectorMoveUp(wxCommandEvent & e);
    void OnCPDetectorMoveDown(wxCommandEvent & e);
    void OnCPDetectorDefault(wxCommandEvent & e);
    void OnCPDetectorListDblClick(wxCommandEvent & e);
    /** event handler for loading cp detector settings */
    void OnCPDetectorLoad(wxCommandEvent & e);
    /** event handler for saving cp detector settings */
    void OnCPDetectorSave(wxCommandEvent & e);
    /** event handler if default file format was changed */
    void OnFileFormatChanged(wxCommandEvent & e);
    /** event handler if processor was changed */
    void OnProcessorChanged(wxCommandEvent & e);
    /** event handler if blender was changed */
    void OnBlenderChanged(wxCommandEvent & e);
    /** event handler to update preview for project filename */
    void OnUpdateProjectFilename(wxCommandEvent & e);
    /** event handler to update preview for project filename */
    void OnUpdateOutputFilename(wxCommandEvent & e);

    void EnableRotationCtrls(bool enable);

private:
    void UpdateFileFormatControls();
    void UpdateProcessorControls();
    void UpdateBlenderControls();
    wxListBox* m_CPDetectorList;
    CPDetectorConfig cpdetector_config_edit;

    DECLARE_EVENT_TABLE()
};



#endif // _PREFERENCESDIALOG_H
