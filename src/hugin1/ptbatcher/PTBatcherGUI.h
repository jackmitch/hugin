// -*- c-basic-offset: 4 -*-

/** @file PTBatcherGUI.h
 *
 *  @brief Batch processor for Hugin with GUI
 *
 *  @author Marko Kuder <marko.kuder@gmail.com>
 *
 *  $Id: PTBatcherGUI.h 3448 2008-09-23 3:42:07 mkuder $
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

#include "RunStitchFrame.h"
#include "Batch.h"
#include "BatchFrame.h"

#include <wx/dir.h>
#include <wx/wfstream.h>
#include <wx/filefn.h>
#include <wx/snglinst.h>
#include <wx/ipc.h>

#include <hugin_config.h>
#include <wx/cmdline.h>

#ifndef FRAMEARRAY
#define FRAMEARRAY
WX_DEFINE_ARRAY_PTR(RunStitchFrame*,FrameArray);
#endif

#ifndef PTBATCHERGUI_H
#define PTBATCHERGUI_H
// **********************************************************************

/** class for communication between different PTBatcherGUI instances
 *
 * this class is used to transfer the commandline parameters of the second instance of PTBatcherGUI
 * to the first and only running instance of PTBatcherGUI
*/
class BatchIPCConnection : public wxConnection
{
public:
    /** request handler for transfer */
    virtual const void* OnRequest(const wxString& topic, const wxString& item, size_t* size = NULL, wxIPCFormat format = wxIPC_TEXT);
};

/** server which implements the communication between different PTBatcherGUI instances (see BatchIPCConnection) */
class BatchIPCServer : public wxServer
{
public:
    /**accept connection handler (establish the connection) */
    virtual wxConnectionBase* OnAcceptConnection (const wxString& topic);
};

/** topic name for BatchIPCConnection and BatchIPCServer */
const wxString IPC_START(wxT("BatchStart"));

/** The application class for hugin_stitch_project
 *
 *  it contains the main frame.
 */
class PTBatcherGUI : public wxApp
{
public:
    /** pseudo constructor. with the ability to fail gracefully.
     */
    virtual bool OnInit();
    virtual int OnExit();
#if wxUSE_ON_FATAL_EXCEPTION
#if wxCHECK_VERSION(3,1,0)
    virtual void OnFatalException() wxOVERRIDE;
#else
    virtual void OnFatalException();
#endif
#endif

    //Handles some input keys for the frame
    void OnItemActivated(wxListEvent& event);
    void OnKeyDown(wxKeyEvent& event);

    //Main batch list
    ProjectArray projList;
    //List of projects in progress (their RunStitchFrames)
    FrameArray stitchFrames;
    BatchFrame* GetFrame()
    {
        return m_frame;
    };

#ifdef __WXMAC__
    /** the wx calls this method when the app gets "Open files" AppleEvent */
    void MacOpenFiles(const wxArrayString &fileNames);
#endif

private:
    BatchFrame* m_frame;
    wxLocale m_locale;
    wxString m_xrcPrefix;
    wxSingleInstanceChecker* m_checker;
    BatchIPCServer* m_server;

    DECLARE_EVENT_TABLE()
};

DECLARE_APP(PTBatcherGUI)

#endif //PTBATCHERGUI_H
