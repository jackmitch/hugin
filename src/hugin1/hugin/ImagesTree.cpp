// -*- c-basic-offset: 4 -*-

/** @file ImagesTree.cpp
 *
 *  @brief implementation of images tree control
 *
 *  @author T. Modes
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

#include "base_wx/wxPlatform.h"
#include "base_wx/LensTools.h"
#include "hugin/ImagesTree.h"
#include "base_wx/wxImageCache.h"
#include "base_wx/platform.h"
#include "hugin_base/algorithms/basic/LayerStacks.h"
#include <panodata/ImageVariableGroup.h>
#include <panodata/StandardImageVariableGroups.h>
#include "base_wx/CommandHistory.h"
#include "base_wx/PanoCommand.h"
#include <hugin_utils/stl_utils.h>
#include "hugin/ImageVariableDialog.h"
#include "hugin/huginApp.h"
#include "hugin/MainFrame.h"
#include "hugin/GLPreviewFrame.h"
#include <wx/renderer.h>

enum
{
    ID_LINK=wxID_HIGHEST+230,
    ID_UNLINK,
    ID_EDIT,
    ID_SELECT_ALL,
    ID_UNSELECT_ALL,
    ID_SELECT_LENS_STACK,
    ID_UNSELECT_LENS_STACK,
    ID_ACTIVATE_IMAGE,
    ID_DEACTIVATE_IMAGE,
    ID_OPERATION_START=wxID_HIGHEST+300
};

BEGIN_EVENT_TABLE(ImagesTreeCtrl, wxTreeListCtrl)
    EVT_LIST_COL_END_DRAG(-1, ImagesTreeCtrl::OnColumnWidthChange)
    EVT_TREE_ITEM_MENU(-1, ImagesTreeCtrl::OnContextMenu)
    EVT_LIST_COL_RIGHT_CLICK(-1, ImagesTreeCtrl::OnHeaderContextMenu)
    EVT_MENU(ID_LINK, ImagesTreeCtrl::OnLinkImageVariables)
    EVT_MENU(ID_UNLINK, ImagesTreeCtrl::OnUnlinkImageVariables)
    EVT_MENU(ID_EDIT, ImagesTreeCtrl::OnEditImageVariables)
    EVT_MENU(ID_SELECT_ALL, ImagesTreeCtrl::OnSelectAll)
    EVT_MENU(ID_UNSELECT_ALL, ImagesTreeCtrl::OnUnselectAll)
    EVT_MENU(ID_SELECT_LENS_STACK, ImagesTreeCtrl::OnSelectLensStack)
    EVT_MENU(ID_UNSELECT_LENS_STACK, ImagesTreeCtrl::OnUnselectLensStack)
    EVT_MENU(ID_ACTIVATE_IMAGE, ImagesTreeCtrl::OnActivateImage)
    EVT_MENU(ID_DEACTIVATE_IMAGE, ImagesTreeCtrl::OnDeactivateImage)
    EVT_MENU_RANGE(ID_OPERATION_START, ID_OPERATION_START+50, ImagesTreeCtrl::OnExecuteOperation)
    EVT_TREE_BEGIN_DRAG(-1, ImagesTreeCtrl::OnBeginDrag)
    EVT_LEFT_UP(ImagesTreeCtrl::OnLeftUp)
    EVT_LEFT_DCLICK(ImagesTreeCtrl::OnLeftDblClick)
    EVT_TREE_KEY_DOWN(-1, ImagesTreeCtrl::OnChar)
    EVT_TREE_BEGIN_LABEL_EDIT(-1, ImagesTreeCtrl::OnBeginEdit)
    EVT_TREE_END_LABEL_EDIT(-1, ImagesTreeCtrl::OnEndEdit)
END_EVENT_TABLE()

class ImagesTreeData : public wxTreeItemData
{
public:
    explicit ImagesTreeData(const long& nr) : m_nr(nr) { };
    const long GetImgNr() const { return m_nr;};
    void SetImgNr(long newImgNr) { m_nr=newImgNr;};
    const long GetGroupNr() const { return -(m_nr+1);};
    const bool IsGroup() const { return (m_nr<0); };

private:
    long m_nr;
};

// Define a constructor for the Images Panel
ImagesTreeCtrl::ImagesTreeCtrl()
{
    m_pano = NULL;
    m_groupMode=GROUP_NONE;
    m_guiLevel=GUI_SIMPLE;
    m_dragging=false;
    m_displayMode=DISPLAY_GENERAL;
    m_optimizerMode=false;
    m_needsUpdate=true;
    m_markDisabledImages = false;
}

bool ImagesTreeCtrl::Create(wxWindow* parent, wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name)
{
    DEBUG_TRACE("List");
    if (! wxTreeListCtrl::Create(parent, id, pos, size, style | wxTR_DEFAULT_STYLE|wxTR_HIDE_ROOT|wxTR_NO_LINES|wxTR_FULL_ROW_HIGHLIGHT|wxTR_ROW_LINES|wxTR_LINES_AT_ROOT|wxTR_MULTIPLE) )
    {
        return false;
    }

    DEBUG_TRACE("Tree, adding columns");
    m_configClassName = wxT("/ImagesTree");
    CreateColumns();
    DEBUG_TRACE("");
    m_degDigits = wxConfigBase::Get()->Read(wxT("/General/DegreeFractionalDigits"),1);
    m_pixelDigits = wxConfigBase::Get()->Read(wxT("/General/PixelFractionalDigits"),1);
    m_distDigits = wxConfigBase::Get()->Read(wxT("/General/DistortionFractionalDigits"),3);
    //create root
    m_root=AddRoot(wxT("root"));
    return true;
}

void ImagesTreeCtrl::CreateColumns()
{
    size_t counter=0;
#define ADDCOLUMN(header, mapName, width, align, isEditable, IVE, tooltip) \
    AddColumn(header, width, align, -1, true, false, tooltip);\
    m_columnMap[mapName]=counter;\
    m_columnVector.push_back(mapName);\
    if(isEditable)\
    {\
        m_editableColumns.insert(counter);\
    };\
    m_variableVector.push_back(IVE);\
    counter++;
    ADDCOLUMN(wxT("#"), "imgNr", 35, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Image number"))
    ADDCOLUMN(_("Filename"), "filename", 200, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Filename"))
    ADDCOLUMN(_("Width"), "width", 60, wxALIGN_RIGHT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Image width"))
    ADDCOLUMN(_("Height"), "height", 60, wxALIGN_RIGHT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Image height"))
    ADDCOLUMN(_("Anchor"), "anchor", 60, wxALIGN_RIGHT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Anchor image for position and/or exposure"))
    ADDCOLUMN(_("# Ctrl Pnts"), "cps", 60, wxALIGN_RIGHT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Number of control points in this image"))
    ADDCOLUMN(_("Lens no."), "lensNr", 60, wxALIGN_RIGHT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Assigned lens number"))
    ADDCOLUMN(_("Stack no."), "stackNr", 60, wxALIGN_RIGHT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Assigned stack number"))

    ADDCOLUMN(_("Maker"), "maker", 100, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Camera maker"))
    ADDCOLUMN(_("Model"), "model", 100, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Camera model"))
    ADDCOLUMN(_("Lens"), "lens", 100, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Used lens"))
    ADDCOLUMN(_("Capture date"), "date", 100, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Date, image was taken"))
    ADDCOLUMN(_("Focal length"), "focallength", 80, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Focal length"))
    ADDCOLUMN(_("Aperture"), "aperture", 50, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Aperture"))
    ADDCOLUMN(_("Shutter Speed"), "time", 50, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Shutter speed"))
    ADDCOLUMN(_("ISO"), "iso", 50, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("ISO speed"))

    ADDCOLUMN(_("Yaw (y)"), "y", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Yaw"))
    ADDCOLUMN(_("Pitch (p)"), "p", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Pitch"))
    ADDCOLUMN(_("Roll (r)"), "r", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Roll"))
    ADDCOLUMN(wxT("X (TrX)"), "TrX", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Camera translation X"))
    ADDCOLUMN(wxT("Y (TrY)"), "TrY", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Camera translation Y"))
    ADDCOLUMN(wxT("Z (TrZ)"), "TrZ", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Camera translation Z"))
    ADDCOLUMN(_("Plane yaw"), "Tpy", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Translation remap plane yaw"))
    ADDCOLUMN(_("Plane pitch"), "Tpp", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Translation remap plane pitch"))
    ADDCOLUMN(_("Camera translation"), "cam_trans", 60, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Yaw, _("Camera translation"))

    ADDCOLUMN(_("Lens type (f)"), "projection", 100, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Lens type (rectilinear, fisheye, equirectangular, ...)"))
    ADDCOLUMN(_("Hfov (v)"), "v", 80, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_HFOV, _("Horizontal field of view (v)"))
    ADDCOLUMN(wxT("a"), "a", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialDistortion, _("Radial distortion (a)"))
    ADDCOLUMN(wxT("b"), "b", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialDistortion, _("Radial distortion (b, barrel)"))
    ADDCOLUMN(wxT("c"), "c", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialDistortion, _("Radial distortion (c)"))
    ADDCOLUMN(wxT("d"), "d", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialDistortionCenterShift, _("Horizontal image center shift (d)"))
    ADDCOLUMN(wxT("e"), "e", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialDistortionCenterShift, _("Vertical image center shift (e)"))
    ADDCOLUMN(wxT("g"), "g", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Shear, _("Horizontal image shearing (g)"))
    ADDCOLUMN(wxT("t"), "t", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_Shear, _("Vertical image shearing (t)"))

    ADDCOLUMN(wxT("EV"), "Eev", 50, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_ExposureValue, _("Exposure value (Eev)"))
    ADDCOLUMN(wxT("Er"), "Er", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_WhiteBalanceRed, _("Red multiplier (Er)"))
    ADDCOLUMN(wxT("Eb"), "Eb", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_WhiteBalanceBlue, _("Blue multiplier (Eb)"))
    ADDCOLUMN(wxT("Vb"), "Vb", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialVigCorrCoeff, _("Vignetting (Vb, Vc, Vd)"))
    ADDCOLUMN(wxT("Vc"), "Vc", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialVigCorrCoeff, _("Vignetting (Vb, Vc, Vd)"))
    ADDCOLUMN(wxT("Vd"), "Vd", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialVigCorrCoeff, _("Vignetting (Vb, Vc, Vd)"))
    ADDCOLUMN(wxT("Vx"), "Vx", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialVigCorrCenterShift, _("Horizontal vignetting center shift (Vx)"))
    ADDCOLUMN(wxT("Vy"), "Vy", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_RadialVigCorrCenterShift, _("Vertical vignetting center shift (Vy)"))
    ADDCOLUMN(_("Response type"), "response", 80, wxALIGN_LEFT, false, HuginBase::ImageVariableGroup::IVE_Filename, _("Camera response type"))
    ADDCOLUMN(wxT("Ra"), "Ra", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_EMoRParams, _("Camera response parameter"))
    ADDCOLUMN(wxT("Rb"), "Rb", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_EMoRParams, _("Camera response parameter"))
    ADDCOLUMN(wxT("Rc"), "Rc", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_EMoRParams, _("Camera response parameter"))
    ADDCOLUMN(wxT("Rd"), "Rd", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_EMoRParams, _("Camera response parameter"))
    ADDCOLUMN(wxT("Re"), "Re", 40, wxALIGN_LEFT, true, HuginBase::ImageVariableGroup::IVE_EMoRParams, _("Camera response parameter"))

    //empty column to have enough space on the right side
    AddColumn(wxEmptyString,10);

    //get saved width
    for ( int j=0; j < GetColumnCount() ; j++ )
    {
        // -1 is auto
        int width = wxConfigBase::Get()->Read(wxString::Format(m_configClassName+wxT("/ColumnWidth%d"), j ), -1);
        if(width != -1)
            SetColumnWidth(j, width);
    }
};

void ImagesTreeCtrl::Init(HuginBase::Panorama * panorama)
{
    m_pano = panorama;
    m_pano->addObserver(this);
    
    /// @todo check new didn't return NULL in non-debug builds.
    m_variable_groups = new HuginBase::StandardImageVariableGroups(*m_pano);
    DEBUG_ASSERT(m_variable_groups);
    
#ifdef __WXMAC__
    SetDropTarget(new PanoDropTarget(*m_pano, true));
#endif
}

ImagesTreeCtrl::~ImagesTreeCtrl(void)
{
    DEBUG_TRACE("");
    m_pano->removeObserver(this);
    delete m_variable_groups;
};

void ImagesTreeCtrl::panoramaChanged(HuginBase::Panorama & pano)
{
    if(m_optimizerMode)
    {
        Freeze();
        UpdateOptimizerVariables();
        Thaw();
#ifdef __WXGTK__
        Refresh();
#endif
    };
    if(m_needsUpdate && (m_groupMode==GROUP_OUTPUTLAYERS || m_groupMode==GROUP_OUTPUTSTACK))
    {
        HuginBase::UIntSet imgs;
        fill_set(imgs, 0, pano.getNrOfImages()-1);
        panoramaImagesChanged(pano, imgs);
    };
    m_needsUpdate=true;
};

void ImagesTreeCtrl::panoramaImagesChanged(HuginBase::Panorama &pano, const HuginBase::UIntSet &changed)
{
    DEBUG_TRACE("");

    Freeze();
    HuginBase::UIntSet changedImgs(changed);
    // Make sure the part numbers are up to date before writing them to the table.
    size_t oldLensCount=m_variable_groups->getLenses().getNumberOfParts();
    size_t oldStackCount=m_variable_groups->getStacks().getNumberOfParts();
    m_variable_groups->update();
    //if the number of lenses or stacks have changed we need to update all images
    //because the changed images set contains only the list of the changed imagges
    //but not these images where the stack or lens number has changed because
    //an images has been inserted
    if(pano.getNrOfImages()>0)
    {
        if(m_variable_groups->getLenses().getNumberOfParts()!=oldLensCount ||
            m_variable_groups->getStacks().getNumberOfParts()!=oldStackCount)
        {
            fill_set(changedImgs, 0, pano.getNrOfImages()-1);
        };
    };
    if(m_optimizerMode)
    {
        if(m_groupMode==GROUP_NONE && m_pano->getNrOfImages()>m_variable_groups->getStacks().getNumberOfParts())
        {
            SetGroupMode(GROUP_STACK);
        };
        if(m_groupMode==GROUP_STACK && m_pano->getNrOfImages()==m_variable_groups->getStacks().getNumberOfParts())
        {
            SetGroupMode(GROUP_NONE);
        };
    };
    if(m_groupMode==GROUP_NONE)
    {
        HuginBase::UIntSet imgs;
        if(m_pano->getNrOfImages()>0)
        {
            fill_set(imgs,0,m_pano->getNrOfImages()-1);
        };
        UpdateGroup(m_root,imgs,changedImgs);
    }
    else
    {
        HuginBase::UIntSetVector imageGroups;
        switch (m_groupMode)
        {
            case GROUP_LENS:
                imageGroups = m_variable_groups->getLenses().getPartsSet();
                break;
            case GROUP_STACK:
                imageGroups = m_variable_groups->getStacks().getPartsSet();
                break;
            case GROUP_OUTPUTSTACK:
                imageGroups=getHDRStacks(*m_pano,m_pano->getActiveImages(), m_pano->getOptions());
                break;
            case GROUP_OUTPUTLAYERS:
                imageGroups=getExposureLayers(*m_pano,m_pano->getActiveImages(), m_pano->getOptions());
                break;
        };

        size_t nrItems=GetChildrenCount(m_root,false);
        if(nrItems!=imageGroups.size())
        {
            if(nrItems<imageGroups.size())
            {
                for(size_t i=nrItems;i<imageGroups.size();i++)
                {
                    AppendItem(m_root,wxEmptyString,-1,-1,new ImagesTreeData(-(long)i-1));
                };
            }
            else
            {
                while(GetChildrenCount(m_root,false)>imageGroups.size())
                {
                    wxTreeItemIdValue cookie;
                    wxTreeItemId item=GetLastChild(m_root,cookie);
                    Delete(item);
                };
            };
        };

        wxTreeItemIdValue cookie;
        wxTreeItemId item=GetFirstChild(m_root, cookie);
        size_t i=0;
        while(item.IsOk())
        {
            UpdateGroup(item,imageGroups[i++],changedImgs);
            UpdateGroupText(item);
            item=GetNextChild(m_root, cookie);
        };
    };
    // updates checkboxes images for optimizer variables
    if(m_optimizerMode)
    {
        UpdateOptimizerVariables();
    };
    if (m_markDisabledImages)
    {
        UpdateItemFont();
    };

    Thaw();
    m_needsUpdate = false;

    // HACK! need to notify clients anyway... send dummy event..
    // lets hope our clients query for the selected images with GetSelected()
    // and do not try to interpret the event.
    wxListEvent e;
    e.SetEventType(wxEVT_COMMAND_LIST_ITEM_SELECTED);
    e.m_itemIndex = -1;
    GetEventHandler()->ProcessEvent(e);
}

void ImagesTreeCtrl::UpdateImageText(wxTreeItemId item)
{
    ImagesTreeData* itemData=static_cast<ImagesTreeData*>(GetItemData(item));
    // NAMEisLinked returns always false, if the lens or stacks contains only a single image
    // so add a special handling for this case
    bool isSingleImage = false;
    if (m_groupMode != GROUP_NONE)
    {
        isSingleImage = (GetChildrenCount(GetItemParent(item)) == 1);
    };
    wxString s;
    const size_t imgNr=itemData->GetImgNr();
    const HuginBase::SrcPanoImage & img = m_pano->getImage(imgNr);
    wxFileName fn(wxString (img.getFilename().c_str(), HUGIN_CONV_FILENAME));

    s << imgNr;
    SetItemText(item, m_columnMap["imgNr"], s);
    s.Clear();
    SetItemText(item, m_columnMap["filename"], fn.GetFullName() );
    SetItemText(item, m_columnMap["width"], wxString::Format(wxT("%d"), img.getSize().width()));
    SetItemText(item, m_columnMap["height"], wxString::Format(wxT("%d"), img.getSize().height()));

    wxChar flags[] = wxT("--");
    if (m_pano->getOptions().optimizeReferenceImage == imgNr)
    {
        flags[0]='A';
    }
    if (m_pano->getOptions().colorReferenceImage == imgNr)
    {
        flags[1]='C';
    }
    SetItemText(item, m_columnMap["anchor"], wxString(flags, wxConvLocal));
    std::vector<unsigned int> cps = m_pano->getCtrlPointsForImage(imgNr);
    s << cps.size();
    SetItemText(item, m_columnMap["cps"], s);
    s.Clear();
    const unsigned int stackNumber = m_variable_groups->getStacks().getPartNumber(imgNr);
    SetItemText(item, m_columnMap["stackNr"], wxString::Format(wxT("%u"), stackNumber));
    const unsigned int lensNr=m_variable_groups->getLenses().getPartNumber(imgNr);
    SetItemText(item, m_columnMap["lensNr"], wxString::Format(wxT("%u"), lensNr));

    SetItemText(item, m_columnMap["maker"], wxString(img.getExifMake().c_str(), wxConvLocal));
    SetItemText(item, m_columnMap["model"], wxString(img.getExifModel().c_str(), wxConvLocal));
    SetItemText(item, m_columnMap["lens"], wxString(img.getExifLens().c_str(), wxConvLocal));
    struct tm exifdatetime;
    if(img.getExifDateTime(&exifdatetime)==0)
    {
        wxDateTime s_datetime=wxDateTime(exifdatetime);
        s=s_datetime.Format();
    }
    else
    {
        s = wxString(img.getExifDate().c_str(),wxConvLocal);
    }
    SetItemText(item, m_columnMap["date"], s);

    if(img.getExifFocalLength()>0.0)
    {
        if(img.getExifFocalLength35()>0.0)
        {
            s = wxString::Format(wxT("%0.1f mm (%0.0f mm)"),img.getExifFocalLength(),img.getExifFocalLength35());
        }
        else
        {
            s = wxString::Format(wxT("%0.1f mm"),img.getExifFocalLength());
        };
    }
    else
    {
        s = wxEmptyString;
    };
    SetItemText(item, m_columnMap["focallength"], s);

    if(img.getExifAperture()>0)
    {
        s=wxString::Format(wxT("F%.1f"),img.getExifAperture());
    }
    else
    {
        s=wxEmptyString;
    };
    SetItemText(item, m_columnMap["aperture"], s);

    if(img.getExifExposureTime()>0.5)
    {
        if(img.getExifExposureTime()>=1.0) 
        {
            if(img.getExifExposureTime()>=10.0) 
            {
                s=wxString::Format(wxT("%3.0f s"),img.getExifExposureTime());
            }
            else
            {
                s=wxString::Format(wxT("%1.1f s"),img.getExifExposureTime());
            }
        }
        else
        {
            s=wxString::Format(wxT("%1.2f s"),img.getExifExposureTime());
        }
    }
    else
    {
        if (img.getExifExposureTime() > 1e-9)
        {
            s=wxString::Format(wxT("1/%0.0f s"),1.0/img.getExifExposureTime());
        } 
        else
        {
            //Sanity
            s=wxT("");
        }
    }
    SetItemText(item, m_columnMap["time"], s);

    if(img.getExifISO()>0)
    {
        s=wxString::Format(wxT("%0.0f"),img.getExifISO());
    }
    else
    {
        s=wxEmptyString;
    };
    SetItemText(item, m_columnMap["iso"], s);

    if (m_groupMode == GROUP_STACK && (img.YawisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["y"], wxEmptyString);
        SetItemText(item, m_columnMap["p"], wxEmptyString);
        SetItemText(item, m_columnMap["r"], wxEmptyString);
        SetItemText(item, m_columnMap["TrX"], wxEmptyString);
        SetItemText(item, m_columnMap["TrY"], wxEmptyString);
        SetItemText(item, m_columnMap["TrZ"], wxEmptyString);
        SetItemText(item, m_columnMap["Tpy"], wxEmptyString);
        SetItemText(item, m_columnMap["Tpp"], wxEmptyString);
        SetItemText(item, m_columnMap["cam_trans"], wxEmptyString);
    }
    else
    {
        SetItemText(item, m_columnMap["y"], hugin_utils::doubleTowxString(img.getYaw(), m_degDigits));
        SetItemText(item, m_columnMap["p"], hugin_utils::doubleTowxString(img.getPitch(), m_degDigits));
        SetItemText(item, m_columnMap["r"], hugin_utils::doubleTowxString(img.getRoll(), m_degDigits));
        SetItemText(item, m_columnMap["TrX"], hugin_utils::doubleTowxString(img.getX(), m_distDigits));
        SetItemText(item, m_columnMap["TrY"], hugin_utils::doubleTowxString(img.getY(), m_distDigits));
        SetItemText(item, m_columnMap["TrZ"], hugin_utils::doubleTowxString(img.getZ(), m_distDigits));
        SetItemText(item, m_columnMap["Tpy"], hugin_utils::doubleTowxString(img.getTranslationPlaneYaw(), m_degDigits));
        SetItemText(item, m_columnMap["Tpp"], hugin_utils::doubleTowxString(img.getTranslationPlanePitch(), m_degDigits));
        wxString text=_("not active");
        if(img.getX()!=0.0 || img.getY()!=0.0 || img.getZ()!=0.0 || img.getTranslationPlaneYaw()!=0.0 || img.getTranslationPlanePitch()!=0.0)
        {
            text=_("active");
        };
        text.Prepend(wxT(" "));
        SetItemText(item, m_columnMap["cam_trans"], text);
    };

    if(m_groupMode==GROUP_LENS)
    {
        SetItemText(item, m_columnMap["projection"], wxEmptyString);
        SetItemText(item, m_columnMap["response"], wxEmptyString);
    }
    else
    {
        SetItemText(item, m_columnMap["projection"], getProjectionString(img));
        SetItemText(item, m_columnMap["response"], getResponseString(img));
    };

    if (m_groupMode == GROUP_LENS && (img.HFOVisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["v"], wxEmptyString);
    }
    else
    {
        SetItemText(item, m_columnMap["v"], hugin_utils::doubleTowxString(img.getHFOV(), m_degDigits));
    };

    if (m_groupMode == GROUP_LENS && (img.RadialDistortionisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["a"], wxEmptyString);
        SetItemText(item, m_columnMap["b"], wxEmptyString);
        SetItemText(item, m_columnMap["c"], wxEmptyString);
    }
    else
    {
        std::vector<double> dist=img.getRadialDistortion();
        SetItemText(item, m_columnMap["a"], hugin_utils::doubleTowxString(dist[0],m_distDigits));
        SetItemText(item, m_columnMap["b"], hugin_utils::doubleTowxString(dist[1],m_distDigits));
        SetItemText(item, m_columnMap["c"], hugin_utils::doubleTowxString(dist[2],m_distDigits));
    };

    if (m_groupMode == GROUP_LENS && (img.RadialDistortionCenterShiftisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["d"], wxEmptyString);
        SetItemText(item, m_columnMap["e"], wxEmptyString);
    }
    else
    {
        hugin_utils::FDiff2D p=img.getRadialDistortionCenterShift();
        SetItemText(item, m_columnMap["d"], hugin_utils::doubleTowxString(p.x,m_pixelDigits));
        SetItemText(item, m_columnMap["e"], hugin_utils::doubleTowxString(p.y,m_pixelDigits));
    };

    if (m_groupMode == GROUP_LENS && (img.ShearisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["g"], wxEmptyString);
        SetItemText(item, m_columnMap["t"], wxEmptyString);
    }
    else
    {
        hugin_utils::FDiff2D p=img.getShear();
        SetItemText(item, m_columnMap["g"], hugin_utils::doubleTowxString(p.x,m_distDigits));
        SetItemText(item, m_columnMap["t"], hugin_utils::doubleTowxString(p.y,m_distDigits));
    };

    if (m_groupMode == GROUP_LENS && (img.ExposureValueisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["Eev"], wxEmptyString);
    }
    else
    {
        SetItemText(item, m_columnMap["Eev"], hugin_utils::doubleTowxString(img.getExposureValue(), m_pixelDigits));
    };

    if (m_groupMode == GROUP_LENS && (img.WhiteBalanceRedisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["Er"], wxEmptyString);
    }
    else
    {
        SetItemText(item, m_columnMap["Er"], hugin_utils::doubleTowxString(img.getWhiteBalanceRed(), m_pixelDigits+1));
    };

    if (m_groupMode == GROUP_LENS && (img.WhiteBalanceBlueisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["Eb"], wxEmptyString);
    }
    else
    {
        SetItemText(item, m_columnMap["Eb"], hugin_utils::doubleTowxString(img.getWhiteBalanceBlue(), m_pixelDigits+1));
    };

    if (m_groupMode == GROUP_LENS && (img.RadialVigCorrCoeffisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["Vb"], wxEmptyString);
        SetItemText(item, m_columnMap["Vc"], wxEmptyString);
        SetItemText(item, m_columnMap["Vd"], wxEmptyString);
    }
    else
    {
        std::vector<double> dist=img.getRadialVigCorrCoeff();
        SetItemText(item, m_columnMap["Vb"], hugin_utils::doubleTowxString(dist[1],m_distDigits));
        SetItemText(item, m_columnMap["Vc"], hugin_utils::doubleTowxString(dist[2],m_distDigits));
        SetItemText(item, m_columnMap["Vd"], hugin_utils::doubleTowxString(dist[3],m_distDigits));
    };

    if (m_groupMode == GROUP_LENS && (img.RadialVigCorrCenterShiftisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["Vx"], wxEmptyString);
        SetItemText(item, m_columnMap["Vy"], wxEmptyString);
    }
    else
    {
        hugin_utils::FDiff2D p=img.getRadialVigCorrCenterShift();
        SetItemText(item, m_columnMap["Vx"], hugin_utils::doubleTowxString(p.x,m_distDigits));
        SetItemText(item, m_columnMap["Vy"], hugin_utils::doubleTowxString(p.y,m_distDigits));
    };

    if (m_groupMode == GROUP_LENS && (img.EMoRParamsisLinked() || isSingleImage))
    {
        SetItemText(item, m_columnMap["Ra"], wxEmptyString);
        SetItemText(item, m_columnMap["Rb"], wxEmptyString);
        SetItemText(item, m_columnMap["Rc"], wxEmptyString);
        SetItemText(item, m_columnMap["Rd"], wxEmptyString);
        SetItemText(item, m_columnMap["Re"], wxEmptyString);
    }
    else
    {
        std::vector<float> vec=img.getEMoRParams();
        SetItemText(item, m_columnMap["Ra"], hugin_utils::doubleTowxString(vec[0],m_distDigits));
        SetItemText(item, m_columnMap["Rb"], hugin_utils::doubleTowxString(vec[1],m_distDigits));
        SetItemText(item, m_columnMap["Rc"], hugin_utils::doubleTowxString(vec[2],m_distDigits));
        SetItemText(item, m_columnMap["Rd"], hugin_utils::doubleTowxString(vec[3],m_distDigits));
        SetItemText(item, m_columnMap["Re"], hugin_utils::doubleTowxString(vec[4],m_distDigits));
    };
};

void ImagesTreeCtrl::UpdateGroupText(wxTreeItemId item)
{
    ImagesTreeData* itemData=static_cast<ImagesTreeData*>(GetItemData(item));
    switch(m_groupMode)
    {
        case GROUP_LENS:
            SetItemText(item, 1, wxString::Format(_("Lens %ld"),itemData->GetGroupNr()));
            break;
        case GROUP_STACK:
            SetItemText(item, 1, wxString::Format(_("Stack %ld"),itemData->GetGroupNr()));
            break;
        case GROUP_OUTPUTSTACK:
            SetItemText(item, 1, wxString::Format(_("Output stack %ld"),itemData->GetGroupNr()));
            break;
        case GROUP_OUTPUTLAYERS:
            SetItemText(item, 1, wxString::Format(_("Output exposure layer %ld"),itemData->GetGroupNr()));
            break;
    };
    SetItemBold(item,1,true);
    wxTreeItemIdValue cookie;
    wxTreeItemId childItem=GetFirstChild(item,cookie);
    // NAMEisLinked returns always false, if the lens or stacks contains only a single image
    // so add a special handling for this case
    bool haveSingleChild = false;
    if (m_groupMode != GROUP_NONE)
    {
        haveSingleChild = (GetChildrenCount(item, false) == 1);
    };
    if(childItem.IsOk())
    {
        ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(childItem));
        const HuginBase::SrcPanoImage& img=m_pano->getImage(data->GetImgNr());

        if (m_groupMode == GROUP_STACK && (img.YawisLinked() || haveSingleChild))
        {
            SetItemText(item, m_columnMap["y"], hugin_utils::doubleTowxString(img.getYaw(), m_degDigits));
            SetItemText(item, m_columnMap["p"], hugin_utils::doubleTowxString(img.getPitch(), m_degDigits));
            SetItemText(item, m_columnMap["r"], hugin_utils::doubleTowxString(img.getRoll(), m_degDigits));
            SetItemText(item, m_columnMap["TrX"], hugin_utils::doubleTowxString(img.getX(), m_degDigits));
            SetItemText(item, m_columnMap["TrY"], hugin_utils::doubleTowxString(img.getY(), m_degDigits));
            SetItemText(item, m_columnMap["TrZ"], hugin_utils::doubleTowxString(img.getZ(), m_degDigits));
            SetItemText(item, m_columnMap["Tpy"], hugin_utils::doubleTowxString(img.getTranslationPlaneYaw(), m_degDigits));
            SetItemText(item, m_columnMap["Tpp"], hugin_utils::doubleTowxString(img.getTranslationPlanePitch(), m_degDigits));
            wxString text=_("not active");
            if(img.getX()!=0.0 || img.getY()!=0.0 || img.getZ()!=0.0 || img.getTranslationPlaneYaw()!=0.0 || img.getTranslationPlanePitch()!=0.0)
            {
                text=_("active");
            };
            text.Prepend(wxT(" "));
            SetItemText(item, m_columnMap["cam_trans"], text);
        }
        else
        {
            SetItemText(item, m_columnMap["y"], wxEmptyString);
            SetItemText(item, m_columnMap["p"], wxEmptyString);
            SetItemText(item, m_columnMap["r"], wxEmptyString);
            SetItemText(item, m_columnMap["TrX"], wxEmptyString);
            SetItemText(item, m_columnMap["TrY"], wxEmptyString);
            SetItemText(item, m_columnMap["TrZ"], wxEmptyString);
            SetItemText(item, m_columnMap["Tpy"], wxEmptyString);
            SetItemText(item, m_columnMap["Tpp"], wxEmptyString);
            SetItemText(item, m_columnMap["cam_trans"], wxEmptyString);
        };

        if(m_groupMode==GROUP_LENS)
        {
            SetItemText(item, m_columnMap["projection"], getProjectionString(img));
            SetItemText(item, m_columnMap["response"], getResponseString(img));
        }
        else
        {
            SetItemText(item, m_columnMap["projection"], wxEmptyString);
            SetItemText(item, m_columnMap["response"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.HFOVisLinked() || haveSingleChild))
        {
            SetItemText(item, m_columnMap["v"], hugin_utils::doubleTowxString(img.getHFOV(), m_degDigits));
        }
        else
        {
            SetItemText(item, m_columnMap["v"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.RadialDistortionisLinked() || haveSingleChild))
        {
            std::vector<double> dist=img.getRadialDistortion();
            SetItemText(item, m_columnMap["a"], hugin_utils::doubleTowxString(dist[0],m_distDigits));
            SetItemText(item, m_columnMap["b"], hugin_utils::doubleTowxString(dist[1],m_distDigits));
            SetItemText(item, m_columnMap["c"], hugin_utils::doubleTowxString(dist[2],m_distDigits));
        }
        else
        {
            SetItemText(item, m_columnMap["a"], wxEmptyString);
            SetItemText(item, m_columnMap["b"], wxEmptyString);
            SetItemText(item, m_columnMap["c"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.RadialDistortionCenterShiftisLinked() || haveSingleChild))
        {
            hugin_utils::FDiff2D p=img.getRadialDistortionCenterShift();
            SetItemText(item, m_columnMap["d"], hugin_utils::doubleTowxString(p.x,m_pixelDigits));
            SetItemText(item, m_columnMap["e"], hugin_utils::doubleTowxString(p.y,m_pixelDigits));
        }
        else
        {
            SetItemText(item, m_columnMap["d"], wxEmptyString);
            SetItemText(item, m_columnMap["e"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.ShearisLinked() || haveSingleChild))
        {
            hugin_utils::FDiff2D p=img.getShear();
            SetItemText(item, m_columnMap["g"], hugin_utils::doubleTowxString(p.x,m_distDigits));
            SetItemText(item, m_columnMap["t"], hugin_utils::doubleTowxString(p.y,m_distDigits));
        }
        else
        {
            SetItemText(item, m_columnMap["g"], wxEmptyString);
            SetItemText(item, m_columnMap["t"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.ExposureValueisLinked() || haveSingleChild))
        {
            SetItemText(item, m_columnMap["Eev"], hugin_utils::doubleTowxString(img.getExposureValue(), m_pixelDigits));
        }
        else
        {
            SetItemText(item, m_columnMap["Eev"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.WhiteBalanceRedisLinked() || haveSingleChild))
        {
            SetItemText(item, m_columnMap["Er"], hugin_utils::doubleTowxString(img.getWhiteBalanceRed(), m_pixelDigits+1));
        }
        else
        {
            SetItemText(item, m_columnMap["Er"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.WhiteBalanceBlueisLinked() || haveSingleChild))
        {
            SetItemText(item, m_columnMap["Eb"], hugin_utils::doubleTowxString(img.getWhiteBalanceBlue(), m_pixelDigits+1));
        }
        else
        {
            SetItemText(item, m_columnMap["Eb"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.RadialVigCorrCoeffisLinked() || haveSingleChild))
        {
            std::vector<double> dist=img.getRadialVigCorrCoeff();
            SetItemText(item, m_columnMap["Vb"], hugin_utils::doubleTowxString(dist[1],m_distDigits));
            SetItemText(item, m_columnMap["Vc"], hugin_utils::doubleTowxString(dist[2],m_distDigits));
            SetItemText(item, m_columnMap["Vd"], hugin_utils::doubleTowxString(dist[3],m_distDigits));
        }
        else
        {
            SetItemText(item, m_columnMap["Vb"], wxEmptyString);
            SetItemText(item, m_columnMap["Vc"], wxEmptyString);
            SetItemText(item, m_columnMap["Vd"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.RadialVigCorrCenterShiftisLinked() || haveSingleChild))
        {
            hugin_utils::FDiff2D p=img.getRadialVigCorrCenterShift();
            SetItemText(item, m_columnMap["Vx"], hugin_utils::doubleTowxString(p.x,m_distDigits));
            SetItemText(item, m_columnMap["Vy"], hugin_utils::doubleTowxString(p.y,m_distDigits));
        }
        else
        {
            SetItemText(item, m_columnMap["Vx"], wxEmptyString);
            SetItemText(item, m_columnMap["Vy"], wxEmptyString);
        };

        if (m_groupMode == GROUP_LENS && (img.EMoRParamsisLinked() || haveSingleChild))
        {
            std::vector<float> vec=img.getEMoRParams();
            SetItemText(item, m_columnMap["Ra"], hugin_utils::doubleTowxString(vec[0],m_distDigits));
            SetItemText(item, m_columnMap["Rb"], hugin_utils::doubleTowxString(vec[1],m_distDigits));
            SetItemText(item, m_columnMap["Rc"], hugin_utils::doubleTowxString(vec[2],m_distDigits));
            SetItemText(item, m_columnMap["Rd"], hugin_utils::doubleTowxString(vec[3],m_distDigits));
            SetItemText(item, m_columnMap["Re"], hugin_utils::doubleTowxString(vec[4],m_distDigits));
        }
        else
        {
            SetItemText(item, m_columnMap["Ra"], wxEmptyString);
            SetItemText(item, m_columnMap["Rb"], wxEmptyString);
            SetItemText(item, m_columnMap["Rc"], wxEmptyString);
            SetItemText(item, m_columnMap["Rd"], wxEmptyString);
            SetItemText(item, m_columnMap["Re"], wxEmptyString);
        };
    };
};

void ImagesTreeCtrl::UpdateGroup(wxTreeItemId parent, const HuginBase::UIntSet imgs, HuginBase::UIntSet& changed)
{
    size_t nrItems=GetChildrenCount(parent,false);
    bool forceUpdate=false;
    if(nrItems!=imgs.size())
    {
        forceUpdate=true;
        if(nrItems<imgs.size())
        {
            for(size_t i=nrItems;i<imgs.size();i++)
            {
                AppendItem(parent,wxEmptyString,-1,-1,new ImagesTreeData(-1));
            };
        }
        else
        {
            while(GetChildrenCount(parent,false)>imgs.size())
            {
                wxTreeItemIdValue cookie;
                wxTreeItemId item=GetLastChild(parent,cookie);
                Delete(item);
            };
        };
    };
    //now update values
    wxTreeItemIdValue cookie;
    wxTreeItemId item=GetFirstChild(parent,cookie);
    HuginBase::UIntSet::const_iterator it=imgs.begin();
    while(item.IsOk())
    {
        ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(item));
        if(it==imgs.end())
        {
            break;
        };
        bool needsUpdate=false;
        if(data->GetImgNr()!=*it)
        {
            data->SetImgNr(*it);
            needsUpdate=true;
        }
        else
        {
            if(set_contains(changed,*it))
            {
                needsUpdate=true;
            };
        };
        if(needsUpdate || forceUpdate)
        {
            UpdateImageText(item);
            changed.erase(*it);
        };
        item=GetNextChild(parent, cookie);
        ++it;
    };
};

void ImagesTreeCtrl::UpdateOptimizerVariables()
{
    HuginBase::OptimizeVector optVec=m_pano->getOptimizeVector();
    wxTreeItemIdValue cookie;
    wxTreeItemId item=GetFirstChild(m_root, cookie);
    while(item.IsOk())
    {
        ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(item));
        HuginBase::UIntSet imgNrs;
        if(data->IsGroup())
        {
            wxTreeItemIdValue childCookie;
            wxTreeItemId child=GetFirstChild(item, childCookie);
            while(child.IsOk())
            {
                data=static_cast<ImagesTreeData*>(GetItemData(child));
                imgNrs.insert(data->GetImgNr());
                child=GetNextChild(item, childCookie);
            };
        }
        else
        {
            imgNrs.insert(data->GetImgNr());
        };
        if(imgNrs.size()>0)
        {
            for(size_t i=0;i<GetColumnCount();i++)
            {
                if(set_contains(m_editableColumns,i))
                {
                    if (GetItemText(item, i).IsEmpty())
                    {
                        // item with no text can have no checkbox
                        // this can happen with linked variables
                        SetItemImage(item, i, -1);
                    }
                    else
                    {
                        bool opt=false;
                        for(HuginBase::UIntSet::const_iterator it=imgNrs.begin(); it!=imgNrs.end() && !opt;++it)
                        {
                            if(m_columnVector[i]=="cam_trans")
                            {
                                opt=set_contains(optVec[*it], "TrX") &&
                                    set_contains(optVec[*it], "TrY") &&
                                    set_contains(optVec[*it], "TrZ") &&
                                    set_contains(optVec[*it], "Tpy") &&
                                    set_contains(optVec[*it], "Tpp");
                            }
                            else
                            {
                                opt=set_contains(optVec[*it], m_columnVector[i]);
                            };
                        };
                        SetItemImage(item, i, opt ? 1 : 0);
                    };
                };
            };
        };
        item=GetNext(item);
    };
};

HuginBase::UIntSet ImagesTreeCtrl::GetSelectedImages()
{
    wxArrayTreeItemIds selected;
    HuginBase::UIntSet imgs;
    if(GetSelections (selected)>0)
    {
        for(size_t i=0;i<selected.size();i++)
        {
            ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(selected[i]));
            if(data->IsGroup())
            {
                wxTreeItemIdValue cookie;
                wxTreeItemId item=GetFirstChild(selected[i],cookie);
                while(item.IsOk())
                {
                    data=static_cast<ImagesTreeData*>(GetItemData(item));
                    imgs.insert(data->GetImgNr());
                    item=GetNextChild(selected[i], cookie);
                };
            }
            else
            {
                imgs.insert(data->GetImgNr());
            };
        };
    }
    return imgs;
}

void ImagesTreeCtrl::MarkActiveImages(const bool markActive)
{
    m_markDisabledImages = markActive;
    UpdateItemFont();
};

void ImagesTreeCtrl::OnColumnWidthChange( wxListEvent & e )
{
    if(m_configClassName != wxT(""))
    {
        int colNum = e.GetColumn();
        wxConfigBase::Get()->Write( m_configClassName+wxString::Format(wxT("/ColumnWidth%d"),colNum), GetColumnWidth(colNum) );
    }
}

void ImagesTreeCtrl::SetGuiLevel(GuiLevel newSetting)
{
    m_guiLevel=newSetting;
    //update visible column
    SetDisplayMode(m_displayMode);
};

void ImagesTreeCtrl::SetOptimizerMode()
{
    m_optimizerMode=true;
    // connnect events with handlers
    Bind(wxEVT_MOTION, &ImagesTreeCtrl::OnMouseMove, this);
    Bind(wxEVT_LEFT_DOWN, &ImagesTreeCtrl::OnLeftDown, this);
    //create bitmaps for different checkboxes state
    wxRendererNative& renderer = wxRendererNative::Get();
    const wxSize checkBoxSize = renderer.GetCheckBoxSize(this);
    wxImageList* checkboxImageList = new wxImageList(checkBoxSize.GetWidth(), checkBoxSize.GetHeight(), true, 0);
    wxBitmap checkBoxImage(checkBoxSize, 32);
    wxMemoryDC dc(checkBoxImage);
    // unchecked checkbox
    dc.Clear();
    renderer.DrawCheckBox(this, dc, wxRect(checkBoxSize));
    dc.SelectObject(wxNullBitmap);
    checkboxImageList->Add(checkBoxImage);
    // checked checkbox
    dc.SelectObject(checkBoxImage);
    dc.Clear();
    renderer.DrawCheckBox(this, dc, wxRect(checkBoxSize), wxCONTROL_CHECKED);
    dc.SelectObject(wxNullBitmap);
    checkboxImageList->Add(checkBoxImage);
    // mouse over unchecked checkbox
    dc.SelectObject(checkBoxImage);
    dc.Clear();
    renderer.DrawCheckBox(this, dc, wxRect(checkBoxSize), wxCONTROL_CURRENT);
    dc.SelectObject(wxNullBitmap);
    checkboxImageList->Add(checkBoxImage);
    // mouse over checked checkbox
    dc.SelectObject(checkBoxImage);
    dc.Clear();
    renderer.DrawCheckBox(this, dc, wxRect(checkBoxSize), wxCONTROL_CHECKED | wxCONTROL_CURRENT);
    dc.SelectObject(wxNullBitmap);
    checkboxImageList->Add(checkBoxImage);

    AssignImageList(checkboxImageList);
    // activate edit mode
    for(HuginBase::UIntSet::const_iterator it=m_editableColumns.begin(); it!=m_editableColumns.end(); ++it)
    {
        if(m_columnVector[*it]!="cam_trans")
        {
            SetColumnEditable(*it,true);
        };
    };
};

void ImagesTreeCtrl::SetGroupMode(GroupMode newGroup)
{
    if(newGroup!=m_groupMode)
    {
        m_groupMode=newGroup;
        if(m_groupMode==GROUP_NONE)
        {
            SetWindowStyle(GetWindowStyle() | wxTR_NO_LINES);
        }
        else
        {
            SetWindowStyle(GetWindowStyle() & ~wxTR_NO_LINES);
        };
        DeleteChildren(m_root);
        HuginBase::UIntSet imgs;
        if(m_pano->getNrOfImages()>0)
        {
            fill_set(imgs,0,m_pano->getNrOfImages()-1);
        };
        panoramaImagesChanged(*m_pano,imgs);
        ExpandAll(m_root);
    };
};

void ImagesTreeCtrl::SetDisplayMode(DisplayMode newMode)
{
    m_displayMode=newMode;

    SetColumnShown(m_columnMap["width"], m_displayMode==DISPLAY_GENERAL);
    SetColumnShown(m_columnMap["height"], m_displayMode==DISPLAY_GENERAL);
    SetColumnShown(m_columnMap["anchor"], m_displayMode==DISPLAY_GENERAL);
    SetColumnShown(m_columnMap["cps"], m_displayMode==DISPLAY_GENERAL);
    SetColumnShown(m_columnMap["lensNr"], m_displayMode==DISPLAY_GENERAL);
    SetColumnShown(m_columnMap["stackNr"], m_displayMode==DISPLAY_GENERAL && m_guiLevel>=GUI_ADVANCED);

    SetColumnShown(m_columnMap["maker"], m_displayMode==DISPLAY_EXIF);
    SetColumnShown(m_columnMap["model"], m_displayMode==DISPLAY_EXIF);
    SetColumnShown(m_columnMap["lens"], m_displayMode==DISPLAY_EXIF);
    SetColumnShown(m_columnMap["date"], m_displayMode==DISPLAY_EXIF);
    SetColumnShown(m_columnMap["focallength"], m_displayMode==DISPLAY_EXIF);
    SetColumnShown(m_columnMap["aperture"], m_displayMode==DISPLAY_EXIF);
    SetColumnShown(m_columnMap["time"], m_displayMode==DISPLAY_EXIF);
    SetColumnShown(m_columnMap["iso"], m_displayMode==DISPLAY_EXIF);

    SetColumnShown(m_columnMap["y"], m_displayMode==DISPLAY_POSITION);
    SetColumnShown(m_columnMap["p"], m_displayMode==DISPLAY_POSITION);
    SetColumnShown(m_columnMap["r"], m_displayMode==DISPLAY_POSITION);
    SetColumnShown(m_columnMap["TrX"], m_displayMode==DISPLAY_POSITION && m_guiLevel==GUI_EXPERT);
    SetColumnShown(m_columnMap["TrY"], m_displayMode==DISPLAY_POSITION && m_guiLevel==GUI_EXPERT);
    SetColumnShown(m_columnMap["TrZ"], m_displayMode==DISPLAY_POSITION && m_guiLevel==GUI_EXPERT);
    SetColumnShown(m_columnMap["Tpy"], m_displayMode==DISPLAY_POSITION && m_guiLevel==GUI_EXPERT);
    SetColumnShown(m_columnMap["Tpp"], m_displayMode==DISPLAY_POSITION && m_guiLevel==GUI_EXPERT);
    SetColumnShown(m_columnMap["cam_trans"], m_displayMode==DISPLAY_POSITION && m_guiLevel==GUI_EXPERT);

    SetColumnShown(m_columnMap["projection"], m_displayMode==DISPLAY_LENS);
    SetColumnShown(m_columnMap["v"], m_displayMode==DISPLAY_LENS);
    SetColumnShown(m_columnMap["a"], m_displayMode==DISPLAY_LENS);
    SetColumnShown(m_columnMap["b"], m_displayMode==DISPLAY_LENS);
    SetColumnShown(m_columnMap["c"], m_displayMode==DISPLAY_LENS);
    SetColumnShown(m_columnMap["d"], m_displayMode==DISPLAY_LENS);
    SetColumnShown(m_columnMap["e"], m_displayMode==DISPLAY_LENS);
    SetColumnShown(m_columnMap["g"], m_displayMode==DISPLAY_LENS && m_guiLevel>=GUI_EXPERT);
    SetColumnShown(m_columnMap["t"], m_displayMode==DISPLAY_LENS && m_guiLevel>=GUI_EXPERT);

    SetColumnShown(m_columnMap["Eev"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_IMAGES);
    SetColumnShown(m_columnMap["Er"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_IMAGES);
    SetColumnShown(m_columnMap["Eb"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_IMAGES);
    SetColumnShown(m_columnMap["Vb"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);
    SetColumnShown(m_columnMap["Vc"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);
    SetColumnShown(m_columnMap["Vd"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);
    SetColumnShown(m_columnMap["Vx"], (m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES) && m_guiLevel>=GUI_ADVANCED);
    SetColumnShown(m_columnMap["Vy"], (m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES) && m_guiLevel>=GUI_ADVANCED);
    SetColumnShown(m_columnMap["response"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);
    SetColumnShown(m_columnMap["Ra"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);
    SetColumnShown(m_columnMap["Rb"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);
    SetColumnShown(m_columnMap["Rc"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);
    SetColumnShown(m_columnMap["Rd"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);
    SetColumnShown(m_columnMap["Re"], m_displayMode==DISPLAY_PHOTOMETRICS || m_displayMode==DISPLAY_PHOTOMETRICS_LENSES);

    Refresh();
};

void ImagesTreeCtrl::OnContextMenu(wxTreeEvent & e)
{
    m_selectedColumn=e.GetInt();
    wxMenu menu;
    bool allowMenuExtension=true;
    if(e.GetItem().IsOk())
    {
        //click on item
        if(set_contains(m_editableColumns,m_selectedColumn))
        {
            bool emptyText=GetItemText(e.GetItem(),m_selectedColumn).IsEmpty();
            ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(e.GetItem()));
            if((m_groupMode==GROUP_LENS && m_variableVector[m_selectedColumn]!=HuginBase::ImageVariableGroup::IVE_Yaw) ||
               (m_groupMode==GROUP_STACK && m_variableVector[m_selectedColumn]==HuginBase::ImageVariableGroup::IVE_Yaw) )
            {
                if(data->IsGroup())
                {
                    if(emptyText)
                    {
                        menu.Append(ID_LINK,_("Link"));
                    }
                    else
                    {
                        menu.Append(ID_UNLINK, _("Unlink"));
                    };
                }
                else
                {
                    if(emptyText)
                    {
                        menu.Append(ID_UNLINK,_("Unlink"));
                    }
                    else
                    {
                        menu.Append(ID_LINK, _("Link"));
                    };
                };
                menu.AppendSeparator();
            };
            if(m_optimizerMode)
            {
                if(data->IsGroup() == emptyText)
                {
                    if(m_groupMode==GROUP_LENS && m_variableVector[m_selectedColumn]!=HuginBase::ImageVariableGroup::IVE_Yaw)
                    {
                        menu.Append(ID_SELECT_LENS_STACK, _("Select all for current lens"));
                        menu.Append(ID_UNSELECT_LENS_STACK, _("Unselect all for current lens"));
                    };
                    if(m_groupMode==GROUP_STACK && m_variableVector[m_selectedColumn]==HuginBase::ImageVariableGroup::IVE_Yaw)
                    {
                        menu.Append(ID_SELECT_LENS_STACK, _("Select all for current stack"));
                        menu.Append(ID_UNSELECT_LENS_STACK, _("Unselect all for current stack"));
                    };
                };
                if(m_columnVector[m_selectedColumn]!="cam_trans")
                {
                    menu.Append(ID_SELECT_ALL, _("Select all"));
                };
                menu.Append(ID_UNSELECT_ALL, _("Unselect all"));
                menu.AppendSeparator();
            }
        };
        menu.Append(ID_EDIT, _("Edit image variables..."));
        if (m_markDisabledImages)
        {
            const HuginBase::UIntSet selectedImages = GetSelectedImages();
            if (selectedImages.size() == 1)
            {
                if (m_pano->getImage(*selectedImages.begin()).getActive())
                {
                    menu.Append(ID_DEACTIVATE_IMAGE, _("Deactivate image"));
                }
                else
                {
                    menu.Append(ID_ACTIVATE_IMAGE, _("Activate image"));
                };
            }
            else
            {
                menu.Append(ID_ACTIVATE_IMAGE, _("Activate images"));
                menu.Append(ID_DEACTIVATE_IMAGE, _("Deactivate images"));
            };
        };
    }
    else
    {
        if(m_optimizerMode && set_contains(m_editableColumns, e.GetInt()))
        {
            if(m_columnVector[m_selectedColumn]!="cam_trans")
            {
                menu.Append(ID_SELECT_ALL, _("Select all"));
            };
            menu.Append(ID_UNSELECT_ALL, _("Unselect all"));
            allowMenuExtension=false;
        }
    };
    if(allowMenuExtension)
    {
        if(menu.GetMenuItemCount()>0)
        {
            menu.AppendSeparator();
        };
        int id=ID_OPERATION_START;
        m_menuOperation.clear();
        GenerateSubMenu(&menu, PanoOperation::GetImagesOperationVector(), id);
        wxMenu* subMenu=new wxMenu();
        GenerateSubMenu(subMenu, PanoOperation::GetLensesOperationVector(), id);
        if(subMenu->GetMenuItemCount()>0)
        {
            menu.Append(-1,_("Lens"), subMenu);
        }
        else
        {
            delete subMenu;
        };
        if(m_guiLevel>GUI_SIMPLE)
        {
            subMenu=new wxMenu();
            GenerateSubMenu(subMenu, PanoOperation::GetStacksOperationVector(), id);
            if(subMenu->GetMenuItemCount()>0)
            {
                menu.Append(-1,_("Stacks"), subMenu);
            }
            else
            {
                delete subMenu;
            };
        };
        subMenu=new wxMenu();
        GenerateSubMenu(subMenu, PanoOperation::GetControlPointsOperationVector(), id);
        if(subMenu->GetMenuItemCount()>0)
        {
            menu.Append(-1, _("Control points"), subMenu);
        }
        else
        {
            delete subMenu;
        };
        subMenu=new wxMenu();
        GenerateSubMenu(subMenu, PanoOperation::GetResetOperationVector(), id);
        if(subMenu->GetMenuItemCount()>0)
        {
            menu.Append(-1, _("Reset"), subMenu);
        }
        else
        {
            delete subMenu;
        };
    };
    if(menu.GetMenuItemCount()>0)
    {
        PopupMenu(&menu);
    };
    e.Skip();
};

void ImagesTreeCtrl::GenerateSubMenu(wxMenu* menu, PanoOperation::PanoOperationVector* operations, int& id)
{
    HuginBase::UIntSet imgs=GetSelectedImages();
    for(size_t i=0; i<operations->size(); i++)
    {
        if((*operations)[i]->IsEnabled(*m_pano, imgs, m_guiLevel))
        {
            menu->Append(id, (*operations)[i]->GetLabel());
            m_menuOperation[id]=(*operations)[i];
            id++;
        }
    };
}

void ImagesTreeCtrl::UpdateItemFont()
{
    const wxColour normalColour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    const wxColour disabledColour = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
    if (m_markDisabledImages)
    {
        wxTreeItemIdValue cookie;
        wxTreeItemId item = GetFirstChild(m_root, cookie);
        while (item.IsOk())
        {
            ImagesTreeData* itemData = static_cast<ImagesTreeData*>(GetItemData(item));
            bool isActive = true;
            if (itemData->IsGroup())
            {
                wxTreeItemIdValue childCookie;
                wxTreeItemId child = GetFirstChild(item, childCookie);
                while (child.IsOk())
                {
                    ImagesTreeData* childItemData = static_cast<ImagesTreeData*>(GetItemData(child));
                    isActive = m_pano->getImage(childItemData->GetImgNr()).getActive();
                    if (isActive)
                    {
                        break;
                    };
                    child = GetNextChild(item, childCookie);
                };
            }
            else
            {
                isActive = m_pano->getImage(itemData->GetImgNr()).getActive();
            };
            if (isActive)
            {
                SetItemTextColour(item, normalColour);
            }
            else
            {
                SetItemTextColour(item, disabledColour);
            };
            if(itemData->IsGroup())
            {
                wxTreeItemIdValue childCookie;
                wxTreeItemId child = GetFirstChild(item, childCookie);
                while (child.IsOk())
                {
                    ImagesTreeData* childItemData = static_cast<ImagesTreeData*>(GetItemData(child));
                    if (m_pano->getImage(childItemData->GetImgNr()).getActive())
                    {
                        SetItemTextColour(child, normalColour);
                    }
                    else
                    {
                        SetItemTextColour(child, disabledColour);
                    };
                    child = GetNextChild(item, childCookie);
                };
            };
            item = GetNextChild(m_root, cookie);
        };
    }
    else
    {
        wxTreeItemIdValue cookie;
        wxTreeItemId item = GetFirstChild(m_root, cookie);
        while (item.IsOk())
        {
            ImagesTreeData* itemData=static_cast<ImagesTreeData*>(GetItemData(item));
            SetItemTextColour(item, normalColour);
            if (itemData->IsGroup())
            {
                wxTreeItemIdValue childCookie;
                wxTreeItemId child = GetFirstChild(item, childCookie);
                while (child.IsOk())
                {
                    SetItemTextColour(child, normalColour);
                    child = GetNextChild(item, childCookie);
                };
            };
            item = GetNextChild(m_root, cookie);
        };
    };
};

void ImagesTreeCtrl::OnHeaderContextMenu(wxListEvent & e)
{
    m_selectedColumn=e.GetColumn();
    wxMenu menu;
    if(m_optimizerMode && set_contains(m_editableColumns, m_selectedColumn))
    {
        menu.Append(ID_SELECT_ALL, _("Select all"));
        menu.Append(ID_UNSELECT_ALL, _("Unselect all"));
        PopupMenu(&menu);
    };
    e.Skip();
}
void ImagesTreeCtrl::OnActivateImage(wxCommandEvent & e)
{
    HuginBase::UIntSet activeImages = m_pano->getActiveImages();
    const HuginBase::UIntSet selectedImages = GetSelectedImages();
    std::copy(selectedImages.begin(), selectedImages.end(), std::inserter(activeImages, activeImages.end()));
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetActiveImagesCmd(*m_pano, activeImages)
    );
};

void ImagesTreeCtrl::OnDeactivateImage(wxCommandEvent & e)
{
    HuginBase::UIntSet activeImages = m_pano->getActiveImages();
    const HuginBase::UIntSet selectedImages = GetSelectedImages();
    HuginBase::UIntSet newActiveImages;
    std::set_difference(activeImages.begin(), activeImages.end(), selectedImages.begin(), selectedImages.end(), std::inserter(newActiveImages, newActiveImages.end()));
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::SetActiveImagesCmd(*m_pano, newActiveImages)
    );
};

void ImagesTreeCtrl::UnLinkImageVariables(bool linked)
{
    HuginBase::UIntSet images=GetSelectedImages();
    if(images.size()>0 && m_variableVector[m_selectedColumn]!=HuginBase::ImageVariableGroup::IVE_Filename)
    {
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> variables;
        variables.insert(m_variableVector[m_selectedColumn]);
        if(m_variableVector[m_selectedColumn]==HuginBase::ImageVariableGroup::IVE_Yaw)
        {
            variables.insert(HuginBase::ImageVariableGroup::IVE_Pitch);
            variables.insert(HuginBase::ImageVariableGroup::IVE_Roll);
            variables.insert(HuginBase::ImageVariableGroup::IVE_X);
            variables.insert(HuginBase::ImageVariableGroup::IVE_Y);
            variables.insert(HuginBase::ImageVariableGroup::IVE_Z);
            variables.insert(HuginBase::ImageVariableGroup::IVE_TranslationPlaneYaw);
            variables.insert(HuginBase::ImageVariableGroup::IVE_TranslationPlanePitch);
        };
        if(m_groupMode==GROUP_LENS)
        {
            PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::ChangePartImagesLinkingCmd(*m_pano, images, variables, linked, HuginBase::StandardImageVariableGroups::getLensVariables())
            );
        }
        else
        {
            PanoCommand::GlobalCmdHist::getInstance().addCommand(
                new PanoCommand::ChangePartImagesLinkingCmd(*m_pano, images, variables, linked, HuginBase::StandardImageVariableGroups::getStackVariables())
            );
        };
    };
};

void ImagesTreeCtrl::OnLinkImageVariables(wxCommandEvent &e)
{
    UnLinkImageVariables(true);
};

void ImagesTreeCtrl::OnUnlinkImageVariables(wxCommandEvent &e)
{
    UnLinkImageVariables(false);
};

void ImagesTreeCtrl::OnEditImageVariables(wxCommandEvent &e)
{
    HuginBase::UIntSet imgs=GetSelectedImages();
    if(imgs.size()>0)
    {
        ImageVariableDialog dlg(this, m_pano, imgs);
        dlg.SetGuiLevel(m_guiLevel);
        switch(m_displayMode)
        {
            case DISPLAY_LENS:
                dlg.SelectTab(1);
                break;
            case DISPLAY_PHOTOMETRICS:
            case DISPLAY_PHOTOMETRICS_IMAGES:
            case DISPLAY_PHOTOMETRICS_LENSES:
                if(m_selectedColumn==m_columnMap["response"] ||
                    m_selectedColumn==m_columnMap["Ra"] ||
                    m_selectedColumn==m_columnMap["Rb"] ||
                    m_selectedColumn==m_columnMap["Rc"] ||
                    m_selectedColumn==m_columnMap["Rd"] ||
                    m_selectedColumn==m_columnMap["Re"] )
                {
                    dlg.SelectTab(3);
                }
                else
                {
                    dlg.SelectTab(2);
                };
                break;
            default:
                break;
        };
        dlg.ShowModal();
    };
};

void ImagesTreeCtrl::OnBeginDrag(wxTreeEvent &e)
{
    const bool ctrlPressed=wxGetKeyState(WXK_COMMAND);
    if(m_pano->getNrOfImages()>0 && !m_dragging)
    {
        m_draggingImages=GetSelectedImages();
        if(m_draggingImages.size()>0)
        {
            if((m_groupMode==GROUP_NONE && m_draggingImages.size()==1 && !ctrlPressed) ||
                m_groupMode==GROUP_LENS || m_groupMode==GROUP_STACK)
            {
                e.Allow();
                SetCursor(wxCURSOR_HAND);
                m_dragging=true;
            };
        };
    };
};

void ImagesTreeCtrl::OnLeftUp(wxMouseEvent &e)
{
    //we can't use wxEVT_TREE_END_DRAG because this event is fire several times, e.g. when
    // the mouse leaves the area of the tree
    // so we are listing to left mouse up, as described in documentation
    if(m_dragging)
    {
        SetCursor(wxCURSOR_ARROW);
        m_dragging=false;
        wxTreeItemId item=HitTest(e.GetPosition());
        if(m_groupMode==GROUP_NONE)
        {
            size_t img1=*m_draggingImages.begin();
            size_t img2=-1;
            if(item.IsOk())
            {
                ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(item));
                img2=data->GetImgNr();
            }
            else
            {
                //we are checking the points above, if we find then
                // an item, the user drop it below the last item, if not
                // the drop happened right beside the item
                wxPoint pos(e.GetPosition());
                pos.y-=10;
                while(pos.y>0)
                {
                    item=HitTest(pos);
                    if(item.IsOk())
                    {
                        img2=m_pano->getNrOfImages()-1;
                        break;
                    };
                    pos.y-=10;
                };
            };
            if(img2!=-1)
            {
                if(img1!=img2)
                {
                    PanoCommand::GlobalCmdHist::getInstance().addCommand(
                        new PanoCommand::MoveImageCmd(*m_pano, img1, img2)
                    );
                    // now update drag images groups in fast preview window, this information is not stored in Panorama class
                    HuginBase::UIntSet images = MainFrame::Get()->getGLPreview()->GetDragGroupImages();
                    if (!images.empty() && images.size() < m_pano->getNrOfImages())
                    {
                        std::vector<bool> imgList(m_pano->getNrOfImages(), false);
                        for (auto& i : images)
                        {
                            imgList[i] = true;
                        };
                        const bool moveImageChecked = imgList[img1];
                        imgList.erase(imgList.begin() + img1);
                        if (img2 < imgList.size())
                        {
                            imgList.insert(imgList.begin() + img2, moveImageChecked);
                        }
                        else
                        {
                            imgList.push_back(moveImageChecked);
                        };
                        images.clear();
                        for (size_t i = 0; i < imgList.size(); ++i)
                        {
                            if (imgList[i])
                            {
                                images.insert(i);
                            };
                        };
                        MainFrame::Get()->getGLPreview()->SetDragGroupImages(images, true);
                    };
                };
            };
        }
        else
        {
            //dragging to stack/lenses
            if(item.IsOk())
            {
                ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(item));
                long groupNr=-1;
                if(data->IsGroup())
                {
                    groupNr=data->GetGroupNr();
                }
                else
                {
                    item=GetItemParent(item);
                    if(item.IsOk())
                    {
                        data=static_cast<ImagesTreeData*>(GetItemData(item));
                        groupNr=data->GetGroupNr();
                    };
                };
                if(groupNr>=0)
                {
                    if(m_groupMode==GROUP_LENS)
                    {
                        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                            new PanoCommand::ChangePartNumberCmd(*m_pano, m_draggingImages, groupNr, HuginBase::StandardImageVariableGroups::getLensVariables())
                        );
                    }
                    else
                    {
                        PanoCommand::GlobalCmdHist::getInstance().addCommand(
                            new PanoCommand::ChangePartNumberCmd(*m_pano, m_draggingImages, groupNr, HuginBase::StandardImageVariableGroups::getStackVariables())
                        );
                    };
                };
            }
            else // item not ok, drop not on existing item, create new lens/stack
            {
                if(m_groupMode==GROUP_LENS)
                {
                    PanoCommand::GlobalCmdHist::getInstance().addCommand(
                        new PanoCommand::NewPartCmd(*m_pano, m_draggingImages, HuginBase::StandardImageVariableGroups::getLensVariables())
                    );
                }
                else
                {
                    PanoCommand::GlobalCmdHist::getInstance().addCommand(
                        new PanoCommand::NewPartCmd(*m_pano, m_draggingImages, HuginBase::StandardImageVariableGroups::getStackVariables())
                    );
                }
            };
        }
        m_draggingImages.clear();
    }
    else
    {
        if (m_optimizerMode && m_leftDownItem.IsOk())
        {
            int flags;
            int col;
            wxTreeItemId item = HitTest(e.GetPosition(), flags, col);
            // check if left up is on the same item as left down
            if (item.IsOk() && item == m_leftDownItem && col == m_leftDownColumn && (flags & wxTREE_HITTEST_ONITEMICON))
            {
                HuginBase::UIntSet imgs;
                const bool markSelected = IsSelected(m_leftDownItem);
                if (markSelected)
                {
                    // if the user clicked on one of the selected pass the selection to all selected images
                    imgs = GetSelectedImages();
                }
                else
                {
                    // the user clicked on a non-selected images, work only on this image
                    // or all images of the clicked lens/stack
                    ImagesTreeData* data = static_cast<ImagesTreeData*>(GetItemData(m_leftDownItem));
                    if (data->IsGroup())
                    {
                        wxTreeItemIdValue cookie;
                        wxTreeItemId childItem = GetFirstChild(m_leftDownItem, cookie);
                        while (childItem.IsOk())
                        {
                            data = static_cast<ImagesTreeData*>(GetItemData(childItem));
                            imgs.insert(data->GetImgNr());
                            childItem = GetNextChild(m_leftDownItem, cookie);
                        };
                    }
                    else
                    {
                        imgs.insert(data->GetImgNr());
                    };
                };
                HuginBase::OptimizeVector optVec = m_pano->getOptimizeVector();
                std::set<std::string> var;
                if (m_columnVector[m_leftDownColumn] == "cam_trans")
                {
                    var.insert("TrX");
                    var.insert("TrY");
                    var.insert("TrZ");
                    var.insert("Tpy");
                    var.insert("Tpp");
                }
                else
                {
                    var.insert(m_columnVector[m_leftDownColumn]);
                    if (m_columnVector[m_leftDownColumn] == "Tpy" || m_columnVector[m_leftDownColumn] == "Tpp")
                    {
                        var.insert("Tpy");
                        var.insert("Tpp");
                    };
                    if (m_columnVector[m_leftDownColumn] == "Vb" || m_columnVector[m_leftDownColumn] == "Vc" || m_columnVector[m_leftDownColumn] == "Vd")
                    {
                        var.insert("Vb");
                        var.insert("Vc");
                        var.insert("Vd");
                    };
                    if (m_columnVector[m_leftDownColumn] == "Vx" || m_columnVector[m_leftDownColumn] == "Vy")
                    {
                        var.insert("Vx");
                        var.insert("Vy");
                    };
                    if (m_columnVector[m_leftDownColumn] == "Ra" || m_columnVector[m_leftDownColumn] == "Rb" || m_columnVector[m_leftDownColumn] == "Rc" ||
                        m_columnVector[m_leftDownColumn] == "Rd" || m_columnVector[m_leftDownColumn] == "Re")
                    {
                        var.insert("Ra");
                        var.insert("Rb");
                        var.insert("Rc");
                        var.insert("Rd");
                        var.insert("Re");
                    };
                };
                bool deactivate = false;
                if (markSelected)
                {
                    // check which state the clicked image have
                    deactivate = GetItemImage(m_leftDownItem, m_leftDownColumn) & 1;
                }
                else
                {
                    for (std::set<std::string>::const_iterator varIt = var.begin(); varIt != var.end(); ++varIt)
                    {
                        //search, if image variable is marked for optimise for at least one image of group
                        for (HuginBase::UIntSet::const_iterator imgIt = imgs.begin(); imgIt != imgs.end() && !deactivate; ++imgIt)
                        {
                            if (set_contains(optVec[*imgIt], *varIt))
                            {
                                deactivate = true;
                            };
                        };
                    };
                };
                // now deactivate or activate the image variable for optimisation
                if (deactivate)
                {
                    for (std::set<std::string>::const_iterator varIt = var.begin(); varIt != var.end(); ++varIt)
                    {
                        for (HuginBase::UIntSet::const_iterator imgIt = imgs.begin(); imgIt != imgs.end(); ++imgIt)
                        {
                            optVec[*imgIt].erase(*varIt);
                        };
                    }
                }
                else
                {
                    for (std::set<std::string>::const_iterator varIt = var.begin(); varIt != var.end(); ++varIt)
                    {
                        for (HuginBase::UIntSet::const_iterator imgIt = imgs.begin(); imgIt != imgs.end(); ++imgIt)
                        {
                            optVec[*imgIt].insert(*varIt);
                        };
                    };
                };
                PanoCommand::GlobalCmdHist::getInstance().addCommand(
                    new PanoCommand::UpdateOptimizeVectorCmd(*m_pano, optVec)
                );
                m_leftDownItem.Unset();
                return;
            };
        };
        if (m_leftDownItem.IsOk())
        {
            m_leftDownItem.Unset();
        };
        e.Skip();
    };
};

void ImagesTreeCtrl::OnLeftDown(wxMouseEvent &e)
{
    if (!m_dragging)
    {
        if (e.LeftDown())
        {
            // force end editing
            EndEdit(false);
            // find where user clicked
            int flags;
            int col;
            wxTreeItemId item = HitTest(e.GetPosition(), flags, col);
            if (item.IsOk() && (flags & wxTREE_HITTEST_ONITEMICON))
            {
                if (set_contains(m_editableColumns, col))
                {
                    if (!GetItemText(item, col).IsEmpty())
                    {
                        // store for later and stop processing this event further
                        m_leftDownItem = item;
                        m_leftDownColumn = col;
                        return;
                    };
                };
            };
        };
    };
    e.Skip();
};

void ImagesTreeCtrl::OnMouseMove(wxMouseEvent &e)
{
    int flags;
    int col;
    wxTreeItemId item = HitTest(e.GetPosition(), flags, col);
    if (item.IsOk() && (flags & wxTREE_HITTEST_ONITEMICON))
    {
        // mouse moved over optimizer checkbox
        int imgNr = GetItemImage(item, col);
        if (imgNr >= 0 && imgNr < 2)
        {
            // update checkbox image and store position for later
            SetItemImage(item, col, imgNr + 2);
            m_lastCurrentItem = item;
            m_lastCurrentCol = col;
        };
    }
    else
    {
        // mouse moved from checkbox away, so update images
        if (m_lastCurrentItem.IsOk())
        {
            int imgNr = GetItemImage(m_lastCurrentItem, m_lastCurrentCol);
            if (imgNr > 1)
            {
                SetItemImage(m_lastCurrentItem, m_lastCurrentCol, imgNr - 2);
            };
            m_lastCurrentItem.Unset();
        };
    };
    e.Skip();
};

void ImagesTreeCtrl::SelectAllParameters(bool select, bool allImages)
{
    std::set<std::string> imgVars;
    std::string var=m_columnVector[m_selectedColumn];
    if(var=="cam_trans")
    {
        imgVars.insert("TrX");
        imgVars.insert("TrY");
        imgVars.insert("TrZ");
        imgVars.insert("Tpy");
        imgVars.insert("Tpp");
    }
    else
    {
        imgVars.insert(var);
        if(var=="Vb" || var=="Vc" || var=="Vd")
        {
            imgVars.insert("Vb");
            imgVars.insert("Vc");
            imgVars.insert("Vd");
        };
        if(var=="Vx" || var=="Vy")
        {
            imgVars.insert("Vx");
            imgVars.insert("Vy");
        };
        if(var=="Ra" || var=="Rb" || var=="Rc" || var=="Rd" || var=="Re")
        {
            imgVars.insert("Ra");
            imgVars.insert("Rb");
            imgVars.insert("Rc");
            imgVars.insert("Rd");
            imgVars.insert("Re");
        };

    };

    HuginBase::UIntSet imgs;
    if(allImages)
    {
        fill_set(imgs, 0, m_pano->getNrOfImages()-1);
    }
    else
    {
        wxArrayTreeItemIds selectedItem;
        if(GetSelections(selectedItem)>0)
        {
            for(size_t i=0;i<selectedItem.size();i++)
            {
                ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(selectedItem[i]));
                wxTreeItemId startItem;
                if(data->IsGroup())
                {
                    startItem=selectedItem[i];
                }
                else
                {
                    startItem=GetItemParent(selectedItem[i]);
                };
                wxTreeItemIdValue cookie;
                wxTreeItemId item=GetFirstChild(startItem,cookie);
                while(item.IsOk())
                {
                    data=static_cast<ImagesTreeData*>(GetItemData(item));
                    imgs.insert(data->GetImgNr());
                    item=GetNextChild(startItem, cookie);
                };
            };
        };
    };

    HuginBase::OptimizeVector optVec = m_pano->getOptimizeVector();
    for(HuginBase::UIntSet::iterator img=imgs.begin(); img!=imgs.end(); ++img)
    {
        for(std::set<std::string>::const_iterator it=imgVars.begin(); it!=imgVars.end(); ++it)
        {
            if(select)
            {
                if((*it=="y" || *it=="p" || *it=="r" || *it=="TrX" || *it=="TrY" || *it=="TrZ" || *it=="Tpy" || *it=="Tpp") && 
                    *img==m_pano->getOptions().optimizeReferenceImage)
                {
                    optVec[*img].erase(*it);
                    continue;
                };
                if((*it=="Eev" || *it=="Er" || *it=="Eb") && *img==m_pano->getOptions().colorReferenceImage)
                {
                    optVec[*img].erase(*it);
                    continue;
                };
                optVec[*img].insert(*it);
            }
            else
            {
                optVec[*img].erase(*it);
            };
        };
    };
    PanoCommand::GlobalCmdHist::getInstance().addCommand(
        new PanoCommand::UpdateOptimizeVectorCmd(*m_pano, optVec)
    );
};

void ImagesTreeCtrl::OnSelectAll(wxCommandEvent &e)
{
    SelectAllParameters(true, true);
};

void ImagesTreeCtrl::OnUnselectAll(wxCommandEvent &e)
{
    SelectAllParameters(false, true);
};

void ImagesTreeCtrl::OnSelectLensStack(wxCommandEvent &e)
{
    SelectAllParameters(true, false);
};

void ImagesTreeCtrl::OnUnselectLensStack(wxCommandEvent &e)
{
    SelectAllParameters(false, false);
};

void ImagesTreeCtrl::OnChar(wxTreeEvent &e)
{
    switch(e.GetKeyCode())
    {
        case WXK_INSERT:
            {
                wxCommandEvent ev(wxEVT_COMMAND_MENU_SELECTED, XRCID("action_add_images"));
                MainFrame::Get()->GetEventHandler()->AddPendingEvent(ev);
                break;
            };
        case WXK_DELETE:
            {
                HuginBase::UIntSet imgs=GetSelectedImages();
                if(imgs.size()>0)
                {
                    PanoCommand::GlobalCmdHist::getInstance().addCommand(
                        new PanoCommand::RemoveImagesCmd(*m_pano, imgs)
                    );
                };
                break;
            };
#if defined __WXMAC__
        case 'A':
        case 'a':
            // check for cmd+A -> select all
            if (e.GetExtraLong() == wxMOD_CMD)
            {
                SelectAll();
            };
            break;
#endif
        case 1: //Ctrl+A
            {
                SelectAll();
                break;
            }
        default:
            e.Skip();
    };
};

void ImagesTreeCtrl::OnBeginEdit(wxTreeEvent &e)
{
    m_editOldString=GetItemText(e.GetItem(), e.GetInt());
    if(m_editOldString.IsEmpty())
    {
        e.Veto();
    }
    else
    {
        ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(e.GetItem()));
        if(data->IsGroup())
        {
            wxTreeItemIdValue cookie;
            wxTreeItemId item=GetFirstChild(e.GetItem(), cookie);
            data=static_cast<ImagesTreeData*>(GetItemData(item));
        };
        m_editVal=m_pano->getImage(data->GetImgNr()).getVar(m_columnVector[e.GetInt()]);
        SetItemText(e.GetItem(), e.GetInt(), hugin_utils::doubleTowxString(m_editVal));
        e.Allow();
    };
};

void ImagesTreeCtrl::OnEndEdit(wxTreeEvent &e)
{
    if(e.IsEditCancelled())
    {
        //restore old string
        SetItemText(e.GetItem(), e.GetInt(), m_editOldString);
        Refresh();
    }
    else
    {
        double val;
        if(!hugin_utils::str2double(e.GetLabel(),val))
        {
            //restore old string
            SetItemText(e.GetItem(), e.GetInt(), m_editOldString);
            Refresh();
            e.Veto();
        }
        else
        {
            //only update if value was changed
            if(val!=m_editVal)
            {
                ImagesTreeData* data=static_cast<ImagesTreeData*>(GetItemData(e.GetItem()));
                if(data->IsGroup())
                {
                    wxTreeItemIdValue cookie;
                    wxTreeItemId item=GetFirstChild(e.GetItem(), cookie);
                    data=static_cast<ImagesTreeData*>(GetItemData(item));
                };
                HuginBase::UIntSet imgs;
                imgs.insert(data->GetImgNr());
                if(m_columnVector[e.GetInt()]=="v")
                {
                    if(m_pano->getImage(data->GetImgNr()).getProjection()==HuginBase::SrcPanoImage::FISHEYE_ORTHOGRAPHIC && val>190)
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
                            //restore old string
                            SetItemText(e.GetItem(), e.GetInt(), m_editOldString);
                            Refresh();
                            e.Veto();
                            e.Skip();
                            return;
                        };
                    };
                };
                HuginBase::Variable var(m_columnVector[e.GetInt()], val);
                PanoCommand::GlobalCmdHist::getInstance().addCommand(
                    new PanoCommand::SetVariableCmd(*m_pano, imgs, var)
                );
                //we need to veto the event otherwise the internal
                //function does update the text to the full number
                e.Veto();
            }
            else
            {
                //restore old string
                SetItemText(e.GetItem(), e.GetInt(), m_editOldString);
                Refresh();
            };
        };
    };
    e.Skip();
};

void ImagesTreeCtrl::OnExecuteOperation(wxCommandEvent & e)
{
    PanoOperation::PanoOperation* op=m_menuOperation[e.GetId()];
    PanoCommand::PanoCommand* cmd=op->GetCommand(this,*m_pano, GetSelectedImages(), m_guiLevel);
    if(cmd!=NULL)
    {
        PanoCommand::GlobalCmdHist::getInstance().addCommand(cmd);
    };
};

void ImagesTreeCtrl::OnLeftDblClick(wxMouseEvent &e)
{
    if(!m_optimizerMode)
    {
        int flags;
        int col;
        wxTreeItemId item=HitTest(e.GetPosition(), flags, col);
        m_selectedColumn=col;
        wxCommandEvent commandEvent(wxEVT_COMMAND_MENU_SELECTED, ID_EDIT);
        GetEventHandler()->AddPendingEvent(commandEvent);
    };
    e.Skip();
};

IMPLEMENT_DYNAMIC_CLASS(ImagesTreeCtrl, wxTreeListCtrl)

ImagesTreeCtrlXmlHandler::ImagesTreeCtrlXmlHandler()
    : wxTreeListCtrlXmlHandler()
{
}

wxObject *ImagesTreeCtrlXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(cp, ImagesTreeCtrl)

    cp->Create(m_parentAsWindow,
               GetID(),
               GetPosition(), GetSize(),
               GetStyle(wxT("style")),
               GetName());

    SetupWindow( cp);

    return cp;
}

bool ImagesTreeCtrlXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("ImagesTreeList"));
}

IMPLEMENT_DYNAMIC_CLASS(ImagesTreeCtrlXmlHandler, wxTreeListCtrlXmlHandler)

      
