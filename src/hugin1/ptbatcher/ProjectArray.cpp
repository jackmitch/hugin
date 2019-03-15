// -*- c-basic-offset: 4 -*-

/** @file ProjectArray.cpp
 *
 *  @brief Batch processor for Hugin
 *
 *  @author Marko Kuder <marko.kuder@gmail.com>
 *
 *  $Id: ProjectArray.cpp 3322 2008-08-16 5:00:07Z mkuder $
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

#include "ProjectArray.h"
#include <wx/arrimpl.cpp>
#include "base_wx/huginConfig.h"
#include <fstream>

long Project::idGenerator=1;

Project::Project(wxString pth,wxString pfx, Project::Target newTarget)
{
    id = Project::idGenerator;
    Project::idGenerator++;
    path = pth;
    prefix = pfx;
    wxFileName file(path);
    if(file.FileExists())
    {
        file.GetTimes(NULL,&modDate,NULL);
    }
    isAligned = true;
    options = ReadOptions(pth);
    skip = false;
    status = WAITING;
    target = newTarget;
}

Project::Project(wxString command)
{
    path = command;
    id = -Project::idGenerator;
    Project::idGenerator++;
    skip = false;
    isAligned = true;
    status = WAITING;
    target = STITCHING;
}

wxString Project::GetStatusText()
{
    switch(status)
    {
        case WAITING:
            return _("Waiting");
        case RUNNING:
            return _("In progress");
        case FINISHED:
            return _("Complete");
        case FAILED:
            return _("Failed");
        case MISSING:
            return _("File missing");
        case PAUSED:
            return _("Paused");
        default:
            return _T("");
    }
}

HuginBase::PanoramaOptions Project::ReadOptions(wxString projectFile)
{
    std::ifstream prjfile((const char*)projectFile.mb_str(HUGIN_CONV_FILENAME));
    if (prjfile.bad())
    {
        wxLogError( wxString::Format(_("could not open script: %s"), projectFile.c_str()) );
    }

    wxString pathToPTO;
    wxFileName::SplitPath(projectFile, &pathToPTO, NULL, NULL);
    pathToPTO.Append(wxT("/"));

    HuginBase::Panorama pano;
    HuginBase::PanoramaMemento newPano;
    int ptoVersion = 0;
    if (newPano.loadPTScript(prjfile, ptoVersion, (const char*)pathToPTO.mb_str(HUGIN_CONV_FILENAME)))
    {
        pano.setMemento(newPano);
        if (ptoVersion < 2)
        {
            HuginBase::PanoramaOptions opts = pano.getOptions();
            // no options stored in file, use default arguments in config
            opts.enblendOptions = wxConfigBase::Get()->Read(wxT("/Enblend/Args"), wxT("")).mb_str(wxConvLocal);
            opts.enfuseOptions = wxConfigBase::Get()->Read(wxT("/Enfuse/Args"), wxT("")).mb_str(wxConvLocal);
            pano.setOptions(opts);
        }
        // set default prefix, if not given
        if(prefix.IsEmpty())
        {
            wxFileName prefixFilename(getDefaultOutputName(projectFile, pano));
            prefixFilename.Normalize();
            prefix=prefixFilename.GetFullPath();
        };
        // if project contains at least 2 images and no control points add to assistant queue
        // a single image project is assumed as target for reprojection/remapping
        if(pano.getNrOfImages()>1 && pano.getNrOfCtrlPoints()==0)
        {
            isAligned=false;
        };
    }
    else
    {
        wxLogError( wxString::Format(_("error while parsing panotools script: %s"), projectFile.c_str()) );
    }
    // get options and correct for correct makefile
    HuginBase::PanoramaOptions opts = pano.getOptions();
    return opts;
}

void Project::ResetOptions()
{
    wxFileName file(path);
    if(file.FileExists())
    {
        file.GetTimes(NULL,&modDate,NULL);
    }
    options = ReadOptions(path);
}

WX_DEFINE_OBJARRAY(ProjectArray); //define the array in ProjectArray.h
