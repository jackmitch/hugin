// -*- c-basic-offset: 4 -*-

/** @file autooptimiser.cpp
 *
 *  @brief a smarter PTOptimizer, with pairwise optimisation
 *         before global optimisation starts
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

#include <hugin_config.h>

#include <fstream>
#include <sstream>
#include <getopt.h>

#include <hugin_basic.h>
#include <hugin_utils/stl_utils.h>
#include <appbase/ProgressDisplay.h>
#include <algorithms/optimizer/PTOptimizer.h>
#include <algorithms/nona/CenterHorizontally.h>
#include <algorithms/basic/StraightenPanorama.h>
#include <algorithms/basic/CalculateMeanExposure.h>
#include <algorithms/nona/FitPanorama.h>
#include <algorithms/basic/CalculateOptimalScale.h>
#include <algorithms/optimizer/PhotometricOptimizer.h>
#include <panodata/ImageVariableGroup.h>
#include <panodata/StandardImageVariableGroups.h>
#include "ExtractPoints.h"

static void usage(const char* name)
{
    std::cout << name << ": optimize image positions" << std::endl
        << "autooptimiser version " << hugin_utils::GetHuginVersion() << std::endl
        << std::endl
        << "Usage:  " << name << " [options] input.pto" << std::endl
        << "   To read a project from stdio, specify - as input file." << std::endl
        << std::endl
        << "  Options:" << std::endl
        << "     -o file.pto  output file. If omitted, stdout is used." << std::endl
        << std::endl
         << "    Optimisation options (if not specified, no optimisation takes place)" << std::endl
         << "     -a       auto align mode, includes various optimisation stages, depending" << std::endl
         << "               on the amount and distribution of the control points" << std::endl
         << "     -p       pairwise optimisation of yaw, pitch and roll, starting from" << std::endl
         << "              first image" << std::endl
         << "     -m       Optimise photometric parameters" << std::endl
         << "     -n       Optimize parameters specified in script file (like PTOptimizer)" << std::endl
         << std::endl
         << "    Postprocessing options:" << std::endl
         << "     -l       level horizon (works best for horizontal panos)" << std::endl
         << "     -s       automatically select a suitable output projection and size" << std::endl
         << "    Other options:" << std::endl
         << "     -q       quiet operation (no progress is reported)" << std::endl
         << "     -v HFOV  specify horizontal field of view of input images." << std::endl
         << "               Used if the .pto file contains invalid HFOV values" << std::endl
         << "               (autopano-SIFT writes .pto files with invalid HFOV)" << std::endl
         << std::endl
         << "   When using -a -l -m and -s options together, a similar operation to the \"Align\"" << std::endl
         << "    button in hugin is performed." << std::endl
         << std::endl;
}

int main(int argc, char* argv[])
{
    // parse arguments
    const char* optstring = "alho:npqsv:m";
    int c;
    static struct option longOptions[] =
    {
        { "output", required_argument, NULL, 'o'},
        { "help", no_argument, NULL, 'h' },
        0
    };
    std::string output;
    bool doPairwise = false;
    bool doAutoOpt = false;
    bool doNormalOpt = false;
    bool doLevel = false;
    bool chooseProj = false;
    bool quiet = false;
    bool doPhotometric = false;
    double hfov = 0.0;
    while ((c = getopt_long(argc, argv, optstring, longOptions, nullptr)) != -1)
    {
        switch (c)
        {
            case 'o':
                output = optarg;
                break;
            case 'h':
                usage(hugin_utils::stripPath(argv[0]).c_str());
                return 0;
            case 'p':
                doPairwise = true;
                break;
            case 'a':
                doAutoOpt = true;
                break;
            case 'n':
                doNormalOpt = true;
                break;
            case 'l':
                doLevel = true;
                break;
            case 's':
                chooseProj = true;
                break;
            case 'q':
                quiet = true;
                break;
            case 'v':
                hfov = atof(optarg);
                break;
            case 'm':
                doPhotometric = true;
                break;
            case ':':
            case '?':
                // missing argument or invalid switch
                return 1;
                break;
            default:
                // this should not happen
                abort ();
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
    }

    const char* scriptFile = argv[optind];

    HuginBase::Panorama pano;
    if (scriptFile[0] == '-')
    {
        AppBase::DocumentData::ReadWriteError err = pano.readData(std::cin);
        if (err != AppBase::DocumentData::SUCCESSFUL)
        {
            std::cerr << "error while reading script file from stdin." << std::endl
                << "DocumentData::ReadWriteError code: " << err << std::endl;
            return 1;
        }
    }
    else
    {
        std::ifstream prjfile(scriptFile);
        if (!prjfile.good())
        {
            std::cerr << "could not open script : " << scriptFile << std::endl;
            return 1;
        }
        pano.setFilePrefix(hugin_utils::getPathPrefix(scriptFile));
        AppBase::DocumentData::ReadWriteError err = pano.readData(prjfile);
        if (err != AppBase::DocumentData::SUCCESSFUL)
        {
            std::cerr << "error while parsing panos tool script: " << scriptFile << std::endl
                << "DocumentData::ReadWriteError code: " << err << std::endl;
            return 1;
        }
    }

    if (pano.getNrOfImages() == 0)
    {
        std::cerr << "Panorama should consist of at least one image" << std::endl;
        return 1;
    }

    // for bad HFOV (from autopano-SIFT)
    for (unsigned i=0; i < pano.getNrOfImages(); i++)
    {
        HuginBase::SrcPanoImage img = pano.getSrcImage(i);
        if (img.getProjection() == HuginBase::SrcPanoImage::RECTILINEAR
                && img.getHFOV() >= 180)
        {
            // something is wrong here, try to read from exif data
            std::cerr << "HFOV of image " << img.getFilename() << " invalid, trying to read EXIF tags" << std::endl;
            img.readEXIF();
            bool ok = img.applyEXIFValues(false);
            if (! ok)
            {
                if (hfov)
                {
                    img.setHFOV(hfov);
                }
                else
                {
                    std::cerr << "EXIF reading failed, please specify HFOV with -v" << std::endl;
                    return 1;
                }
            }
            pano.setSrcImage(i, img);
        }
    }

    if(pano.getNrOfCtrlPoints()==0 && (doPairwise || doAutoOpt || doNormalOpt))
    {
        std::cerr << "Panorama have to have control points to optimise positions" << std::endl;
        return 1;
    };
    if (doPairwise && ! doAutoOpt)
    {
        // do pairwise optimisation
        HuginBase::AutoOptimise::autoOptimise(pano);

        // do global optimisation
        if (!quiet)
        {
            std::cerr << "*** Pairwise position optimisation" << std::endl;
        }
        HuginBase::PTools::optimize(pano);
    }
    else if (doAutoOpt)
    {
        if (!quiet)
        {
            std::cerr << "*** Adaptive geometric optimisation" << std::endl;
        }
        HuginBase::SmartOptimise::smartOptimize(pano);
    }
    else if (doNormalOpt)
    {
        if (!quiet)
        {
            std::cerr << "*** Optimising parameters specified in PTO file" << std::endl;
        }
        HuginBase::PTools::optimize(pano);
    }
    else
    {
        if (!quiet)
        {
            std::cerr << "*** Geometric parameters not optimized" << std::endl;
        }
    }

    if (doLevel)
    {
        bool hasVerticalLines=false;
        HuginBase::CPVector allCP=pano.getCtrlPoints();
        if(allCP.size()>0 && (doPairwise || doAutoOpt || doNormalOpt))
        {
            for(size_t i=0; i<allCP.size() && !hasVerticalLines; i++)
            {
                hasVerticalLines=(allCP[i].mode==HuginBase::ControlPoint::X);
            };
        };
        // straighten only if there are no vertical control points
        if(hasVerticalLines)
        {
            std::cout << "Skipping automatic leveling because of existing vertical control points." << std::endl;
        }
        else
        {
            HuginBase::StraightenPanorama(pano).run();
            HuginBase::CenterHorizontally(pano).run();
        };
    }

    if (chooseProj)
    {
        HuginBase::PanoramaOptions opts = pano.getOptions();
        HuginBase::CalculateFitPanorama fitPano(pano);
        fitPano.run();
        opts.setHFOV(fitPano.getResultHorizontalFOV());
        opts.setHeight(hugin_utils::roundi(fitPano.getResultHeight()));
        const double vfov = opts.getVFOV();
        const double hfov = opts.getHFOV();
        // avoid perspective projection if field of view > 100 deg
        const double mf = 100;
        bool changedProjection=false;
        if (pano.getNrOfImages() == 1)
        {
            // special case for single image projects
            switch (pano.getImage(0).getProjection())
            {
                case HuginBase::SrcPanoImage::RECTILINEAR:
                    // single rectilinear image, keep rectilinear projection
                    if (opts.getProjection() != HuginBase::PanoramaOptions::RECTILINEAR)
                    {
                        opts.setProjection(HuginBase::PanoramaOptions::RECTILINEAR);
                        changedProjection = true;
                    };
                    break;
                default:
                    if (vfov < mf)
                    {
                        // small vfov, use cylindrical
                        if (opts.getProjection() != HuginBase::PanoramaOptions::CYLINDRICAL)
                        {
                            opts.setProjection(HuginBase::PanoramaOptions::CYLINDRICAL);
                            changedProjection = true;
                        };
                    }
                    else
                    {
                        // otherwise go to equirectangular
                        if (opts.getProjection() != HuginBase::PanoramaOptions::EQUIRECTANGULAR)
                        {
                            opts.setProjection(HuginBase::PanoramaOptions::EQUIRECTANGULAR);
                            changedProjection = true;
                        };
                    };
                    break;
            };
        }
        else
        {
            if (vfov < mf)
            {
                // cylindrical or rectilinear
                if (hfov < mf)
                {
                    if (opts.getProjection() != HuginBase::PanoramaOptions::RECTILINEAR)
                    {
                        opts.setProjection(HuginBase::PanoramaOptions::RECTILINEAR);
                        changedProjection = true;
                    };
                }
                else
                {
                    if (opts.getProjection() != HuginBase::PanoramaOptions::CYLINDRICAL)
                    {
                        opts.setProjection(HuginBase::PanoramaOptions::CYLINDRICAL);
                        changedProjection = true;
                    };
                };
            }
            else
            {
                // vfov > 100, use equirectangular projection
                if (opts.getProjection() != HuginBase::PanoramaOptions::EQUIRECTANGULAR)
                {
                    opts.setProjection(HuginBase::PanoramaOptions::EQUIRECTANGULAR);
                    changedProjection = true;
                };
            };
        };
        pano.setOptions(opts);
        // the projection could be changed, calculate fit again
        if(changedProjection)
        {
            HuginBase::CalculateFitPanorama fitPano2(pano);
            fitPano2.run();
            opts.setHFOV(fitPano2.getResultHorizontalFOV());
            opts.setHeight(hugin_utils::roundi(fitPano2.getResultHeight()));
            pano.setOptions(opts);
        };

        // downscale pano a little
        const double sizeFactor = 0.7;
        const double w = HuginBase::CalculateOptimalScale::calcOptimalScale(pano);
        opts.setWidth(hugin_utils::roundi(opts.getWidth()*w*sizeFactor), true);
        pano.setOptions(opts);
    }

    if(doPhotometric)
    {
        // photometric estimation
        HuginBase::PanoramaOptions opts = pano.getOptions();
        int nPoints = 200;
        nPoints = nPoints * pano.getNrOfImages();

        std::vector<vigra_ext::PointPairRGB> points;
        AppBase::ProgressDisplay* progressDisplay;
        if(!quiet)
        {
            progressDisplay=new AppBase::StreamProgressDisplay(std::cout);
        }
        else
        {
            progressDisplay=new AppBase::DummyProgressDisplay();
        }
        float imageStepSize;
        try
        {
            loadImgsAndExtractPoints(pano, nPoints, 3, true, *progressDisplay, points, !quiet, imageStepSize);
        }
        catch (std::exception& e)
        {
            std::cerr << "caught exception: " << e.what() << std::endl;
            return 1;
        };
        if(!quiet)
        {
            std::cout << "\rSelected " << points.size() << " points" << std::endl;
        }

        if (points.size() == 0)
        {
            std::cerr << "Error: no overlapping points found, exiting" << std::endl;
            return 1;
        }

        progressDisplay->setMessage("Photometric Optimization");
        // first, ensure that vignetting and response coefficients are linked
        const HuginBase::ImageVariableGroup::ImageVariableEnum vars[] =
        {
            HuginBase::ImageVariableGroup::IVE_EMoRParams,
            HuginBase::ImageVariableGroup::IVE_ResponseType,
            HuginBase::ImageVariableGroup::IVE_VigCorrMode,
            HuginBase::ImageVariableGroup::IVE_RadialVigCorrCoeff,
            HuginBase::ImageVariableGroup::IVE_RadialVigCorrCenterShift
        };
        HuginBase::StandardImageVariableGroups variable_groups(pano);
        HuginBase::ImageVariableGroup& lenses = variable_groups.getLenses();
        for (size_t i = 0; i < lenses.getNumberOfParts(); i++)
        {
            std::set<HuginBase::ImageVariableGroup::ImageVariableEnum> links_needed;
            links_needed.clear();
            for (int v = 0; v < 5; v++)
            {
                if (!lenses.getVarLinkedInPart(vars[v], i))
                {
                    links_needed.insert(vars[v]);
                }
            };
            if (!links_needed.empty())
            {
                std::set<HuginBase::ImageVariableGroup::ImageVariableEnum>::iterator it;
                for (it = links_needed.begin(); it != links_needed.end(); ++it)
                {
                    lenses.linkVariablePart(*it, i);
                }
            }
        }

        HuginBase::SmartPhotometricOptimizer::PhotometricOptimizeMode optmode =
            HuginBase::SmartPhotometricOptimizer::OPT_PHOTOMETRIC_LDR_WB;
        if (opts.outputMode == HuginBase::PanoramaOptions::OUTPUT_HDR)
        {
            optmode = HuginBase::SmartPhotometricOptimizer::OPT_PHOTOMETRIC_HDR;
        }
        HuginBase::SmartPhotometricOptimizer photoOpt(pano, progressDisplay, pano.getOptimizeVector(), points, imageStepSize, optmode);
        photoOpt.run();

        // calculate the mean exposure.
        opts.outputExposureValue = HuginBase::CalculateMeanExposure::calcMeanExposure(pano);
        pano.setOptions(opts);
        progressDisplay->taskFinished();
        delete progressDisplay;
    };

    // write result
    HuginBase::OptimizeVector optvec = pano.getOptimizeVector();
    HuginBase::UIntSet imgs;
    fill_set(imgs,0, pano.getNrOfImages()-1);
    if (output != "")
    {
        std::ofstream of(output.c_str());
        pano.printPanoramaScript(of, optvec, pano.getOptions(), imgs, false, hugin_utils::getPathPrefix(scriptFile));
    }
    else
    {
        pano.printPanoramaScript(std::cout, optvec, pano.getOptions(), imgs, false, hugin_utils::getPathPrefix(scriptFile));
    }
    return 0;
}
