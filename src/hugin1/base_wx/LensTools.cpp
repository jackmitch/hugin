// -*- c-basic-offset: 4 -*-
/** @file LensTools.cpp
 *
 *  @brief some helper classes for working with lenses
 *
 *  @author T. Modes
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

#include "panoinc_WX.h"
#include <wx/msgdlg.h>
#include "panoinc.h"
#include "LensTools.h"
#include <algorithm>
#include "panodata/ImageVariableTranslate.h"
#include "panodata/StandardImageVariableGroups.h"
#include "base_wx/PanoCommand.h"

void FillLensProjectionList(wxControlWithItems* list)
{
    list->Clear();
    list->Append(_("Normal (rectilinear)"),(void*)HuginBase::SrcPanoImage::RECTILINEAR);
    list->Append(_("Panoramic (cylindrical)"),(void*)HuginBase::SrcPanoImage::PANORAMIC);
    list->Append(_("Circular fisheye"),(void*)HuginBase::SrcPanoImage::CIRCULAR_FISHEYE);
    list->Append(_("Full frame fisheye"),(void*)HuginBase::SrcPanoImage::FULL_FRAME_FISHEYE);
    list->Append(_("Equirectangular"),(void*)HuginBase::SrcPanoImage::EQUIRECTANGULAR);
    list->Append(_("Orthographic"),(void*)HuginBase::SrcPanoImage::FISHEYE_ORTHOGRAPHIC);
    list->Append(_("Stereographic"),(void*)HuginBase::SrcPanoImage::FISHEYE_STEREOGRAPHIC);
    list->Append(_("Equisolid"),(void*)HuginBase::SrcPanoImage::FISHEYE_EQUISOLID);
    list->Append(_("Fisheye Thoby"),(void*)HuginBase::SrcPanoImage::FISHEYE_THOBY);
    list->SetSelection(0);
};

void FillBlenderList(wxControlWithItems* list)
{
    list->Clear();
    list->Append(_("enblend"), (void*)HuginBase::PanoramaOptions::ENBLEND_BLEND);
    list->Append(_("builtin"), (void*)HuginBase::PanoramaOptions::INTERNAL_BLEND);
    list->SetSelection(0);
};

void SelectListValue(wxControlWithItems* list, size_t newValue)
{
    for(unsigned int i=0;i<list->GetCount();i++)
    {
        if((size_t)list->GetClientData(i)==newValue)
        {
            list->SetSelection(i);
            return;
        };
    };
    list->SetSelection(0);
};

size_t GetSelectedValue(wxControlWithItems* list)
{
    return (size_t)(list->GetClientData(list->GetSelection()));
};

wxString getProjectionString(const HuginBase::SrcPanoImage& img)
{
    wxString ps;
    switch (img.getProjection())
    {
    case HuginBase::SrcPanoImage::RECTILINEAR:          ps << _("Normal (rectilinear)"); break;
    case HuginBase::SrcPanoImage::PANORAMIC:            ps << _("Panoramic (cylindrical)"); break;
    case HuginBase::SrcPanoImage::CIRCULAR_FISHEYE:     ps << _("Circular fisheye"); break;
    case HuginBase::SrcPanoImage::FULL_FRAME_FISHEYE:   ps << _("Full frame fisheye"); break;
    case HuginBase::SrcPanoImage::EQUIRECTANGULAR:      ps << _("Equirectangular"); break;
    case HuginBase::SrcPanoImage::FISHEYE_ORTHOGRAPHIC: ps << _("Orthographic"); break;
    case HuginBase::SrcPanoImage::FISHEYE_STEREOGRAPHIC:ps << _("Stereographic"); break;
    case HuginBase::SrcPanoImage::FISHEYE_EQUISOLID:    ps << _("Equisolid"); break;
    case HuginBase::SrcPanoImage::FISHEYE_THOBY:        ps << _("Fisheye Thoby"); break;
    }
    return ps;
};

wxString getResponseString(const HuginBase::SrcPanoImage& img)
{
    wxString s;
    switch (img.getResponseType())
    {
    case HuginBase::BaseSrcPanoImage::RESPONSE_EMOR:
        s = _("custom (EMoR)");
        break;
    case HuginBase::BaseSrcPanoImage::RESPONSE_LINEAR:
        s = _("Linear");
        break;
    default:
        s = wxEmptyString;
        break;
    };
    return s;
};

void SaveLensParameters(const wxString filename, HuginBase::Panorama* pano, unsigned int imgNr)
{
    HuginBase::StandardImageVariableGroups variable_groups(*pano);
    const HuginBase::Lens & lens = variable_groups.getLensForImage(imgNr);
    const HuginBase::VariableMap & vars = pano->getImageVariables(imgNr);
    // get the variable map
    char * p = setlocale(LC_NUMERIC,NULL);
    char * old_locale = strdup(p);
    setlocale(LC_NUMERIC,"C");
    wxFileConfig cfg(wxT("hugin lens file"),wxT(""),filename);
    cfg.Write(wxT("Lens/image_width"), (long) lens.getImageSize().x);
    cfg.Write(wxT("Lens/image_height"), (long) lens.getImageSize().y);
    cfg.Write(wxT("Lens/type"), (long) lens.getProjection());
    cfg.Write(wxT("Lens/hfov"), const_map_get(vars,"v").getValue());
    cfg.Write(wxT("Lens/hfov_link"), const_map_get(lens.variables,"v").isLinked() ? 1:0);
    cfg.Write(wxT("Lens/crop"), lens.getCropFactor());

    // loop to save lens variables
    const char ** varname = HuginBase::Lens::variableNames;
    while (*varname)
    {
        //ignore exposure value and hfov, hfov is separately handled by the code above
        if (std::string(*varname) == "Eev" || std::string(*varname) == "v")
        {
            varname++;
            continue;
        }
        wxString key(wxT("Lens/"));
        key.append(wxString(*varname, wxConvLocal));
        cfg.Write(key, const_map_get(vars,*varname).getValue());
        key.append(wxT("_link"));
        cfg.Write(key, const_map_get(lens.variables,*varname).isLinked() ? 1:0);
        varname++;
    }

    const HuginBase::SrcPanoImage & image = pano->getImage(imgNr);
    cfg.Write(wxT("Lens/crop/enabled"), image.getCropMode()==HuginBase::SrcPanoImage::NO_CROP ? 0l : 1l);
    cfg.Write(wxT("Lens/crop/autoCenter"), image.getAutoCenterCrop() ? 1l : 0l);
    const vigra::Rect2D cropRect=image.getCropRect();
    cfg.Write(wxT("Lens/crop/left"), cropRect.left());
    cfg.Write(wxT("Lens/crop/top"), cropRect.top());
    cfg.Write(wxT("Lens/crop/right"), cropRect.right());
    cfg.Write(wxT("Lens/crop/bottom"), cropRect.bottom());

    if (!image.getExifMake().empty() && !image.getExifModel().empty() && image.getExifFocalLength()>0)
    {
        // write exif data to ini file
        cfg.Write(wxT("EXIF/CameraMake"),  wxString(image.getExifMake().c_str(), wxConvLocal));
        cfg.Write(wxT("EXIF/CameraModel"), wxString(image.getExifModel().c_str(), wxConvLocal));
        cfg.Write(wxT("EXIF/FocalLength"), image.getExifFocalLength());
        cfg.Write(wxT("EXIF/Aperture"), image.getExifAperture());
        cfg.Write(wxT("EXIF/ISO"), image.getExifISO());
        cfg.Write(wxT("EXIF/CropFactor"), image.getCropFactor()); 
        cfg.Write(wxT("EXIF/Distance"), image.getExifDistance()); 
    }
    cfg.Flush();

    // reset locale
    setlocale(LC_NUMERIC,old_locale);
    free(old_locale);
};

bool ApplyLensParameters(wxWindow * parent, HuginBase::Panorama *pano, HuginBase::UIntSet images, PanoCommand::PanoCommand*& cmd)
{
    HuginBase::StandardImageVariableGroups variable_groups(*pano);
    HuginBase::Lens lens=variable_groups.getLensForImage(*images.begin());
    bool cropped=false;
    bool autoCenterCrop=false;
    vigra::Rect2D cropRect;
    if (LoadLensParametersChoose(parent, lens, cropped, autoCenterCrop, cropRect))
    {
        // Merge the lens parameters with the image variable map.
        HuginBase::VariableMapVector vars(images.size());
        size_t i=0;
        for(HuginBase::UIntSet::const_iterator it=images.begin();it!=images.end();++it,++i)
        {
            vars[i]=pano->getImageVariables(*it);
            for (HuginBase::LensVarMap::iterator it2 = lens.variables.begin(); it2 != lens.variables.end(); ++it2)
            {
                if(it2->second.getName()=="EeV")
                {
                    continue;
                };
                vars[i].find(it2->first)->second.setValue(it2->second.getValue());
            }
        };

        /** @todo I think the sensor size should be copied over,
            * but SrcPanoImage doesn't have such a variable yet.
            */
        std::vector<PanoCommand::PanoCommand*> cmds;
        // update links
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> linkedVariables;
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> unlinkedVariables;
        for (HuginBase::LensVarMap::iterator it = lens.variables.begin(); it != lens.variables.end(); ++it)
        {
            if(it->second.getName()=="EeV")
            {
                continue;
            };
#define image_variable( name, type, default_value ) \
            if (HuginBase::PTOVariableConverterFor##name::checkApplicability(it->second.getName()))\
            {\
                if(it->second.isLinked())\
                    linkedVariables.insert(HuginBase::ImageVariableGroup::IVE_##name);\
                else\
                    unlinkedVariables.insert(HuginBase::ImageVariableGroup::IVE_##name);\
            }
#include "panodata/image_variables.h"
#undef image_variable
        }
        if (!linkedVariables.empty())
        {
            cmds.push_back(new PanoCommand::ChangePartImagesLinkingCmd(*pano, images, linkedVariables,
                true,HuginBase::StandardImageVariableGroups::getLensVariables()));
        }
        if (!unlinkedVariables.empty())
        {
            cmds.push_back(new PanoCommand::ChangePartImagesLinkingCmd(*pano, images, unlinkedVariables,
                false,HuginBase::StandardImageVariableGroups::getLensVariables()));
        }
        //update lens parameters
        cmds.push_back(new PanoCommand::UpdateImagesVariablesCmd(*pano, images, vars));

        // Set the lens projection type.
        cmds.push_back(new PanoCommand::ChangeImageProjectionCmd(*pano, images, (HuginBase::SrcPanoImage::Projection) lens.getProjection()));
        // update crop factor
        cmds.push_back(new PanoCommand::ChangeImageCropFactorCmd(*pano,images,lens.getCropFactor()));
        // update the crop rect
        cmds.push_back(new PanoCommand::ChangeImageAutoCenterCropCmd(*pano,images,autoCenterCrop));
        if(cropped)
        {
            cmds.push_back(new PanoCommand::ChangeImageCropRectCmd(*pano,images,cropRect));
        }
        else
        {
            cmds.push_back(new PanoCommand::ChangeImageCropModeCmd(*pano,images,HuginBase::BaseSrcPanoImage::NO_CROP));
        };
        cmd=new PanoCommand::CombinedPanoCommand(*pano, cmds);
        return true;
    }
    else
    {
        return false;
    };
};

bool LoadLensParametersChoose(wxWindow * parent, HuginBase::Lens & lens, 
     bool & cropped, bool & autoCenterCrop, vigra::Rect2D & cropRect)
{
    wxString fname;
    wxFileDialog dlg(parent,
                        _("Load lens parameters"),
                        wxConfigBase::Get()->Read(wxT("/lensPath"),wxT("")), wxT(""),
                        _("Lens Project Files (*.ini)|*.ini|All files (*.*)|*.*"),
                        wxFD_OPEN, wxDefaultPosition);
    dlg.SetDirectory(wxConfigBase::Get()->Read(wxT("/lensPath"),wxT("")));
    if (dlg.ShowModal() == wxID_OK)
    {
        fname = dlg.GetPath();
        wxConfig::Get()->Write(wxT("/lensPath"), dlg.GetDirectory());  // remember for later
        // read with with standart C numeric format
        char * p = setlocale(LC_NUMERIC,NULL);
        char * old_locale = strdup(p);
        setlocale(LC_NUMERIC,"C");
        {
            wxFileConfig cfg(wxT("hugin lens file"),wxT(""),fname);
            long w=0;
            cfg.Read(wxT("Lens/image_width"), &w);
            long h=0;
            cfg.Read(wxT("Lens/image_height"), &h);
            if (w>0 && h>0) {
                vigra::Size2D sz = lens.getImageSize();
                if (w != sz.x || h != sz.y) {
                    std::cerr << "Image size: " << sz << " size in lens parameter file: " << w << "x" << h << std::endl;
                    int ret = wxMessageBox(_("Incompatible lens parameter file, image sizes do not match\nApply settings anyway?"), _("Error loading lens parameters"), wxICON_QUESTION |wxYES_NO);
                    if (ret == wxNO) {
                        setlocale(LC_NUMERIC,old_locale);
                        free(old_locale);
                        return false;
                    }
                }
            } else {
                // lens ini file didn't store the image size,
                // assume everything is all right.
            }
            long integer=0;
            if(cfg.Read(wxT("Lens/type"), &integer))
            {
                lens.setProjection((HuginBase::Lens::LensProjectionFormat) integer);
            };
            double d=1;
            if(cfg.Read(wxT("Lens/crop"), &d))
            {
                lens.setCropFactor(d);
            };
            //special treatment for hfov, we are reading hfov and hfov_linked instead of v and v_linked
            d=50;
            if(cfg.Read(wxT("Lens/hfov"), &d))
            {
                map_get(lens.variables,"v").setValue(d);
            };
            integer=1;
            if(cfg.Read(wxT("Lens/hfov_linked"), &integer))
            {
                map_get(lens.variables,"v").setLinked(integer != 0);
            };
            DEBUG_DEBUG("read lens hfov: " << d);

            // loop to load lens variables
            const char ** varname = HuginBase::Lens::variableNames;
            while (*varname) {
                if (std::string(*varname) == "Eev")
                {
                    varname++;
                    continue;
                }
                wxString key(wxT("Lens/"));
                key.append(wxString(*varname, wxConvLocal));
                d = 0;
                if (cfg.Read(key,&d))
                {
                    // only set value if variabe was found in the script
                    map_get(lens.variables, *varname).setValue(d);
                    integer = 1;
                    key.append(wxT("_link"));
                    if(cfg.Read(key, &integer))
                    {
                        map_get(lens.variables, *varname).setLinked(integer != 0);
                    };
                }
                varname++;
            }

            // crop parameters
            long v=0;
            cfg.Read(wxT("Lens/crop/enabled"), &v);
            cropped=(v!=0);
            if(cropped)
            {
                long left=0;
                long top=0;
                long right=0;
                long bottom=0;
                if(cfg.Read(wxT("Lens/crop/left"), &left) && cfg.Read(wxT("Lens/crop/top"), &top) &&
                    cfg.Read(wxT("Lens/crop/right"), &right) && cfg.Read(wxT("Lens/crop/bottom"), &bottom))
                {
                    cropped=true;
                    cropRect=vigra::Rect2D(left,top,right,bottom);
                }
                else
                {
                    cropped=false;
                };
            };
            v=1;
            if(cfg.Read(wxT("Lens/crop/autoCenter"), &v))
            {
                autoCenterCrop=(v!=0);
            };
        }
        // reset locale
        setlocale(LC_NUMERIC,old_locale);
        free(old_locale);
        return true;
    }
    else
    {
        return false;
    };
};

void SaveLensParametersToIni(wxWindow * parent, HuginBase::Panorama *pano, const HuginBase::UIntSet images)
{
    if (images.size() == 1)
    {
        unsigned int imgNr = *(images.begin());
        wxFileDialog dlg(parent,
                         _("Save lens parameters file"),
                         wxConfigBase::Get()->Read(wxT("/lensPath"),wxT("")), wxT(""),
                         _("Lens Project Files (*.ini)|*.ini|All files (*)|*"),
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);
        dlg.SetDirectory(wxConfigBase::Get()->Read(wxT("/lensPath"),wxT("")));
        if (dlg.ShowModal() == wxID_OK)
        {
            wxFileName filename(dlg.GetPath());
            if(!filename.HasExt())
            {
                filename.SetExt(wxT("ini"));
                if (filename.Exists())
                {
                    int d = wxMessageBox(wxString::Format(_("File %s exists. Overwrite?"), filename.GetFullPath().c_str()),
                        _("Save project"), wxYES_NO | wxICON_QUESTION);
                    if (d != wxYES) {
                        return;
                    }
                }
            }
            wxConfig::Get()->Write(wxT("/lensPath"), dlg.GetDirectory());  // remember for later
            SaveLensParameters(filename.GetFullPath(),pano,imgNr);
        }
    }
    else 
    {
        wxLogError(_("Please select an image and try again"));
    }
}

bool CheckLensStacks(HuginBase::Panorama* pano, bool allowCancel)
{
    if (pano->getNrOfImages() < 2)
    {
        return true;
    };
    const size_t nrImages = pano->getNrOfImages();
    bool stacksCorrectLinked = true;
    for (size_t i = 0; i < nrImages - 1; ++i)
    {
        const HuginBase::SrcPanoImage& image1 = pano->getImage(i);
        if (image1.YawisLinked())
        {
            for (size_t j = i + 1; j < nrImages && stacksCorrectLinked; ++j)
            {
                const HuginBase::SrcPanoImage& image2 = pano->getImage(j);
                if (image1.YawisLinkedWith(image2))
                {
                    stacksCorrectLinked = stacksCorrectLinked &
                        image1.HFOVisLinkedWith(image2) &
                        image1.RadialDistortionisLinkedWith(image2) &
                        image1.RadialDistortionCenterShiftisLinkedWith(image2) &
                        image1.ShearisLinkedWith(image2);
                };
            };
        };
    };
    if (stacksCorrectLinked)
    {
        return true;
    }
    else
    {
        int flags = wxICON_EXCLAMATION | wxOK;
        if (allowCancel)
        {
            flags = flags | wxCANCEL;
        };
        if (wxMessageBox(_("This project contains stacks with linked positions. But the lens parameters are not linked for these images.\nThis will result in unwanted results.\nPlease check and correct this before proceeding."),
#ifdef _WIN32
            _("Hugin"),
#else
            wxT(""),
#endif
            flags)==wxOK)
        {
            return true;
        }
        else
        {
            return false;
        };
    };
};

