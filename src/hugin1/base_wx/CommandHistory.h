// -*- c-basic-offset: 4 -*-
/** @file CommandHistory.h
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

#ifndef _COMMANDHISTORY_H
#define _COMMANDHISTORY_H

#include <hugin_shared.h>
#include "Command.h"
#include <vector>

namespace PanoCommand
{
    /** A history for Command, provides undo/redo functionality.
     *
     *  To use this, all modifications to the model have to be done
     *  through commands that are executed with addCommand();
     */
    class WXIMPEX CommandHistory
    {
    public:

        /** ctor.
         */
        CommandHistory();

        /** dtor.
         */
        virtual ~CommandHistory();

        /**
         * Erases all the undo/redo history.
         * Use this when reloading the data, for instance, since this invalidates
         * all the commands.
         */
        void clear();

        /**
         * Adds a command to the history. Call this for each @p command you create.
         * Unless you set @p execute to false, this will also execute the command.
         * This means, most of the application's code will look like
         * \code
         *    MyCommand * cmd = new MyCommand(...);
         *    m_historyCommand.addCommand( cmd );
         * \endcode
         *
         * Ownership of @p command is transfered to CommandHistory
         */
        void addCommand(PanoCommand *command, bool execute = true);

        /**
         * Undoes the last action.
         */
        virtual void undo();
        /**
         * Redoes the last undone action.
         */
        virtual void redo();

        /// Return true iff there is a command to undo.
        bool canUndo() const;

        /// Return true iff there is a command to redo.
        bool canRedo() const;

        /** returns the name of the last command */
        std::string getLastCommandName() const;
    private:
        // our commands
        std::vector<PanoCommand*> commands;
        size_t nextCmd;

    };


    /** Singleton CommandHistory
     *
     *  for application that have only one subject that will receive
     *  commands
     */
    class WXIMPEX GlobalCmdHist : public CommandHistory
    {
    public:
        static GlobalCmdHist & getInstance();
    protected:
        GlobalCmdHist();
    private:
        static GlobalCmdHist * instance;
    };

} // namespace

#endif // _COMMANDHISTORY_H
