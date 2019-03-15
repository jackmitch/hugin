// -*- c-basic-offset: 4 -*-

/** @file pto_var.cpp
 *
 *  @brief program to manipulate variables for scripting
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

#include <fstream>
#include <sstream>
#include <getopt.h>
#include <panodata/Panorama.h>
#include <panodata/ImageVariableTranslate.h>
#include <panodata/ImageVariableGroup.h>
#include <panodata/StandardImageVariableGroups.h>
#include "hugin_utils/utils.h"
#include "panodata/ParseExp.h"

// parse a single variable and put result in struct ParseVar
void ParseSingleOptVar(Parser::ParseVarVec& varVec, const std::string& s, std::ostream& errorStream)
{
    // parse following regex ([!]?)([a-zA-Z]{1,3})(\\d*?) 
    std::string tempString(s);
    Parser::ParseVar var;
    var.flag = (tempString[0] == '!');
    if (var.flag)
    {
        tempString.erase(0, 1);
    };
    if (Parser::ParseVarNumber(tempString, var))
    {
        varVec.push_back(var);
    }
    else
    {
        errorStream << "The expression \"" << tempString << "\" is not a valid image variable." << std::endl;
    };
};

void ParseSingleLinkVar(Parser::ParseVarVec& varVec, const std::string& s, std::ostream& errorStream)
{
    // parse following regex ([a-zA-Z]{1,3})(\\d+?)
    Parser::ParseVar var;
    if (Parser::ParseVarNumber(s, var))
    {
        if (var.imgNr >= 0)
        {
            varVec.push_back(var);
        }
        else
        {
            errorStream << "The expression \"" << s << "\" does not contain a valid image number." << std::endl;
        }
    }
    else
    {
        errorStream << "The expression \"" << s << "\" is not a valid image variable." << std::endl;
    };
};

// adds given varname to optVec
// does some additional checking:
//   1. don't add y,p,r for anchor image
//   2. handle vignetting and EMoR parameters as group
void AddToOptVec(HuginBase::OptimizeVector& optVec, std::string varname, size_t imgNr,
                 std::set<size_t> refImgs, bool linkRefImgsYaw, bool linkRefImgsPitch, bool linkRefImgsRoll, std::vector<std::set<std::string> > groupedVars)
{
    if(varname=="y")
    {
        if(!set_contains(refImgs, imgNr) || linkRefImgsYaw)
        {
            optVec[imgNr].insert(varname);
        };
    }
    else
    {
        if(varname=="p")
        {
            if(!set_contains(refImgs, imgNr) || linkRefImgsPitch)
            {
                optVec[imgNr].insert(varname);
            };
        }
        else
        {
            if(varname=="r")
            {
                if(!set_contains(refImgs, imgNr) || linkRefImgsRoll)
                {
                    optVec[imgNr].insert(varname);
                };
            }
            else
            {
                if(varname=="TrX" || varname=="TrY" || varname=="TrZ" || varname=="Tpy" || varname=="Tpp")
                {
                    if(!set_contains(refImgs, imgNr))
                    {
                        optVec[imgNr].insert(varname);
                    };
                }
                else
                {
                    for(size_t i=0; i<groupedVars.size(); i++)
                    {
                        if(set_contains(groupedVars[i], varname))
                        {
                            for(std::set<std::string>::const_iterator it=groupedVars[i].begin(); it!=groupedVars[i].end(); ++it)
                            {
                                optVec[imgNr].insert(*it);
                            };
                            return;
                        };
                    };
                    optVec[imgNr].insert(varname);
                };
            };
        };
    };
};

// remove given variable from optvec, handle also correct grouped variables
void RemoveFromOptVec(HuginBase::OptimizeVector& optVec, std::string varname, size_t imgNr, std::vector<std::set<std::string> > groupedVars)
{
    for(size_t i=0; i<groupedVars.size(); i++)
    {
        if(set_contains(groupedVars[i], varname))
        {
            for(std::set<std::string>::const_iterator it=groupedVars[i].begin(); it!=groupedVars[i].end(); ++it)
            {
                optVec[imgNr].erase(*it);
            };
            return;
        };
    };
    optVec[imgNr].erase(varname);
};

// link or unlink the parsed image variables
void UnLinkVars(HuginBase::Panorama& pano, Parser::ParseVarVec parseVec, bool link)
{
    for(size_t i=0; i<parseVec.size(); i++)
    {
        //skip invalid image numbers
        if(parseVec[i].imgNr<0 || parseVec[i].imgNr>=(int)pano.getNrOfImages())
        {
            continue;
        };

        //convert to ImageVariableGroup::IVE_name format
        std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> variables;
#define image_variable( name, type, default_value ) \
    if (HuginBase::PTOVariableConverterFor##name::checkApplicability(parseVec[i].varname))\
    {\
        variables.insert(HuginBase::ImageVariableGroup::IVE_##name);\
    };
#include "panodata/image_variables.h"
#undef image_variable

        if(!variables.empty())
        {
            //lens variable
            if(set_contains(HuginBase::StandardImageVariableGroups::getLensVariables(), *variables.begin()))
            {
                HuginBase::ImageVariableGroup group(HuginBase::StandardImageVariableGroups::getLensVariables(), pano);
                if (link)
                {
                    std::cout << "Linking";
                    group.linkVariableImage(*variables.begin(), parseVec[i].imgNr);
                }
                else
                {
                    std::cout << "Unlinking";
                    group.unlinkVariableImage(*variables.begin(), parseVec[i].imgNr);
                    group.updatePartNumbers();
                }
                std::cout << " image variable " << parseVec[i].varname << " for image " << parseVec[i].imgNr << std::endl;
            }
            else
            {
                //stack variables
                // handle yaw, pitch, roll, TrX, TrY and TrZ always together
                if(set_contains(HuginBase::StandardImageVariableGroups::getStackVariables(), *variables.begin()))
                {
                    HuginBase::ImageVariableGroup group(HuginBase::StandardImageVariableGroups::getStackVariables(), pano);
                    if (link)
                    {
                        std::cout << "Linking";
                        group.linkVariableImage(HuginBase::ImageVariableGroup::IVE_Yaw, parseVec[i].imgNr);
                        group.linkVariableImage(HuginBase::ImageVariableGroup::IVE_Pitch, parseVec[i].imgNr);
                        group.linkVariableImage(HuginBase::ImageVariableGroup::IVE_Roll, parseVec[i].imgNr);
                        group.linkVariableImage(HuginBase::ImageVariableGroup::IVE_X, parseVec[i].imgNr);
                        group.linkVariableImage(HuginBase::ImageVariableGroup::IVE_Y, parseVec[i].imgNr);
                        group.linkVariableImage(HuginBase::ImageVariableGroup::IVE_Z, parseVec[i].imgNr);
                        group.linkVariableImage(HuginBase::ImageVariableGroup::IVE_TranslationPlaneYaw, parseVec[i].imgNr);
                        group.linkVariableImage(HuginBase::ImageVariableGroup::IVE_TranslationPlanePitch, parseVec[i].imgNr);
                    }
                    else
                    {
                        std::cout << "Unlinking";
                        group.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_Yaw, parseVec[i].imgNr);
                        group.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_Pitch, parseVec[i].imgNr);
                        group.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_Roll, parseVec[i].imgNr);
                        group.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_X, parseVec[i].imgNr);
                        group.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_Y, parseVec[i].imgNr);
                        group.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_Z, parseVec[i].imgNr);
                        group.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_TranslationPlaneYaw, parseVec[i].imgNr);
                        group.unlinkVariableImage(HuginBase::ImageVariableGroup::IVE_TranslationPlanePitch, parseVec[i].imgNr);
                        group.updatePartNumbers();
                    }
                    std::cout << " image position (ypr, TrXYZ, Tpyp) for image " << parseVec[i].imgNr << std::endl;
                }
                else
                {
                    std::cerr << "Warning: " << parseVec[i].varname << " is not a valid linkable variable." << std::endl;
                };
            };
        };
    };
};

// parse number, take an optional % sign into account
// in case of a relativ value the result is normed to 1
bool ParseCoordinateRelativ(const std::string& s, double& val, bool& relativ)
{
    size_t pos = s.find('%');
    relativ = (pos != std::string::npos);
    if (relativ)
    {
        if (pos < s.length() - 1)
        {
            // there are characters afters the percent sign
            // this is not supported
            return false;
        };
        if( hugin_utils::stringToDouble(s.substr(0, pos), val))
        {
            val /= 100.0;
            return true;
        };
    }
    else
    {
        // enforce integer values
        int intValue;
        if (hugin_utils::stringToInt(s, intValue))
        {
            val = intValue;
            return true;
        };
    };
    return false;
}

// parse given crop string s and set corresponding crop in pano for images in UIntSet
void SetCropToImages(const std::string& s, HuginBase::Panorama& pano, HuginBase::UIntSet& imgs)
{
    std::vector<std::string> coords = hugin_utils::SplitString(s, ",");
    if (coords.size() == 2)
    {
        // only 2 coordinates, take them as width and height and activate autocenter crop
        double width, height;
        bool relWidth = false;
        bool relHeight = false;
        if (ParseCoordinateRelativ(coords[0], width, relWidth) && ParseCoordinateRelativ(coords[1], height, relHeight))
        {
            for (auto& i : imgs)
            {
                HuginBase::SrcPanoImage img = pano.getSrcImage(i);
                vigra::Rect2D cropRect;
                cropRect.setSize(relWidth ? (img.getWidth()*width) : width, relHeight ? (img.getHeight()*height) : height);
                img.setCropRect(cropRect);
                img.setAutoCenterCrop(true);
                pano.setSrcImage(i, img);
                pano.changeFinished();
                // print result
                std::cout << "Set crop for image " << i << " to " << pano.getImage(i).getCropRect() << std::endl;
            };
        }
        else
        {
            std::cerr << "Could not parse crop string \"" << s << "\"." << std::endl;
            return;
        };
    }
    else
    {
        if (coords.size() == 4)
        {
            // we got 4 coordinates
            int left, right, top, bottom;
            if (hugin_utils::stringToInt(coords[0], left) && hugin_utils::stringToInt(coords[1], right) &&
                hugin_utils::stringToInt(coords[2], top) && hugin_utils::stringToInt(coords[3], bottom))
            {
                vigra::Rect2D cropRect;
                if (right > left && bottom > top)
                {
                    cropRect.setUpperLeft(vigra::Point2D(left, top));
                    cropRect.setLowerRight(vigra::Point2D(right, bottom));
                }
                else
                {
                    std::cerr << "Crop \"" << s << "\" is an invalid crop area." << std::endl;
                    return;
                };
                for (auto& i : imgs)
                {
                    HuginBase::SrcPanoImage img = pano.getSrcImage(i);
                    img.setAutoCenterCrop(false);
                    img.setCropRect(cropRect);
                    pano.setSrcImage(i, img);
                    pano.changeFinished();
                    // print result
                    std::cout << "Set crop for image " << i << " to " << pano.getImage(i).getCropRect() << std::endl;
                };
            }
            else
            {
                std::cerr << "Could not parse crop values \"" << s << "\"." << std::endl;
                return;
            };
        }
        else
        {
            std::cerr << "Invalid coordinates \"" << s << "\"." << std::endl;
            return;
        }
    };
    return;
}

// set the crop from the string
void SetCrop(HuginBase::Panorama& pano, const std::string& crop)
{
    // split string at ';'
    std::vector<std::string> splitResult = hugin_utils::SplitString(crop, ";");
    // now process each sub-string
    for (auto& s : splitResult)
    {
        std::string subString = hugin_utils::StrTrim(s);
        if (subString.empty())
        {
            continue;
        };
        size_t pos = subString.find("=");
        if(subString[0]=='i' && pos==std::string::npos)
        { 
            std::cerr << "Crop expression \"" << s << "\" is not a valid crop." << std::endl;
            continue;
        };
        if (subString[0] == 'i')
        {
            if (pos < 2)
            {
                std::cerr << "Crop expression \"" << s << "\" does not contain a valid image number."<< std::endl;
                continue;
            };
            unsigned int imgNr;
            if(!hugin_utils::stringToUInt(subString.substr(1, pos-1), imgNr))
            { 
                std::cerr << "Crop expression \"" << s << "\" does not contain a valid image number." << std::endl;
                continue;
            };
            if (imgNr >= pano.getNrOfImages())
            {
                std::cerr << "Pano does not contain image with number " << imgNr << std::endl;
                continue;
            };
            HuginBase::UIntSet imgs;
            imgs.insert(imgNr);
            SetCropToImages(subString.substr(pos + 1), pano, imgs);
        }
        else
        {
            // update crop for all images
            HuginBase::UIntSet imgs;
            fill_set(imgs, 0, pano.getNrOfImages() - 1);
            SetCropToImages(subString, pano, imgs);
        };
    };
};

static void usage(const char* name)
{
    std::cout << name << ": change image variables inside pto files" << std::endl
        << name << " version " << hugin_utils::GetHuginVersion() << std::endl
        << std::endl
        << "Usage:  " << name << " [options] --opt|--link|--unlink|--set varlist input.pto" << std::endl
        << std::endl
        << "     -o, --output=file.pto  Output Hugin PTO file. Default: <filename>_var.pto" << std::endl
        << "     -h, --help             Shows this help" << std::endl
        << std::endl
        << "     --opt varlist          Change optimizer variables" << std::endl
        << "     --modify-opt           Modify the existing optimizer variables" << std::endl
        << "                            (without pto_var will start with an" << std::endl
        << "                             empty variables set)" << std::endl
        << "                            Examples:" << std::endl
        << "           --opt=y,p,r        Optimize yaw, pitch and roll of all images" << std::endl
        << "                              (special treatment for anchor image applies)" << std::endl
        << "           --opt=v0,b2        Optimize hfov of image 0 and barrel distortion" << std::endl
        << "                              of image 2" << std::endl
        << "           --opt=v,!v0        Optimize field of view for all images except" << std::endl
        << "                              for the first image" << std::endl
        << "           --opt=!a,!b,!c     Don't optimise distortion (works only with" << std::endl
        << "                              switch --modify-opt together)" << std::endl
        << std::endl
        << "     --link varlist         Link given variables" << std::endl
        << "                            Example:" << std::endl
        << "           --link=v3          Link hfov of image 3" << std::endl
        << "           --link=a1,b1,c1    Link distortions parameter for image 1" << std::endl
        << std::endl
        << "     --unlink varlist       Unlink given variables" << std::endl
        << "                            Examples:" << std::endl
        << "           --unlink=v5        Unlink hfov for image 5" << std::endl
        << "           --unlink=a2,b2,c2  Unlink distortions parameters for image 2" << std::endl
        << std::endl
        << "     --set varlist          Sets variables to new values" << std::endl
        << "                            Examples:" << std::endl
        << "           --set=y0=0,r0=0,p0=0  Resets position of image 0" << std::endl
        << "           --set=Vx4=-10,Vy4=10  Sets vignetting offset for image 4" << std::endl
        << "           --set=v=20            Sets the field of view to 20 for all images" << std::endl
        << "           --set=y=val+20        Increase yaw by 20 deg for all images" << std::endl
        << "           --set=v=val*1.1       Increase fov by 10 % for all images" << std::endl
        << "           --set=y=i*20          Set yaw to 0, 20, 40, ..." << std::endl
        << "     --set-from-file filename  Sets variables to new values" << std::endl
        << "                               It reads the varlist from a file" << std::endl
        << std::endl
        << "     --crop=left,right,top,bottom Set the crop for all images" << std::endl
        << "     --crop=width,height       Set the crop for all images and activate" << std::endl
        << "                               autocenter for crop" << std::endl
        << "                               relative values can be used with %" << std::endl
        << "     --crop=iNUM=left,right,top,bottom Set the crop for image NUM" << std::endl
        << "     --crop=iNUM=width,height  Set the crop for image NUM and" <<std::endl
        << "                             activate autocenter for crop"<<std::endl
        << "                             These switches can be used several times."<<std::endl
        << std::endl;
}

int main(int argc, char* argv[])
{
    // parse arguments
    const char* optstring = "o:h";

    enum
    {
        SWITCH_OPT=1000,
        SWITCH_LINK,
        SWITCH_UNLINK,
        SWITCH_SET,
        SWITCH_SET_FILE,
        OPT_MODIFY_OPTVEC,
        SWITCH_CROP
    };
    static struct option longOptions[] =
    {
        {"output", required_argument, NULL, 'o' },
        {"opt", required_argument, NULL, SWITCH_OPT },
        {"link", required_argument, NULL, SWITCH_LINK },
        {"unlink", required_argument, NULL, SWITCH_UNLINK },
        {"set", required_argument, NULL, SWITCH_SET },
        {"set-from-file", required_argument, NULL, SWITCH_SET_FILE },
        {"modify-opt", no_argument, NULL, OPT_MODIFY_OPTVEC },
        {"crop", required_argument, NULL, SWITCH_CROP },
        {"help", no_argument, NULL, 'h' },
        0
    };

    Parser::ParseVarVec optVars;
    Parser::ParseVarVec linkVars;
    Parser::ParseVarVec unlinkVars;
    std::string setVars, crop;
    bool modifyOptVec=false;
    int c;
    std::string output;
    while ((c = getopt_long (argc, argv, optstring, longOptions,nullptr)) != -1)
    {
        switch (c)
        {
            case 'o':
                output = optarg;
                break;
            case 'h':
                usage(hugin_utils::stripPath(argv[0]).c_str());
                return 0;
            case SWITCH_OPT:
                Parser::ParseVariableString(optVars, std::string(optarg), std::cerr, ParseSingleOptVar);
                break;
            case SWITCH_LINK:
                Parser::ParseVariableString(linkVars, std::string(optarg), std::cerr, ParseSingleLinkVar);
                break;
            case SWITCH_UNLINK:
                Parser::ParseVariableString(unlinkVars, std::string(optarg), std::cerr, ParseSingleLinkVar);
                break;
            case SWITCH_SET:
                if (!setVars.empty())
                {
                    setVars.append(",");
                };
                setVars.append(optarg);
                break;
            case SWITCH_SET_FILE:
                {
                    std::ifstream ifs(optarg);
                    if(ifs.is_open())
                    {
                        std::ostringstream contents;
                        contents << ifs.rdbuf();
                        ifs.close();
                        setVars = contents.str();
                    }
                    else
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": Could not open file " << optarg << std::endl;
                        return 1;
                    };
                };
                break;
            case OPT_MODIFY_OPTVEC:
                modifyOptVec=true;
                break;
            case SWITCH_CROP:
                if (!crop.empty())
                {
                    crop.append(";");
                };
                crop.append(optarg);
                break;
            case ':':
            case '?':
                // missing argument or invalid switch
                return 1;
                break;
            default:
                // this should not happen
                abort();

        }
    }

    if (argc - optind != 1)
    {
        if (argc - optind < 1)
        {
            std::cerr << hugin_utils::stripPath(argv[0]) << ": No project file given." << std::endl;
        }
        else
        {
            std::cerr << hugin_utils::stripPath(argv[0]) << ": Only one project file expected." << std::endl;
        };
        return 1;
    };

    if(optVars.empty() && linkVars.empty() && unlinkVars.empty() && setVars.empty() && crop.empty())
    {
        std::cerr << hugin_utils::stripPath(argv[0]) << ": no variables to modify given" << std::endl;
        return 1;
    };

    std::string input=argv[optind];
    // read panorama
    HuginBase::Panorama pano;
    std::ifstream prjfile(input.c_str());
    if (!prjfile.good())
    {
        std::cerr << "could not open script : " << input << std::endl;
        return 1;
    }
    pano.setFilePrefix(hugin_utils::getPathPrefix(input));
    AppBase::DocumentData::ReadWriteError err = pano.readData(prjfile);
    if (err != AppBase::DocumentData::SUCCESSFUL)
    {
        std::cerr << "error while parsing panos tool script: " << input << std::endl;
        std::cerr << "AppBase::DocumentData::ReadWriteError code: " << err << std::endl;
        return 1;
    }

    if(pano.getNrOfImages()==0)
    {
        std::cerr << "error: project file does not contains any image" << std::endl;
        std::cerr << "aborting processing" << std::endl;
        return 1;
    };

    //link/unlink variables
    if(!linkVars.empty())
    {
        std::cout << "Linking image variables" << std::endl;
        UnLinkVars(pano, linkVars, true);
        std::cout << std::endl;
    };

    if(!unlinkVars.empty())
    {
        std::cout << "Unlinking image variables" << std::endl;
        UnLinkVars(pano, unlinkVars, false);
        std::cout << std::endl;
    };

    // set variables to new value
    if(!setVars.empty())
    {
        std::cout << "Setting image variables" << std::endl;
        Parser::PanoParseExpression(pano, setVars);
        std::cout << std::endl;
    };

    // update optimzer vector
    if(!optVars.empty())
    {
        std::cout << "Updating optimizer variables" << std::endl;
        std::set<size_t> refImgs=pano.getRefImages();
        bool linkRefImgsYaw=false;
        bool linkRefImgsPitch=false;
        bool linkRefImgsRoll=false;
        pano.checkRefOptStatus(linkRefImgsYaw, linkRefImgsPitch, linkRefImgsRoll);

        //simplify handling of variable groups
        std::vector<std::set<std::string> > groupedVars;
        std::set<std::string> varSet;
        varSet.insert("Vb");
        varSet.insert("Vc");
        varSet.insert("Vd");
        groupedVars.push_back(varSet);
        varSet.clear();
        varSet.insert("Vx");
        varSet.insert("Vy");
        groupedVars.push_back(varSet);
        varSet.clear();
        varSet.insert("Ra");
        varSet.insert("Rb");
        varSet.insert("Rc");
        varSet.insert("Rd");
        varSet.insert("Re");
        groupedVars.push_back(varSet);

        HuginBase::OptimizeVector optVec;
        if(modifyOptVec)
        {
            optVec=pano.getOptimizeVector();
        };
        if(optVec.size()!=pano.getNrOfImages())
        {
            optVec.resize(pano.getNrOfImages());
        };
        for(size_t i=0; i<optVars.size(); i++)
        {
            //skip invalid image numbers
            if(optVars[i].imgNr>=(int)pano.getNrOfImages())
            {
                continue;
            };
            if(optVars[i].imgNr==-1)
            {
                for(size_t imgNr=0; imgNr<pano.getNrOfImages(); imgNr++)
                {
                    if(optVars[i].flag)
                    {
                        RemoveFromOptVec(optVec, optVars[i].varname, imgNr, groupedVars);
                    }
                    else
                    {
                        AddToOptVec(optVec, optVars[i].varname, imgNr, refImgs, linkRefImgsYaw, linkRefImgsPitch, linkRefImgsRoll, groupedVars);
                    };
                };
            }
            else
            {
                if(optVars[i].flag)
                {
                    RemoveFromOptVec(optVec, optVars[i].varname, optVars[i].imgNr, groupedVars);
                }
                else
                {
                    AddToOptVec(optVec, optVars[i].varname, optVars[i].imgNr, refImgs, true, true, true, groupedVars);
                };
            };
        };
        pano.setOptimizerSwitch(0);
        pano.setPhotometricOptimizerSwitch(0);
        pano.setOptimizeVector(optVec);
        std::cout << "Optimizer variables:" << std::endl;
        for (size_t i = 0; i < optVec.size(); ++i)
        {
            std::cout << "Image " << i << ": ";
            std::copy(optVec[i].begin(), optVec[i].end(), std::ostream_iterator<std::string>(std::cout, " "));
            std::cout << std::endl;
        };
        std::cout << std::endl;
    };

    // update crop
    if (!crop.empty())
    {
        std::cout << "Setting image crop" << std::endl;
        SetCrop(pano, crop);
        std::cout << std::endl;
    };

    //write output
    HuginBase::UIntSet imgs;
    fill_set(imgs,0, pano.getNrOfImages()-1);
    // Set output .pto filename if not given
    if (output=="")
    {
        output=input.substr(0,input.length()-4).append("_var.pto");
    }
    std::ofstream of(output.c_str());
    pano.printPanoramaScript(of, pano.getOptimizeVector(), pano.getOptions(), imgs, false, hugin_utils::getPathPrefix(input));
    std::cout << std::endl << "Written output to " << output << std::endl;
    return 0;
}
