// -*- c-basic-offset: 4 -*-

/** @file ImageVariableDialog.cpp
 *
 *  @brief implementation of dialog to edit image variables
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

#include "hugin/ImageVariableDialog.h"
#include "base_wx/GraphTools.h"
#include "base_wx/wxPlatform.h"
#include "panoinc.h"
#include "photometric/ResponseTransform.h"
#include <map>
#include <string>

#include "hugin/huginApp.h"
#include "base_wx/CommandHistory.h"
#include "base_wx/PanoCommand.h"

BEGIN_EVENT_TABLE(ImageVariableDialog,wxDialog)
    EVT_BUTTON(wxID_OK, ImageVariableDialog::OnOk)
    EVT_BUTTON(wxID_HELP, ImageVariableDialog::OnHelp)
    EVT_BUTTON(XRCID("image_show_distortion_graph"), ImageVariableDialog::OnShowDistortionGraph)
    EVT_BUTTON(XRCID("image_show_vignetting_graph"), ImageVariableDialog::OnShowVignettingGraph)
    EVT_BUTTON(XRCID("image_show_response_graph"), ImageVariableDialog::OnShowResponseGraph)
END_EVENT_TABLE()

ImageVariableDialog::ImageVariableDialog(wxWindow *parent, HuginBase::Panorama* pano, HuginBase::UIntSet imgs)
{
    // load our children. some children might need special
    // initialization. this will be done later.
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("image_variables_dialog"));

#ifdef __WXMSW__
    wxIcon myIcon(huginApp::Get()->GetXRCPath() + wxT("data/hugin.ico"),wxBITMAP_TYPE_ICO);
#else
    wxIcon myIcon(huginApp::Get()->GetXRCPath() + wxT("data/hugin.png"),wxBITMAP_TYPE_PNG);
#endif
    SetIcon(myIcon);
    wxConfigBase * cfg = wxConfigBase::Get();
    //position
    int x = cfg->Read(wxT("/ImageVariablesDialog/positionX"),-1l);
    int y = cfg->Read(wxT("/ImageVariablesDialog/positionY"),-1l);
    if ( y >= 0 && x >= 0) 
    {
        this->Move(x, y);
    } 
    else 
    {
        this->Move(0, 44);
    };
    m_pano=pano;
    m_images=imgs;
    m_popup=NULL;
    InitValues();
};

ImageVariableDialog::~ImageVariableDialog()
{
    wxConfigBase * cfg = wxConfigBase::Get();
    wxPoint ps = this->GetPosition();
    cfg->Write(wxT("/ImageVariablesDialog/positionX"), ps.x);
    cfg->Write(wxT("/ImageVariablesDialog/positionY"), ps.y);
    cfg->Flush();
};

void ImageVariableDialog::SelectTab(size_t i)
{
    XRCCTRL(*this, "image_variable_notebook", wxNotebook)->SetSelection(i);
};

wxTextCtrl* GetImageVariableControl(const wxWindow* parent, const char* varname)
{
    return wxStaticCast(
                parent->FindWindow(
                    wxXmlResource::GetXRCID(
                        wxString(wxT("image_variable_")).append(wxString(varname, wxConvLocal)).c_str()
                    )
                ), wxTextCtrl
           );
};

void ImageVariableDialog::InitValues()
{
    if(m_images.size()==0)
    {
        return;
    };
    HuginBase::BaseSrcPanoImage::ResponseType responseType = m_pano->getImage(*m_images.begin()).getResponseType();
    bool identical=true;

    for (HuginBase::UIntSet::const_iterator it = m_images.begin(); it != m_images.end() && identical; ++it)
    {
        identical=(responseType==m_pano->getImage(*it).getResponseType());
    };
    if(identical)
    {
        XRCCTRL(*this, "image_variable_responseType", wxChoice)->SetSelection(responseType);
    };

    int degDigits = wxConfigBase::Get()->Read(wxT("/General/DegreeFractionalDigitsEdit"),3);
    int pixelDigits = wxConfigBase::Get()->Read(wxT("/General/PixelFractionalDigitsEdit"),2);
    int distDigitsEdit = wxConfigBase::Get()->Read(wxT("/General/DistortionFractionalDigitsEdit"),5);
    
    HuginBase::VariableMapVector imgVarVector = m_pano->getVariables();

    for (const char** varname = m_varNames; *varname != 0; ++varname)
    {
        // update parameters
        int ndigits = distDigitsEdit;
        if (strcmp(*varname, "y") == 0 || strcmp(*varname, "p") == 0 ||
            strcmp(*varname, "r") == 0 || strcmp(*varname, "TrX") == 0 ||
            strcmp(*varname, "TrY") == 0 || strcmp(*varname, "TrZ") == 0 )
        {
            ndigits=degDigits;
        };
        if (strcmp(*varname, "v") == 0 || strcmp(*varname, "d") == 0 ||
            strcmp(*varname, "e") == 0 )
        {
            ndigits = pixelDigits;
        }
        double val=const_map_get(imgVarVector[*m_images.begin()],*varname).getValue();
        bool identical=true;
        for(HuginBase::UIntSet::const_iterator it=m_images.begin();it!=m_images.end() && identical;++it)
        {
            identical=(val==const_map_get(imgVarVector[*it],*varname).getValue());
        };
        if(identical)
        {
            GetImageVariableControl(this, *varname)->SetValue(hugin_utils::doubleTowxString(val,ndigits));
        };
    };
};

void ImageVariableDialog::SetGuiLevel(GuiLevel newLevel)
{
    // translation
    XRCCTRL(*this, "image_variable_text_translation", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_text_translation_x", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_text_translation_y", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_text_translation_z", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_text_translation_Tpy", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_text_translation_Tpp", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_TrX", wxTextCtrl)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_TrY", wxTextCtrl)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_TrZ", wxTextCtrl)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_Tpy", wxTextCtrl)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_Tpp", wxTextCtrl)->Show(newLevel==GUI_EXPERT);
    // shear
    XRCCTRL(*this, "image_variable_text_shear", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_text_shear_g", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_text_shear_t", wxStaticText)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_g", wxTextCtrl)->Show(newLevel==GUI_EXPERT);
    XRCCTRL(*this, "image_variable_t", wxTextCtrl)->Show(newLevel==GUI_EXPERT);
};

bool ImageVariableDialog::ApplyNewVariables()
{
    std::vector<PanoCommand::PanoCommand*> commands;
    HuginBase::VariableMap varMap;
    for (const char** varname = m_varNames; *varname != 0; ++varname)
    {
        wxString s=GetImageVariableControl(this, *varname)->GetValue();
        if(!s.empty())
        {
            double val;
            if(hugin_utils::str2double(s,val))
            {
                if(strcmp(*varname, "v")==0)
                {
                    switch(m_pano->getImage(*m_images.begin()).getProjection())
                    {
                        case HuginBase::SrcPanoImage::RECTILINEAR:
                            if(val>179)
                            {
                                val=179;
                            };
                            break;
                        case HuginBase::SrcPanoImage::FISHEYE_ORTHOGRAPHIC:
                            if(val>190)
                            {
                                if(wxMessageBox(
                                    wxString::Format(_("You have given a field of view of %.2f degrees.\n But the orthographic projection is limited to a field of view of 180 degress.\nDo you want still use that high value?"), val),
#ifdef __WXMSW__
                                    _("Hugin"),
#else
                                    wxT(""),
#endif
                                    wxICON_EXCLAMATION | wxYES_NO)==wxNO)
                                {
                                    return false;
                                };
                            };
                            break;
                        default:
                            break;
                    };
                };
                varMap.insert(std::make_pair(std::string(*varname), HuginBase::Variable(std::string(*varname), val)));
            }
            else
            {
                wxLogError(_("Value must be numeric."));
                return false;
            };
        };
    };
    int sel=XRCCTRL(*this, "image_variable_responseType", wxChoice)->GetSelection();
    if(sel!=wxNOT_FOUND)
    {
        std::vector<HuginBase::SrcPanoImage> SrcImgs;
        for (HuginBase::UIntSet::const_iterator it=m_images.begin(); it!=m_images.end(); ++it)
        {
            HuginBase::SrcPanoImage img=m_pano->getSrcImage(*it);
            img.setResponseType((HuginBase::SrcPanoImage::ResponseType)sel);
            SrcImgs.push_back(img);
        }
        commands.push_back(new PanoCommand::UpdateSrcImagesCmd( *m_pano, m_images, SrcImgs ));
    }
    if(varMap.size()>0)
    {
        for(HuginBase::UIntSet::const_iterator it=m_images.begin();it!=m_images.end();++it)
        {
            commands.push_back(
                new PanoCommand::UpdateImageVariablesCmd(*m_pano,*it,varMap)
                );
        };
    };
    if(commands.size()>0)
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(new PanoCommand::CombinedPanoCommand(*m_pano, commands));
        return true;
    }
    else
    {
        return false;
    };
};

void ImageVariableDialog::OnOk(wxCommandEvent & e)
{
    if(ApplyNewVariables())
    {
        e.Skip();
    };
};

void ImageVariableDialog::OnHelp(wxCommandEvent & e)
{
    // open help on appropriate page
    switch(XRCCTRL(*this, "image_variable_notebook", wxNotebook)->GetSelection())
    {
        //lens parameters
        case 1:
            MainFrame::Get()->DisplayHelp(wxT("Lens_correction_model.html"));
            break;
        case 2:
            MainFrame::Get()->DisplayHelp(wxT("Vignetting.html"));
            break;
        case 3:
            MainFrame::Get()->DisplayHelp(wxT("Camera_response_curve.html"));
            break;
        default:
            MainFrame::Get()->DisplayHelp(wxT("Image_positioning_model.html"));
            break;
    };
};

const char *ImageVariableDialog::m_varNames[] = { "y", "p", "r", "TrX", "TrY", "TrZ", "Tpy", "Tpp", 
                                  "v", "a", "b", "c", "d", "e", "g", "t",
                                  "Eev", "Er", "Eb", 
                                  "Vb", "Vc", "Vd", "Vx", "Vy",
                                  "Ra", "Rb", "Rc", "Rd", "Re", 0};

void ImageVariableDialog::OnShowDistortionGraph(wxCommandEvent & e)
{
    wxString stringa=GetImageVariableControl(this, "a")->GetValue();
    wxString stringb=GetImageVariableControl(this, "b")->GetValue();
    wxString stringc=GetImageVariableControl(this, "c")->GetValue();
    if(stringa.empty() || stringb.empty() || stringc.empty())
    {
        wxBell();
        return;
    };
    std::vector<double> radialDist(4 ,0);
    if(!hugin_utils::str2double(stringa, radialDist[0]) || !hugin_utils::str2double(stringb, radialDist[1]) || !hugin_utils::str2double(stringc, radialDist[2]))
    {
        wxBell();
        return;
    };
    radialDist[3] = 1 - radialDist[0] - radialDist[1] - radialDist[2];

    //create transformation
    HuginBase::SrcPanoImage srcImage;
    srcImage.setSize(m_pano->getImage(*(m_images.begin())).getSize());
    // set projection to rectilinear, just in case, it should be the default value
    srcImage.setProjection(HuginBase::SrcPanoImage::RECTILINEAR);
    srcImage.setRadialDistortion(radialDist);

    delete m_popup;
    //show popup
    m_popup = new wxGraphTools::GraphPopupWindow(this, wxGraphTools::GetDistortionGraph(srcImage));
    wxWindow *button = (wxWindow*) e.GetEventObject();
    wxPoint pos=button->ClientToScreen(wxPoint(0,0));
    m_popup->Position(pos, button->GetSize());
    m_popup->Popup();
};

void ImageVariableDialog::OnShowVignettingGraph(wxCommandEvent & e)
{
    wxString stringVb=GetImageVariableControl(this, "Vb")->GetValue();
    wxString stringVc=GetImageVariableControl(this, "Vc")->GetValue();
    wxString stringVd=GetImageVariableControl(this, "Vd")->GetValue();
    if(stringVb.empty() || stringVc.empty() || stringVd.empty())
    {
        wxBell();
        return;
    };
    std::vector<double> vigCorr(4,0);
    vigCorr[0]=1.0;
    if(!hugin_utils::str2double(stringVb, vigCorr[1]) || !hugin_utils::str2double(stringVc, vigCorr[2]) || !hugin_utils::str2double(stringVd,  vigCorr[3]))
    {
        wxBell();
        return;
    };
    delete m_popup;
    wxGraphTools::Graph graph(300, 200, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    graph.SetChartArea(10, 10, 290, 190);
    graph.SetChartDisplay(0, 0.8, 1, 1);
    graph.DrawGrid(6, 6);

    //create ResponseTransform with vignetting only
    HuginBase::SrcPanoImage srcImage;
    srcImage.setRadialVigCorrCoeff(vigCorr);
#define NRPOINTS 100
    srcImage.setSize(vigra::Size2D(2*NRPOINTS, 2*NRPOINTS));
    HuginBase::Photometric::ResponseTransform<double> transform(srcImage);
    transform.enforceMonotonicity();

    //now calc vignetting curve
    std::vector<hugin_utils::FDiff2D> points;
#define NRPOINTS 100
    for(size_t i=0; i<=NRPOINTS; i++)
    {
        points.push_back(hugin_utils::FDiff2D(i/double(NRPOINTS),transform(1.0, hugin_utils::FDiff2D(NRPOINTS-i, NRPOINTS-i))));
    };
    graph.DrawLine(points, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), 2);

    //display popup
    m_popup = new wxGraphTools::GraphPopupWindow(this, graph.GetGraph());
    wxWindow *button = (wxWindow*) e.GetEventObject();
    wxPoint pos=button->ClientToScreen(wxPoint(0,0));
    m_popup->Position(pos, button->GetSize());
    m_popup->Popup();
};

void ImageVariableDialog::OnShowResponseGraph(wxCommandEvent & e)
{
    HuginBase::SrcPanoImage::ResponseType responseType=(HuginBase::SrcPanoImage::ResponseType)XRCCTRL(*this, "image_variable_responseType", wxChoice)->GetSelection();
    wxString stringRa=GetImageVariableControl(this, "Ra")->GetValue();
    wxString stringRb=GetImageVariableControl(this, "Rb")->GetValue();
    wxString stringRc=GetImageVariableControl(this, "Rc")->GetValue();
    wxString stringRd=GetImageVariableControl(this, "Rd")->GetValue();
    wxString stringRe=GetImageVariableControl(this, "Re")->GetValue();
    if(stringRa.empty() || stringRb.empty() || stringRc.empty() || stringRd.empty() || stringRe.empty())
    {
        wxBell();
        return;
    };
    double Ra, Rb, Rc, Rd, Re;
    if(!hugin_utils::str2double(stringRa, Ra) || !hugin_utils::str2double(stringRb, Rb) || !hugin_utils::str2double(stringRc, Rc) ||
        !hugin_utils::str2double(stringRd, Rd) || !hugin_utils::str2double(stringRe, Re))
    {
        wxBell();
        return;
    };
    delete m_popup;
    wxGraphTools::Graph graph(300, 200, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    graph.SetChartArea(10, 10, 290, 190);
    graph.SetChartDisplay(0, 0, 1, 1);
    graph.DrawGrid(6, 6);
    switch(responseType)
    {
        case HuginBase::SrcPanoImage::RESPONSE_EMOR:
            {
                //draw standard EMOR curve
                std::vector<float> emor(5, 0.0);
                std::vector<double> outLutStd;
                vigra_ext::EMoR::createEMoRLUT(emor, outLutStd);
                vigra_ext::enforceMonotonicity(outLutStd);
                graph.SetChartDisplay(0, 0, outLutStd.size()-1.0, 1.0);
                std::vector<hugin_utils::FDiff2D> points;
                for(size_t i=0; i<outLutStd.size(); i++)
                {
                    points.push_back(hugin_utils::FDiff2D(i, outLutStd[i]));
                };
                graph.DrawLine(points, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), 1);
                outLutStd.clear();
                points.clear();
                // now draw our curve
                emor[0]=Ra;
                emor[1]=Rb;
                emor[2]=Rc;
                emor[3]=Rd;
                emor[4]=Re;
                std::vector<double> outLut;
                vigra_ext::EMoR::createEMoRLUT(emor, outLut);
                vigra_ext::enforceMonotonicity(outLut);
                graph.SetChartDisplay(0, 0, outLut.size()-1.0, 1.0);
                for(size_t i=0; i<outLut.size(); i++)
                {
                    points.push_back(hugin_utils::FDiff2D(i, outLut[i]));
                };
                graph.DrawLine(points, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), 2);
            };
            break;
        case HuginBase::SrcPanoImage::RESPONSE_LINEAR:
        default:
            {
                std::vector<hugin_utils::FDiff2D> points;
                points.push_back(hugin_utils::FDiff2D(0, 0));
                points.push_back(hugin_utils::FDiff2D(1, 1));
                graph.DrawLine(points, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), 2);
            };
            break;
    };
    //show popup
    m_popup = new wxGraphTools::GraphPopupWindow(this, graph.GetGraph());
    wxWindow *button = (wxWindow*) e.GetEventObject();
    wxPoint pos=button->ClientToScreen(wxPoint(0,0));
    m_popup->Position(pos, button->GetSize());
    m_popup->Popup();
};
