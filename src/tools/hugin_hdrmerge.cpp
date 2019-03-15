// -*- c-basic-offset: 4 -*-

/** @file hugin_hdrmerge.cpp
 *
 *  @brief merge images
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
#include <cmath>
#include <algorithm>

#include <memory>

#include <vigra/error.hxx>
#include <vigra/functorexpression.hxx>
#include <vigra/transformimage.hxx>

#include <hugin_utils/utils.h>

#include <vigra_ext/impexalpha.hxx>
#include <vigra_ext/HDRUtils.h>
#include <vigra_ext/ReduceOpenEXR.h>

#include <getopt.h>

#include "../deghosting/deghosting.h"
#include "../deghosting/khan.h"

// define the types of images we will use.
// use float for RGB
typedef vigra::FRGBImage ImageType;

// smart pointers to the images.
typedef std::shared_ptr<ImageType> ImagePtr;

static int g_verbose = 0;

const uint16_t OTHER_GRAY = 1;

// load all images and apply a weighted average merge, with
// special cases for completely over or underexposed pixels.
bool mergeWeightedAverage(std::vector<std::string> inputFiles, vigra::FRGBImage& output, vigra::BImage& alpha)
{
    // load all images into memory
    std::vector<ImagePtr> images;
    std::vector<deghosting::BImagePtr> weightImages;

    for (size_t i=0; i < inputFiles.size(); i++)
    {
        ImagePtr img = ImagePtr(new ImageType());
        deghosting::BImagePtr weight = deghosting::BImagePtr(new vigra::BImage());

        if (g_verbose > 0)
        {
            std::cout << "Loading image: " << inputFiles[i] << std::endl;
        }
        vigra::ImageImportInfo info(inputFiles[i].c_str());
        img->resize(info.size());
        weight->resize(info.size().width(), info.size().height(), 255);
        if (info.numBands() == 4)
        {
            importImageAlpha(info, destImage(*img), destImage(*weight));
        }
        else
        {
            importImage(info, destImage(*img));
        }
        images.push_back(img);
        weightImages.push_back(weight);
    }

    // ensure all images have the same size (cropped images not supported yet)
    int width=images[0]->width();
    int height=images[0]->height();
    for (unsigned i=1; i < images.size(); i++)
    {
        if (images[i]->width() != width || images[i]->height() != height)
        {
            std::cerr << "Error: Input images need to be of the same size" << std::endl;
            return false;
        }
    }
    output.resize(width,height);
    alpha.resize(width, height, 0);
    if (g_verbose > 0)
    {
        std::cout << "Calculating weighted average " << std::endl;
    }
    // apply weighted average functor with
    // heuristic to deal with pixels that are overexposed in all images
    vigra_ext::ReduceToHDRFunctor<ImageType::value_type> waverage;

    // loop over all pixels in the image (very low level access)
    for (int y=0; y < height; y++)
    {
        for (int x=0; x < width; x++)
        {
            waverage.reset();
            // loop over all exposures
            bool hasValues = false;
            for (unsigned imgNr=0; imgNr < images.size(); imgNr++)
            {
                // add pixel to weighted average
                const vigra::UInt8 weight = (*weightImages[imgNr])(x, y);
                waverage( (*images[imgNr])(x,y), weight);
                hasValues |= (weight > 0);
            }
            // get result
            if(hasValues)
            {
                output(x,y) = waverage();
                alpha(x, y) = 255;
            };
        }
    }
    return true;
}

/** compute output image when given source images */
bool weightedAverageOfImageFiles(const std::vector<std::string>& inputFiles,
                                 const std::vector<deghosting::FImagePtr>& weights,
                                 vigra::FRGBImage& output,
                                 vigra::BImage& alpha)
{
    if(g_verbose > 0)
    {
        std::cout << "Merging input images" << std::endl;
    }
    int width = (weights[0])->width();
    int height = (weights[0])->height();

    assert(inputFiles.size() == weights.size());

    vigra::BasicImage<vigra::NumericTraits<vigra::FRGBImage::PixelType>::Promote> weightedImg(width, height);
    vigra::BasicImage<vigra::NumericTraits<vigra::FImage::PixelType>::Promote> weightAdded(width, height);
    for(unsigned i = 0; i < inputFiles.size(); i++)
    {
        vigra::ImageImportInfo inputInfo(inputFiles[i].c_str());
        vigra::BasicImage<vigra::NumericTraits<vigra::FRGBImage::PixelType>::Promote> tmpImg(inputInfo.size());

        //load image
        if (inputInfo.numBands() == 4)
        {
            vigra::BImage tmpMask(inputInfo.size());
            vigra::importImageAlpha(inputInfo, vigra::destImage(tmpImg), vigra::destImage(tmpMask));
        }
        else
        {
            vigra::importImage(inputInfo, vigra::destImage(tmpImg));
        }

        //combine with weight
        vigra::combineThreeImages(srcImageRange(tmpImg), srcImage(*(weights[i])), srcImage(weightedImg), destImage(weightedImg), vigra::functor::Arg1() *vigra::functor::Arg2() + vigra::functor::Arg3());
        vigra::combineTwoImages(srcImageRange(weightAdded), srcImage(*(weights[i])), destImage(weightAdded), vigra::functor::Arg1() + vigra::functor::Arg2());
    }
    output.resize(width, height);
    alpha.resize(width, height, 0);
    vigra::combineTwoImages(srcImageRange(weightedImg), srcImage(weightAdded), destImage(output), vigra::functor::Arg1() / vigra::functor::Arg2());
    vigra::transformImage(vigra::srcImageRange(weightAdded), vigra::destImage(alpha),
        vigra::Threshold<vigra::FImage::PixelType, vigra::BImage::PixelType>(1e-7f, FLT_MAX, 0, 255));
    return true;
}

static void usage(const char* name)
{
    std::cout << name << ": merge overlapping images" << std::endl
         << std::endl
         << "hugin_hdrmerge version " << hugin_utils::GetHuginVersion() << std::endl
         << std::endl
         << "Usage: " << name  << " [options] -o output.exr <input-files>" << std::endl
         << "Valid options are:" << std::endl
         << "  -o|--output prefix output file" << std::endl
         << "  -m mode   merge mode, can be one of: avg (default), avg_slow, khan, if avg, no" << std::endl
         << "            -i and -s options apply" << std::endl
         << "  -i iter   number of iterations to execute (default is 4). Khan only" << std::endl
         << "  -s sigma  standard deviation of Gaussian weighting" << std::endl
         << "            function (sigma > 0); default: 30. Khan only" << std::endl
         << "  -a set    advanced settings. Possible options are:" << std::endl
         << "              f   use gray images for computation. It's about two times faster" << std::endl
         << "                  but it usually returns worse results." << std::endl
         << "              g   use gamma 2.2 correction instead of logarithm" << std::endl
         << "              m   do not scale image, NOTE: slows down process" << std::endl
         << "  -c        Only consider pixels that are defined in all images (avg mode only)" << std::endl
         << "  -v|--verbose   Verbose, print progress messages, repeat for" << std::endl
         << "                 even more verbose output" << std::endl
         << "  -h|help   Display help (this text)" << std::endl
         << std::endl;
}


int main(int argc, char* argv[])
{

    // parse arguments
    const char* optstring = "chvo:m:i:s:a:el";
    static struct option longOptions[] =
    {
        { "output", required_argument, NULL, 'o' },
        { "verbose", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },
        0
    };
    int c;

    g_verbose = 0;
    std::string outputFile = "merged.exr";
    std::string mode = "avg";
    bool onlyCompleteOverlap = false;
    int iterations = 4;
    double sigma = 30;
    uint16_t flags = 0;
    uint16_t otherFlags = 0;

    while ((c = getopt_long(argc, argv, optstring, longOptions, nullptr)) != -1)
    {
        switch (c)
        {
            case 'm':
                mode = optarg;
                break;
            case 'i':
                iterations = atoi(optarg);
                break;
            case 's':
                sigma = atof(optarg);
                break;
            case 'a':
                for(char* c = optarg; *c; c++)
                {
                    switch(*c)
                    {
                        case 'f':
                            otherFlags += OTHER_GRAY;
                            break;
                        case 'g':
                            flags += deghosting::ADV_GAMMA;
                            break;
                        case 'm':
                            flags -= deghosting::ADV_MULTIRES;
                            break;
                        default:
                            std::cerr << hugin_utils::stripPath(argv[0]) << ": unknown option" << std::endl;
                            exit(1);
                    }
                }
            case 'c':
                onlyCompleteOverlap = true;
                break;
            case 'o':
                outputFile = optarg;
                break;
            case 'v':
                g_verbose++;
                break;
            case 'h':
                usage(hugin_utils::stripPath(argv[0]).c_str());
                return 0;
            case ':':
            case '?':
                // missing argument or invalid switch
                return 1;
                break;
            default:
                // this should not happen
                abort();
        }
    }//end while

    unsigned nFiles = argc - optind;
    if (nFiles == 0)
    {
        std::cerr << hugin_utils::stripPath(argv[0]) << ": at least one input image needed" << std::endl;
        return 1;
    }
    else if (nFiles == 1)
    {
        std::cout << std::endl << "Only one input image given. Copying input image to output image." << std::endl;
        // simply copy image file
        std::ifstream infile(argv[optind], std::ios_base::binary);
        std::ofstream outfile(outputFile.c_str(), std::ios_base::binary);
        outfile << infile.rdbuf();
        return 0;
    }

    // load all images
    std::vector<std::string> inputFiles;
    for (size_t i=optind; i < (size_t)argc; i++)
    {
        inputFiles.push_back(argv[i]);
    }

    // output image
    ImageType output;
    try
    {
        if (mode == "avg_slow")
        {
            // use a weighted average, with special consideration of pixels
            // that are completely over or underexposed in all exposures.
            if (g_verbose > 0)
            {
                std::cout << "Running simple weighted avg algorithm" << std::endl;
            }
            vigra::BImage alpha;
            mergeWeightedAverage(inputFiles, output, alpha);
            // save output file
            if (g_verbose > 0)
            {
                std::cout << "Writing " << outputFile << std::endl;
            }
            vigra::ImageExportInfo exinfo(outputFile.c_str());
            exinfo.setPixelType("FLOAT");
            vigra::exportImageAlpha(srcImageRange(output), srcImage(alpha), exinfo);
        }
        else if (mode == "avg")
        {
            // apply weighted average functor with
            // heuristic to deal with pixels that are overexposed in all images
            vigra_ext::ReduceToHDRFunctor<ImageType::value_type> waverage;
            // calc weighted average without loading the whole images into memory
            reduceFilesToHDR(inputFiles, outputFile, onlyCompleteOverlap, waverage);
        }
        else if (mode == "khan")
        {
            deghosting::Deghosting* deghoster = NULL;
            std::vector<deghosting::FImagePtr> weights;

            if (otherFlags & OTHER_GRAY)
            {
                deghosting::Khan<float> khanDeghoster(inputFiles, flags, 0, iterations, sigma, g_verbose);
                deghoster = &khanDeghoster;
                weights = deghoster->createWeightMasks();
            }
            else
            {
                deghosting::Khan<vigra::RGBValue<float> > khanDeghoster(inputFiles, flags, 0, iterations, sigma, g_verbose);
                deghoster = &khanDeghoster;
                weights = deghoster->createWeightMasks();
            }
            vigra::BImage alpha;
            weightedAverageOfImageFiles(inputFiles, weights, output, alpha);
            if (g_verbose > 0)
            {
                std::cout << "Writing " << outputFile << std::endl;
            }
            vigra::ImageExportInfo exinfo(outputFile.c_str());
            exinfo.setPixelType("FLOAT");
            vigra::exportImageAlpha(vigra::srcImageRange(output), vigra::srcImage(alpha), exinfo);
        }
        else
        {
            std::cerr << "Unknown merge mode, see help for a list of possible modes" << std::endl;
            return 1;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "caught exception: " << e.what() << std::endl;
        abort();
    }

    return 0;
}


