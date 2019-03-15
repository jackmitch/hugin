// -*- c-basic-offset: 4 -*-
/** @file ImagesTree.h
 *
 *  @brief declaration of main image tree control 
 *
 *  @author T. Modes
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

#ifndef _IMAGESTREE_H
#define _IMAGESTREE_H

#include <map>
#include "panodata/Panorama.h"
#include <panodata/StandardImageVariableGroups.h>
#include "treelistctrl.h"
#include "GuiLevel.h"
#include "hugin/PanoOperation.h"

/** the main images tree control, used on images and optimizer tabs */
class ImagesTreeCtrl: public wxcode::wxTreeListCtrl, public HuginBase::PanoramaObserver
{
public:
    /** enumeration for grouping mode */
    enum GroupMode
    {
        GROUP_NONE=0,
        GROUP_LENS=1,
        GROUP_STACK=2,
        GROUP_OUTPUTLAYERS=3,
        GROUP_OUTPUTSTACK=4,
    };
    /** enumeration for display mode, limits the displayed columns */
    enum DisplayMode
    {
        DISPLAY_GENERAL=0,
        DISPLAY_EXIF=1,
        DISPLAY_POSITION=2,
        DISPLAY_LENS=3,
        DISPLAY_PHOTOMETRICS=4,
        DISPLAY_PHOTOMETRICS_IMAGES=32,
        DISPLAY_PHOTOMETRICS_LENSES=33,
    };
    /** general constructor */
    ImagesTreeCtrl();

    /** creates the control */
    bool Create(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxT("panel"));

    /** initialization, connects all control with Panorama, register observer */
    void Init(HuginBase::Panorama * pano);
   
    /** destructor */
    virtual ~ImagesTreeCtrl(void) ;

    /** sets the group mode to given mode */
    void SetGroupMode(GroupMode newMode);
    /** sets the display mode to given mode */
    void SetDisplayMode(DisplayMode newMode);

    /** sets the GuiLevel of the control */
    void SetGuiLevel(GuiLevel newSetting);
    /** sets to control into optimizer mode
     *  
     *  this marks the variables, which should be optimized, bold and underlined,
     *  also activates the context menu for optimizer */
    void SetOptimizerMode();
    /** receives notification about panorama changes */
    virtual void panoramaChanged(HuginBase::Panorama & pano);
    /** receive the update signal and update display accordingly
     */
    void panoramaImagesChanged(HuginBase::Panorama &pano, const HuginBase::UIntSet & imgNr);
    /** returns the selected images
     *
     *  if a lens or stack is selected it returns all images of the selected lens/stack */
    HuginBase::UIntSet GetSelectedImages();
    /** sets the flag, if active/disabled image should be marked with different colour */
    void MarkActiveImages(const bool markActive);

protected:
    /** updates the information for the given image in tree */
    void UpdateImageText(wxTreeItemId item);
    /** updates the information fot the given lens/stack in the tree */
    void UpdateGroupText(wxTreeItemId item);
    /** updates the given group, updates number of images and the images itself */
    void UpdateGroup(wxTreeItemId parent, const HuginBase::UIntSet imgs, HuginBase::UIntSet& changed);
    /** event handler, when column width was changed, save into wxConfig */
    void OnColumnWidthChange(wxListEvent & e );
    /** event handler for linking image variables */
    void OnLinkImageVariables(wxCommandEvent &e);
    /** event handler for unlinking image variables */
    void OnUnlinkImageVariables(wxCommandEvent &e);
    /** event handler for showing image variables editing dialog */
    void OnEditImageVariables(wxCommandEvent &e);
    /** event handler when dragging begins, veto if dragging is not possible */
    void OnBeginDrag(wxTreeEvent &e);
    /** event handler for left up, handles end of dragging and updates of optimizer variables states */
    void OnLeftUp(wxMouseEvent &e);
    /** event handler for left mouse down, handles toggle of optimizer variables */
    void OnLeftDown(wxMouseEvent &e);
    /** event handler for mouse motion, handles focussing of check boxes */
    void OnMouseMove(wxMouseEvent &e);
    /** event handler for left double click */
    void OnLeftDblClick(wxMouseEvent &e);
    /** event handler for select all optimizer variables */
    void OnSelectAll(wxCommandEvent &e);
    /** event handler for unselect all optimizer variables */
    void OnUnselectAll(wxCommandEvent &e);
    /** event handler for select all optimizer variables for selected lens/stack*/
    void OnSelectLensStack(wxCommandEvent &e);
    /** event handler for unselect all optimizer variables for selected lens/stack */
    void OnUnselectLensStack(wxCommandEvent &e);
    /** event handler for key events */
    void OnChar(wxTreeEvent &e);
    /** event handler for beginning editing */
    void OnBeginEdit(wxTreeEvent &e);
    /** event handler for ending editing, updates the Panorama with modified value */
    void OnEndEdit(wxTreeEvent &e);
    /** menu event handler for PanoOperation (context menu items) */
    void OnExecuteOperation(wxCommandEvent & e);
    /** event handler to display context menu */
    void OnContextMenu(wxTreeEvent & e);
    /** event handler for context menu on header */
    void OnHeaderContextMenu(wxListEvent & e);
    /** event handler for activate image */
    void OnActivateImage(wxCommandEvent& e);
    /** event handler for deactivate image */
    void OnDeactivateImage(wxCommandEvent& e);

private:
    /** creates all columns and stores information in m_columnMap, m_columnVector, m_editableColumns and m_variableVector */
    void CreateColumns();
    /** helper procedure for link/unlink image variables */
    void UnLinkImageVariables(bool linked);
    /** select/unselect all variables in the active column 
      *  @select true selects all, false unselect all
      *  @allImages true works on all images, false only on images of current lens or stack
      */
    void SelectAllParameters(bool select, bool allImages);
    /** updates the display of the optimizer variables (set font) */
    void UpdateOptimizerVariables();
    /** generates submenu for given PanoOperationVector */
    void GenerateSubMenu(wxMenu* menu, PanoOperation::PanoOperationVector* operations, int& id);
    /** update the font colour for all items */
    void UpdateItemFont();

    // the model
    HuginBase::Panorama * m_pano;
    /** the active group mode */
    GroupMode m_groupMode;
    
    // image variable group information
    HuginBase::StandardImageVariableGroups * m_variable_groups;

    // number of digits for display
    int m_degDigits;
    int m_distDigits;
    int m_pixelDigits;
    /** stores the active GuiLevel */
    GuiLevel m_guiLevel;
    /** true, if in optimizer mode */
    bool m_optimizerMode;
    /** true, if disabled images should be marked with other font color */
    bool m_markDisabledImages;
    /** the active display mode */
    DisplayMode m_displayMode;
    /** map for easier access to column information */
    std::map<std::string,size_t> m_columnMap;
    /** vector for easier access to column information */
    std::vector<std::string> m_columnVector;
    /** set, which contains editable columns (all column which contains numeric image variables */
    HuginBase::UIntSet m_editableColumns;
    /** vector for easier access to linking information */
    std::vector<HuginBase::ImageVariableGroup::ImageVariableEnum> m_variableVector;
    /** map with current active context menu PanoOperation */
    std::map<int,PanoOperation::PanoOperation*> m_menuOperation;
    /** selected column */
    size_t m_selectedColumn;
    /** UIntSet of dragging images */
    HuginBase::UIntSet m_draggingImages;
    /** true, if dragging */
    bool m_dragging;
    /** value, which is currently edited */
    double m_editVal;
    /** wxString, as shown before editing started */
    wxString m_editOldString;
    /** helper variable for update of output stacks/layers */
    bool m_needsUpdate;

    /** pointer to root item, not shown */
    wxTreeItemId m_root;

    /** stores last item on which the mouse was hovering */
    wxTreeItemId m_lastCurrentItem;
    long m_lastCurrentCol;
    /** stores where left mouse click happend */
    wxTreeItemId m_leftDownItem;
    long m_leftDownColumn;

    //for saving column width
    wxString m_configClassName;
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(ImagesTreeCtrl)
};


/** xrc handler */
class ImagesTreeCtrlXmlHandler : public wxcode::wxTreeListCtrlXmlHandler
{
    DECLARE_DYNAMIC_CLASS(ImagesTreeCtrlXmlHandler)

    public:
        ImagesTreeCtrlXmlHandler();
        virtual wxObject *DoCreateResource();
        virtual bool CanHandle(wxXmlNode *node);
};

#endif // _IMAGESTREE_H
