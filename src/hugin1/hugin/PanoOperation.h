// -*- c-basic-offset: 4 -*-
/**  @file PanoOperation.h
 *
 *  @brief Definition of PanoOperation class
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

#ifndef PANOOPERATION_H
#define PANOOPERATION_H

#include "panoinc_WX.h"
#include "panoinc.h"
#include <vector>
#include "icpfind/CPDetectorConfig.h"
#include "GuiLevel.h"
#include "base_wx/PanoCommand.h"

namespace PanoOperation
{
/** base class for different PanoOperations 
  * derived classes should overwrite protected PanoOperation::GetInternalCommand to implement the operation
  */
class PanoOperation
{
public:
    /** virtual destructor, does currently nothing */
    virtual ~PanoOperation() {};
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    /** returns the appropriate PanoCommand::PanoCommand to be inserted into GlobalCmdHistory, checks if operation is enabled
      * @returns pointer to valid PanoCommand::PanoCommand or NULL if not enabled*/
    virtual PanoCommand::PanoCommand* GetCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    /** main working function, overwrite it in derived classes */
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images)=0;
    GuiLevel m_guiLevel;
};

/** PanoOperation which works only with one selected image */
class PanoSingleImageOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
};

/** PanoOperation with works with at least one image */
class PanoMultiImageOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
};

/** PanoOperation to add several user selected images to the panorama */
class AddImageOperation : public PanoOperation
{
public:
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to add all image in a defined timeinterval to the panorama */
class AddImagesSeriesOperation : public PanoOperation
{
public:
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to modify image variables by parsing an expression*/
class ImageVariablesExpressionOperation : public PanoOperation
{
public:
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
};

/** PanoOperation to remove selected images */
class RemoveImageOperation : public PanoMultiImageOperation
{
public:
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to change anchor image */
class ChangeAnchorImageOperation : public PanoSingleImageOperation
{
public:
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to change exposure anchor image */
class ChangeColorAnchorImageOperation : public PanoSingleImageOperation
{
public:
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to assign new lens */
class NewLensOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to change lens number */
class ChangeLensOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to load lens from ini file or lens database*/
class LoadLensOperation : public PanoMultiImageOperation
{
public:
    LoadLensOperation(bool fromDatabase);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
private:
    bool m_fromDatabase;
};

/** PanoOperation to save lens to ini file or database */
class SaveLensOperation : public PanoSingleImageOperation
{
public:
    SaveLensOperation(bool toDatabase);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
private:
    // if true: save to database, false: save to ini
    bool m_database;
};

/** PanoOperation to remove control points */
class RemoveControlPointsOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to clean control points with statistically method */
class CleanControlPointsOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to reset image variables */
class ResetOperation : public PanoOperation
{
public:
    enum ResetMode
    {
        RESET_DIALOG=0,
        RESET_POSITION,
        RESET_TRANSLATION,
        RESET_LENS,
        RESET_PHOTOMETRICS,
        RESET_DIALOG_LENS,
        RESET_DIALOG_PHOTOMETRICS,
    };
    ResetOperation(ResetMode newResetMode);
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
private:
    bool ShowDialog(wxWindow* parent);
    ResetMode m_resetMode;
    bool m_resetPos;
    bool m_resetTranslation;
    bool m_resetHFOV;
    bool m_resetLens;
    int m_resetExposure;
    bool m_resetVignetting;
    int m_resetColor;
    bool m_resetCameraResponse;
};

/** PanoOperation to clean control points with Celeste */
class CelesteOperation : public CleanControlPointsOperation
{
public:
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to assign new stack */
class NewStackOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to change lens number */
class ChangeStackOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

/** PanoOperation to assigns stacks */
class AssignStacksOperation : public PanoOperation
{
public:
    /** return true, if operation is enabled with the given image set */
    virtual bool IsEnabled(HuginBase::Panorama& pano, HuginBase::UIntSet images, GuiLevel guiLevel);
    virtual wxString GetLabel();
protected:
    virtual PanoCommand::PanoCommand* GetInternalCommand(wxWindow* parent, HuginBase::Panorama& pano, HuginBase::UIntSet images);
};

typedef std::vector<PanoOperation*> PanoOperationVector;

/** generates the PanoOperationVector for context menu */
void GeneratePanoOperationVector();
/** clears the PanoOperationVector */
void CleanPanoOperationVector();

/** returns list of PanoOperation for work with images */
PanoOperationVector* GetImagesOperationVector();
/** returns list of PanoOperation for work with lenses */
PanoOperationVector* GetLensesOperationVector();
/** returns list of PanoOperation for stacks */
PanoOperationVector* GetStacksOperationVector();
/** returns list of PanoOperation for work with control points */
PanoOperationVector* GetControlPointsOperationVector();
/** returns list of PanoOperation for resetting */
PanoOperationVector* GetResetOperationVector();

} //namespace
#endif