// -*- c-basic-offset: 4 -*-
/**  @file ChangeImageVariableDialog.h
 *
 *  @brief Definition of dialog to edit image variables by parsing an expression
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

#ifndef _PARSEIMAGEVARIABLEDIALOG_H
#define _PARSEIMAGEVARIABLEDIALOG_H

#include "panoinc_WX.h"
#include "panoinc.h"

/** Dialog for editing expression to change image variables */
class ImageVariablesExpressionDialog : public wxDialog
{
public:
    /** Constructor, read from xrc ressource; restore last uses settings and position */
    ImageVariablesExpressionDialog(wxWindow *parent, HuginBase::Panorama* pano);
    /** destructor, saves position */
    ~ImageVariablesExpressionDialog();
    std::string GetExpression();

protected:
    /** Saves current expression when closing dialog with Ok */
    void OnOk(wxCommandEvent & e);
    /** loads the selected preset into control */
    void OnLoad(wxCommandEvent & e);
    /** saves the current expression as preset */
    void OnSave(wxCommandEvent & e);
    /** deletes the seletected preset */
    void OnDelete(wxCommandEvent & e);
    /** tests the current expression */
    void OnTest(wxCommandEvent & e);
    /** text change event, used to format text */
    void OnTextChange(wxCommandEvent & e);
private:
    void SetExpression(const wxString& s);
    HuginBase::Panorama* m_pano;
    wxChoice* m_presetsList;
    wxTextCtrl* m_textInput;
    wxFileConfig* m_presets;
    wxTextAttr m_textAttrInactive, m_textAttrDefault;

    DECLARE_EVENT_TABLE()
};

#endif //_PARSEIMAGEVARIABLEDIALOG_H
