/**
* @file AssistantExecutor.cpp
* @brief implementation of CommandQueue for assistant
*
* @author T. Modes
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

#include "AssistantExecutor.h"

#include <wx/config.h>
#include <wx/translation.h>
#include <wx/fileconf.h>
#include <wx/wfstream.h>
#include "hugin_utils/utils.h"
#include "algorithms/optimizer/ImageGraph.h"
#include "base_wx/platform.h"
#include "base_wx/wxPlatform.h"
#include "hugin/config_defaults.h"


namespace HuginQueue
{
    CommandQueue* GetAssistantCommandQueue(const HuginBase::Panorama & pano, const wxString& ExePath, const wxString& project)
    {
        CommandQueue* commands=new CommandQueue();
        wxString quotedProject(wxEscapeFilename(project));

        //read main settings
        wxConfigBase* config = wxConfigBase::Get();
        const bool runCeleste = config->Read(wxT("/Celeste/Auto"), HUGIN_CELESTE_AUTO) != 0;
        double celesteThreshold;
        config->Read(wxT("/Celeste/Threshold"), &celesteThreshold, HUGIN_CELESTE_THRESHOLD);
        const bool celesteSmallRadius = config->Read(wxT("/Celeste/Filter"), HUGIN_CELESTE_FILTER) == 0;
        const bool runLinefind = (pano.getNrOfImages()==1) ? true : (config->Read(wxT("/Assistant/Linefind"), HUGIN_ASS_LINEFIND) != 0);
        const bool runCPClean = config->Read(wxT("/Assistant/AutoCPClean"), HUGIN_ASS_AUTO_CPCLEAN) != 0;
        double scale;
        config->Read(wxT("/Assistant/panoDownsizeFactor"), &scale, HUGIN_ASS_PANO_DOWNSIZE_FACTOR);

        bool runicp = (pano.getNrOfCtrlPoints() == 0);
        if (!runicp)
        {
            //we check, if all images are connected
            //if not, we run also icpfind
            HuginGraph::ImageGraph graph(pano);
            runicp = !graph.IsConnected();
        };
        //build commandline for icpfind
        if (runicp && pano.getNrOfImages() > 1)
        {
            //create cp find
            commands->push_back(new NormalCommand(GetInternalProgram(ExePath, wxT("icpfind")),
                wxT("-o ") + quotedProject + wxT(" ") + quotedProject, _("Searching for control points...")));
            //building celeste command
            if (runCeleste)
            {
                wxString args;
                args << wxT("-t ") << wxStringFromCDouble(celesteThreshold) << wxT(" ");
                if (celesteSmallRadius)
                {
                    args.Append(wxT("-r 1 "));
                }
                args.Append(wxT("-o ") + quotedProject + wxT(" ") + quotedProject);
                commands->push_back(new NormalCommand(GetInternalProgram(ExePath, wxT("celeste_standalone")),
                    args, _("Removing control points in clouds...")));
            };
            //building cpclean command
            if (runCPClean)
            {
                commands->push_back(new NormalCommand(GetInternalProgram(ExePath, wxT("cpclean")),
                    wxT("-o ") + quotedProject + wxT(" ") + quotedProject, _("Statistically cleaning of control points...")));
            };
        };
        //vertical line detector
        if (runLinefind)
        {
            const HuginBase::CPVector allCP = pano.getCtrlPoints();
            bool hasVerticalLines = false;
            if (allCP.size() > 0)
            {
                for (size_t i = 0; i < allCP.size() && !hasVerticalLines; i++)
                {
                    hasVerticalLines = (allCP[i].mode == HuginBase::ControlPoint::X);
                };
            };
            if (!hasVerticalLines)
            {
                commands->push_back(new NormalCommand(GetInternalProgram(ExePath, wxT("linefind")),
                    wxT("--output=") + quotedProject + wxT(" ") + quotedProject, _("Searching for vertical lines...")));
            };
        };
        //now optimise all
        commands->push_back(new NormalCommand(GetInternalProgram(ExePath, wxT("checkpto")), quotedProject));
        if (pano.getNrOfImages() == 1)
        {
            commands->push_back(new NormalCommand(GetInternalProgram(ExePath, wxT("autooptimiser")),
                wxT("-a -s -o ") + quotedProject + wxT(" ") + quotedProject, _("Optimizing...")));
        }
        else
        {
            commands->push_back(new NormalCommand(GetInternalProgram(ExePath, wxT("autooptimiser")),
                wxT("-a -m -l -s -o ") + quotedProject + wxT(" ") + quotedProject, _("Optimizing...")));
        };
        wxString panoModifyArgs;
        // if necessary scale down final pano
        if (scale <= 1.0)
        {
            panoModifyArgs << wxT("--canvas=") << hugin_utils::roundi(scale * 100) << wxT("% ");
        };
        panoModifyArgs.Append(wxT("--crop=AUTO --output=") + quotedProject + wxT(" ") + quotedProject);
        commands->push_back(new NormalCommand(GetInternalProgram(ExePath, wxT("pano_modify")),
            panoModifyArgs, _("Searching for best crop...")));
        return commands;
    }
    
    CommandQueue * GetAssistantCommandQueueUserDefined(const HuginBase::Panorama & pano, const wxString & ExePath, const wxString & project, const wxString & assistantSetting)
    {
        CommandQueue* commands = new CommandQueue;
        const wxString quotedProject(wxEscapeFilename(project));

        if (pano.getNrOfImages()==0)
        {
            std::cerr << "ERROR: Project contains no images. Nothing to do." << std::endl;
            return commands;
        };
        const wxString quotedImage0(wxEscapeFilename(wxString(pano.getImage(0).getFilename().c_str(), HUGIN_CONV_FILENAME)));
        wxFileInputStream input(assistantSetting);
        if (!input.IsOk())
        {
            std::cerr << "ERROR: Can not open file \"" << assistantSetting.mb_str(wxConvLocal) << "\"." << std::endl;
            return commands;
        }
        wxFileConfig settings(input);
        long stepCount;
        settings.Read(wxT("/General/StepCount"), &stepCount, 0);
        if (stepCount == 0)
        {
            std::cerr << "ERROR: User-setting does not define any assistant steps." << std::endl;
            return commands;
        };
        // create some condition variables
        const bool hascp = (pano.getNrOfCtrlPoints() > 0);
        bool isConnected = false;
        bool hasVerticalLines = false;
        if (hascp)
        {
            //we check, if all images are connected
            HuginGraph::ImageGraph graph(pano);
            isConnected = graph.IsConnected();
            // check, if there are line cp
            const HuginBase::CPVector allCP = pano.getCtrlPoints();
            if (!allCP.empty())
            {
                for (size_t i = 0; i < allCP.size() && !hasVerticalLines; i++)
                {
                    hasVerticalLines = (allCP[i].mode == HuginBase::ControlPoint::X);
                };
            };
        };

        for (size_t i = 0; i < stepCount; ++i)
        {
            wxString stepString(wxT("/Step"));
            stepString << i;
            if (!settings.HasGroup(stepString))
            {
                std::cerr << "ERROR: Assistant specifies " << stepCount << " steps, but step " << i << " is missing in configuration." << std::endl;
                CleanQueue(commands);
                return commands;
            }
            settings.SetPath(stepString);
            // check some conditions
            const wxString condition = GetSettingString(&settings, "Condition");
            if (condition.CmpNoCase("no cp") == 0 && hascp)
            {
                continue;
            };
            if (condition.CmpNoCase("not connected") == 0 && isConnected)
            {
                continue;
            };
            if (condition.CmpNoCase("no line cp") == 0 && hasVerticalLines)
            {
                continue;
            };
            if (condition.CmpNoCase("single image") == 0 && pano.getNrOfImages() != 1)
            {
                continue;
            };
            // read program name
            const wxString progName = GetSettingString(&settings, wxT("Program"));
            if (progName.IsEmpty())
            {
                std::cerr << "ERROR: Step " << i << " has no program name specified." << std::endl;
                CleanQueue(commands);
                return commands;
            };
#ifdef __WXMAC__
            // check if program can be found in bundle
            const wxString prog = GetExternalProgram(wxConfig::Get(), ExePath, progName);
#elif defined __WXMSW__

            const wxString prog = MSWGetProgname(ExePath, progName);
#else
            const wxString prog = progName;
#endif
            wxString args = GetSettingString(&settings, wxT("Arguments"));
            if (args.IsEmpty())
            {
                std::cerr << "ERROR: Step " << i << " has no arguments given." << std::endl;
                CleanQueue(commands);
                return commands;
            }
            args.Replace("%project%", quotedProject, true);
            args.Replace("%image0%", quotedImage0, true);
            const wxString description = GetSettingString(&settings, wxT("Description"));
            commands->push_back(new NormalCommand(prog, args, description));
        }
        return commands;
    };

}; // namespace