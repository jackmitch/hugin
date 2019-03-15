// -*- c-basic-offset: 4 -*-

/** @file OptimizePhotometricPanel.cpp
 *
 *  @brief implementation of OptimizePhotometricPanel
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 */

/*  This program is free software; you can redistribute it and/or
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
#include "panoinc_WX.h"

#include "panoinc.h"

#include <algorithms/point_sampler/PointSampler.h>
#include <algorithms/optimizer/PhotometricOptimizer.h>
#include <algorithms/basic/CalculateMeanExposure.h>

#include <vigra_ext/Pyramid.h>
#include <vigra_ext/openmp_vigra.h>
#include "hugin/OptimizePhotometricPanel.h"
#include "base_wx/CommandHistory.h"
#include "base_wx/PanoCommand.h"
#include "hugin/MainFrame.h"
#include "base_wx/MyProgressDialog.h"
#include "hugin/config_defaults.h"
#include "base_wx/wxImageCache.h"
#include "hugin/ImagesTree.h"
#include "hugin_base/panodata/OptimizerSwitches.h"
#include "hugin/PanoOperation.h"

//============================================================================
//============================================================================
//============================================================================

BEGIN_EVENT_TABLE(OptimizePhotometricPanel, wxPanel)
    EVT_CLOSE(OptimizePhotometricPanel::OnClose)
    EVT_BUTTON(XRCID("optimize_photo_panel_optimize"), OptimizePhotometricPanel::OnOptimizeButton)
    EVT_BUTTON(XRCID("optimize_photo_panel_reset"), OptimizePhotometricPanel::OnReset)
    EVT_CHECKBOX(XRCID("optimize_photo_panel_only_active_images"), OptimizePhotometricPanel::OnCheckOnlyActiveImages)
END_EVENT_TABLE()

OptimizePhotometricPanel::OptimizePhotometricPanel() : m_pano(0)
{
};

bool OptimizePhotometricPanel::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                      long style, const wxString& name)
{
    DEBUG_TRACE("");
    if (! wxPanel::Create(parent, id, pos, size, style, name))
    {
        return false;
    }

    wxXmlResource::Get()->LoadPanel(this, wxT("optimize_photo_panel"));
    wxPanel * panel = XRCCTRL(*this, "optimize_photo_panel", wxPanel);

    wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );
    topsizer->Add(panel, 1, wxEXPAND, 0);
    SetSizer(topsizer);

    m_only_active_images_cb = XRCCTRL(*this, "optimize_photo_panel_only_active_images", wxCheckBox);
    DEBUG_ASSERT(m_only_active_images_cb);

    m_images_tree = XRCCTRL(*this, "optimize_photo_panel_images", ImagesTreeCtrl);
    DEBUG_ASSERT(m_images_tree);
    m_lens_tree = XRCCTRL(*this, "optimize_photo_panel_lens", ImagesTreeCtrl);
    DEBUG_ASSERT(m_lens_tree);

    XRCCTRL(*this, "optimize_photo_panel_splitter", wxSplitterWindow)->SetSashGravity(0.66);

    return true;
}

void OptimizePhotometricPanel::Init(HuginBase::Panorama * panorama)
{
    m_pano = panorama;
    // observe the panorama
    m_pano->addObserver(this);
    m_images_tree->Init(m_pano);
    m_images_tree->SetOptimizerMode();
    m_images_tree->SetDisplayMode(ImagesTreeCtrl::DISPLAY_PHOTOMETRICS_IMAGES);

    m_lens_tree->Init(m_pano);
    m_lens_tree->SetOptimizerMode();
    m_lens_tree->SetGroupMode(ImagesTreeCtrl::GROUP_LENS);
    m_lens_tree->SetDisplayMode(ImagesTreeCtrl::DISPLAY_PHOTOMETRICS_LENSES);
    
}

void OptimizePhotometricPanel::SetGuiLevel(GuiLevel newGuiLevel)
{
    m_images_tree->SetGuiLevel(newGuiLevel);
    m_lens_tree->SetGuiLevel(newGuiLevel);
};

OptimizePhotometricPanel::~OptimizePhotometricPanel()
{
    DEBUG_TRACE("dtor, writing config");

    m_pano->removeObserver(this);
    DEBUG_TRACE("dtor end");
}

void OptimizePhotometricPanel::OnCheckOnlyActiveImages(wxCommandEvent & e)
{
    MainFrame::Get()->SetOptimizeOnlyActiveImages(e.IsChecked());
};

void OptimizePhotometricPanel::SetOnlyActiveImages(const bool onlyActive)
{
    m_only_active_images_cb->SetValue(onlyActive);
    m_images_tree->MarkActiveImages(onlyActive);
    m_lens_tree->MarkActiveImages(onlyActive);
};

void OptimizePhotometricPanel::OnOptimizeButton(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    // run optimizer

    HuginBase::UIntSet imgs;
    if (m_only_active_images_cb->IsChecked() || m_pano->getPhotometricOptimizerSwitch()!=0)
    {
        // use only selected images.
        imgs = m_pano->getActiveImages();
        if (imgs.size() < 2)
        {
            wxMessageBox(_("The project does not contain any active images.\nPlease activate at least 2 images in the (fast) preview window.\nOptimization canceled."),
#ifdef _WIN32
                _("Hugin"),
#else
                wxT(""),
#endif
                wxICON_ERROR | wxOK);
            return;
        } 
    }
    else
    {
        fill_set(imgs, 0, m_pano->getNrOfImages()-1);
    }
    runOptimizer(imgs);
}

void OptimizePhotometricPanel::panoramaChanged(HuginBase::Panorama & pano)
{
    m_images_tree->Enable(m_pano->getPhotometricOptimizerSwitch()==0);
    m_lens_tree->Enable(m_pano->getPhotometricOptimizerSwitch()==0);
}

void OptimizePhotometricPanel::panoramaImagesChanged(HuginBase::Panorama &pano,
                                          const HuginBase::UIntSet & imgNr)
{
    XRCCTRL(*this, "optimize_photo_panel_optimize", wxButton)->Enable(pano.getNrOfImages()>1);
    XRCCTRL(*this, "optimize_photo_panel_reset", wxButton)->Enable(pano.getNrOfImages()>0);    
}

void OptimizePhotometricPanel::runOptimizer(const HuginBase::UIntSet & imgs)
{
    DEBUG_TRACE("");
    int mode = m_pano->getPhotometricOptimizerSwitch();

    // check if vignetting and response are linked, display a warning if they are not
    // The variables to check:
    const HuginBase::ImageVariableGroup::ImageVariableEnum vars[] = {
            HuginBase::ImageVariableGroup::IVE_EMoRParams,
            HuginBase::ImageVariableGroup::IVE_ResponseType,
            HuginBase::ImageVariableGroup::IVE_VigCorrMode,
            HuginBase::ImageVariableGroup::IVE_RadialVigCorrCoeff,
            HuginBase::ImageVariableGroup::IVE_RadialVigCorrCenterShift
        };
    // keep a list of commands needed to fix it:
    std::vector<PanoCommand::PanoCommand *> commands;
    HuginBase::ConstStandardImageVariableGroups variable_groups(*m_pano);
    HuginBase::ConstImageVariableGroup & lenses = variable_groups.getLenses();
    for (size_t i = 0; i < lenses.getNumberOfParts(); i++)
    {
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> links_needed;
        links_needed.clear();
        for (int v = 0; v < 5; v++)
        {
            if (!lenses.getVarLinkedInPart(vars[v], i))
            {
                links_needed.insert(vars[v]);
            }
        };
        if (!links_needed.empty())
        {
            commands.push_back(new PanoCommand::LinkLensVarsCmd(*m_pano, i, links_needed));
        }
    }
    // if the list of commands is empty, all is good and we don't need a warning.
    if (!commands.empty()) {
        int ok = wxMessageBox(_("The same vignetting and response parameters should\nbe applied for all images of a lens.\nCurrently each image can have different parameters.\nLink parameters?"), _("Link parameters"), wxYES_NO | wxICON_INFORMATION);
        if (ok == wxYES)
        {
            // perform all the commands we stocked up earilier.
            for (std::vector<PanoCommand::PanoCommand *>::iterator it = commands.begin(); it != commands.end(); ++it)
            {
                PanoCommand::GlobalCmdHist::getInstance().addCommand(*it);
            }
        }
        else
        {
            // free all the commands, the user doesn't want them used.
            for (std::vector<PanoCommand::PanoCommand *>::iterator it = commands.begin(); it != commands.end(); ++it)
            {
                delete *it;
            }
        }
    }

    HuginBase::Panorama optPano = m_pano->getSubset(imgs);
    HuginBase::PanoramaOptions opts = optPano.getOptions();

    HuginBase::OptimizeVector optvars;
    if(mode==0)
    {
        optvars = optPano.getOptimizeVector();
        bool valid=false;
        for(unsigned int i=0;i<optvars.size() && !valid;i++)
        {
            if(set_contains(optvars[i], "Eev") || set_contains(optvars[i], "Er") || set_contains(optvars[i], "Eb") ||
                set_contains(optvars[i], "Vb") || set_contains(optvars[i], "Vx") || set_contains(optvars[i], "Ra"))
            {
                valid=true;
            };
        };
        if(!valid)
        {
            wxMessageBox(_("You selected no parameters to optimize.\nTherefore optimization will be canceled."), _("Exposure optimization"), wxOK | wxICON_INFORMATION);
            return;
        };
    };

    double error = 0;
    {
        std::vector<vigra_ext::PointPairRGB> points;
        long nPoints = 200;
        wxConfigBase::Get()->Read(wxT("/OptimizePhotometric/nRandomPointsPerImage"), &nPoints , HUGIN_PHOTOMETRIC_OPTIMIZER_NRPOINTS);

        ProgressReporterDialog progress(optPano.getNrOfImages()+2, _("Photometric alignment"), _("Loading images"));
        progress.Show();

        nPoints = nPoints * optPano.getNrOfImages();
        // get the small images
        std::vector<vigra::FRGBImage *> srcImgs;
        HuginBase::LimitIntensityVector limits;
        float imageStepSize = 1 / 255.0f;
        for (size_t i=0; i < optPano.getNrOfImages(); i++)
        {
            ImageCache::EntryPtr e = ImageCache::getInstance().getSmallImage(optPano.getImage(i).getFilename());
            if (!e)
            {
                wxMessageBox(_("Error: could not load all images"), _("Error"));
                return;
            }
            HuginBase::LimitIntensity limit;
            vigra::FRGBImage * img = new vigra::FRGBImage;
            if (e->imageFloat && e->imageFloat->size().area() > 0)
            {
                if (e->mask->size().area() > 0)
                {
                    vigra::BImage maskSmall;
                    vigra_ext::reduceToNextLevel(*(e->imageFloat), *(e->mask), *img, maskSmall);
                    vigra_ext::FindComponentsMinMax<float> minmax;
                    vigra::inspectImageIf(srcImageRange(*img), srcImage(maskSmall), minmax);
                    imageStepSize = std::min(imageStepSize, (minmax.max - minmax.min) / 16384.0f);
                }
                else
                {
                    vigra_ext::reduceToNextLevel(*(e->imageFloat), *img);
                    vigra_ext::FindComponentsMinMax<float> minmax;
                    vigra::inspectImage(srcImageRange(*img), minmax);
                    imageStepSize = std::min(imageStepSize, (minmax.max - minmax.min) / 16384.0f);
                };
                limit = HuginBase::LimitIntensity(HuginBase::LimitIntensity::LIMIT_FLOAT);
            }
            else
            {
                if (e->image16 && e->image16->size().area() > 0)
                {
                    vigra_ext::reduceToNextLevel(*(e->image16), *img);
                    vigra::omp::transformImage(vigra::srcImageRange(*img), vigra::destImage(*img),
                        vigra::functor::Arg1() / vigra::functor::Param(65535.0));
                    limit = HuginBase::LimitIntensity(HuginBase::LimitIntensity::LIMIT_UINT16);
                    imageStepSize = std::min(imageStepSize, 1 / 65536.0f);
                }
                else
                {
                    vigra_ext::reduceToNextLevel(*(e->get8BitImage()), *img);
                    vigra::omp::transformImage(vigra::srcImageRange(*img), vigra::destImage(*img),
                        vigra::functor::Arg1() / vigra::functor::Param(255.0));
                    limit = HuginBase::LimitIntensity(HuginBase::LimitIntensity::LIMIT_UINT8);
                    imageStepSize = std::min(imageStepSize, 1 / 255.0f);
                };
            };
            srcImgs.push_back(img);
            limits.push_back(limit);
            if (!progress.updateDisplayValue())
            {
                // check if user pressed cancel
                return;
            };
        }   
        points= HuginBase::RandomPointSampler(optPano, &progress, srcImgs, limits, nPoints).execute().getResultPoints();

        if (!progress.updateDisplayValue())
        {
            return;
        };
        if (points.size() == 0)
        {
            wxMessageBox(_("Error: no overlapping points found, Photometric optimization aborted"), _("Error"));
            return;
        }

        progress.setMaximum(0);
        progress.updateDisplay(_("Optimize..."));
        try
        {
            if (mode != 0)
            {
                // run automatic optimisation
                // ensure that we have a valid anchor.
                HuginBase::PanoramaOptions opts = optPano.getOptions();
                if (opts.colorReferenceImage >= optPano.getNrOfImages())
                {
                    opts.colorReferenceImage = 0;
                    optPano.setOptions(opts);
                }
                HuginBase::SmartPhotometricOptimizer::PhotometricOptimizeMode optMode;
                switch(mode)
                {
                    case (HuginBase::OPT_EXPOSURE | HuginBase::OPT_VIGNETTING | HuginBase::OPT_RESPONSE):
                        optMode = HuginBase::SmartPhotometricOptimizer::OPT_PHOTOMETRIC_LDR;
                        break;
                    case (HuginBase::OPT_EXPOSURE | HuginBase::OPT_VIGNETTING | HuginBase::OPT_RESPONSE | HuginBase::OPT_WHITEBALANCE):
                        optMode = HuginBase::SmartPhotometricOptimizer::OPT_PHOTOMETRIC_LDR_WB;
                        break;
                    case (HuginBase::OPT_VIGNETTING | HuginBase::OPT_RESPONSE):
                        optMode = HuginBase::SmartPhotometricOptimizer::OPT_PHOTOMETRIC_HDR;
                        break;
                    case (HuginBase::OPT_WHITEBALANCE | HuginBase::OPT_VIGNETTING | HuginBase::OPT_RESPONSE):
                        optMode = HuginBase::SmartPhotometricOptimizer::OPT_PHOTOMETRIC_HDR_WB;
                        break;
                    default:
                        //invalid combination
                        return;
                };
                HuginBase::SmartPhotometricOptimizer::smartOptimizePhotometric(optPano, optMode, points, imageStepSize, &progress, error);
            }
            else
            {
                // optimize selected parameters
                HuginBase::PhotometricOptimizer::optimizePhotometric(optPano, optvars, points, imageStepSize, &progress, error);
            }
            if (progress.wasCancelled())
            {
                return;
            }
        }
        catch (std::exception & error)
        {
            wxMessageBox(_("Internal error during photometric optimization:\n") + wxString(error.what(), wxConvLocal), _("Internal error"));
            return;
        }
    }
    wxYield();

    // display information about the estimation process:
    int ret = wxMessageBox(wxString::Format(_("Photometric optimization results:\nAverage difference (RMSE) between overlapping pixels: %.2f gray values (0..255)\n\nApply results?"), error*255),
                           _("Photometric optimization finished"), wxYES_NO | wxICON_INFORMATION,this);

    if (ret == wxYES)
    {
        DEBUG_DEBUG("Applying vignetting corr");
        // TODO: merge into a single update command
        const HuginBase::VariableMapVector & vars = optPano.getVariables();
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::UpdateImagesVariablesCmd(*m_pano, imgs, vars)
                                               );
        //now update panorama exposure value
        HuginBase::PanoramaOptions opts = m_pano->getOptions();
        opts.outputExposureValue = HuginBase::CalculateMeanExposure::calcMeanExposure(*m_pano);
        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::SetPanoOptionsCmd(*m_pano, opts)
                                               );
    }
}

void OptimizePhotometricPanel::OnClose(wxCloseEvent& event)
{
    DEBUG_TRACE("OnClose");
    // do not close, just hide if we're not forced
    if (event.CanVeto())
    {
        event.Veto();
        Hide();
        DEBUG_DEBUG("Hiding");
    }
    else
    {
        DEBUG_DEBUG("Closing");
        Destroy();
    }
}

void OptimizePhotometricPanel::OnReset(wxCommandEvent& e)
{
    PanoOperation::ResetOperation op(PanoOperation::ResetOperation::RESET_DIALOG_PHOTOMETRICS);
    PanoCommand::PanoCommand* cmd=op.GetCommand(this,*m_pano, m_images_tree->GetSelectedImages(), MainFrame::Get()->GetGuiLevel());
    if(cmd!=NULL)
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(cmd);
    };
};

IMPLEMENT_DYNAMIC_CLASS(OptimizePhotometricPanel, wxPanel)

OptimizePhotometricPanelXmlHandler::OptimizePhotometricPanelXmlHandler()
                : wxXmlResourceHandler()
{
    AddWindowStyles();
}

wxObject *OptimizePhotometricPanelXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(cp, OptimizePhotometricPanel)

    cp->Create(m_parentAsWindow,
                   GetID(),
                   GetPosition(), GetSize(),
                   GetStyle(wxT("style")),
                   GetName());

    SetupWindow( cp);

    return cp;
}

bool OptimizePhotometricPanelXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("OptimizePhotometricPanel"));
}

IMPLEMENT_DYNAMIC_CLASS(OptimizePhotometricPanelXmlHandler, wxXmlResourceHandler)
