// -*- c-basic-offset: 4 -*-

/** @file wxPanoCommand.cpp
 *
 *  @brief implementation of some PanoCommands
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

#include "panoinc_WX.h"
#include "panoinc.h"
#include <wx/window.h> 
#include "wxPlatform.h"
#include "LensTools.h"

#include "wxImageCache.h"
#include "platform.h"
#include "wxPanoCommand.h"
#include "HFOVDialog.h"
#include <panodata/OptimizerSwitches.h>

#include <vigra/cornerdetection.hxx>
#include <vigra/localminmax.hxx>
#include <panodata/StandardImageVariableGroups.h>

#include <hugin_utils/alphanum.h>

#ifdef HUGIN_HSI
#include "hugin_script_interface/hpi.h"
#endif

namespace PanoCommand
{

bool wxAddCtrlPointGridCmd::processPanorama(HuginBase::Panorama& pano)
{
    // TODO: rewrite, and use same function for modular image creation.
    const HuginBase::SrcPanoImage & i1 = pano.getImage(img1);
    const HuginBase::SrcPanoImage & i2 = pano.getImage(img1);

    // run both images through the harris corner detector
    ImageCache::EntryPtr eptr = ImageCache::getInstance().getSmallImage(i1.getFilename());

    vigra::BImage leftImg(eptr->get8BitImage()->size());

    vigra::GreenAccessor<vigra::RGBValue<vigra::UInt8> > ga;
    vigra::copyImage(srcImageRange(*(eptr->get8BitImage()), ga ),
                     destImage(leftImg));

    double scale = i1.getSize().width() / (double) leftImg.width();

    //const vigra::BImage & leftImg = ImageCache::getInstance().getPyramidImage(
    //    i1.getFilename(),1);

    vigra::BImage leftCorners(leftImg.size());
    vigra::FImage leftCornerResponse(leftImg.size());

    // empty corner image
    leftCorners.init(0);

    DEBUG_DEBUG("running corner detector threshold: " << cornerThreshold << "  scale: " << scale );

    // find corner response at scale scale
    vigra::cornerResponseFunction(srcImageRange(leftImg),
        destImage(leftCornerResponse),
        scale);

    //    saveScaledImage(leftCornerResponse,"corner_response.png");
    DEBUG_DEBUG("finding local maxima");
    // find local maxima of corner response, mark with 1
    vigra::localMaxima(vigra::srcImageRange(leftCornerResponse), vigra::destImage(leftCorners), 255);

//    exportImage(srcImageRange(leftCorners), vigra::ImageExportInfo("c:/corner_response_maxima.png"));

    DEBUG_DEBUG("thresholding corner response");
    // threshold corner response to keep only strong corners (above 400.0)
    vigra::transformImage(vigra::srcImageRange(leftCornerResponse), vigra::destImage(leftCornerResponse),
        vigra::Threshold<double, double>(
        cornerThreshold, DBL_MAX, 0.0, 1.0));

    vigra::combineTwoImages(srcImageRange(leftCorners), srcImage(leftCornerResponse),
        destImage(leftCorners), std::multiplies<float>());

//    exportImage(srcImageRange(leftCorners), vigra::ImageExportInfo("c:/corner_response_threshold.png"));

    // create transform from img1 -> sphere
    HuginBase::PTools::Transform img1ToSphere;
    HuginBase::PTools::Transform sphereToImg2;

    HuginBase::PanoramaOptions opts;
    opts.setProjection(HuginBase::PanoramaOptions::EQUIRECTANGULAR);
    opts.setHFOV(360);
    opts.setWidth(360);
    opts.setVFOV(180);

    img1ToSphere.createInvTransform(pano, img1, opts);
    sphereToImg2.createTransform(pano, img2, opts);


    int border = 5;
    double sphx, sphy;
    double img2x, img2y;
    // need to scale the images.
    // sample grid on img1 and try to add ctrl points
    for (unsigned int x=0; x < (unsigned int)leftImg.width(); x++ ) {
        for (unsigned int y=0; y < (unsigned int)leftImg.height(); y++) {
            if (leftCorners(x,y) > 0) {
                img1ToSphere.transformImgCoord(sphx, sphy, scale*x, scale*y);
                sphereToImg2.transformImgCoord(img2x, img2y, sphx, sphy);
                // check if it is inside..
                if (   img2x > border && img2x < i2.getWidth() - border
                    && img2y > border && img2y < i2.getHeight() - border )
                {
                    // add control point
                    HuginBase::ControlPoint p(img1, scale*x, scale*y, img2, img2x, img2y);
                    pano.addCtrlPoint(p);
                }
            }
        }
    }
    return true;
}

void applyColorBalanceValue(HuginBase::SrcPanoImage& srcImg, HuginBase::Panorama& pano)
{
    double redBal=1;
    double blueBal=1;
    if(pano.getNrOfImages()>=1)
    {
        const HuginBase::SrcPanoImage &anchor = pano.getImage(pano.getOptions().colorReferenceImage);
        // use EXIF Red/BlueBalance data only if image and anchor image are from the same camera
        if(srcImg.getExifMake() == anchor.getExifMake() &&
            srcImg.getExifModel() == anchor.getExifModel())
        {
            double redBalanceAnchor=pano.getImage(pano.getOptions().colorReferenceImage).getExifRedBalance();
            double blueBalanceAnchor=pano.getImage(pano.getOptions().colorReferenceImage).getExifBlueBalance();
            if(fabs(redBalanceAnchor)<1e-2)
            {
                redBalanceAnchor=1;
            };
            if(fabs(blueBalanceAnchor)<1e-2)
            {
                blueBalanceAnchor=1;
            };
            redBal=fabs(srcImg.getExifRedBalance()/redBalanceAnchor);
            blueBal=fabs(srcImg.getExifBlueBalance()/blueBalanceAnchor);
            if(redBal<1e-2)
            {
                redBal=1;
            };
            if(blueBal<1e-2)
            {
                blueBal=1;
            };
        };
    }
    srcImg.setWhiteBalanceRed(redBal);
    srcImg.setWhiteBalanceBlue(blueBal);
};

void copySrcImageExif(HuginBase::SrcPanoImage& destImg, HuginBase::SrcPanoImage srcImg)
{
    destImg.setExifExposureTime(srcImg.getExifExposureTime());
    destImg.setExifAperture(srcImg.getExifAperture());
    destImg.setExifExposureMode(srcImg.getExifExposureMode());
    destImg.setExifISO(srcImg.getExifISO());
    destImg.setExifMake(srcImg.getExifMake());
    destImg.setExifModel(srcImg.getExifModel());
    destImg.setExifLens(srcImg.getExifLens());
    destImg.setExifOrientation(srcImg.getExifOrientation());
    destImg.setExifFocalLength(srcImg.getExifFocalLength());
    destImg.setExifFocalLength35(srcImg.getExifFocalLength35());
    destImg.setExifCropFactor(srcImg.getExifCropFactor());
    destImg.setExifDistance(srcImg.getExifDistance());
    destImg.setExifDate(srcImg.getExifDate());
    destImg.setExifRedBalance(srcImg.getExifRedBalance());
    destImg.setExifBlueBalance(srcImg.getExifBlueBalance());
    destImg.setFileMetadata(srcImg.getFileMetadata());
};

bool getLensDataFromUser(wxWindow * parent, HuginBase::SrcPanoImage & srcImg)
{
    // display lens dialog
    HFOVDialog dlg(parent, srcImg);
    dlg.CenterOnParent();
    int ret = dlg.ShowModal();
    if (ret == wxID_OK)
    {
        // assume a cancel dialog.
        srcImg = dlg.GetSrcImage();
        if (dlg.GetCropFactor() <= 0)
        {
            srcImg.setCropFactor(1);
        }
        return true;
    }
    else {
        return false;
    }
}

bool wxAddImagesCmd::processPanorama(HuginBase::Panorama& pano)
{
    // check if the files should be sorted by date
    long sort = wxConfigBase::Get()->Read(wxT("General/SortNewImgOnAdd"), HUGIN_GUI_SORT_NEW_IMG_ON_ADD);

    switch (sort) {
        case 1:
                // sort by filename
            std::sort(files.begin(), files.end(), doj::alphanum_less());
            break;
        case 2:
                // sort by date
            std::sort(files.begin(), files.end(), FileIsNewer());
            break;
        default:
                // no or unknown sort method
            break;
    }

    std::vector<std::string>::const_iterator it;
    HuginBase::Lens lens;
    HuginBase::SrcPanoImage srcImg;
    HuginBase::SrcPanoImage srcImgExif;
    HuginBase::StandardImageVariableGroups variable_groups(pano);
    HuginBase::ImageVariableGroup & lenses = variable_groups.getLenses();
    const size_t oldImgCount = pano.getNrOfImages();

    // load additional images...
    for (it = files.begin(); it != files.end(); ++it) {
        const std::string &filename = *it;
        wxString fname(filename.c_str(), HUGIN_CONV_FILENAME);

        // try to read settings automatically.
        srcImg.setFilename(filename);
        try
        {
            vigra::ImageImportInfo info(filename.c_str());
            if(info.width()==0 || info.height()==0)
            {
                wxMessageBox(wxString::Format(_("Could not decode image:\n%s\nAbort"), fname.c_str()), _("Unsupported image file format"));
                return false;
            };
            srcImg.setSize(info.size());
            const std::string pixelType=info.getPixelType();
            // refuse black/white images
            if (pixelType == "BILEVEL")
            {
                wxMessageBox(wxString::Format(_("File \"%s\" is a black/white image.\nHugin does not support this image type. Skipping this image.\nConvert image to grayscale image and try loading again."), fname.c_str()),
                    _("Warning"), wxOK|wxICON_EXCLAMATION);
                continue;
            }
            // check if images is grayscale or RGB image, maybe with alpha channel
            // reject CMYK or other images
            const int bands = info.numBands();
            const int extraBands = info.numExtraBands();
            if (bands != 1 && bands != 3 && !(bands == 2 && extraBands == 1) && !(bands == 4 && extraBands == 1))
            {
                wxMessageBox(wxString::Format(_("Hugin supports only grayscale and RGB images (without and with alpha channel).\nBut file \"%s\" has %d channels and %d extra channels (probably alpha channels).\nHugin does not support this image type. Skipping this image.\nConvert this image to grayscale or RGB image and try loading again."), fname.c_str(), bands, extraBands),
                    _("Warning"), wxOK | wxICON_EXCLAMATION);
                continue;
            };
            if (pano.getNrOfImages() == 0)
            {
                pano.setNrOfBands(bands - extraBands);
            };
            if (pano.getNrOfBands() != bands - extraBands)
            {
                wxString s(_("Hugin supports only grayscale or RGB images (without and with alpha channel)."));
                s.Append(wxT("\n"));
                if (pano.getNrOfBands() == 3)
                {
                    s.Append(wxString::Format(_("File \"%s\" is a grayscale image, but other images in project are color images."), fname.c_str()));
                }
                else
                {
                    s.Append(wxString::Format(_("File \"%s\" is a color image, but other images in project are grayscale images."), fname.c_str()));
                };
                s.Append(wxT("\n"));
                s.Append(_("Hugin does not support this mixing. Skipping this image.\nConvert this image to grayscale or RGB image respectively and try loading again."));
                wxMessageBox(s, _("Warning"), wxOK | wxICON_EXCLAMATION);
                continue;
            };
            if((pixelType=="UINT8") || (pixelType=="UINT16") || (pixelType=="INT16"))
                srcImg.setResponseType(HuginBase::SrcPanoImage::RESPONSE_EMOR);
            else
                srcImg.setResponseType(HuginBase::SrcPanoImage::RESPONSE_LINEAR);
            if (pano.getNrOfImages() > 0)
            {
                const std::string newICCProfileDesc = hugin_utils::GetICCDesc(info.getICCProfile());
                if (newICCProfileDesc != pano.getICCProfileDesc())
                {
                    // icc profiles does not match
                    wxString warning;
                    if (newICCProfileDesc.empty())
                    { 
                        warning = wxString::Format(_("File \"%s\" has no embedded icc profile, but other images in project have profile \"%s\" embedded."), fname.c_str(), wxString(pano.getICCProfileDesc().c_str(), wxConvLocal).c_str());
                    }
                    else
                    {
                        if (pano.getICCProfileDesc().empty())
                        {
                            warning = wxString::Format(_("File \"%s\" has icc profile \"%s\" embedded, but other images in project have no embedded color profile."), fname.c_str(), wxString(newICCProfileDesc.c_str(), wxConvLocal).c_str());
                        }
                        else
                        {
                            warning = wxString::Format(_("File \"%s\" has icc profile \"%s\" embedded, but other images in project have color profile \"%s\" embedded."), fname.c_str(), wxString(newICCProfileDesc.c_str(), wxConvLocal).c_str(), wxString(pano.getICCProfileDesc().c_str(), wxConvLocal).c_str());
                        }
                    }
                    warning.Append(wxT("\n"));
                    warning.Append(_("Hugin expects all images in the same color profile.\nPlease convert all images to same color profile and try again."));
                    wxMessageBox(warning, _("Warning"), wxOK | wxICON_EXCLAMATION);
                    continue;
                }
            }
            else
            {
                // remember icc profile name
                if (!info.getICCProfile().empty())
                {
                    pano.setICCProfileDesc(hugin_utils::GetICCDesc(info.getICCProfile()));
                }
                else
                {
                    pano.setICCProfileDesc("");
                };
            }
        }
        catch(std::exception & e)
        {
            std::cerr << "ERROR: caught exception: " << e.what() << std::endl;
            std::cerr << "Could not get pixel type for file " << filename << std::endl;
             wxMessageBox(wxString::Format(_("Could not decode image:\n%s\nAbort"), fname.c_str()), _("Unsupported image file format"));
             return false;
        };
        bool ok = srcImg.readEXIF();
        if(ok)
        {
            ok = srcImg.applyEXIFValues();
            if (srcImg.getProjection() != HuginBase::BaseSrcPanoImage::EQUIRECTANGULAR)
            {
                // if projection is equirectangular, we loaded info from gpano tags
                // in this case we don't need to look up the database
                srcImg.readProjectionFromDB();
            };
        };
        // save EXIF data for later to prevent double loading of EXIF data
        srcImgExif=srcImg;
        applyColorBalanceValue(srcImg, pano);
        double redBal=srcImg.getWhiteBalanceRed();
        double blueBal=srcImg.getWhiteBalanceBlue();
        if(srcImg.getCropFactor()<0.1)
        {
            srcImg.readCropfactorFromDB();
            ok=(srcImg.getExifFocalLength()>0 && srcImg.getCropFactor()>0.1);
        };
        if (! ok ) {
                // search for image with matching size and exif data
                // and re-use it.
                bool added = false;
                for (unsigned int i=0; i < pano.getNrOfImages(); i++) {
                    HuginBase::SrcPanoImage other = pano.getSrcImage(i);
                    if ( other.getSize() == srcImg.getSize() &&
                         other.getExifModel() == srcImg.getExifModel() &&
                         other.getExifMake()  == srcImg.getExifMake() &&
                         other.getExifFocalLength() == srcImg.getExifFocalLength()
                       )
                    {
                        double ev = srcImg.getExposureValue();
                        srcImg = pano.getSrcImage(i);
                        srcImg.setFilename(filename);
                        srcImg.deleteAllMasks();
                        copySrcImageExif(srcImg, srcImgExif);
                        // add image
                        int imgNr = pano.addImage(srcImg);
                        variable_groups.update();
                        lenses.switchParts(imgNr, lenses.getPartNumber(i));
                        lenses.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_ExposureValue, i);
                        srcImg.setExposureValue(ev);
                        lenses.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_WhiteBalanceRed, i);
                        lenses.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_WhiteBalanceBlue, i);
                        applyColorBalanceValue(srcImg, pano);
                        pano.setSrcImage(imgNr, srcImg);
                        added=true;
                        break;
                    }
                }
                if (added) continue;
        }
        int matchingLensNr=-1;
        // if no similar image found, ask user
        if (! ok) {
            if (!getLensDataFromUser(wxGetActiveWindow(), srcImg)) {
                // assume a standart lens
                srcImg.setHFOV(50);
                srcImg.setCropFactor(1);
            }
        }

        // check the image hasn't disappeared on us since the HFOV dialog was
        // opened
        wxString fn(srcImg.getFilename().c_str(),HUGIN_CONV_FILENAME);
        if (!wxFileName::FileExists(fn)) {
            DEBUG_INFO("Image: " << fn.mb_str() << " has disappeared, skipping...");
            continue;
        }

        // FIXME: check if the exif information
        // indicates this image matches a already used lens
        variable_groups.update();
        double ev = 0;
        bool set_exposure = false;
        for (unsigned int i=0; i < pano.getNrOfImages(); i++) {
            HuginBase::SrcPanoImage other = pano.getSrcImage(i);
            if (other.getExifFocalLength()>0) {
                if (other.getSize() == srcImg.getSize()
                    && other.getExifModel() == srcImg.getExifModel()
                    && other.getExifMake()  == srcImg.getExifMake()
                    && other.getExifFocalLength() == srcImg.getExifFocalLength()
                   )
                {
                    matchingLensNr = lenses.getPartNumber(i);
                    // copy data from other image, just keep
                    // the file name and reload the exif data (for exposure)
                    ev = srcImg.getExposureValue();
                    redBal = srcImg.getWhiteBalanceRed();
                    blueBal = srcImg.getWhiteBalanceBlue();
                    set_exposure = true;
                    srcImg = pano.getSrcImage(i);
                    srcImg.setFilename(filename);
                    srcImg.deleteAllMasks();
                    copySrcImageExif(srcImg, srcImgExif);
                    srcImg.setExposureValue(ev);
                    srcImg.setWhiteBalanceRed(redBal);
                    srcImg.setWhiteBalanceBlue(blueBal);
                    break;
                }
            }
            else
            {
                // no exiv information, just check image size.
                if (other.getSize() == srcImg.getSize() )
                {
                    matchingLensNr = lenses.getPartNumber(i);
                    // copy data from other image, just keep
                    // the file name
                    srcImg = pano.getSrcImage(i);
                    srcImg.setFilename(filename);
                    srcImg.deleteAllMasks();
                    copySrcImageExif(srcImg, srcImgExif);
                    break;
                }
            }
        }

        // If matchingLensNr == -1 still, we haven't found a good lens to use.
        // We shouldn't attach the image to a lens in this case, it will have
        // its own new lens.
        int imgNr = pano.addImage(srcImg);
        variable_groups.update();
        if (matchingLensNr != -1)
        {
            lenses.switchParts(imgNr, matchingLensNr);
            // unlink and set exposure value, if wanted.
            if (set_exposure)
            {
                lenses.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_ExposureValue, imgNr);
                lenses.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_WhiteBalanceRed, imgNr);
                lenses.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_WhiteBalanceBlue, imgNr);
                //don't link image size, this will foul the photometric optimizer
                lenses.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_Size, imgNr);
                /// @todo avoid copying the SrcPanoImage.
                HuginBase::SrcPanoImage t = pano.getSrcImage(imgNr);
                t.setExposureValue(ev);
                t.setWhiteBalanceRed(redBal);
                t.setWhiteBalanceBlue(blueBal);
                pano.setSrcImage(imgNr, t);
            }
        }
        if (imgNr == 0) {
            // get initial value for output exposure
            HuginBase::PanoramaOptions opts = pano.getOptions();
            opts.outputExposureValue = srcImg.getExposureValue();
            pano.setOptions(opts);
            // set the exposure, but there isn't anything to link to so don't try unlinking.
            // links are made by default when adding new images.
            if (set_exposure)
            {
                /// @todo avoid copying the SrcPanoImage.
                HuginBase::SrcPanoImage t = pano.getSrcImage(imgNr);
                t.setExposureValue(ev);
                t.setWhiteBalanceRed(redBal);
                t.setWhiteBalanceBlue(blueBal);
                pano.setSrcImage(imgNr, t);
            }
        }
    }
    if (pano.hasPossibleStacks())
    {
        wxString message;
        if (oldImgCount == 0)
        {
            // all images added
            message = _("Hugin has image stacks detected in the added images and will assign corresponding stack numbers to the images.");
        }
        else
        {
            message = _("Hugin has image stacks detected in the whole project. Stack numbers will be re-assigned on base of this detection. Existing stack assignments will be overwritten.");
        };
        message.append(wxT("\n"));
        message.append(_("Should the position of images in each stack be linked?"));
        wxMessageDialog dialog(wxGetActiveWindow(), message,
#ifdef _WIN32
            _("Hugin"),
#else
            wxT(""),
#endif
            wxICON_EXCLAMATION | wxYES_NO | wxCANCEL);
        dialog.SetExtendedMessage(_("When shooting bracketed image stacks from a sturdy tripod the position of the images in each stack can be linked to help Hugin to process the panorama. But if the images in each stack require a fine tune of the position (e. g. when shooting hand held), then don't link the position."));
        if (oldImgCount == 0)
        {
            dialog.SetYesNoCancelLabels(_("Link position"), _("Don't link position"), _("Don't assign stacks"));
        }
        else
        {
            dialog.SetYesNoCancelLabels(_("Link position"), _("Don't link position"), _("Keep existing stacks"));
        };
        switch (dialog.ShowModal())
        {
            case wxID_OK:
            case wxID_YES:
                pano.linkPossibleStacks(true);
                break;
            case wxID_NO:
                pano.linkPossibleStacks(false);
                break;
        };
    }
    else
    {
        bool hasStacks = false;
        for (size_t i = 0; i<pano.getNrOfImages(); i++)
        {
            if (pano.getImage(i).StackisLinked())
            {
                hasStacks = true;
                break;
            };
        };
        wxConfigBase* config = wxConfigBase::Get();
        bool showExposureWarning = config->Read(wxT("/ShowExposureWarning"), 1l) == 1l;
        if (!hasStacks && pano.getMaxExposureDifference() > 2 && showExposureWarning)
        {
            wxDialog dlg;
            wxXmlResource::Get()->LoadDialog(&dlg, NULL, wxT("warning_exposure_dlg"));
            if (dlg.ShowModal() == wxID_OK)
            {
                if (XRCCTRL(dlg, "dont_show_again_checkbox", wxCheckBox)->GetValue())
                {
                    config->Write(wxT("/ShowExposureWarning"), 0l);
                }
                else
                {
                    config->Write(wxT("/ShowExposureWarning"), 1l);
                };
                config->Flush();
            }
        };
    };
    return true;
}


bool wxLoadPTProjectCmd::processPanorama(HuginBase::Panorama& pano)
{
    HuginBase::PanoramaMemento newPano;
    int ptoVersion = 0;
    std::ifstream in(filename.c_str());
    if (newPano.loadPTScript(in, ptoVersion, prefix))
    {
        pano.setMemento(newPano);
        HuginBase::PanoramaOptions opts = pano.getOptions();
        // always reset to TIFF_m ...
        opts.outputFormat = HuginBase::PanoramaOptions::TIFF_m;
        // get enblend and enfuse options from preferences
        if (ptoVersion < 2)
        {
            // no options stored in file, use default arguments in config file
            opts.enblendOptions = wxConfigBase::Get()->Read(wxT("/Enblend/Args"), wxT(HUGIN_ENBLEND_ARGS)).mb_str(wxConvLocal);
            opts.enfuseOptions = wxConfigBase::Get()->Read(wxT("/Enfuse/Args"), wxT(HUGIN_ENFUSE_ARGS)).mb_str(wxConvLocal);
        }
        // Set the nona gpu flag base on what is in preferences as it is not
        // stored in the file.
        opts.remapUsingGPU = wxConfigBase::Get()->Read(wxT("/Nona/UseGPU"),HUGIN_NONA_USEGPU) == 1;
        pano.setOptions(opts);

        HuginBase::StandardImageVariableGroups variableGroups(pano);
        HuginBase::ImageVariableGroup & lenses = variableGroups.getLenses();

        unsigned int nImg = pano.getNrOfImages();
        wxString basedir;
        bool autopanoSiftFile=false;
        HuginBase::SrcPanoImage autopanoSiftRefImg;
        for (unsigned int i = 0; i < nImg; i++) {
            wxFileName fname(wxString (pano.getImage(i).getFilename().c_str(), HUGIN_CONV_FILENAME));
            while (! fname.FileExists()){
                        // Is file in the new path
                if (basedir != wxT("")) {
                    DEBUG_DEBUG("Old filename: " << pano.getImage(i).getFilename());
                    std::string fn = hugin_utils::stripPath(pano.getImage(i).getFilename());
                    DEBUG_DEBUG("Old filename, without path): " << fn);
                    wxString newname(fn.c_str(), HUGIN_CONV_FILENAME);
                            // GetFullName does only work with local paths (not compatible across platforms)
//                            wxString newname = fname.GetFullName();
                    fname.AssignDir(basedir);
                    fname.SetFullName(newname);
                    DEBUG_TRACE("filename with new path: " << fname.GetFullPath().mb_str(wxConvLocal));
                    if (fname.FileExists()) {
                        pano.setImageFilename(i, (const char *)fname.GetFullPath().mb_str(HUGIN_CONV_FILENAME));
                        DEBUG_TRACE("New filename set: " << fname.GetFullPath().mb_str(wxConvLocal));
                                // TODO - set pano dirty flag so that new paths are saved
                        continue;
                    }
                }

                wxMessageBox(wxString::Format(_("The project file \"%s\" refers to image \"%s\" which was not found.\nPlease manually select the correct image."), filename, fname.GetFullPath()), _("Image file not found"));

                if (basedir == wxT("")) {
                    basedir = fname.GetPath();
                }

                // open file dialog
                wxFileDialog dlg(wxGetActiveWindow(), wxString::Format(_("Select image %s"), fname.GetFullName()),
                                 basedir, fname.GetFullName(),
                                 HUGIN_WX_FILE_IMG_FILTER, wxFD_OPEN  | wxFD_FILE_MUST_EXIST | wxFD_PREVIEW, wxDefaultPosition);
                dlg.SetDirectory(basedir);
                if (dlg.ShowModal() == wxID_OK) {
                    pano.setImageFilename(i, (const char *)dlg.GetPath().mb_str(HUGIN_CONV_FILENAME));
                            // save used path
                    basedir = dlg.GetDirectory();
                    DEBUG_INFO("basedir is: " << basedir.mb_str(wxConvLocal));
                } else {
                    HuginBase::PanoramaMemento emptyPano;
                    pano.setMemento(emptyPano);
                            // set an empty panorama
                    return true;
                }
                fname.Assign(dlg.GetPath());
            }
            // check if image size is correct
            HuginBase::SrcPanoImage srcImg = pano.getSrcImage(i);
            //
            vigra::ImageImportInfo imginfo(srcImg.getFilename().c_str());
            if (srcImg.getSize() != imginfo.size()) {
                // adjust size properly.
                srcImg.resize(imginfo.size());
            }
            // check if script contains invalid HFOV
            double hfov = pano.getImage(i).getHFOV();
            if (pano.getImage(i).getProjection() == HuginBase::SrcPanoImage::RECTILINEAR
                && hfov >= 180 && autopanoSiftFile == false)
            {
                autopanoSiftFile = true;
                // something is wrong here, try to read from exif data (all images)
                bool ok = srcImg.readEXIF();
                if(ok) {
                    ok = srcImg.applyEXIFValues();
                };
                if (! ok) {
                    getLensDataFromUser(wxGetActiveWindow(), srcImg);
                }
                autopanoSiftRefImg = srcImg;
            }
            else 
            {
                // load exif data
                srcImg.readEXIF();
                if (autopanoSiftFile)
                {
                // need to copy the lens parameters from the first lens.
                    srcImg.setHFOV(autopanoSiftRefImg.getHFOV());
                };
            };
            // remember icc profile, only from first image
            if (i == 0)
            {
                pano.setNrOfBands(imginfo.numBands() - imginfo.numExtraBands());
                pano.setICCProfileDesc(hugin_utils::GetICCDesc(imginfo.getICCProfile()));
            }
            pano.setSrcImage(i, srcImg);
        }
        // Link image projection across each lens, since it is not saved.
        const HuginBase::UIntSetVector imgSetLens = lenses.getPartsSet();
        for (size_t i = 0; i < imgSetLens.size(); ++i)
        {
            const HuginBase::UIntSet imgLens = imgSetLens[i];
            if (imgLens.size()>1)
            {
                HuginBase::UIntSet::const_iterator it = imgLens.begin();
                const size_t img1 = *it;
                ++it;
                do
                {
                    pano.linkImageVariableProjection(img1, *it);
                    ++it;
                } while (it != imgLens.end());
            };
        };
    } else {
        DEBUG_ERROR("could not load panotools script");
    }
    in.close();

    // Verify control points are valid
    // loop through entire list of points, confirming they are inside the
    // bounding box of their images
    const HuginBase::CPVector & oldCPs = pano.getCtrlPoints();
    HuginBase::CPVector goodCPs;
    int bad_cp_count = 0;
    for (HuginBase::CPVector::const_iterator it = oldCPs.begin();
            it != oldCPs.end(); ++it)
    {
        HuginBase::ControlPoint point = *it;
        const HuginBase::SrcPanoImage & img1 = pano.getImage(point.image1Nr);
        const HuginBase::SrcPanoImage & img2 = pano.getImage(point.image2Nr);
        if (0 > point.x1 || point.x1 > img1.getSize().x ||
            0 > point.y1 || point.y1 > img1.getSize().y ||
            0 > point.x2 || point.x2 > img2.getSize().x ||
            0 > point.y2 || point.y2 > img2.getSize().y)
        {
            bad_cp_count++;
        } else
        {
            goodCPs.push_back(point);
        }
    }

    if (bad_cp_count > 0)
    {
        wxString errMsg = wxString::Format(_("%d invalid control point(s) found.\n\nPress OK to remove."), bad_cp_count);
        wxMessageBox(errMsg, _("Error Detected"), wxICON_ERROR);
        pano.setCtrlPoints(goodCPs);
    }

    // check stacks and warn users in case
    CheckLensStacks(&pano, false);
    // Update control point error values
    HuginBase::PTools::calcCtrlPointErrors(pano);
    if(markAsOptimized)
    {
        pano.markAsOptimized();
    };
    return true;
}

bool wxNewProjectCmd::processPanorama(HuginBase::Panorama& pano)
{
    pano.reset();

    // Setup pano with options from preferences
    HuginBase::PanoramaOptions opts = pano.getOptions();
    wxConfigBase* config = wxConfigBase::Get();
    opts.quality = config->Read(wxT("/output/jpeg_quality"),HUGIN_JPEG_QUALITY);
    switch(config->Read(wxT("/output/tiff_compression"), HUGIN_TIFF_COMPRESSION))
    {
        case 0:
        default:
            opts.outputImageTypeCompression = "NONE";
            opts.tiffCompression = "NONE";
            break;
        case 1:
            opts.outputImageTypeCompression = "PACKBITS";
            opts.tiffCompression = "PACKBITS";
            break;
        case 2:
            opts.outputImageTypeCompression = "LZW";
            opts.tiffCompression = "LZW";
            break;
        case 3:
            opts.outputImageTypeCompression = "DEFLATE";
            opts.tiffCompression = "DEFLATE";
            break;
    }
    switch (config->Read(wxT("/output/ldr_format"), HUGIN_LDR_OUTPUT_FORMAT)) {
    case 1:
        opts.outputImageType ="jpg";
        break;
    case 2:
        opts.outputImageType ="png";
        break;
    case 3:
        opts.outputImageType ="exr";
        break;
    default:
    case 0:
        opts.outputImageType ="tif";
        break;
    }
    // HDR disabled because there is no real choice at the moment:  HDR TIFF is broken and there is only EXR
    // opts.outputImageTypeHDR = config->Read(wxT("/output/hdr_format"), HUGIN_HDR_OUTPUT_FORMAT);
    opts.outputFormat = HuginBase::PanoramaOptions::TIFF_m;
    opts.blendMode = static_cast<HuginBase::PanoramaOptions::BlendingMechanism>(config->Read(wxT("/default_blender"), HUGIN_DEFAULT_BLENDER));
    opts.enblendOptions = config->Read(wxT("Enblend/Args"),wxT(HUGIN_ENBLEND_ARGS)).mb_str(wxConvLocal);
    opts.enfuseOptions = config->Read(wxT("Enfuse/Args"),wxT(HUGIN_ENFUSE_ARGS)).mb_str(wxConvLocal);
    opts.verdandiOptions = config->Read(wxT("/VerdandiDefaultArgs"), wxEmptyString).mb_str(wxConvLocal);
    opts.interpolator = (vigra_ext::Interpolator)config->Read(wxT("Nona/Interpolator"),HUGIN_NONA_INTERPOLATOR);
    opts.remapUsingGPU = config->Read(wxT("Nona/useGPU"),HUGIN_NONA_USEGPU)!=0;
    opts.tiff_saveROI = config->Read(wxT("Nona/CroppedImages"),HUGIN_NONA_CROPPEDIMAGES)!=0;
    opts.hdrMergeMode = HuginBase::PanoramaOptions::HDRMERGE_AVERAGE;
    opts.hdrmergeOptions = HUGIN_HDRMERGE_ARGS;
    pano.setOptions(opts);

    pano.setOptimizerSwitch(HuginBase::OPT_PAIR);
    pano.setPhotometricOptimizerSwitch(HuginBase::OPT_EXPOSURE | HuginBase::OPT_VIGNETTING | HuginBase::OPT_RESPONSE);
    return true;
}


bool wxApplyTemplateCmd::processPanorama(HuginBase::Panorama& pano)
{
    wxConfigBase* config = wxConfigBase::Get();

    if (pano.getNrOfImages() == 0) {
        // TODO: prompt for images!
        wxString path = config->Read(wxT("actualPath"), wxT(""));
        wxFileDialog dlg(wxGetActiveWindow(), _("Add images"),
                path, wxT(""),
                HUGIN_WX_FILE_IMG_FILTER, wxFD_OPEN|wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST | wxFD_PREVIEW , wxDefaultPosition);
        dlg.SetDirectory(path);

        // remember the image extension
        wxString img_ext;
        if (config->HasEntry(wxT("lastImageType"))){
            img_ext = config->Read(wxT("lastImageType")).c_str();
        }
        if (img_ext == wxT("all images"))
            dlg.SetFilterIndex(0);
        else if (img_ext == wxT("jpg"))
            dlg.SetFilterIndex(1);
        else if (img_ext == wxT("tiff"))
            dlg.SetFilterIndex(2);
        else if (img_ext == wxT("png"))
            dlg.SetFilterIndex(3);
        else if (img_ext == wxT("hdr"))
            dlg.SetFilterIndex(4);
        else if (img_ext == wxT("exr"))
            dlg.SetFilterIndex(5);
        else if (img_ext == wxT("all files"))
            dlg.SetFilterIndex(6);
        DEBUG_INFO ( "Image extention: " << img_ext.mb_str(wxConvLocal) );

        // call the file dialog
        if (dlg.ShowModal() == wxID_OK) {
            // get the selections
            wxArrayString Pathnames;
            dlg.GetPaths(Pathnames);

            // save the current path to config
#ifdef __WXGTK__
            //workaround a bug in GTK, see https://bugzilla.redhat.com/show_bug.cgi?id=849692 and http://trac.wxwidgets.org/ticket/14525
            config->Write(wxT("/actualPath"), wxPathOnly(Pathnames[0]));
#else
            config->Write(wxT("/actualPath"), dlg.GetDirectory());
#endif
            DEBUG_INFO ( wxString::Format(wxT("img_ext: %d"), dlg.GetFilterIndex()).mb_str(wxConvLocal) );
            // save the image extension
            switch ( dlg.GetFilterIndex() ) {
                case 0: config->Write(wxT("lastImageType"), wxT("all images")); break;
                case 1: config->Write(wxT("lastImageType"), wxT("jpg")); break;
                case 2: config->Write(wxT("lastImageType"), wxT("tiff")); break;
                case 3: config->Write(wxT("lastImageType"), wxT("png")); break;
                case 4: config->Write(wxT("lastImageType"), wxT("hdr")); break;
                case 5: config->Write(wxT("lastImageType"), wxT("exr")); break;
                case 6: config->Write(wxT("lastImageType"), wxT("all files")); break;
            }

            HuginBase::StandardImageVariableGroups variable_groups(pano);
            HuginBase::ImageVariableGroup & lenses = variable_groups.getLenses();
            // add images.
            for (unsigned int i=0; i< Pathnames.GetCount(); i++) {
                std::string filename = (const char *)Pathnames[i].mb_str(HUGIN_CONV_FILENAME);
                vigra::ImageImportInfo inf(filename.c_str());
                HuginBase::SrcPanoImage img;
                img.setFilename(filename);
                img.setSize(inf.size());
                img.readEXIF();
                img.applyEXIFValues();
                int imgNr = pano.addImage(img);
                lenses.updatePartNumbers();
                if (i > 0) lenses.switchParts(imgNr, 0);
            }

        }
    }

    unsigned int nOldImg = pano.getNrOfImages();
    HuginBase::PanoramaMemento newPanoMem;

    int ptoVersion = 0;
    if (newPanoMem.loadPTScript(in, ptoVersion, "")) {
        HuginBase::Panorama newPano;
        newPano.setMemento(newPanoMem);

        unsigned int nNewImg = newPano.getNrOfImages();
        if (nOldImg != nNewImg) {
            wxString errMsg = wxString::Format(_("Error, template expects %d images,\ncurrent project contains %d images\n"), nNewImg, nOldImg);
            wxMessageBox(errMsg, _("Could not apply template"), wxICON_ERROR);
            return false;
        }

        // check image sizes, and correct parameters if required.
        for (unsigned int i = 0; i < nNewImg; i++) {

            // check if image size is correct
            const HuginBase::SrcPanoImage & oldSrcImg = pano.getImage(i);
            HuginBase::SrcPanoImage newSrcImg = newPano.getSrcImage(i);

            // just keep the file name
            DEBUG_DEBUG("apply template fn:" <<  newSrcImg.getFilename() << " real fn: " << oldSrcImg.getFilename());
            newSrcImg.setFilename(oldSrcImg.getFilename());
            if (oldSrcImg.getSize() != newSrcImg.getSize()) {
                // adjust size properly.
                newSrcImg.resize(oldSrcImg.getSize());
            }
            newPano.setSrcImage(i, newSrcImg);
        }
        // keep old control points.
        newPano.setCtrlPoints(pano.getCtrlPoints());
        newPanoMem = newPano.getMemento();
        pano.setMemento(newPanoMem);
    } else {
        wxMessageBox(_("Error loading project file"), _("Could not apply template"), wxICON_ERROR);
    }
    return true;
}

#ifdef HUGIN_HSI
bool PythonScriptPanoCmd::processPanorama(HuginBase::Panorama& pano)
{
    std::cout << "run python script: " << m_scriptFile.c_str() << std::endl;

    int success = hpi::callhpi ( m_scriptFile.c_str() , 1 ,
                   "HuginBase::Panorama*" , &pano ) ;

    if(success!=0)
        wxMessageBox(wxString::Format(wxT("Script returned %d"),success),_("Result"), wxICON_INFORMATION);
    std::cout << "Python interface returned " << success << endl ;
    // notify other of change in panorama
    if(pano.getNrOfImages()>0)
    {
        for(unsigned int i=0;i<pano.getNrOfImages();i++)
        {
            pano.imageChanged(i);
        };
    };

    return true;
}
#endif

} // namespace

