/**
 * @file AssistantExecutor.h
 * @brief create a CommandQueue for running the assistant using CLI tools
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

#ifndef ASSISTANTEXECUTOR_H
#define ASSISTANTEXECUTOR_H

#include <hugin_shared.h>
#include <panodata/Panorama.h>
#include "Executor.h"

namespace HuginQueue
{
    /** generates the command queue for running the assistant 
        @param[in] pano contains to panorama structure for which the queue should generated
        @param[in] ExePath base path to all used utilities
        @param[in] project name of the project file, the assistant modifies the given file
        @return pointer to CommandQueue
    */
    WXIMPEX CommandQueue* GetAssistantCommandQueue(const HuginBase::Panorama & pano, const wxString& ExePath, const wxString& project);
    /** generates the command queue for running the assistant
    @param[in] pano contains to panorama structure for which the queue should generated
    @param[in] ExePath base path to all used utilities
    @param[in] project name of the project file, the assistant modifies the given file
    @param[in] assistantSetting assistant file from which the settings should be reads
    @return pointer to CommandQueue
    */
    WXIMPEX CommandQueue* GetAssistantCommandQueueUserDefined(const HuginBase::Panorama & pano, const wxString& ExePath, const wxString& project, const wxString& assistantSetting);
}; // namespace 

#endif
