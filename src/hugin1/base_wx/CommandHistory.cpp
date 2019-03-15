// -*- c-basic-offset: 4 -*-

/** @file CommandHistory.cpp
 *
 *  @brief implementation of CommandHistory Class
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id$
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

// standard include
#include "hugin_config.h"
#include "panoinc_WX.h"
#include "panoinc.h"
#include "CommandHistory.h"

#include "hugin/config_defaults.h"

namespace PanoCommand
{

    CommandHistory::CommandHistory()
        : nextCmd(0)
    {
    }

    CommandHistory::~CommandHistory()
    {
        std::vector<PanoCommand*>::iterator it;
        for (it = commands.begin(); it != commands.end(); ++it)
        {
            delete *it;
        };
    }

    void CommandHistory::clear()
    {
        std::vector<PanoCommand*>::iterator it;
        for (it = commands.begin(); it != commands.end(); ++it)
        {
            delete *it;
        };
        commands.clear();
        nextCmd = 0;
    }

    void CommandHistory::addCommand(PanoCommand *command, bool execute)
    {
        assert(command);
        if (nextCmd > commands.size())
        {
            DEBUG_FATAL("Invalid state in Command History: nextCmd:" << nextCmd
                << " size:" << commands.size());
        }
        else
        {
            if (nextCmd < (commands.size()))
            {
                // case: there were redoable commands, remove them now, the
                // current command has invalidated them.
                size_t nrDelete = commands.size() - nextCmd;
                for (size_t i = 0; i < nrDelete; i++)
                {
                    delete commands.back();
                    commands.pop_back();
                }
            };
        };
        commands.push_back(command);
        nextCmd++;

        if (execute)
        {
            // execute command
            command->execute();
        };
    }

    void CommandHistory::undo()
    {
        if (nextCmd > 0)
        {
            // undo the current command
            DEBUG_DEBUG("undo: " << commands[nextCmd - 1]->getName());
            // change nextCmd before the panorama, so panorama changed listeners get
            // correct results from canUndo() and canRedo().
            nextCmd--;
            commands[nextCmd]->undo();

            // smart undo: keep undoing simple visibility toggles according to user preference
            bool t = (wxConfigBase::Get()->Read(wxT("smartUndo"), HUGIN_SMART_UNDO) != 0);
            if (t)
            {
                while ((commands[nextCmd]->getName() == "change active images") && (nextCmd > 0))
                {
                    commands[nextCmd - 1]->undo();
                    nextCmd--;
                }
            };
            // TODO: reestablish visibility based on preferences
        }
        else
        {
            DEBUG_ERROR("no command in undo history");
        }
    }

    void CommandHistory::redo()
    {
        if (nextCmd < commands.size())
        {
            DEBUG_DEBUG("redo: " << commands[nextCmd]->getName());
            nextCmd++;
            commands[nextCmd - 1]->execute();
            // smart redo: keep redoing simple visibility toggles according to user preference
            bool t = (wxConfigBase::Get()->Read(wxT("smartUndo"), HUGIN_SMART_UNDO) != 0);
            if (t)
            {
                while ((nextCmd < commands.size()) && (commands[nextCmd]->getName() == "change active images"))
                {
                    commands[nextCmd]->execute();
                    nextCmd++;
                }
            }
            // TODO: reestablish visibility based on preferences
        }
        else
        {
            DEBUG_ERROR("no command in redo history");
        }
    }

    bool CommandHistory::canUndo() const
    {
        return nextCmd > 0;
    }

    bool CommandHistory::canRedo() const
    {
        return nextCmd < commands.size();
    }

    std::string CommandHistory::getLastCommandName() const
    {
        if (canUndo())
        {
            return commands[nextCmd-1]->getName();
        }
        return std::string();
    }

    // ======================================================================
    // ======================================================================


    GlobalCmdHist * GlobalCmdHist::instance = 0;

    GlobalCmdHist::GlobalCmdHist()
    {
    }

    GlobalCmdHist & GlobalCmdHist::getInstance()
    {
        if (!instance)
        {
            instance = new GlobalCmdHist();
        };
        return *instance;
    }
}
