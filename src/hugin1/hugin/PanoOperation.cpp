// -*- c-basic-offset: 4 -*-

/** @file PanoOperation.cpp
 *
 *  @brief Implementation of PanoOperation class
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

#include "hugin/PanoOperation.h"
#include "hugin/config_defaults.h"
#include "base_wx/PanoCommand.h"
#include "base_wx/wxPanoCommand.h"
#include "huginapp/ImageCache.h"
#include "base_wx/MyProgressDialog.h"
#include "base_wx/PTWXDlg.h"
#include "algorithms/optimizer/ImageGraph.h"
#include "algorithms/control_points/CleanCP.h"
#include "celeste/Celeste.h"
#include <exiv2/exif.hpp>
#include <exiv2/image.hpp>
#include "base_wx/LensTools.h"
#include "base_wx/wxLensDB.h"
#include "hugin/ResetDialog.h"
#include "hugin/ChangeImageVariableDialog.h"
#include "hugin/MainFrame.h"
#include <vigra_ext/openmp_vigra.h>
#include <vigra_ext/cms.h>

namespace PanoOperation
{

wxString PanoOperation::GetLabel()
{
    return wxEmptyString;
};

bool PanoOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    return true;
};

PanoCommand::PanoCommand* PanoOperation::GetCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    //remember gui level, only used by some PanoOperation's
    m_guiLevel=guiLevel;
    if(IsEnabled(pano, images, m_guiLevel))
    {
        return GetInternalCommand(parent,pano,images);
    }
    else
    {
        return NULL;
    };
};

bool PanoSingleImageOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    return images.size()==1;
};

bool PanoMultiImageOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    return !images.empty();
};

/** small function to show add image dialog
  * @param parent pointer to window, for showing dialog
  * @param files vector, to which the selected valid filenames will be added
  * @returns true, if a valid image was selected, otherwise false
  */
bool AddImageDialog(wxWindow* parent, std::vector<std::string>& files)
{
    // get stored path
    wxConfigBase* config = wxConfigBase::Get();
    wxString path = config->Read(wxT("/actualPath"), wxT(""));
    wxFileDialog dlg(parent,_("Add images"),
                     path, wxT(""),
                     HUGIN_WX_FILE_IMG_FILTER,
                     wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST | wxFD_PREVIEW, wxDefaultPosition);
    dlg.SetDirectory(path);

    // remember the image extension
    wxString img_ext;
    if (config->HasEntry(wxT("lastImageType")))
    {
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

    // call the file dialog
    if (dlg.ShowModal() == wxID_OK)
    {
        // get the selections
        wxArrayString Pathnames;
        dlg.GetPaths(Pathnames);

        // remember path for later
#ifdef __WXGTK__
        //workaround a bug in GTK, see https://bugzilla.redhat.com/show_bug.cgi?id=849692 and http://trac.wxwidgets.org/ticket/14525
        config->Write(wxT("/actualPath"), wxPathOnly(Pathnames[0]));
#else
        config->Write(wxT("/actualPath"), dlg.GetDirectory());
#endif
        // save the image extension
        switch (dlg.GetFilterIndex())
        {
            case 0: config->Write(wxT("lastImageType"), wxT("all images")); break;
            case 1: config->Write(wxT("lastImageType"), wxT("jpg")); break;
            case 2: config->Write(wxT("lastImageType"), wxT("tiff")); break;
            case 3: config->Write(wxT("lastImageType"), wxT("png")); break;
            case 4: config->Write(wxT("lastImageType"), wxT("hdr")); break;
            case 5: config->Write(wxT("lastImageType"), wxT("exr")); break;
            case 6: config->Write(wxT("lastImageType"), wxT("all files")); break;
        }

        //check for forbidden/non working chars
        wxArrayString invalidFiles;
        for(unsigned int i=0;i<Pathnames.GetCount(); i++)
        {
           if(containsInvalidCharacters(Pathnames[i]))
           {
               invalidFiles.Add(Pathnames[i]);
           };
        };
        if(invalidFiles.size()>0)
        {
            ShowFilenameWarning(parent, invalidFiles);
            return false;
        }
        for (unsigned int i=0; i<Pathnames.GetCount(); i++)
        {
            files.push_back((const char *)Pathnames[i].mb_str(HUGIN_CONV_FILENAME));
        };
        return true;
    };
    return false;
};

wxString AddImageOperation::GetLabel()
{
    return _("Add individual images...");
};

PanoCommand::PanoCommand* AddImageOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    std::vector<std::string> files;
    if(AddImageDialog(parent, files))
    {
        if(files.size()>0)
        {
            return new PanoCommand::wxAddImagesCmd(pano,files);
        };
    };
    return NULL;
};

WX_DECLARE_STRING_HASH_MAP(time_t, StringToPointerHash);
WX_DECLARE_STRING_HASH_MAP(int, StringToFlagHash);

time_t ReadExifTime(const char* filename)
{
    Exiv2::Image::AutoPtr image;
    try
    {
        image = Exiv2::ImageFactory::open(filename);
    }
    catch(...)
    {
        return 0;
    }
    if (image.get() == 0)
    {
        return 0;
    }

    image->readMetadata();
    Exiv2::ExifData &exifData = image->exifData();
    if (exifData.empty())
    {
        return 0;
    }

    Exiv2::Exifdatum& tag = exifData["Exif.Image.DateTime"];
    const std::string date_time = tag.toString();

    // Remember the file and a shutter timestamp.
    struct tm when;
    memset(&when, 0, sizeof(when));
    when.tm_wday = -1;

    // parse into the tm_structure
    const int a = sscanf(date_time.c_str(), "%d:%d:%d %d:%d:%d",
            &when.tm_year, &when.tm_mon, &when.tm_mday,
            &when.tm_hour, &when.tm_min, &when.tm_sec);

    if (a == 6)
    {
        when.tm_isdst = -1;
        when.tm_mon -= 1;      // Adjust for unix zero-based months
        when.tm_year -= 1900;  // Adjust for year starting at 1900
    }
    else
    {
        // Not in EXIF format
        return 0;
    }

    time_t stamp;
    stamp = mktime(&when);
    if (stamp == (time_t)(-1))
        return 0;

    return stamp;
}

struct sortbytime
{
    explicit sortbytime(std::map<std::string, time_t> & h) : m_time(h) {};
    bool operator()(const std::string & s1, const std::string & s2)
    {
        time_t t1 = m_time[s1];
        time_t t2 = m_time[s2];
        return t1 < t2;
    };
    std::map<std::string, time_t> & m_time;
};

wxString AddImagesSeriesOperation::GetLabel()
{
    return _("Add time-series of images...");
};

PanoCommand::PanoCommand* AddImagesSeriesOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    //load image if pano contains no images
    std::vector<std::string> files;
    if(pano.getNrOfImages()==0)
    {
        if(!AddImageDialog(parent,files))
        {
            return NULL;
        };
        //just in case
        if(files.size()==0)
        {
            return NULL;
        };
    }
    else
    {
        for(size_t i=0;i<pano.getNrOfImages();i++)
        {
            files.push_back(pano.getImage(i).getFilename());
        };
    };

    DEBUG_TRACE("seeking similarly timed images");

    // Collect potential image-mates.
    StringToPointerHash filenames;
    StringToFlagHash preloaded;
    for(size_t i=0;i<files.size();i++)
    {
        wxString file(files[i].c_str(), HUGIN_CONV_FILENAME);
        preloaded[file] = 1;

        // Glob for all files of same type in same directory.
        wxString path = ::wxPathOnly(file) + wxT("/*");
        file = ::wxFindFirstFile(path);
        while (!file.IsEmpty())
        {
            // Associated with a NULL dummy timestamp for now.
            if(vigra::isImage(files[i].c_str()))
            {
                filenames[file] = 0;
            };
            file = ::wxFindNextFile();
        }
    }

    DEBUG_INFO("found " << filenames.size() << " candidate files to search.");

    // For each globbed or loaded file,
    StringToPointerHash::iterator found;
    std::map<std::string, time_t> timeMap;
    for (found = filenames.begin(); found != filenames.end(); ++found)
    {
        wxString file = found->first;
        // Check the time if it's got a camera EXIF timestamp.
        time_t stamp = ReadExifTime(file.mb_str(HUGIN_CONV_FILENAME));
        if (stamp)
        {
            filenames[file] = stamp;
            timeMap[(const char *)file.mb_str(HUGIN_CONV_FILENAME)] = stamp;
        }
    }

    //TODO: sorting the filenames keys by timestamp would be useful
    int maxtimediff = wxConfigBase::Get()->Read(wxT("CaptureTimeSpan"), HUGIN_CAPTURE_TIMESPAN);
    // For each timestamped file,
    for (found = filenames.begin(); found != filenames.end(); ++found)
    {
        wxString recruit = found->first;
        if (preloaded[recruit] == 1)
            continue;
        time_t pledge = filenames[recruit];
        if (!pledge)
            continue;

        // For each other image already loaded,
        for(size_t i=0;i<files.size();i++)
        {
            wxString file(files[i].c_str(), HUGIN_CONV_FILENAME);
            if (file == recruit)
                continue;

            // If it is within threshold time,
            time_t stamp = filenames[file];
            if (abs((int)(pledge - stamp)) < maxtimediff)
            {
                // Load this file, and remember it.
                DEBUG_TRACE("Recruited " << recruit.mb_str(wxConvLocal));
                std::string file = (const char *)recruit.mb_str(HUGIN_CONV_FILENAME);
                files.push_back(file);
                // Don't recruit it again.
                filenames[recruit] = 0;
                break;
            }
        }
    }

    if(files.size()>0)
    {
        // sort files by date
        sortbytime spred(timeMap);
        sort(files.begin(), files.end(), spred);
        // Load all of the named files.
        return new PanoCommand::wxAddImagesCmd(pano,files);
    }
    else
    {
        wxMessageBox(
            _("No matching images found."),
#ifdef _WIN32
            _("Hugin"),
#else
            wxT(""),
#endif
            wxOK | wxICON_INFORMATION, parent);
        return NULL;
    };
};

wxString ImageVariablesExpressionOperation::GetLabel()
{
    return _("Manipulate image variables...");
}

PanoCommand::PanoCommand * ImageVariablesExpressionOperation::GetInternalCommand(wxWindow * parent, HuginBase::Panorama & pano, HuginBase::UIntSet images)
{
    ImageVariablesExpressionDialog dlg(parent, &pano);
    if (dlg.ShowModal() == wxID_OK)
    {
        const std::string expression = dlg.GetExpression();
        if (!expression.empty())
        {
            return new PanoCommand::UpdateVariablesByParseExpression(pano, expression);
        };
    };
    return NULL;
}

bool ImageVariablesExpressionOperation::IsEnabled(HuginBase::Panorama & pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    return pano.getNrOfImages() > 0 && guiLevel >= GuiLevel::GUI_ADVANCED;
}

wxString RemoveImageOperation::GetLabel()
{
    return _("Remove selected image(s)");
};

PanoCommand::PanoCommand* RemoveImageOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    //remove images from cache
    for (HuginBase::UIntSet::iterator it = images.begin(); it != images.end(); ++it)
    {
        HuginBase::ImageCache::getInstance().removeImage(pano.getImage(*it).getFilename());
    }
    return new PanoCommand::RemoveImagesCmd(pano, images);
};

wxString ChangeAnchorImageOperation::GetLabel()
{
    return _("Anchor this image for position");
};

PanoCommand::PanoCommand* ChangeAnchorImageOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    HuginBase::PanoramaOptions opt = pano.getOptions();
    opt.optimizeReferenceImage = *(images.begin());
    return new PanoCommand::SetPanoOptionsCmd(pano,opt);
};

wxString ChangeColorAnchorImageOperation::GetLabel()
{
    return _("Anchor this image for exposure");
};

PanoCommand::PanoCommand* ChangeColorAnchorImageOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    HuginBase::PanoramaOptions opt = pano.getOptions();
    opt.colorReferenceImage = *(images.begin());
    // Set the color correction mode so that the anchor image is persisted
    if (opt.colorCorrection == 0)
    {
        opt.colorCorrection = (HuginBase::PanoramaOptions::ColorCorrection) 1;
    }
    return new PanoCommand::SetPanoOptionsCmd(pano, opt);
};

bool NewLensOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    if(pano.getNrOfImages()==0 || images.size()==0)
    {
        return false;
    }
    else
    {
        HuginBase::StandardImageVariableGroups variable_groups(pano);
        return variable_groups.getLenses().getNumberOfParts()<pano.getNrOfImages();
    };
};

wxString NewLensOperation::GetLabel()
{
    return _("New lens");
};

PanoCommand::PanoCommand* NewLensOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    return new PanoCommand::NewPartCmd(pano, images, HuginBase::StandardImageVariableGroups::getLensVariables());
};

bool ChangeLensOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    if(pano.getNrOfImages()==0 || images.size()==0)
    {
        return false;
    }
    else
    {
        //project must have more than 1 lens before you can assign another lens number
        HuginBase::StandardImageVariableGroups variableGroups(pano);
        return variableGroups.getLenses().getNumberOfParts() > 1;
    };
};

wxString ChangeLensOperation::GetLabel()
{
    return _("Change lens...");
};

PanoCommand::PanoCommand* ChangeLensOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    HuginBase::StandardImageVariableGroups variable_groups(pano);
    long nr = wxGetNumberFromUser(
                            _("Enter new lens number"),
                            _("Lens number"),
                            _("Change lens number"), 0, 0,
                            variable_groups.getLenses().getNumberOfParts()-1
                                 );
    if (nr >= 0)
    {
        // user accepted
        return new PanoCommand::ChangePartNumberCmd(pano, images, nr, HuginBase::StandardImageVariableGroups::getLensVariables());
    }
    else
    {
        return NULL;
    };
};

LoadLensOperation::LoadLensOperation(bool fromDatabase)
{
    m_fromDatabase = fromDatabase;
};

wxString LoadLensOperation::GetLabel()
{
    if (m_fromDatabase)
    {
        return _("Load lens from lens database");
    }
    else
    {
        return _("Load lens from ini file");
    };
};

PanoCommand::PanoCommand* LoadLensOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    HuginBase::StandardImageVariableGroups variable_groups(pano);
    HuginBase::UIntSet lensImages = variable_groups.getLenses().getPartsSet()[variable_groups.getLenses().getPartNumber(*images.begin())];
    if (!m_fromDatabase && images.size() == 1 && lensImages.size() > 1)
    {
        // database is always linking the parameters, so no need to ask user
        if(wxMessageBox(_("You selected only one image.\nShould the loaded parameters be applied to all images with the same lens?"),_("Question"), wxICON_QUESTION | wxYES_NO)==wxYES)
        {
            // get all images with the current lens.
            std::copy(lensImages.begin(), lensImages.end(), std::inserter(images, images.end()));
        };
    };
    vigra::Size2D sizeImg0=pano.getImage(*(images.begin())).getSize();
    //check if all images have the same size
    bool differentImageSize=false;
    for(HuginBase::UIntSet::const_iterator it=images.begin();it!=images.end() && !differentImageSize;++it)
    {
        differentImageSize=(pano.getImage(*it).getSize()!=sizeImg0);
    };
    if(differentImageSize)
    {
        if(wxMessageBox(_("You selected images with different sizes.\nApply lens parameter file can result in unwanted results.\nApply settings anyway?"), _("Error"), wxICON_QUESTION |wxYES_NO)==wxID_NO)
        {
            return NULL;
        };
    };
    PanoCommand::PanoCommand* cmd=NULL;
    bool isLoaded=false;
    if (m_fromDatabase)
    {
        isLoaded=ApplyLensDBParameters(parent,&pano,images,cmd);
    }
    else
    {
        isLoaded=ApplyLensParameters(parent,&pano,images,cmd);
    };
    if(isLoaded)
    {
        return cmd;
    }
    else
    {
        return NULL;
    }
};

SaveLensOperation::SaveLensOperation(bool toDatabase)
{
    m_database = toDatabase;
};

wxString SaveLensOperation::GetLabel()
{
    if (m_database)
    {
        return _("Save lens parameters to lens database");
    }
    else
    {
        return _("Save lens to ini file");
    };
};

PanoCommand::PanoCommand* SaveLensOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    unsigned int imgNr = *(images.begin());
    if (m_database)
    {
        SaveLensParameters(parent, pano.getImage(imgNr));
    }
    else
    {
        SaveLensParametersToIni(parent, &pano, images);
    };
    return NULL;
};

wxString RemoveControlPointsOperation::GetLabel()
{
    return _("Remove control points");
};

bool RemoveControlPointsOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    return pano.getNrOfImages()>0 && pano.getNrOfCtrlPoints()>0;
};

PanoCommand::PanoCommand* RemoveControlPointsOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    HuginBase::UIntSet selImages;
    if(images.size()==0)
    {
        fill_set(selImages,0,pano.getNrOfCtrlPoints()-1);
    }
    else
    {
        selImages=images;
    };
    HuginBase::UIntSet cpsToDelete;
    const HuginBase::CPVector & cps = pano.getCtrlPoints();
    for (HuginBase::CPVector::const_iterator it = cps.begin(); it != cps.end(); ++it)
    {
        if (set_contains(selImages, (*it).image1Nr) && set_contains(selImages, (*it).image2Nr) )
        {
            cpsToDelete.insert(it - cps.begin());
        }
    }
    if(cpsToDelete.size()==0)
    {
        wxMessageBox(_("Selected images have no control points."),
#ifdef __WXMSW__
            wxT("Hugin"),
#else
            wxT(""),
#endif
            wxICON_EXCLAMATION | wxOK);
        return NULL;
    };
    int r =wxMessageBox(wxString::Format(_("Really delete %lu control points?"),
                                         (unsigned long int) cpsToDelete.size()),
                        _("Delete Control Points"),
                        wxICON_QUESTION | wxYES_NO);
    if (r == wxYES)
    {
        return new PanoCommand::RemoveCtrlPointsCmd(pano, cpsToDelete );
    }
    else
    {
        return NULL;
    };
};

wxString CleanControlPointsOperation::GetLabel()
{
    return _("Clean control points");
};

bool CleanControlPointsOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    return pano.getNrOfCtrlPoints()>2;
};

PanoCommand::PanoCommand* CleanControlPointsOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    deregisterPTWXDlgFcn();
    ProgressReporterDialog progress(0, _("Cleaning Control points"), _("Checking pairwise"), parent);

    HuginBase::UIntSet removedCPs=getCPoutsideLimit_pair(pano, progress, 2.0);

    //create a copy to work with
    //we copy remaining control points to new pano object for running second step
    HuginBase::Panorama newPano=pano.duplicate();
    std::map<size_t,size_t> cpMap;
    HuginBase::CPVector allCPs=newPano.getCtrlPoints();
    HuginBase::CPVector firstCleanedCP;
    size_t j=0;
    for(size_t i=0;i<allCPs.size();i++)
    {
        HuginBase::ControlPoint cp = allCPs[i];
        if (cp.mode == HuginBase::ControlPoint::X_Y && !set_contains(removedCPs, i))
        {
            firstCleanedCP.push_back(cp);
            cpMap[j++]=i;
        };
    };
    newPano.setCtrlPoints(firstCleanedCP);

    //check for unconnected images
    HuginGraph::ImageGraph graph(newPano);
    if (!progress.updateDisplayValue(_("Checking whole project")))
    {
        return NULL;
    }
    if (graph.IsConnected())
    {
        //now run the second step
        HuginBase::UIntSet removedCP2=getCPoutsideLimit(newPano, 2.0);
        if(removedCP2.size()>0)
        {
            for(HuginBase::UIntSet::const_iterator it=removedCP2.begin();it!=removedCP2.end();++it)
            {
                removedCPs.insert(cpMap[*it]);
            };
        };
    }
    registerPTWXDlgFcn();
    if (!progress.updateDisplay(_("Finished cleaning")))
    {
        return NULL;
    }
    if (removedCPs.size()>0)
    {
        wxMessageBox(wxString::Format(_("Removed %lu control points"), (unsigned long int)removedCPs.size()), _("Cleaning"), wxOK | wxICON_INFORMATION, parent);
        return new PanoCommand::RemoveCtrlPointsCmd(pano,removedCPs);
    };
    return NULL;
};

wxString CelesteOperation::GetLabel()
{
    return _("Remove control points on clouds");
};

PanoCommand::PanoCommand* CelesteOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    ProgressReporterDialog progress(images.size() + 2, _("Running Celeste"), _("Running Celeste"), parent);
    progress.updateDisplay(_("Loading model file"));

    struct celeste::svm_model* model=MainFrame::Get()->GetSVMModel();
    if (model == NULL || !progress.updateDisplay(_("Loading images")))
    {
        return NULL;
    };

    // Get Celeste parameters
    wxConfigBase *cfg = wxConfigBase::Get();
    // SVM threshold
    double threshold = HUGIN_CELESTE_THRESHOLD;
    cfg->Read(wxT("/Celeste/Threshold"), &threshold, HUGIN_CELESTE_THRESHOLD);

    // Mask resolution - 1 sets it to fine
    bool t = (cfg->Read(wxT("/Celeste/Filter"), HUGIN_CELESTE_FILTER) == 0);
    int radius=(t)?10:20;
    DEBUG_TRACE("Running Celeste");

    HuginBase::UIntSet cpsToRemove;
    for (HuginBase::UIntSet::const_iterator it=images.begin(); it!=images.end(); ++it)
    {
        // Image to analyse
        HuginBase::CPointVector cps=pano.getCtrlPointsVectorForImage(*it);
        if(cps.size()==0)
        {
            if (!progress.updateDisplayValue())
            {
                return NULL;
            };
            continue;
        };
        HuginBase::ImageCache::EntryPtr img = HuginBase::ImageCache::getInstance().getImage(pano.getImage(*it).getFilename());
        vigra::UInt16RGBImage in;
        if(img->image16->width()>0)
        {
            in.resize(img->image16->size());
            vigra::omp::copyImage(srcImageRange(*(img->image16)),destImage(in));
        }
        else
        {
            HuginBase::ImageCache::ImageCacheRGB8Ptr im8 = img->get8BitImage();
            in.resize(im8->size());
            vigra::omp::transformImage(srcImageRange(*im8),destImage(in),vigra::functor::Arg1()*vigra::functor::Param(65535/255));
        };
        if (!img->iccProfile->empty())
        {
            HuginBase::Color::ApplyICCProfile(in, *(img->iccProfile), TYPE_RGB_16);
        };
        if (!progress.updateDisplay(_("Running Celeste")))
        {
            return NULL;
        };
        HuginBase::UIntSet cloudCP=celeste::getCelesteControlPoints(model,in,cps,radius,threshold,800);
        in.resize(0,0);
        if (!progress.updateDisplay())
        {
            return NULL;
        };
        if(cloudCP.size()>0)
        {
            for(HuginBase::UIntSet::const_iterator it2=cloudCP.begin();it2!=cloudCP.end(); ++it2)
            {
                cpsToRemove.insert(*it2);
            };
        };
        if (!progress.updateDisplayValue(_("Loading images")))
        {
            return NULL;
        };
    };

    if (!progress.updateDisplayValue())
    {
        return NULL;
    }
    if (cpsToRemove.size()>0)
    {
        wxMessageBox(wxString::Format(_("Removed %lu control points"), (unsigned long int) cpsToRemove.size()), _("Celeste result"),wxOK|wxICON_INFORMATION);
        return new PanoCommand::RemoveCtrlPointsCmd(pano,cpsToRemove);
    }
    else
    {
        return NULL;
    };
};

ResetOperation::ResetOperation(ResetMode newResetMode)
{
    m_resetMode=newResetMode;
    m_resetPos=(m_resetMode==RESET_POSITION);
    m_resetTranslation=(m_resetMode==RESET_TRANSLATION);
    m_resetHFOV=(m_resetMode==RESET_LENS);
    m_resetLens=(m_resetMode==RESET_LENS);
    m_resetExposure=0;
    if(m_resetMode==RESET_PHOTOMETRICS)
    {
        m_resetExposure=3;
    };
    m_resetVignetting=(m_resetMode==RESET_PHOTOMETRICS);
    m_resetColor = 0;
    if(m_resetMode==RESET_PHOTOMETRICS)
    {
        m_resetColor=1;
    };
    m_resetCameraResponse=(m_resetMode==RESET_PHOTOMETRICS);
};

wxString ResetOperation::GetLabel()
{
    switch(m_resetMode)
    {
        case RESET_DIALOG:
        case RESET_DIALOG_LENS:
        case RESET_DIALOG_PHOTOMETRICS:
            return _("Reset user defined...");
            break;
        case RESET_POSITION:
            return _("Reset positions");
            break;
        case RESET_TRANSLATION:
            return _("Reset translation parameters");
            break;
        case RESET_LENS:
            return _("Reset lens parameters");
            break;
        case RESET_PHOTOMETRICS:
            return _("Reset photometric parameters");
    };
    return wxEmptyString;
};

bool ResetOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    switch(m_resetMode)
    {
        case RESET_TRANSLATION:
            return guiLevel>=GUI_EXPERT && pano.getNrOfImages()>0;
            break;
        default:
            return pano.getNrOfImages()>0;
    };
};

PanoCommand::PanoCommand* ResetOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    if(m_resetMode==RESET_DIALOG || m_resetMode==RESET_DIALOG_LENS || m_resetMode==RESET_DIALOG_PHOTOMETRICS)
    {
        if(!ShowDialog(parent))
        {
            return NULL;
        };
    };
    if(images.size()==0)
    {
        fill_set(images,0,pano.getNrOfImages()-1);
    };
    // If we should unlink exposure value (to load it from EXIF)
    bool needs_unlink_exposure = false;
    bool needs_unlink_redbal = false;
    bool needs_unlink_bluebal = false;
    double redBalanceAnchor = pano.getImage(pano.getOptions().colorReferenceImage).getExifRedBalance();
    double blueBalanceAnchor = pano.getImage(pano.getOptions().colorReferenceImage).getExifBlueBalance();
    if(fabs(redBalanceAnchor)<1e-2)
    {
        redBalanceAnchor=1;
    };
    if(fabs(blueBalanceAnchor)<1e-2)
    {
        blueBalanceAnchor=1;
    };

    HuginBase::VariableMapVector vars;
    for(HuginBase::UIntSet::const_iterator it = images.begin(); it != images.end(); ++it)
    {
        unsigned int imgNr = *it;
        HuginBase::VariableMap ImgVars = pano.getImageVariables(imgNr);
        if(m_resetPos)
        {
            map_get(ImgVars,"y").setValue(0);
            map_get(ImgVars,"p").setValue(0);
            map_get(ImgVars,"r").setValue(pano.getSrcImage(imgNr).getExifOrientation());
            map_get(ImgVars,"TrX").setValue(0);
            map_get(ImgVars,"TrY").setValue(0);
            map_get(ImgVars,"TrZ").setValue(0);
            map_get(ImgVars,"Tpy").setValue(0);
            map_get(ImgVars,"Tpp").setValue(0);
        };
        if(m_resetTranslation)
        {
            map_get(ImgVars,"TrX").setValue(0);
            map_get(ImgVars,"TrY").setValue(0);
            map_get(ImgVars,"TrZ").setValue(0);
            map_get(ImgVars,"Tpy").setValue(0);
            map_get(ImgVars,"Tpp").setValue(0);
        };
        HuginBase::SrcPanoImage srcImg = pano.getSrcImage(imgNr);
        if(m_resetHFOV)
        {
            double focalLength=srcImg.getExifFocalLength();
            double cropFactor=srcImg.getExifCropFactor();
            if(focalLength!=0 && cropFactor!=0)
            {
                double newHFOV=HuginBase::SrcPanoImage::calcHFOV(srcImg.getProjection(), focalLength, cropFactor, srcImg.getSize());
                if(newHFOV!=0)
                {
                    map_get(ImgVars,"v").setValue(newHFOV);
                };
            };
        };
        if(m_resetLens)
        {
            map_get(ImgVars,"a").setValue(0);
            map_get(ImgVars,"b").setValue(0);
            map_get(ImgVars,"c").setValue(0);
            map_get(ImgVars,"d").setValue(0);
            map_get(ImgVars,"e").setValue(0);
            map_get(ImgVars,"g").setValue(0);
            map_get(ImgVars,"t").setValue(0);
        };
        if(m_resetExposure>0)
        {
            if(m_resetExposure==1 || m_resetExposure==3)
            {
                if (pano.getImage(imgNr).ExposureValueisLinked())
                {
                    /* Unlink exposure value variable so the EXIF values can be
                     * independant. */
                    needs_unlink_exposure = true;
                }
                //reset to exif value
                double eV=srcImg.calcExifExposureValue();
                if ((m_resetExposure == 1 && eV != 0) || m_resetExposure == 3)
                {
                    map_get(ImgVars,"Eev").setValue(eV);
                }
            }
            else
            {
                //reset to zero
                map_get(ImgVars,"Eev").setValue(0);
            };
        };
        if(m_resetColor>0)
        {
            if(m_resetColor==1)
            {
                if (pano.getImage(imgNr).WhiteBalanceRedisLinked())
                {
                    /* Unlink red balance variable so the EXIF values can be
                     * independant. */
                    needs_unlink_redbal = true;
                }
                if (pano.getImage(imgNr).WhiteBalanceBlueisLinked())
                {
                    /* Unlink red balance variable so the EXIF values can be
                     * independant. */
                    needs_unlink_bluebal = true;
                }
                double redBal=1;
                double blueBal=1;
                const HuginBase::SrcPanoImage& img=pano.getImage(imgNr);
                const HuginBase::SrcPanoImage& anchor=pano.getImage(pano.getOptions().colorReferenceImage);
                // use EXIF Red/BlueBalance data only if image and anchor image are from the same camera
                if(img.getExifMake() == anchor.getExifMake() &&
                    img.getExifModel() == anchor.getExifModel())
                {
                    redBal=fabs(img.getExifRedBalance()/redBalanceAnchor);
                    if(redBal<1e-2)
                    {
                        redBal=1;
                    };
                    blueBal=fabs(img.getExifBlueBalance()/blueBalanceAnchor);
                    if(blueBal<1e-2)
                    {
                        blueBal=1;
                    };
                };
                map_get(ImgVars,"Er").setValue(redBal);
                map_get(ImgVars,"Eb").setValue(blueBal);
            }
            else
            {
                map_get(ImgVars,"Er").setValue(1);
                map_get(ImgVars,"Eb").setValue(1);
            };
        };
        if(m_resetVignetting)
        {
            map_get(ImgVars,"Vb").setValue(0);
            map_get(ImgVars,"Vc").setValue(0);
            map_get(ImgVars,"Vd").setValue(0);
            map_get(ImgVars,"Vx").setValue(0);
            map_get(ImgVars,"Vy").setValue(0);

        };
        if(m_resetCameraResponse)
        {
            map_get(ImgVars,"Ra").setValue(0);
            map_get(ImgVars,"Rb").setValue(0);
            map_get(ImgVars,"Rc").setValue(0);
            map_get(ImgVars,"Rd").setValue(0);
            map_get(ImgVars,"Re").setValue(0);
        };
        vars.push_back(ImgVars);
    };
    std::vector<PanoCommand::PanoCommand *> reset_commands;
    if (needs_unlink_exposure)
    {
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> variables;
        variables.insert(HuginBase::ImageVariableGroup::IVE_ExposureValue);
        
        reset_commands.push_back(
                new PanoCommand::ChangePartImagesLinkingCmd(
                            pano,
                            images,
                            variables,
                            false,
                            HuginBase::StandardImageVariableGroups::getLensVariables())
                );
    }
    if (needs_unlink_redbal)
    {
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> variables;
        variables.insert(HuginBase::ImageVariableGroup::IVE_WhiteBalanceRed);
        
        reset_commands.push_back(
                new PanoCommand::ChangePartImagesLinkingCmd(
                            pano,
                            images,
                            variables,
                            false,
                            HuginBase::StandardImageVariableGroups::getLensVariables())
                );
    }
    if (needs_unlink_bluebal)
    {
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> variables;
        variables.insert(HuginBase::ImageVariableGroup::IVE_WhiteBalanceBlue);
        
        reset_commands.push_back(
                new PanoCommand::ChangePartImagesLinkingCmd(
                            pano,
                            images,
                            variables,
                            false,
                            HuginBase::StandardImageVariableGroups::getLensVariables())
                );
    }
    reset_commands.push_back(
                            new PanoCommand::UpdateImagesVariablesCmd(pano, images, vars)
                                           );
    if(m_resetExposure>0)
    {
        //reset panorama output exposure value
        reset_commands.push_back(new PanoCommand::ResetToMeanExposure(pano));
    };
    return new PanoCommand::CombinedPanoCommand(pano, reset_commands);
};

bool ResetOperation::ShowDialog(wxWindow* parent)
{
    ResetDialog reset_dlg(parent, m_guiLevel);
    bool checkGeometric;
    bool checkPhotometric;
    switch(m_resetMode)
    {
        case RESET_DIALOG_LENS:
            reset_dlg.LimitToGeometric();
            checkGeometric=true;
            checkPhotometric=false;
            break;
        case RESET_DIALOG_PHOTOMETRICS:
            reset_dlg.LimitToPhotometric();
            checkGeometric=false;
            checkPhotometric=true;
            break;
        case RESET_DIALOG:
        default:
            checkGeometric=true;
            checkPhotometric=true;
            break;
    };
    if(reset_dlg.ShowModal()==wxID_OK)
    {
        if(checkGeometric)
        {
            m_resetPos=reset_dlg.GetResetPos();
            m_resetTranslation=reset_dlg.GetResetTranslation();
            m_resetHFOV=reset_dlg.GetResetFOV();
            m_resetLens=reset_dlg.GetResetLens();
        };
        if(checkPhotometric)
        {
            if(reset_dlg.GetResetExposure())
            {
                if(reset_dlg.GetResetExposureToExif())
                {
                    m_resetExposure=1;
                }
                else
                {
                    m_resetExposure=2;
                };
            }
            else
            {
                m_resetExposure=0;
            };
            m_resetVignetting=reset_dlg.GetResetVignetting();
            if(reset_dlg.GetResetColor())
            {
                if(reset_dlg.GetResetColorToExif())
                {
                    m_resetColor=1;
                }
                else
                {
                    m_resetColor=2;
                };
            }
            else
            {
                m_resetColor=0;
            };
            m_resetCameraResponse=reset_dlg.GetResetResponse();
        };
        return true;
    }
    else
    {
        return false;
    };
};

bool NewStackOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    if(pano.getNrOfImages()==0 || images.size()==0)
    {
        return false;
    }
    else
    {
        HuginBase::StandardImageVariableGroups variable_groups(pano);
        return variable_groups.getStacks().getNumberOfParts()<pano.getNrOfImages();
    };
};

wxString NewStackOperation::GetLabel()
{
    return _("New stack");
};

PanoCommand::PanoCommand* NewStackOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    return new PanoCommand::NewPartCmd(pano, images, HuginBase::StandardImageVariableGroups::getStackVariables());
};

bool ChangeStackOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    if(pano.getNrOfImages()==0 || images.size()==0)
    {
        return false;
    }
    else
    {
        //project must have more than 1 stack before you can assign another stack number
        HuginBase::StandardImageVariableGroups variableGroups(pano);
        return variableGroups.getStacks().getNumberOfParts() > 1;
    };
};

wxString ChangeStackOperation::GetLabel()
{
    return _("Change stack...");
};

PanoCommand::PanoCommand* ChangeStackOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    HuginBase::StandardImageVariableGroups variable_groups(pano);
    long nr = wxGetNumberFromUser(
                            _("Enter new stack number"),
                            _("stack number"),
                            _("Change stack number"), 0, 0,
                            variable_groups.getStacks().getNumberOfParts()-1
                                 );
    if (nr >= 0)
    {
        // user accepted
        return new PanoCommand::ChangePartNumberCmd(pano, images, nr, HuginBase::StandardImageVariableGroups::getStackVariables());
    }
    else
    {
        return NULL;
    };
};

bool AssignStacksOperation::IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel)
{
    return pano.getNrOfImages()>1;
};

wxString AssignStacksOperation::GetLabel()
{
    return _("Set stack size...");
};

PanoCommand::PanoCommand* AssignStacksOperation::GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)
{
    wxConfigBase* cfg = wxConfigBase::Get();
    wxDialog dlg;
    wxXmlResource::Get()->LoadDialog(&dlg, parent, wxT("stack_size_dialog"));
    wxSpinCtrl* stackSpin = XRCCTRL(dlg, "stack_size_spinctrl", wxSpinCtrl);
    stackSpin->SetRange(1, pano.getNrOfImages());
    size_t oldStackSize = cfg->Read(wxT("/StackDialog/StackSize"), 3);
    oldStackSize = std::min(oldStackSize, pano.getNrOfImages());
    stackSpin->SetValue(oldStackSize);
    wxCheckBox* linkCheckBox = XRCCTRL(dlg, "stack_size_link_checkbox", wxCheckBox);
    linkCheckBox->SetValue(cfg->Read(wxT("/StackDialog/LinkPosition"), true) != 0l);
    if (dlg.ShowModal() != wxID_OK)
    {
        // user has canceled dialog
        return NULL;
    };
    long stackSize = stackSpin->GetValue();
    bool linkPosition = linkCheckBox->IsChecked();
    cfg->Write(wxT("/StackDialog/StackSize"), stackSize);
    cfg->Write(wxT("/StackDialog/LinkPosition"), linkPosition);
    if(stackSize<0)
    {
        return NULL;
    };
    std::vector<PanoCommand::PanoCommand *> commands;
    HuginBase::StandardImageVariableGroups variable_groups(pano);
    if(variable_groups.getStacks().getNumberOfParts()<pano.getNrOfImages())
    {
        // first remove all existing stacks
        for(size_t i=1; i<pano.getNrOfImages(); i++)
        {
            HuginBase::UIntSet imgs;
            imgs.insert(i);
            commands.push_back(new PanoCommand::NewPartCmd(pano, imgs, HuginBase::StandardImageVariableGroups::getStackVariables()));
        };
    };

    if (stackSize > 1)
    {
        size_t stackNr=0;
        size_t imgNr=0;
        while(imgNr<pano.getNrOfImages())
        {
            HuginBase::UIntSet imgs;
            for(size_t i=0; i<stackSize && imgNr<pano.getNrOfImages(); i++)
            {
                imgs.insert(imgNr);
                imgNr++;
            };
            commands.push_back(new PanoCommand::ChangePartNumberCmd(pano, imgs, stackNr, HuginBase::StandardImageVariableGroups::getStackVariables()));
            stackNr++;
        };
    };

    if (!linkPosition && stackSize > 1)
    {
        // unlink image position
        HuginBase::UIntSet imgs;
        fill_set(imgs, 0, pano.getNrOfImages() - 1);
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> variables;
        variables.insert(HuginBase::ImageVariableGroup::IVE_Yaw);
        variables.insert(HuginBase::ImageVariableGroup::IVE_Pitch);
        variables.insert(HuginBase::ImageVariableGroup::IVE_Roll);
        variables.insert(HuginBase::ImageVariableGroup::IVE_X);
        variables.insert(HuginBase::ImageVariableGroup::IVE_Y);
        variables.insert(HuginBase::ImageVariableGroup::IVE_Z);
        variables.insert(HuginBase::ImageVariableGroup::IVE_TranslationPlaneYaw);
        variables.insert(HuginBase::ImageVariableGroup::IVE_TranslationPlanePitch);
        commands.push_back(new PanoCommand::ChangePartImagesLinkingCmd(pano, imgs, variables, false, HuginBase::StandardImageVariableGroups::getStackVariables()));
    };
    return new PanoCommand::CombinedPanoCommand(pano, commands);
};

static PanoOperationVector PanoOpImages;
static PanoOperationVector PanoOpLens;
static PanoOperationVector PanoOpStacks;
static PanoOperationVector PanoOpControlPoints;
static PanoOperationVector PanoOpReset;

PanoOperationVector* GetImagesOperationVector()
{
    return &PanoOpImages;
};

PanoOperationVector* GetLensesOperationVector()
{
    return &PanoOpLens;
};

PanoOperationVector* GetStacksOperationVector()
{
    return &PanoOpStacks;
};

PanoOperationVector* GetControlPointsOperationVector()
{
    return &PanoOpControlPoints;
};

PanoOperationVector* GetResetOperationVector()
{
    return &PanoOpReset;
};

void GeneratePanoOperationVector()
{
    PanoOpImages.push_back(new AddImageOperation());
    PanoOpImages.push_back(new AddImagesSeriesOperation());
    PanoOpImages.push_back(new RemoveImageOperation());
    PanoOpImages.push_back(new ChangeAnchorImageOperation());
    PanoOpImages.push_back(new ChangeColorAnchorImageOperation());
    PanoOpImages.push_back(new ImageVariablesExpressionOperation());

    PanoOpLens.push_back(new NewLensOperation());
    PanoOpLens.push_back(new ChangeLensOperation());
    PanoOpLens.push_back(new LoadLensOperation(false));
    PanoOpLens.push_back(new LoadLensOperation(true));
    PanoOpLens.push_back(new SaveLensOperation(false));
    PanoOpLens.push_back(new SaveLensOperation(true));

    PanoOpStacks.push_back(new NewStackOperation());
    PanoOpStacks.push_back(new ChangeStackOperation());
    PanoOpStacks.push_back(new AssignStacksOperation());

    PanoOpControlPoints.push_back(new RemoveControlPointsOperation());
    PanoOpControlPoints.push_back(new CelesteOperation());
    PanoOpControlPoints.push_back(new CleanControlPointsOperation());

    PanoOpReset.push_back(new ResetOperation(ResetOperation::RESET_POSITION));
    PanoOpReset.push_back(new ResetOperation(ResetOperation::RESET_TRANSLATION));
    PanoOpReset.push_back(new ResetOperation(ResetOperation::RESET_LENS));
    PanoOpReset.push_back(new ResetOperation(ResetOperation::RESET_PHOTOMETRICS));
    PanoOpReset.push_back(new ResetOperation(ResetOperation::RESET_DIALOG));

};


void _CleanPanoOperationVector(PanoOperationVector& vec)
{
    for(size_t i=0; i<vec.size(); i++)
    {
        delete vec[i];
    }
    vec.clear();
};

void CleanPanoOperationVector()
{
    _CleanPanoOperationVector(PanoOpImages);
    _CleanPanoOperationVector(PanoOpLens);
    _CleanPanoOperationVector(PanoOpStacks);
    _CleanPanoOperationVector(PanoOpControlPoints);
    _CleanPanoOperationVector(PanoOpReset);
};

} //namespace
