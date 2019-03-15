// -*- c-basic-offset: 4 -*-

/** @file ParseExp.h
 *
 *  @brief function to parse expressions from strings
 *
 *  @author T. Modes
 *
 */

/*  This program is free software; you can redistribute it and/or
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

#ifndef PARSEEXP_H
#define PARSEEXP_H
#include <string>
#include <vector>
#include <iostream>
#include <hugin_shared.h>
#include "hugin_config.h"
#include "panodata/Panorama.h"

namespace Parser
{
/** struct to save parsed variables and optional image numbers */
struct IMPEX ParseVar
{
    std::string varname;
    // contains image number or -1 if it applies to all images
    int imgNr;
    std::string expression;
    bool flag;
    ParseVar();
};
typedef std::vector<ParseVar> ParseVarVec;

/** parse string s and store result in ParseVar var
 *  @return true, if a valid image variable was given */
IMPEX bool ParseVarNumber(const std::string&s, Parser::ParseVar& var);
/** parse complete variables string */
IMPEX void ParseVariableString(ParseVarVec& parseVec, const std::string& input, std::ostream& errorStream, void(*func)(ParseVarVec&, const std::string&, std::ostream&));

/** parses the given expression and apply the changes to the Panorama */
IMPEX void PanoParseExpression(HuginBase::Panorama& pano, const std::string& expression, std::ostream& statusStream = std::cout, std::ostream& errorStream = std::cerr);
};

#endif