/**
 * @file Executor.h
 * @brief basic classes and function for queuing commands in wxWidgets
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

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <hugin_shared.h>
#include <vector>
#include <wx/string.h>
#include <wx/config.h>
#include "base_wx/wxPlatform.h"

namespace HuginQueue
{

    /** normal command for queue, processing is stopped if an error occurred in program */
    class WXIMPEX NormalCommand 
    {
    public:
        NormalCommand(wxString prog, wxString args, wxString comment=wxEmptyString) : m_prog(prog), m_args(args), m_comment(comment) {};
        virtual ~NormalCommand() {};
        virtual bool Execute(bool dryRun);
        virtual bool CheckReturnCode() const;
        virtual wxString GetCommand() const;
        wxString GetComment() const;
    protected:
        wxString m_prog;
        wxString m_args;
        wxString m_comment;
    };

    /** optional command for queue, processing of queue is always continued, also if an error occurred */
    class OptionalCommand : public NormalCommand
    {
    public:
        OptionalCommand(wxString prog, wxString args, wxString comment=wxEmptyString) : NormalCommand(prog, args, comment) {};
        virtual bool Execute(bool dryRun);
        virtual bool CheckReturnCode() const;
    };

    typedef std::vector<NormalCommand*> CommandQueue;

    /** execute the given, set environment variable OMP_NUM_THREADS to threads (ignored for 0) 
        after running the function the queue is cleared */
    WXIMPEX bool RunCommandsQueue(CommandQueue* queue, size_t threads, bool dryRun);
    /** clean the queue, delete all entries, but not the queue itself */
    WXIMPEX void CleanQueue(CommandQueue* queue);

    /** return path and name of external program, which comes bundled with Hugin */
    WXIMPEX wxString GetInternalProgram(const wxString& bindir, const wxString& name);
    /** return path and name of external program, which can be overwritten by the user */
    WXIMPEX wxString GetExternalProgram(wxConfigBase * config, const wxString& bindir, const wxString& name);

    /** convert double to wxString, it is always using a '.' as decimal separator */
    WXIMPEX wxString wxStringFromCDouble(double val, int precision=-1);
    
    /** special escaping routine for CommandQueues */
    template <class str>
    str wxEscapeFilename(const str & arg)
    {
#ifdef __WXMSW__
        // on Windows we return the string enclosed in quotes "
        // the quote itself is not allowed in filenames, so no further handling is required
        return str(wxT("\"")) + arg + str(wxT("\""));
#else
        // we use UNIX style escaping, escape all special chars with backslash
        return hugin_utils::wxQuoteStringInternal(arg, str(wxT("\\")), str(wxT("\\ ~$\"|'`{}[]()")));
#endif
    };
    
#ifdef __WXMSW__
    /** search for executable in program folder and in PATH, add exe extension if no one is given */
    WXIMPEX wxString MSWGetProgname(const wxString& bindir, const wxString& name);
#endif

    /** read a string from setting and remove all whitespaces */
    WXIMPEX const wxString GetSettingString(wxConfigBase* setting, const wxString& name, const wxString defaultValue = wxEmptyString);

}; //namespace

#endif
