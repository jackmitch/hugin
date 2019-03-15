// -*- c-basic-offset: 4 -*-
/** @file CPEditorPanel.h
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

#ifndef _CPEDITORPANEL_H
#define _CPEDITORPANEL_H


//-----------------------------------------------------------------------------
// Headers
//-----------------------------------------------------------------------------

#include <vector>
#include <set>
#include <functional>
#include <utility>
#include <string>

// Celeste files
#include "Celeste.h"
#include "CelesteGlobals.h"
#include "Utilities.h"

#include <panodata/Panorama.h>

#include "CPImagesComboBox.h"

#include "CPImageCtrl.h"
#include <panotools/PanoToolsInterface.h>

namespace vigra {
    class Diff2D;
}

namespace vigra_ext{
    struct CorrelationResult;
}

/** control point editor panel.
 *
 *  This panel is used to create/change/edit control points
 *
 *  @todo support control lines
 */
class CPEditorPanel : public wxPanel, public HuginBase::PanoramaObserver
{
public:
    /** ctor.
     */
    CPEditorPanel();

    bool Create(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxT("panel"));

    void Init(HuginBase::Panorama * pano);

    /** dtor.
     */
    virtual ~CPEditorPanel();

    /// set left image
    void setLeftImage(unsigned int imgNr);
    /// set right image
    void setRightImage(unsigned int imgNr);

    void SetPano(HuginBase::Panorama * panorama)
        { m_pano = panorama; };

    /** called when the panorama changes and we should
     *  update our display
     */
    void panoramaChanged(HuginBase::Panorama &pano);
    void panoramaImagesChanged(HuginBase::Panorama &pano, const HuginBase::UIntSet & imgNr);


    /** Select a point.
     *
     *  This should highlight it in the listview and on the pictures.
     *
     *  Does not change the pictures. The control point must be on the
     *  two existing images
     */
    void SelectGlobalPoint(unsigned int globalNr);

    /** show a control point
     *
     *  show control point @p cpNr and the corrosponding images
     */
    void ShowControlPoint(unsigned int cpNr);

    /** unselect a point */
    void ClearSelection();

private:

    /** updates the display after another image has been selected.
     *  updates control points, and other widgets
     */
    void UpdateDisplay(bool newImages);

    /** enable or disable controls for editing other points */
    void EnablePointEdit(bool state);

    /** select a local point.
     *
     *  @todo scroll windows so that the point is centred
     */
    void SelectLocalPoint(unsigned int LVpointNr);

    /// map a global point nr to a local one, if possible
    bool globalPNr2LocalPNr(unsigned int & localNr, unsigned int globalNr) const;
    /// find a local point
    unsigned int localPNr2GlobalPNr(unsigned int localNr) const;

    // function called when a new point has been selected or changed
    // in one of your images
    void NewPointChange(hugin_utils::FDiff2D p, bool left);
//    void CreateNewPointRight(wxPoint p);

    /// this is used to finally create the point in the panorama model
    void CreateNewPoint();

    /// search for region in destImg
//    bool FindTemplate(unsigned int tmplImgNr, const wxRect &region, unsigned int dstImgNr, vigra_ext::CorrelationResult & res);

    bool PointFineTune(unsigned int tmplImgNr,
                       const vigra::Diff2D &tmplPoint,
                       int tmplWidth,
                       unsigned int subjImgNr,
                       const hugin_utils::FDiff2D &subjPoint,
                       int searchWidth,
                       vigra_ext::CorrelationResult & tunedPos);

    const float getVerticalCPBias();
    // event handler functions
    void OnCPEvent(CPEvent &ev);
    void OnLeftChoiceChange(wxCommandEvent & e);
    void OnRightChoiceChange(wxCommandEvent & e);
    void OnCPListSelect(wxListEvent & e);
    void OnCPListDeselect(wxListEvent & e);
    void OnAddButton(wxCommandEvent & e);
    void OnZoom(wxCommandEvent & e);
    void OnTextPointChange(wxCommandEvent &e);
    void OnKey(wxKeyEvent & e);
    void OnDeleteButton(wxCommandEvent & e);
    void OnPrevImg(wxCommandEvent & e);
    void OnNextImg(wxCommandEvent & e);

    void OnColumnWidthChange( wxListEvent & e );

    void OnFineTuneButton(wxCommandEvent & e);
    void OnActionButton(wxCommandEvent& e);
    void OnCreateCPButton(wxCommandEvent& e);
    void OnCelesteButton(wxCommandEvent & e);
    void OnCleanCPButton(wxCommandEvent& e);

    void OnActionSelectCreate(wxCommandEvent& e);
    void OnActionSelectCeleste(wxCommandEvent& e);
    void OnActionSelectCleanCP(wxCommandEvent& e);
    void OnActionContextMenu(wxContextMenuEvent& e);

    void FineTuneSelectedPoint(bool left);
    void FineTuneNewPoint(bool left);
    // local fine tune
    hugin_utils::FDiff2D LocalFineTunePoint(unsigned int srcNr,
                               const vigra::Diff2D & srcPnt,
                               hugin_utils::FDiff2D & movedSrcPnt,
                               unsigned int moveNr,
                               const hugin_utils::FDiff2D & movePnt);

    /** Estimate position of point in the other image
     *
     *  @param p point to warp to other image
     *  @param left true if p is located in left image.
     */
    hugin_utils::FDiff2D EstimatePoint(const hugin_utils::FDiff2D & p, bool left);


    /** the state machine for point selection:
     *  it is set to the current selection
     */
    enum CPCreationState { NO_POINT,  ///< no point selected
                           LEFT_POINT, ///< point in left image selected
                           RIGHT_POINT, ///< selected point in right image
                           RIGHT_POINT_RETRY, ///< point in left image selected, finetune failed in right image
                           LEFT_POINT_RETRY,  ///< right point, finetune for left point failed
                           BOTH_POINTS_SELECTED ///< left and right point selected, waiting for add point.
    };
    // used to change the point selection state
    void changeState(CPCreationState newState);

    /** estimate and set point in other image */
    void estimateAndAddOtherPoint(const hugin_utils::FDiff2D & p,
                                  bool left,
                                  CPImageCtrl * thisImg,
                                  unsigned int thisImgNr,
                                  CPCreationState THIS_POINT,
                                  CPCreationState THIS_POINT_RETRY,
                                  CPImageCtrl * otherImg,
                                  unsigned int otherImgNr,
                                  CPCreationState OTHER_POINT,
                                  CPCreationState OTHER_POINT_RETRY);

    /** calculate rotation required for upright image display from roll, pitch and yaw angles */
    CPImageCtrl::ImageRotation GetRot(double yaw, double roll, double pitch);
    /** updated the internal transform object for drawing line in controls */
    void UpdateTransforms();

    CPCreationState cpCreationState;

    enum CPTabActionButtonMode
    {
        CPTAB_ACTION_CREATE_CP = 0,
        CPTAB_ACTION_CELESTE = 1,
        CPTAB_ACTION_CLEAN_CP = 2
    };
    CPTabActionButtonMode m_cpActionButtonMode;
    wxMenu* m_cpActionContextMenu;
    wxString m_currentCPDetector;
    // GUI controls
    CPImagesComboBox *m_leftChoice;
    CPImagesComboBox *m_rightChoice;
    CPImageCtrl *m_leftImg, *m_rightImg;
    wxListCtrl *m_cpList;

    wxTextCtrl *m_x1Text, *m_y1Text, *m_x2Text, *m_y2Text, *m_errorText;
    wxChoice *m_cpModeChoice;
    wxButton *m_addButton;
    wxButton *m_delButton;
    wxButton* m_actionButton;
    wxCheckBox *m_autoAddCB;
    wxCheckBox *m_fineTuneCB;
    wxCheckBox *m_estimateCB;
    wxPanel *m_cp_ctrls;

    // my data
    HuginBase::Panorama * m_pano;
    // the current images
    unsigned int m_leftImageNr;
    unsigned int m_rightImageNr;
    std::string m_leftFile;
    std::string m_rightFile;
    bool m_listenToPageChange;
    double m_detailZoomFactor;
    // store transformation for drawing line control points
    HuginBase::PTools::Transform m_leftTransform;
    HuginBase::PTools::Transform m_leftInvTransform;
    HuginBase::PTools::Transform m_rightTransform;
    HuginBase::PTools::Transform m_rightInvTransform;

    unsigned int m_selectedPoint;

    // contains the control points shown currently.
    HuginBase::CPointVector currentPoints;
    // this set contains all points that are mirrored (point 1 in right window,
    // point 2 in left window), in local point numbers
    std::set<unsigned int> mirroredPoints;
    size_t m_countCP;

    CPImageCtrl::ImageRotation m_leftRot;
    CPImageCtrl::ImageRotation m_rightRot;

    // needed for receiving events.
    DECLARE_EVENT_TABLE();

    DECLARE_DYNAMIC_CLASS(CPEditorPanel)

};

/** function for fine-tune with remapping to stereographic projection */
vigra_ext::CorrelationResult PointFineTuneProjectionAware(const HuginBase::SrcPanoImage& templ, const vigra::UInt8RGBImage& templImg,
    vigra::Diff2D templPos, int templSize,
    const HuginBase::SrcPanoImage& search, const vigra::UInt8RGBImage& searchImg,
    vigra::Diff2D searchPos, int sWidth);

/** xrc handler */
class CPEditorPanelXmlHandler : public wxXmlResourceHandler
{
    DECLARE_DYNAMIC_CLASS(CPEditorPanelXmlHandler)

public:
    CPEditorPanelXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};

#endif // _CPEDITORPANEL_H
