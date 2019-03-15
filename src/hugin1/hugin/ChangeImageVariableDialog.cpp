// -*- c-basic-offset: 4 -*-

/** @file ChangeImageVariableDialog.cpp
 *
 *  @brief implementation of dialog to edit image variables by parsing an expression
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

#include "hugin/ChangeImageVariableDialog.h"
#include "panoinc.h"
#include "hugin/huginApp.h"
#include "base_wx/platform.h"
#include "panodata/ParseExp.h"
#include <iostream>

BEGIN_EVENT_TABLE(ImageVariablesExpressionDialog, wxDialog)
    EVT_BUTTON(wxID_OK, ImageVariablesExpressionDialog::OnOk)
    EVT_BUTTON(XRCID("change_variable_load"), ImageVariablesExpressionDialog::OnLoad)
    EVT_BUTTON(XRCID("change_variable_save"), ImageVariablesExpressionDialog::OnSave)
    EVT_BUTTON(XRCID("change_variable_delete"), ImageVariablesExpressionDialog::OnDelete)
    EVT_BUTTON(XRCID("change_variable_test"), ImageVariablesExpressionDialog::OnTest)
    EVT_TEXT(XRCID("change_variable_text"), ImageVariablesExpressionDialog::OnTextChange)
END_EVENT_TABLE()

ImageVariablesExpressionDialog::ImageVariablesExpressionDialog(wxWindow *parent, HuginBase::Panorama* pano)
{
    // load our children. some children might need special
    // initialization. this will be done later.
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("image_variables_change_dialog"));

#ifdef __WXMSW__
    wxIcon myIcon(huginApp::Get()->GetXRCPath() + wxT("data/hugin.ico"),wxBITMAP_TYPE_ICO);
#else
    wxIcon myIcon(huginApp::Get()->GetXRCPath() + wxT("data/hugin.png"),wxBITMAP_TYPE_PNG);
#endif
    m_textInput = XRCCTRL(*this, "change_variable_text", wxTextCtrl);
    m_presetsList = XRCCTRL(*this, "change_variable_choice", wxChoice);
    m_textAttrInactive = wxTextAttr(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    m_textAttrDefault = wxTextAttr(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    SetIcon(myIcon);
    RestoreFramePosition(this, "ChangeImageVariablesDialog");
    SetExpression(wxConfig::Get()->Read("/ChangeImageVariablesDialog/LastExpression", wxEmptyString));
    m_pano=pano;
    const wxFileName filename(hugin_utils::GetUserAppDataDir(), "expressions.ini");
    // if no expressions.ini exits in users data dir copy example from global data dir
    if (!filename.FileExists())
    {
        const wxFileName globalFilename(hugin_utils::GetDataDir(), "expressions.ini");
        if (globalFilename.FileExists())
        {
            wxCopyFile(globalFilename.GetFullPath(), filename.GetFullPath());
        };
    };
    m_presets = new wxFileConfig(wxEmptyString, wxEmptyString, filename.GetFullPath());
    wxString name;
    long index;
    if (m_presets->GetFirstGroup(name, index))
    {
        m_presetsList->AppendString(name);
        while (m_presets->GetNextGroup(name, index))
        {
            m_presetsList->AppendString(name);
        };
    };
};

ImageVariablesExpressionDialog::~ImageVariablesExpressionDialog()
{
    StoreFramePosition(this, "ChangeImageVariablesDialog");
    delete m_presets;
};

void ImageVariablesExpressionDialog::SetExpression(const wxString& s)
{
    if (!s.IsEmpty())
    {
        m_textInput->SetValue(s);
    }
    else
    {
        m_textInput->Clear();
    };
    m_textInput->MarkDirty();
    // update colors
    wxCommandEvent dummy;
    OnTextChange(dummy);
};

std::string ImageVariablesExpressionDialog::GetExpression()
{
    return std::string(m_textInput->GetValue().mb_str(wxConvLocal));
}

void ImageVariablesExpressionDialog::OnOk(wxCommandEvent & e)
{
    wxConfigBase* config = wxConfig::Get();
    config->Write("/ChangeImageVariablesDialog/LastExpression", m_textInput->GetValue());
    StoreFramePosition(this, "ChangeImageVariablesDialog");
    config->Flush();
    EndModal(wxID_OK);
}

void ImageVariablesExpressionDialog::OnLoad(wxCommandEvent & e)
{
    if (m_presetsList->GetSelection() != wxNOT_FOUND)
    {
        wxString s = m_presets->Read(m_presetsList->GetString(m_presetsList->GetSelection()) + "/Expression", wxEmptyString);
        if (!s.IsEmpty())
        {
            SetExpression(s);
        }
        else
        {
            wxBell();
        };
    }
    else
    {
        wxBell();
    };
}

void ImageVariablesExpressionDialog::OnSave(wxCommandEvent & e)
{
    wxTextEntryDialog dlg(this, _("Input name for new preset"), _("Save preset"));
    if (dlg.ShowModal() == wxID_OK)
    {
        wxString s = dlg.GetValue();
        // replace slashes with underscore, slashes have a special meaning in wxFileConfig
        s.Replace("/", "_", true);
        s = s.Trim(true).Trim(false);
        if (s.IsEmpty())
        {
            wxBell();
            return;
        }
        bool newPreset = true;
        if (m_presets->HasGroup(s))
        {
            if (wxMessageBox(wxString::Format(_("Preset with name \"%s\" already exists.\nShould this preset be overwritten?"), s),
#ifdef __WXMSW__
                "Hugin",
#else
                wxEmptyString,
#endif
                wxYES_NO | wxYES_DEFAULT | wxICON_WARNING) == wxNO)
            {
                return;
            };
            newPreset = false;
        }
        // now save
        m_presets->Write(s + "/Expression", m_textInput->GetValue());
        m_presets->Flush();
        if (newPreset)
        {
            // add to selection list
            m_presetsList->AppendString(s);
            m_presetsList->Select(m_presetsList->GetCount() - 1);
        };
        Layout();
    };
}

void ImageVariablesExpressionDialog::OnDelete(wxCommandEvent & e)
{
    if (m_presetsList->GetSelection() != wxNOT_FOUND)
    {
        const int selection = m_presetsList->GetSelection();
        m_presets->DeleteGroup(m_presetsList->GetString(selection));
        m_presets->Flush();
        m_presetsList->Delete(selection);
        Layout();
    }
    else
    {
        wxBell();
    };
}

void ImageVariablesExpressionDialog::OnTest(wxCommandEvent & e)
{
    HuginBase::Panorama testPano = m_pano->duplicate();
    std::ostringstream status, error;
    Parser::PanoParseExpression(testPano, GetExpression(), status, error);
    wxDialog dlg;
    if (wxXmlResource::Get()->LoadDialog(&dlg, this, "log_dialog"))
    {
        RestoreFramePosition(&dlg, "LogDialog");
        if (error.str().empty())
        {
        // no error, show status
            XRCCTRL(dlg, "log_dialog_text", wxTextCtrl)->SetValue(wxString(status.str().c_str(), wxConvLocal));
            dlg.SetTitle(_("Result of expression parsing"));
        }
        else
        {
            // show errors
            XRCCTRL(dlg, "log_dialog_text", wxTextCtrl)->SetValue(wxString(error.str().c_str(), wxConvLocal));
            dlg.SetTitle(_("Errors during expression parsing"));
        };
        dlg.ShowModal();
        StoreFramePosition(&dlg, "LogDialog");
    };
}

void ImageVariablesExpressionDialog::OnTextChange(wxCommandEvent & e)
{
    if (!m_textInput->IsModified())
    {
        return;
    };
    bool newLine = true;
    size_t commentStart=-1;
    // reset style
    m_textInput->SetStyle(0, m_textInput->GetLastPosition(), m_textAttrDefault);
    for (size_t i = 0; i < m_textInput->GetLastPosition(); ++i)
    {
        const wxString s = m_textInput->GetRange(i, i + 1);
        if (s == " ")
        {
            continue;
        };
        if (newLine && s == "#" && commentStart == -1)
        {
            commentStart = i;
            continue;
        };
        newLine = false;
        if (s == "\n")
        {
            if (commentStart != -1)
            {
                m_textInput->SetStyle(commentStart, i, m_textAttrInactive);
            };
            newLine = true;
            commentStart = -1;
            continue;
        };
    };
    if (commentStart != -1)
    {
        m_textInput->SetStyle(commentStart, m_textInput->GetLastPosition(), m_textAttrInactive);
    };
}
