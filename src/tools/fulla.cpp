// -*- c-basic-offset: 4 -*-

/** @file fulla.cpp
 *
 *  @brief a tool to perform distortion, vignetting and chromatic abberation correction.
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

#include <vigra/error.hxx>
#include <vigra/impex.hxx>
#include <vigra/codec.hxx>
#include <vigra_ext/impexalpha.hxx>
#include <getopt.h>

#include <appbase/ProgressDisplay.h>
#include <nona/SpaceTransform.h>
#include <photometric/ResponseTransform.h>

#include <hugin_basic.h>
#include <lensdb/LensDB.h>

#include <tiffio.h>
#include <vigra_ext/ImageTransforms.h>

template <class SrcImgType, class AlphaImgType, class FlatImgType, class DestImgType>
void correctImage(SrcImgType& srcImg,
                  const AlphaImgType& srcAlpha,
                  const FlatImgType& srcFlat,
                  HuginBase::SrcPanoImage src,
                  vigra_ext::Interpolator interpolator,
                  double maxValue,
                  DestImgType& destImg,
                  AlphaImgType& destAlpha,
                  bool doCrop,
                  AppBase::ProgressDisplay* progress);

template <class PIXELTYPE>
void correctRGB(HuginBase::SrcPanoImage& src, vigra::ImageImportInfo& info, const char* outfile,
                bool crop, const std::string& compression, AppBase::ProgressDisplay* progress);

static void usage(const char* name)
{
    std::cout << name << ": correct lens distortion, vignetting and chromatic abberation" << std::endl
         << "fulla version " << hugin_utils::GetHuginVersion() << endl
         << std::endl
         << "Usage: " << name  << " [options] inputfile(s) " << std::endl
         << "   option are: " << std::endl
         << "      --green=db|a:b:c:d  Correct radial distortion for all channels" << std::endl
         << "                            Specify 'db' for database lookup or" << std::endl
         << "                            the 4 coefficients a:b:c:d" << std::endl
         << "      --blue=db|a:b:c:d   Correct radial distortion for blue channel," << std::endl
         << "                            this is applied on top of the --green" << std::endl
         << "                            distortion coefficients, use for TCA corr" << std::endl
         << "                            Specify 'db' for database lookup or" << std::endl
         << "                            the 4 coefficients a:b:c:d" << std::endl
         << "      --red=db|a:b:c:d    Correct radial distortion for red channel," << std::endl
         << "                            this is applied on top of the --green" << std::endl
         << "                            distortion coefficients, use for TCA corr" << std::endl
         << "                            Specify 'db' for database lookup or" << std::endl
         << "                            the 4 coefficients a:b:c:d" << std::endl
         << "      --camera-maker=Maker Camera manufacturer, for database query" << std::endl
         << "      --camera-model=Cam Camera name, for database query" << std::endl
         << "      --lensname=Lens    Lens name, for database query" << std::endl
         << "                            Specify --camera-maker and --camera-model" << std::endl
         << "                            for fixed lens cameras or --lensname" << std::endl
         << "                            for interchangeable lenses." << std::endl
         << "      --focallength=50   Specify focal length in mm, for database query" << std::endl
         << "      --aperture=3.5     Specify aperture for vignetting data database query" << std::endl
         << "      --dont-rescale     Do not rescale the image to avoid black borders." << std::endl
         << std::endl
         << "      --flatfield=filename  Vignetting correction by flatfield division" << std::endl
         << "                              I = I / c, c = flatfield / mean(flatfield)" << std::endl
         << "      --vignetting=db|a:b:c:d  Correct vignetting (by division)" << std::endl
         << "                            Specify db for database look up or the " << std::endl
         << "                            the 4 coefficients a:b:c:d" << std::endl
         << "                              I = I / ( a + b*r^2 + c*r^4 + d*r^6)" << std::endl
         << "      --linear           Do vignetting correction in linear color space" << std::endl
         << "      --gamma=value      Gamma of input data. used for gamma correction" << std::endl
         << "                           before and after flatfield correction" << std::endl
         << "      --help             Display help (this text)" << std::endl
         << "      --output=name      Set output filename. If more than one image is given," << std::endl
         << "                            the name will be uses as suffix" << std::endl
         << "                            (default suffix: _corr)" << std::endl
         << "      --compression=value Compression of the output files" << std::endl
         << "                            For jpeg output: 0-100" << std::endl
         << "                            For tiff output: PACKBITS, DEFLATE, LZW" << std::endl
         << "      --offset=X:Y       Horizontal and vertical shift" << std::endl
         << "      --verbose          Verbose" << std::endl;
}


int main(int argc, char* argv[])
{
    // parse arguments
    const char* optstring = "e:g:b:r:m:n:l:d:sf:c:i:t:ho:x:va:";
    int o;
    enum
    {
        LINEAR_RESPONSE = 1000
    };
    static struct option longOptions[] =
    {
        { "linear", no_argument, NULL, LINEAR_RESPONSE },
        { "green", required_argument, NULL, 'g' },
        { "blue", required_argument, NULL, 'b' },
        { "red", required_argument, NULL, 'r' },
        { "lensname", required_argument, NULL, 'l' },
        { "camera-maker", required_argument, NULL, 'm' },
        { "camera-model", required_argument, NULL, 'n' },
        { "aperture", required_argument, NULL, 'a' },
        { "focallength", required_argument, NULL, 'd' },
        { "flatfield", required_argument, NULL, 'f' },
        { "dont-rescale", no_argument, NULL, 's' },
        { "vignetting", required_argument, NULL, 'c' },
        { "gamma", required_argument, NULL, 'i' },
        { "threads", required_argument, NULL, 't' },
        { "output", required_argument, NULL, 'o' },
        { "compression", required_argument, NULL, 'e' },
        { "offset", required_argument, NULL, 'x' },
        { "verbose", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },
        0
    };

    std::vector<double> vec4(4);
    bool doFlatfield = false;
    bool doVigRadial = false;
    bool doCropBorders = true;
    unsigned verbose = 0;

    std::string batchPostfix("_corr");
    std::string outputFile;
    std::string compression;
    bool doLookupDistortion = false;
    bool doLookupTCA = false;
    bool doLookupVignetting = false;
    std::string cameraMaker;
    std::string cameraName;
    std::string lensName;
    float focalLength=0;
    float aperture = 0;
    double gamma = 1.0;
    double shiftX = 0;
    double shiftY = 0;
    std::string argument;

    HuginBase::SrcPanoImage srcImg;
    while ((o = getopt_long(argc, argv, optstring, longOptions, nullptr)) != -1)
        switch (o)
        {
            case 'e':
                compression = optarg;
                break;
            case 'r':
                argument = optarg;
                argument = hugin_utils::tolower(argument);
                if (argument == "db")
                {
                    doLookupTCA = true;
                }
                else
                {
                    if (sscanf(optarg, "%lf:%lf:%lf:%lf", &vec4[0], &vec4[1], &vec4[2], &vec4[3]) != 4)
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": invalid -r argument" << std::endl;
                        return 1;
                    }
                    srcImg.setRadialDistortionRed(vec4);
                };
                break;
            case 'g':
                argument = optarg;
                argument = hugin_utils::tolower(argument);
                if (argument == "db")
                {
                    doLookupDistortion = true;
                }
                else
                {
                    if (sscanf(optarg, "%lf:%lf:%lf:%lf", &vec4[0], &vec4[1], &vec4[2], &vec4[3]) != 4)
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": invalid -g argument" << std::endl;
                        return 1;
                    }
                    srcImg.setRadialDistortion(vec4);
                };
                break;
            case 'b':
                argument = optarg;
                argument = hugin_utils::tolower(argument);
                if (argument == "db")
                {
                    doLookupTCA = true;
                }
                else
                {
                    if (sscanf(optarg, "%lf:%lf:%lf:%lf", &vec4[0], &vec4[1], &vec4[2], &vec4[3]) != 4)
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": invalid -b argument" << std::endl;
                        return 1;
                    }
                    srcImg.setRadialDistortionBlue(vec4);
                };
                break;
            case 's':
                doCropBorders = false;
                break;
            case 'f':
                srcImg.setFlatfieldFilename(optarg);
                doFlatfield = true;
                break;
            case 'i':
                gamma = atof(optarg);
                srcImg.setGamma(gamma);
                break;
            case 'm':
                cameraMaker = optarg;
                break;
            case 'n':
                cameraName = optarg;
                break;
            case 'l':
                lensName = optarg;
                break;
            case 'd':
                focalLength = atof(optarg);
                break;
            case 'a':
                aperture = atof(optarg);
                break;
            case 'c':
                argument = optarg;
                argument = hugin_utils::tolower(argument);
                if (argument == "db")
                {
                    doLookupVignetting = true;
                }
                else
                {
                    if (sscanf(optarg, "%lf:%lf:%lf:%lf", &vec4[0], &vec4[1], &vec4[2], &vec4[3]) != 4)
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": invalid -c argument" << std::endl;
                        return 1;
                    }
                    srcImg.setRadialVigCorrCoeff(vec4);
                };
                doVigRadial = true;
                break;
            case 'h':
                usage(hugin_utils::stripPath(argv[0]).c_str());
                return 0;
            case 't':
                std::cout << "WARNING: Switch --threads is deprecated. Set environment variable OMP_NUM_THREADS instead" << std::endl;
                break;
            case 'o':
                outputFile = optarg;
                break;
            case 'x':
                if (sscanf(optarg, "%lf:%lf", &shiftX, &shiftY) != 2)
                {
                    std::cerr << hugin_utils::stripPath(argv[0]) << ": invalid -x argument" << std::endl;
                    return 1;
                }
                srcImg.setRadialDistortionCenterShift(hugin_utils::FDiff2D(shiftX, shiftY));
                break;
            case 'v':
                verbose++;
                break;
            case LINEAR_RESPONSE:
                srcImg.setResponseType(HuginBase::BaseSrcPanoImage::RESPONSE_LINEAR);
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

    if (doVigRadial && doFlatfield)
    {
        std::cerr << hugin_utils::stripPath(argv[0]) << ": cannot use -f and -c at the same time" << std::endl;
        return 1;
    }

    HuginBase::SrcPanoImage::VignettingCorrMode vm = HuginBase::SrcPanoImage::VIGCORR_NONE;

    if (doVigRadial)
    {
        vm = HuginBase::SrcPanoImage::VIGCORR_RADIAL;
    }
    if (doFlatfield)
    {
        vm = HuginBase::SrcPanoImage::VIGCORR_FLATFIELD;
    }

    vm = (HuginBase::SrcPanoImage::VignettingCorrMode) (vm | HuginBase::SrcPanoImage::VIGCORR_DIV);
    srcImg.setVigCorrMode(vm);

    unsigned nFiles = argc - optind;
    if (nFiles == 0)
    {
        std::cerr << hugin_utils::stripPath(argv[0]) << ": No input file(s) specified" << std::endl;
        return 1;
    }

    // get input images.
    std::vector<std::string> inFiles;
    std::vector<std::string> outFiles;
    if (nFiles == 1)
    {
        if (outputFile.length() !=0)
        {
            inFiles.push_back(std::string(argv[optind]));
            outFiles.push_back(outputFile);
        }
        else
        {
            std::string name(argv[optind]);
            inFiles.push_back(name);
            std::string basen = hugin_utils::stripExtension(name);
            outFiles.push_back(basen.append(batchPostfix.append(".").append(hugin_utils::getExtension(name))));
        }
    }
    else
    {
        // multiple files
        bool withExtension = false;
        if (outputFile.length() != 0)
        {
            batchPostfix = outputFile;
            withExtension = (batchPostfix.find('.') != std::string::npos);
        }
        for (int i = optind; i < argc; i++)
        {
            std::string name(argv[i]);
            inFiles.push_back(name);
            if (withExtension)
            {
                outFiles.push_back(hugin_utils::stripExtension(name) + batchPostfix);
            }
            else
            {
                outFiles.push_back(hugin_utils::stripExtension(name) + batchPostfix + "." + hugin_utils::getExtension(name));
            };
        }
    }

    if (doLookupDistortion || doLookupVignetting || doLookupTCA)
    {
        if (lensName.empty())
        {
            if (!cameraMaker.empty() && !cameraName.empty())
            {
                lensName = cameraMaker;
                lensName.append("|");
                lensName.append(cameraName);
            };
        };
    };

    // suppress tiff warnings
    TIFFSetWarningHandler(0);

    AppBase::ProgressDisplay* pdisp;
    if (verbose > 0)
    {
        pdisp = new AppBase::StreamProgressDisplay(std::cout);
    }
    else
    {
        pdisp = new AppBase::DummyProgressDisplay();
    };

    HuginBase::LensDB::LensDB& lensDB = HuginBase::LensDB::LensDB::GetSingleton();
    try
    {
        std::vector<std::string>::iterator outIt = outFiles.begin();
        for (std::vector<std::string>::iterator inIt = inFiles.begin(); inIt != inFiles.end() ; ++inIt, ++outIt)
        {
            if (verbose > 0)
            {
                std::cerr << "Correcting " << *inIt << " -> " << *outIt << endl;
            }
            HuginBase::SrcPanoImage currentImg(srcImg);
            currentImg.setFilename(*inIt);

            // load the input image
            vigra::ImageImportInfo info(inIt->c_str());
            const char* pixelType = info.getPixelType();
            int bands = info.numBands();
            int extraBands = info.numExtraBands();

            // do database lookup
            if (doLookupDistortion || doLookupVignetting || doLookupTCA)
            {
                currentImg.readEXIF();
                if (lensName.empty())
                {
                    if (currentImg.getDBLensName().empty())
                    {
                        std::cerr << "Not enough data for database lookup" << std::endl
                            << "Specify lensname (--lensname) or camera maker and model " << std::endl
                            << "(--camera-maker and --camera-model) as parameter." << std::endl;
                        continue;
                    };
                }
                else
                {
                    currentImg.setExifLens(lensName);
                }
                if (fabs(focalLength) < 0.1)
                {
                    if (fabs(currentImg.getExifFocalLength()) < 0.1)
                    {
                        std::cerr << "Could not determine focal length." << std::endl
                            << "Specify focal length (--focallength) as parameter." << std::endl;
                        continue;
                    };
                }
                else
                {
                    currentImg.setExifFocalLength(focalLength);
                };
                if (verbose > 1)
                {
                    std::cout << "Lookup in database for " << currentImg.getDBLensName() << " @ " << currentImg.getExifFocalLength() << " mm" << std::endl;
                };
                if (doLookupDistortion)
                {
                    if (!currentImg.readDistortionFromDB())
                    {
                        std::cerr << "No suitable distortion data found in database." << std::endl
                            << "Skipping image." << std::endl;
                        continue;
                    };
                    if (verbose > 1)
                    {
                        std::vector<double> dist = currentImg.getRadialDistortion();
                        std::cout << "Read distortion data: " << dist[0] << ", " << dist[1] << ", " << dist[2] << ", " << dist[3] << std::endl;
                    };
                };
                if (doLookupVignetting)
                {
                    if (fabs(aperture) < 0.1)
                    {
                        if (fabs(currentImg.getExifAperture()) < 0.1)
                        {
                            std::cerr << "Could not determine aperture." << std::endl
                                << "Specify aperture (--aperture) as parameter." << std::endl;
                            continue;
                        };
                    }
                    else
                    {
                        currentImg.setExifAperture(aperture);
                    };
                    if (!currentImg.readVignettingFromDB())
                    {
                        std::cerr << "No suitable vignetting data found in database." << std::endl
                            << "Skipping image." << std::endl;
                        continue;
                    };
                    if (verbose > 1)
                    {
                        std::vector<double> vig = currentImg.getRadialVigCorrCoeff();
                        std::cout << "Read vigneting data: " << vig[1] << ", " << vig[2] << ", " << vig[3] << std::endl;
                    };
                };

                if (doLookupTCA)
                {
                    std::vector<double> tcaRed, tcaBlue;
                    if (lensDB.GetTCA(currentImg.getDBLensName(), currentImg.getExifFocalLength(), tcaRed, tcaBlue))
                    {
                        currentImg.setRadialDistortionRed(tcaRed);
                        currentImg.setRadialDistortionBlue(tcaBlue);
                    }
                    else
                    {
                        std::cerr << "No suitable tca data found in database." << std::endl
                            << "Skipping image." << std::endl;
                        continue;
                    };
                    if (verbose>1)
                    {
                        std::cout << "Read tca data red:  " << tcaRed[0] << ", " << tcaRed[1] << ", " << tcaRed[2] << ", " << tcaRed[3] << std::endl;
                        std::cout << "Read tca data blue: " << tcaBlue[0] << ", " << tcaBlue[1] << ", " << tcaBlue[2] << ", " << tcaBlue[3] << std::endl;
                    };
                };
            };
            currentImg.setSize(info.size());
            // stitch the pano with a suitable image type
            if (bands == 3 || (bands == 4 && extraBands == 1))
            {
                // TODO: add more cases
                if (strcmp(pixelType, "UINT8") == 0)
                {
                    correctRGB<vigra::RGBValue<vigra::UInt8> >(currentImg, info, outIt->c_str(), doCropBorders, compression, pdisp);
                }
                else if (strcmp(pixelType, "UINT16") == 0)
                {
                    correctRGB<vigra::RGBValue<vigra::UInt16> >(currentImg, info, outIt->c_str(), doCropBorders, compression, pdisp);
                }
                else if (strcmp(pixelType, "INT16") == 0)
                {
                    correctRGB<vigra::RGBValue<vigra::Int16> >(currentImg, info, outIt->c_str(), doCropBorders, compression, pdisp);
                }
                else if (strcmp(pixelType, "UINT32") == 0)
                {
                    correctRGB<vigra::RGBValue<vigra::UInt32> >(currentImg, info, outIt->c_str(), doCropBorders, compression, pdisp);
                }
                else if (strcmp(pixelType, "FLOAT") == 0)
                {
                    correctRGB<vigra::RGBValue<float> >(currentImg, info, outIt->c_str(), doCropBorders, compression, pdisp);
                }
                else if (strcmp(pixelType, "DOUBLE") == 0)
                {
                    correctRGB<vigra::RGBValue<double> >(currentImg, info, outIt->c_str(), doCropBorders, compression, pdisp);
                }
            }
            else
            {
                DEBUG_ERROR("unsupported depth, only 3 channel images are supported");
                delete pdisp;
                HuginBase::LensDB::LensDB::Clean();
                throw std::runtime_error("unsupported depth, only 3 channels images are supported");
                return 1;
            }
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "caught exception: " << e.what() << std::endl;
        delete pdisp;
        HuginBase::LensDB::LensDB::Clean();
        return 1;
    }
    delete pdisp;
    HuginBase::LensDB::LensDB::Clean();
    return 0;
}

class NullTransform
{
public:
    bool transformImgCoord(double & x_dest, double & y_dest, double x_src, double y_src) const
    {
        x_dest = x_src;
        y_dest = y_src;
        return true;
    };
};


/** remap a single image
 *
 *  Be careful, might modify srcImg (vignetting and brightness correction)
 *
 */
template <class SrcImgType, class AlphaImgType, class FlatImgType, class DestImgType>
void correctImage(SrcImgType& srcImg,
                  const AlphaImgType& srcAlpha,
                  const FlatImgType& srcFlat,
                  HuginBase::SrcPanoImage src,
                  vigra_ext::Interpolator interpolator,
                  double maxValue,
                  DestImgType& destImg,
                  AlphaImgType& destAlpha,
                  bool doCrop,
                  AppBase::ProgressDisplay* progress)
{
    typedef typename SrcImgType::value_type SrcPixelType;
    typedef typename DestImgType::value_type DestPixelType;

    // prepare some information required by multiple types of vignetting correction
    progress->setMessage("correcting image");

    vigra::Diff2D shiftXY(- hugin_utils::roundi(src.getRadialDistortionCenterShift().x),
                          - hugin_utils::roundi(src.getRadialDistortionCenterShift().y));

    if ((src.getVigCorrMode() & HuginBase::SrcPanoImage::VIGCORR_FLATFIELD)
        || (src.getVigCorrMode() & HuginBase::SrcPanoImage::VIGCORR_RADIAL))
    {
        HuginBase::Photometric::InvResponseTransform<SrcPixelType, SrcPixelType> invResp(src);
        if (src.getResponseType() == HuginBase::BaseSrcPanoImage::RESPONSE_EMOR)
        {
            std::vector<double> outLut;
            vigra_ext::EMoR::createEMoRLUT(src.getEMoRParams(), outLut);
            vigra_ext::enforceMonotonicity(outLut);
            invResp.setOutput(1.0 / pow(2.0, src.getExposureValue()), outLut, maxValue);
            if (maxValue != 1.0)
            {
                // transform to range 0..1 for vignetting correction
                vigra::transformImage(srcImageRange(srcImg), destImage(srcImg), vigra::functor::Arg1()/vigra::functor::Param(maxValue));
            };
        };

        invResp.enforceMonotonicity();
        if (src.getVigCorrMode() & HuginBase::SrcPanoImage::VIGCORR_FLATFIELD)
        {
            invResp.setFlatfield(&srcFlat);
        }
        NullTransform transform;
        // dummy alpha channel
        vigra::BImage alpha(destImg.size());
        vigra_ext::transformImage(srcImageRange(srcImg), destImageRange(srcImg), destImage(alpha), vigra::Diff2D(0, 0), transform, invResp, false, vigra_ext::INTERP_SPLINE_16, progress);
    }

    // radial distortion correction
    if (doCrop)
    {
        double scaleFactor = HuginBase::Nona::estScaleFactorForFullFrame(src);
        DEBUG_DEBUG("Black border correction scale factor: " << scaleFactor);
        double sf=scaleFactor;
        std::vector<double> radGreen = src.getRadialDistortion();
        for (int i=0; i < 4; i++)
        {
            radGreen[3-i] *=sf;
            sf *=scaleFactor;
        }
        src.setRadialDistortion(radGreen);
    }

    vigra_ext::PassThroughFunctor<typename SrcPixelType::value_type> ptf;

    if (src.getCorrectTCA())
    {
        /*
        DEBUG_DEBUG("Final distortion correction parameters:" << endl
                << "r: " << radRed[0] << " " << radRed[1] << " " << radRed[2] << " " << radRed[3]  << endl
                << "g: " << radGreen[0] << " " << radGreen[1] << " " << radGreen[2] << " " << radGreen[3] << endl
                << "b: " << radBlue[0] << " " << radBlue[1] << " " << radBlue[2] << " " << radBlue[3] << endl);
        */
        // remap individual channels
        HuginBase::Nona::SpaceTransform transfr;
        transfr.InitRadialCorrect(src, 0);
        AlphaImgType redAlpha(destAlpha.size());
        if (transfr.isIdentity())
        {
            vigra::copyImage(srcIterRange(srcImg.upperLeft(), srcImg.lowerRight(), vigra::RedAccessor<SrcPixelType>()),
                             destIter(destImg.upperLeft(), vigra::RedAccessor<DestPixelType>()));
            vigra::copyImage(srcImageRange(srcAlpha), destImage(redAlpha));
        }
        else
        {
            vigra_ext::transformImageAlpha(srcIterRange(srcImg.upperLeft(), srcImg.lowerRight(), vigra::RedAccessor<SrcPixelType>()),
                                      srcImage(srcAlpha),
                                      destIterRange(destImg.upperLeft(), destImg.lowerRight(), vigra::RedAccessor<DestPixelType>()),
                                      destImage(redAlpha),
                                      shiftXY,
                                      transfr,
                                      ptf,
                                      false,
                                      vigra_ext::INTERP_SPLINE_16,
                                      progress);
        }

        HuginBase::Nona::SpaceTransform transfg;
        transfg.InitRadialCorrect(src, 1);
        AlphaImgType greenAlpha(destAlpha.size());
        if (transfg.isIdentity())
        {
            vigra::copyImage(srcIterRange(srcImg.upperLeft(), srcImg.lowerRight(), vigra::GreenAccessor<SrcPixelType>()),
                             destIter(destImg.upperLeft(), vigra::GreenAccessor<DestPixelType>()));
            vigra::copyImage(srcImageRange(srcAlpha), destImage(greenAlpha));
        }
        else
        {
            transformImageAlpha(srcIterRange(srcImg.upperLeft(), srcImg.lowerRight(), vigra::GreenAccessor<SrcPixelType>()),
                           srcImage(srcAlpha),
                           destIterRange(destImg.upperLeft(), destImg.lowerRight(), vigra::GreenAccessor<DestPixelType>()),
                           destImage(greenAlpha),
                           shiftXY,
                           transfg,
                           ptf,
                           false,
                           vigra_ext::INTERP_SPLINE_16,
                           progress);
        }

        HuginBase::Nona::SpaceTransform transfb;
        transfb.InitRadialCorrect(src, 2);
        AlphaImgType blueAlpha(destAlpha.size());
        if (transfb.isIdentity())
        {
            vigra::copyImage(srcIterRange(srcImg.upperLeft(), srcImg.lowerRight(), vigra::BlueAccessor<SrcPixelType>()),
                             destIter(destImg.upperLeft(), vigra::BlueAccessor<DestPixelType>()));
            vigra::copyImage(srcImageRange(srcAlpha), destImage(blueAlpha));

        }
        else
        {
            transformImageAlpha(srcIterRange(srcImg.upperLeft(), srcImg.lowerRight(), vigra::BlueAccessor<SrcPixelType>()),
                           srcImage(srcAlpha),
                           destIterRange(destImg.upperLeft(), destImg.lowerRight(), vigra::BlueAccessor<DestPixelType>()),
                           destImage(blueAlpha),
                           shiftXY,
                           transfb,
                           ptf,
                           false,
                           vigra_ext::INTERP_SPLINE_16,
                           progress);
        }
        vigra::combineThreeImages(srcImageRange(redAlpha), srcImage(greenAlpha), srcImage(blueAlpha), destImage(destAlpha),
            vigra::functor::Arg1() & vigra::functor::Arg2() & vigra::functor::Arg3());
    }
    else
    {
        // remap with the same coefficient.
        HuginBase::Nona::SpaceTransform transf;
        transf.InitRadialCorrect(src, 1);
        std::vector <double> radCoeff = src.getRadialDistortion();
        if (transf.isIdentity() ||
                (radCoeff[0] == 0.0 && radCoeff[1] == 0.0 && radCoeff[2] == 0.0 && radCoeff[3] == 1.0))
        {
            vigra::copyImage(srcImageRange(srcImg), destImage(destImg));
            vigra::copyImage(srcImageRange(srcAlpha), destImage(destAlpha));
        }
        else
        {
            vigra_ext::PassThroughFunctor<SrcPixelType> ptfRGB;
            transformImageAlpha(srcImageRange(srcImg),
                           srcImage(srcAlpha),
                           destImageRange(destImg),
                           destImage(destAlpha),
                           shiftXY,
                           transf,
                           ptfRGB,
                           false,
                           vigra_ext::INTERP_SPLINE_16,
                           progress);
        }
    }
}


//void correctRGB(SrcImageInfo & src, ImageImportInfo & info, const char * outfile)
template <class PIXELTYPE>
void correctRGB(HuginBase::SrcPanoImage& src, vigra::ImageImportInfo& info, const char* outfile,
                bool crop, const std::string& compression, AppBase::ProgressDisplay* progress)
{
    vigra::BasicImage<vigra::RGBValue<double> > srcImg(info.size());
    vigra::BasicImage<PIXELTYPE> output(info.size());
    vigra::BImage alpha(info.size(), 255);
    vigra::BImage outputAlpha(output.size());
    if (info.numBands() == 3)
    {
        vigra::importImage(info, destImage(srcImg));
    }
    else
    {
        importImageAlpha(info, destImage(srcImg), destImage(alpha));
    };
    vigra::FImage flatfield;
    if (src.getVigCorrMode() & HuginBase::SrcPanoImage::VIGCORR_FLATFIELD)
    {
        vigra::ImageImportInfo finfo(src.getFlatfieldFilename().c_str());
        flatfield.resize(finfo.size());
        vigra::importImage(finfo, destImage(flatfield));
    }
    correctImage(srcImg, alpha, flatfield, src, vigra_ext::INTERP_SPLINE_16, vigra_ext::getMaxValForPixelType(info.getPixelType()),
        output, outputAlpha, crop, progress);
    vigra::ImageExportInfo outInfo(outfile);
    outInfo.setICCProfile(info.getICCProfile());
    outInfo.setPixelType(info.getPixelType());
    if (compression.size() > 0)
    {
        outInfo.setCompression(compression.c_str());
    }
    const std::string filetype(vigra::getEncoder(outInfo.getFileName())->getFileType());
    if (vigra::isBandNumberSupported(filetype, 4))
    {
        // image format supports alpha channel
        std::cout << "Saving " << outInfo.getFileName() << std::endl;
        vigra::exportImageAlpha(srcImageRange(output), srcImage(outputAlpha), outInfo);
    }
    else
    {
        // image format does not support an alpha channel, disregard alpha channel
        std::cout << "Saving " << outInfo.getFileName() << " without alpha channel" << std::endl
            << "because the fileformat " << filetype << " does not support" << std::endl
            << "an alpha channel." << std::endl;
        exportImage(srcImageRange(output), outInfo);
    };
}
