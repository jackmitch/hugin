// -*- c-basic-offset: 4 -*-

/** @file ExtractPoints.h
 *
 *  @brief helper procedure for photometric optimizer on command line
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id$
 *
 */

/**
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

#include <hugin_basic.h>

#include <vigra/error.hxx>
#include <vigra/impex.hxx>

#include <vigra_ext/Pyramid.h>
#include <vigra_ext/impexalpha.hxx>
#include <algorithms/point_sampler/PointSampler.h>

template<class ImageType>
std::vector<ImageType *> loadImagesPyr(std::vector<std::string> files, int pyrLevel, int verbose, HuginBase::LimitIntensityVector& limits, float& imageStepSize)
{
    std::vector<ImageType *> srcImgs(files.size());
    limits.resize(srcImgs.size());
#ifdef _WIN32
    // init vigra codec manager before threaded section
    // otherwise there could be sometimes race conditions
    // which results in exceptions
    std::string s = vigra::impexListExtensions();
#endif
#pragma omp parallel for schedule(dynamic)
    for (int i=0; i < files.size(); i++) {
        ImageType * tImg = new ImageType();
        ImageType * tImg2 = new ImageType();
        std::ostringstream buf;
        vigra::ImageImportInfo info(files[i].c_str());
        tImg->resize(info.size());
        if (verbose)
            buf << "loading: " << files[i] << std::endl;
        
        const std::string pixelType(info.getPixelType());
        if (info.numExtraBands() == 1) {
            // dummy mask
            vigra::BImage mask(info.size());
            vigra::importImageAlpha(info, vigra::destImage(*tImg), vigra::destImage(mask));
            if (pixelType == "FLOAT" || pixelType == "DOUBLE")
            {
                vigra_ext::FindComponentsMinMax<float> minmax;
                vigra::inspectImageIf(vigra::srcImageRange(*tImg), vigra::srcImage(mask), minmax);
                imageStepSize = std::min(imageStepSize, (minmax.max - minmax.min) / 16384.0f);
            };
        } else {
            vigra::importImage(info, vigra::destImage(*tImg));
            if (pixelType == "FLOAT" || pixelType == "DOUBLE")
            {
                vigra_ext::FindComponentsMinMax<float> minmax;
                vigra::inspectImage(vigra::srcImageRange(*tImg), minmax);
                imageStepSize = std::min(imageStepSize, (minmax.max - minmax.min) / 16384.0f);
            };
        }
        float div = 1;
        limits[i] = HuginBase::LimitIntensity(HuginBase::LimitIntensity::LIMIT_FLOAT);
        if (pixelType== "UINT8") {
            div = 255;
            limits[i] = HuginBase::LimitIntensity(HuginBase::LimitIntensity::LIMIT_UINT8);
            imageStepSize = std::min(imageStepSize, 1 / 255.0f);

        } else if (pixelType=="UINT16") {
            div = (1<<16)-1;
            limits[i] = HuginBase::LimitIntensity(HuginBase::LimitIntensity::LIMIT_UINT16);
            imageStepSize = std::min(imageStepSize, 1 / 65536.0f);
        }
        
        if (pyrLevel) {
            ImageType * swap;
            // create downscaled image
            if (verbose > 0) {
                buf << "downscaling: ";
            }
            for (int l=pyrLevel; l > 0; l--) {
                if (verbose > 0) {
                    buf << tImg->size().x << "x" << tImg->size().y << "  " << std::flush;
                }
                vigra_ext::reduceToNextLevel(*tImg, *tImg2);
                swap = tImg;
                tImg = tImg2;
                tImg2 = swap;
            }
            if (verbose > 0)
                buf << std::endl;
        }
        if (div > 1) {
            div = 1/div;
            transformImage(vigra::srcImageRange(*tImg), vigra::destImage(*tImg),
                           vigra::functor::Arg1()*vigra::functor::Param(div));
        }
        srcImgs[i]=tImg;
        delete tImg2;
        if (verbose > 0)
        {
            std::cout << buf.str();
        };
    }
    return srcImgs;
}


// needs 2.0 progress steps
void loadImgsAndExtractPoints(HuginBase::Panorama pano, int nPoints, int pyrLevel, bool randomPoints, AppBase::ProgressDisplay& progress, std::vector<vigra_ext::PointPairRGB> & points, int verbose, float& imageStepSize)
{
    // extract file names
    std::vector<std::string> files;
    for (size_t i=0; i < pano.getNrOfImages(); i++)
        files.push_back(pano.getImage(i).getFilename());
    
    std::vector<vigra::FRGBImage*> images;
    HuginBase::LimitIntensityVector limits;
    imageStepSize = 1 / 255.0f;
    
    // try to load the images.
    images = loadImagesPyr<vigra::FRGBImage>(files, pyrLevel, verbose, limits, imageStepSize);
    
    progress.setMessage("Sampling points");
    if(randomPoints)
        points = HuginBase::RandomPointSampler(pano, &progress, images, limits, nPoints).execute().getResultPoints();
    else
        points = HuginBase::AllPointSampler(pano, &progress, images, limits, nPoints).execute().getResultPoints();
    progress.taskFinished();
}
