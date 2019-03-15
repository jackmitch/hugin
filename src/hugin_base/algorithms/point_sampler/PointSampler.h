// -*- c-basic-offset: 4 -*-
/** @file RandomPointSampler.h
*
*  @author Pablo d'Angelo <pablo.dangelo@web.de>
*
*  $Id: RandomPointSampler.h,v 1.10 2007/05/10 06:26:46 dangelo Exp $
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

#ifndef _POINTSAMPLER_H
#define _POINTSAMPLER_H

#include <ctime>

#include <hugin_shared.h>
#include <hugin_config.h>
#include <vigra/stdimage.hxx>
#include <algorithms/PanoramaAlgorithm.h>

#include <random>
#include <functional>
#include <vigra_ext/utils.h>
#include <appbase/ProgressDisplay.h>
#include <panodata/PanoramaData.h>

namespace HuginBase
{
    /** class for storing the limits of an image
     *   used by the sampler to exclude too dark or too bright pixel */
    class IMPEX LimitIntensity
    {
        public:
            /** some pre-defined limits */
            enum LimitType{LIMIT_UINT8, LIMIT_UINT16, LIMIT_FLOAT};
            /** default constructor, is identical to LimitIntensity(LIMIT_FLOAT) */
            LimitIntensity();
            /** constructor, populuate with values matching the image type */
            LimitIntensity(LimitType limit);
            /** return the lower limit */
            const float GetMinI() const {
                return m_minI;
            };
            /** return the upper limit */
            const float GetMaxI() const {
                return m_maxI;
            }
        private:
            /** internal stored limits */
            float m_minI, m_maxI;
    };
    typedef std::vector<LimitIntensity> LimitIntensityVector;

    class IMPEX PointSampler : public TimeConsumingPanoramaAlgorithm
    {
        protected:
            ///
            PointSampler(PanoramaData& panorama, AppBase::ProgressDisplay* progressDisplay,
                         std::vector<vigra::FRGBImage*> images, LimitIntensityVector limits,
                         int nPoints)
                : TimeConsumingPanoramaAlgorithm(panorama, progressDisplay),
                  o_images(images), o_numPoints(nPoints), m_limits(limits)
            {};
        
        public:        
            ///
            virtual ~PointSampler() {};

        protected:
            ///
            typedef vigra_ext::ImageInterpolator<vigra::FRGBImage::const_traverser,
                                                 vigra::FRGBImage::ConstAccessor,
                                                 vigra_ext::interp_cubic           > InterpolImg;
            
            ///
            void sampleAndExtractPoints(AppBase::ProgressDisplay* progress);
            
            ///
            virtual void samplePoints(const std::vector<InterpolImg>& imgs,
                                      const std::vector<vigra::FImage*>& voteImgs,
                                      const PanoramaData& pano,
                                      const LimitIntensityVector limitI,
                                      std::vector<std::multimap<double,vigra_ext::PointPairRGB> >& radiusHist,
                                      unsigned& nGoodPoints,
                                      unsigned& nBadPoints,
                                      AppBase::ProgressDisplay*) = 0;
            
            /** extract some random points out of the bins.
            *  This should ensure that the radius distribution is
            *  roughly uniform
            */
            template<class PointPairClass>
            static void sampleRadiusUniform(const std::vector<std::multimap<double,PointPairClass> >& radiusHist,
                                            unsigned nPoints,
                                            std::vector<PointPairClass>& selectedPoints,
                                            AppBase::ProgressDisplay*);
            
        public:
            ///
            virtual bool modifiesPanoramaData() const
                { return false; }
            
            ///
            virtual bool runAlgorithm();

            PointSampler & execute()
            {
				run();
                return *this;
            }
            
        public:
            ///
            typedef std::vector<vigra_ext::PointPairRGB> PointPairs;
            
            ///
            PointPairs getResultPoints()
            {
                //[TODO] debug unsuccessful
                
                return o_resultPoints;
            }
            
            
        protected:
            std::vector<vigra::FRGBImage*> o_images;
            int o_numPoints;
            PointPairs o_resultPoints;
            LimitIntensityVector m_limits;
    };


    /**
     *
     */
    class AllPointSampler : public  PointSampler
    {
        public:
            ///
            AllPointSampler(PanoramaData& panorama, AppBase::ProgressDisplay* progressDisplay,
                               std::vector<vigra::FRGBImage*> images, LimitIntensityVector limits,
                               int nPoints)
             : PointSampler(panorama, progressDisplay, images, limits, nPoints)
            {};

            ///
            virtual ~AllPointSampler() {};
            
            
        public:
            /** sample all points inside a panorama and check for create a
             *  bins of point pairs that include a specific radius.
             */
            template <class Img, class VoteImg, class PP>
            static void sampleAllPanoPoints(const std::vector<Img> &imgs,
                                     const std::vector<VoteImg *> &voteImgs,
                                     const PanoramaData& pano,
                                     int nPoints,
                                     const LimitIntensityVector limitI,
                                     std::vector<std::multimap<double, PP > > & radiusHist,
                                     unsigned & nGoodPoints,
                                     unsigned & nBadPoints,
                                     AppBase::ProgressDisplay* progress);
            
        protected:
            ///
            virtual void samplePoints(const std::vector<InterpolImg>& imgs,
                                      const std::vector<vigra::FImage*>& voteImgs,
                                      const PanoramaData& pano,
                                      const LimitIntensityVector limitI,
                                      std::vector<std::multimap<double,vigra_ext::PointPairRGB> >& radiusHist,
                                      unsigned& nGoodPoints,
                                      unsigned& nBadPoints,
                                      AppBase::ProgressDisplay* progress)
            {
                sampleAllPanoPoints(imgs,
                                    voteImgs,
                                    pano,
                                    o_numPoints,
                                    limitI,
                                    radiusHist,
                                    nGoodPoints,
                                    nBadPoints,
                                    progress);
            }
            
    };


    /**
     *
     */
    class RandomPointSampler : public  PointSampler
    {
        public:
            ///
            RandomPointSampler(PanoramaData& panorama, AppBase::ProgressDisplay* progressDisplay,
                               std::vector<vigra::FRGBImage*> images, LimitIntensityVector limits,
                               int nPoints)
             : PointSampler(panorama, progressDisplay, images, limits, nPoints)
            {};

            ///
            virtual ~RandomPointSampler() {};
            
            
        public:
            template <class Img, class VoteImg, class PP>
            static void sampleRandomPanoPoints(const std::vector<Img>& imgs,
                                               const std::vector<VoteImg *> &voteImgs,
                                               const PanoramaData& pano,
                                               int nPoints,
                                               const LimitIntensityVector limitI,
                                               std::vector<std::multimap<double, PP > > & radiusHist,
                                               unsigned & nBadPoints,
                                               AppBase::ProgressDisplay* progress);
            
        protected:
            ///
            virtual void samplePoints(const std::vector<InterpolImg>& imgs,
                                      const std::vector<vigra::FImage*>& voteImgs,
                                      const PanoramaData& pano,
                                      const LimitIntensityVector limitI,
                                      std::vector<std::multimap<double,vigra_ext::PointPairRGB> >& radiusHist,
                                      unsigned& nGoodPoints,
                                      unsigned& nBadPoints,
                                      AppBase::ProgressDisplay* progress)
            {
                 sampleRandomPanoPoints(imgs,
                                        voteImgs,
                                        pano,
                                        5*o_numPoints,
                                        limitI,
                                        radiusHist,
                                        nBadPoints,
                                        progress);
            }
            
    };

    
    
} // namespace
    
    


    
//==============================================================================
//  templated methods

#include <panotools/PanoToolsInterface.h>

namespace HuginBase {

template<class PointPairClass>
void PointSampler::sampleRadiusUniform(const std::vector<std::multimap<double, PointPairClass> >& radiusHist,
                                       unsigned nPoints,
                                       std::vector<PointPairClass> &selectedPoints,
                                       AppBase::ProgressDisplay* progress)
{
    typedef std::multimap<double,PointPairClass> PointPairMap;
    
    // reserve selected points..
    int drawsPerBin = nPoints / radiusHist.size();
    // reallocate output vector.
    selectedPoints.reserve(drawsPerBin*radiusHist.size());
    for (typename std::vector<PointPairMap>::const_iterator bin = radiusHist.begin();
         bin != radiusHist.end(); ++bin) 
    {
        unsigned i=drawsPerBin;
        // copy into vector
        for (typename PointPairMap::const_iterator it= (*bin).begin();
             it != (*bin).end(); ++it) 
        {
            selectedPoints.push_back(it->second);
            // do not extract more than drawsPerBin Point pairs.
            --i;
            if (i == 0)
                break;
        }
    }
}
    
    
    
template <class Img, class VoteImg, class PP>
void AllPointSampler::sampleAllPanoPoints(const std::vector<Img> &imgs,
                                          const std::vector<VoteImg *> &voteImgs,
                                          const PanoramaData& pano,
                                          int nPoints,
                                          const LimitIntensityVector limitI,
                                          //std::vector<vigra_ext::PointPair> &points,
                                          std::vector<std::multimap<double, PP > > & radiusHist,
                                          unsigned & nGoodPoints,
                                          unsigned & nBadPoints,
                                          AppBase::ProgressDisplay* progress)
{
    typedef typename Img::PixelType PixelType;

    // use 10 bins
    radiusHist.resize(10);
    const unsigned pairsPerBin = nPoints / radiusHist.size();

    nGoodPoints = 0;
    nBadPoints = 0;
    vigra_precondition(imgs.size() > 1, "sampleAllPanoPoints: At least two images required");
    
    const unsigned nImg = imgs.size();

    vigra::Size2D srcSize = pano.getImage(0).getSize();
    double maxr = sqrt(((double)srcSize.x)*srcSize.x + ((double)srcSize.y)*srcSize.y) / 2.0;

    // create an array of transforms.
    //std::vector<SpaceTransform> transf(imgs.size());
    std::vector<PTools::Transform*> transf(imgs.size());

    // initialize transforms, and interpolating accessors
    for(unsigned i=0; i < nImg; i++) {
        vigra_precondition(pano.getImage(i).getSize() == srcSize, "images need to have the same size");
        transf[i] = new PTools::Transform;
        transf[i]->createTransform(pano.getImage(i), pano.getOptions());
    }

    const vigra::Rect2D roi = pano.getOptions().getROI();
    for (int y=roi.top(); y < roi.bottom(); ++y) {
        for (int x=roi.left(); x < roi.right(); ++x) {
            hugin_utils::FDiff2D panoPnt(x,y);
            for (unsigned i=0; i< nImg-1; i++) {
            // transform pixel
                hugin_utils::FDiff2D p1;
                if(!transf[i]->transformImgCoord(p1, panoPnt))
                    continue;
                vigra::Point2D p1Int(p1.toDiff2D());
                // is inside:
                if (!pano.getImage(i).isInside(p1Int)) {
                    // point is outside image
                    continue;
                }
                PixelType i1;
                vigra::UInt8 maskI;
                if (imgs[i](p1.x,p1.y, i1, maskI)){
                    float im1 = vigra_ext::getMaxComponent(i1);
                    if (limitI[i].GetMinI() > im1 || limitI[i].GetMaxI() < im1 || maskI == 0) {
                        // ignore pixels that are too dark or bright
                        continue;
                    }
                    double r1 = hugin_utils::norm((p1 - pano.getImage(i).getRadialVigCorrCenter()) / maxr);

                    // check inner image
                    for (unsigned j=i+1; j < nImg; j++) {
                        hugin_utils::FDiff2D p2;
                        if(!transf[j]->transformImgCoord(p2, panoPnt))
                            continue;
                        vigra::Point2D p2Int(p2.toDiff2D());
                        if (!pano.getImage(j).isInside(p2Int)) {
                            // point is outside image
                            continue;
                        }
                        PixelType i2;
                        vigra::UInt8 maskI2;
                        if (imgs[j](p2.x, p2.y, i2, maskI2)){
                            float im2 = vigra_ext::getMaxComponent(i2);
                            if (limitI[j].GetMinI() > im2 || limitI[j].GetMaxI() < im2 || maskI2 == 0) {
                                // ignore pixels that are too dark or bright
                                continue;
                            }
                            double r2 = hugin_utils::norm((p2 - pano.getImage(j).getRadialVigCorrCenter()) / maxr);
                            // add pixel
                            const VoteImg & vimg1 =  *voteImgs[i];
                            const VoteImg & vimg2 =  *voteImgs[j];
                            double laplace = hugin_utils::sqr(vimg1[p1Int]) + hugin_utils::sqr(vimg2[p2Int]);
                            int bin1 = (int)(r1*10);
                            int bin2 = (int)(r2*10);
                            // a center shift might lead to radi > 1.
                            if (bin1 > 9) bin1 = 9;
                            if (bin2 > 9) bin2 = 9;

                            PP pp;
                            if (im1 <= im2) {
                                // choose i1 to be smaller than i2
                                pp = PP(i, i1, p1, r1,   j, i2, p2, r2);
                            } else {
                                pp = PP(j, i2, p2, r2,   i, i1, p1, r1);
                            }

                            // decide which bin should be used.
                            std::multimap<double, PP> * map1 = &radiusHist[bin1];
                            std::multimap<double, PP> * map2 = &radiusHist[bin2];
                            std::multimap<double, PP> * destMap;
                            if (map1->empty()) {
                                destMap = map1;
                            } else if (map2->empty()) {
                                destMap = map2;
                            } else if (map1->size() < map2->size()) {
                                destMap = map1;
                            } else if (map1->size() > map2->size()) {
                                destMap = map2;
                            } else if (map1->rbegin()->first > map2->rbegin()->first) {
                                // heuristic: insert into bin with higher maximum laplacian filter response
                                // (higher probablity of misregistration).
                                destMap = map1;
                            } else {
                                destMap = map2;
                            }
                            // insert
                            destMap->insert(std::make_pair(laplace,pp));
                            // remove last element if too many elements have been gathered
                            if (destMap->size() > pairsPerBin) {
                                destMap->erase((--(destMap->end())));
                            }
                            nGoodPoints++;
                        }
                    }
                }
            }
        }
    }

    for(unsigned i=0; i < nImg; i++) {
        delete transf[i];
    }
}



template <class Img, class VoteImg, class PP>
void RandomPointSampler::sampleRandomPanoPoints(const std::vector<Img>& imgs,
                                                const std::vector<VoteImg *> &voteImgs,
                                                const PanoramaData& pano,
                                                int nPoints,
                                                const LimitIntensityVector limitI,
                                                //std::vector<PP> &points,
                                                std::vector<std::multimap<double, PP > > & radiusHist,
                                                unsigned & nBadPoints,
                                                AppBase::ProgressDisplay* progress)
{
    typedef typename Img::PixelType PixelType;

    vigra_precondition(imgs.size() > 1, "sampleRandomPanoPoints: At least two images required");
    
    const unsigned nImg = imgs.size();

    const unsigned nBins = radiusHist.size();
    const unsigned pairsPerBin = nPoints / nBins;

    // create an array of transforms.
    //std::vector<SpaceTransform> transf(imgs.size());
    std::vector<PTools::Transform *> transf(imgs.size());
    std::vector<double> maxr(imgs.size());

    // initialize transforms, and interpolating accessors
    for(unsigned i=0; i < nImg; i++) {
        // same size is not needed?
//        vigra_precondition(src[i].getSize() == srcSize, "images need to have the same size");
        transf[i] = new PTools::Transform;
        transf[i]->createTransform(pano.getImage(i), pano.getOptions());
        vigra::Size2D srcSize = pano.getImage(i).getSize();
        maxr[i] = sqrt(((double)srcSize.x)*srcSize.x + ((double)srcSize.y)*srcSize.y) / 2.0;
    }
    // init random number generator
    const vigra::Rect2D roi = pano.getOptions().getROI();
    std::mt19937 rng(static_cast<unsigned int>(std::time(0)));
    std::uniform_int_distribution<unsigned int> distribx(roi.left(), roi.right()-1);
    std::uniform_int_distribution<unsigned int> distriby(roi.top(), roi.bottom()-1);
    auto randX = std::bind(distribx, std::ref(rng));
    auto randY = std::bind(distriby, std::ref(rng));

    for (unsigned maxTry = nPoints*5; nPoints > 0 && maxTry > 0; maxTry--) {
        unsigned x = randX();
        unsigned y = randY();
        hugin_utils::FDiff2D panoPnt(x,y);
        for (unsigned i=0; i< nImg-1; i++) {
            // transform pixel
            PixelType i1;
            hugin_utils::FDiff2D p1;
            if(!transf[i]->transformImgCoord(p1, panoPnt))
                continue;
            vigra::Point2D p1Int(p1.toDiff2D());
            // check if pixel is valid
            if (!pano.getImage(i).isInside(p1Int)) {
                // point is outside image
                continue;
            }
            vigra::UInt8 maskI;
            if ( imgs[i](p1.x,p1.y, i1, maskI)){
                float im1 = vigra_ext::getMaxComponent(i1);
                if (limitI[i].GetMinI() > im1 || limitI[i].GetMaxI() < im1) {
                    // ignore pixels that are too dark or bright
                    continue;
                }
                double r1 = hugin_utils::norm((p1 - pano.getImage(i).getRadialVigCorrCenter()) / maxr[i]);
                for (unsigned j=i+1; j < nImg; j++) {
                    PixelType i2;
                    hugin_utils::FDiff2D p2;
                    if(!transf[j]->transformImgCoord(p2, panoPnt))
                        continue;
                    // check if a pixel is inside the source image
                    vigra::Point2D p2Int(p2.toDiff2D());
                    if (!pano.getImage(j).isInside(p2Int)) {
                        // point is outside image
                        continue;
                    }
                    vigra::UInt8 maskI2;
                    if (imgs[j](p2.x, p2.y, i2, maskI2)){
                        float im2 = vigra_ext::getMaxComponent(i2);
                        if (limitI[j].GetMinI() > im2 || limitI[j].GetMaxI() < im2) {
                            // ignore pixels that are too dark or bright
                            continue;
                        }
                        // TODO: add check for gradient radius.
                        double r2 = hugin_utils::norm((p2 - pano.getImage(j).getRadialVigCorrCenter()) / maxr[j]);
#if 0
                        // add pixel
                        if (im1 <= im2) {
                            points.push_back(PP(i, i1, p1, r1,   j, i2, p2, r2) );
                        } else {
                            points.push_back(PP(j, i2, p2, r2,   i, i1, p1, r1) );
                        }
#else
                            // add pixel
                            const VoteImg & vimg1 =  *voteImgs[i];
                            const VoteImg & vimg2 =  *voteImgs[j];
                            double laplace = hugin_utils::sqr(vimg1[p1Int]) + hugin_utils::sqr(vimg2[p2Int]);
                            size_t bin1 = (size_t)(r1*nBins);
                            size_t bin2 = (size_t)(r2*nBins);
                            // a center shift might lead to radi > 1.
                            if (bin1+1 > nBins) bin1 = nBins-1;
                            if (bin2+1 > nBins) bin2 = nBins-1;

                            PP pp;
                            if (im1 <= im2) {
                                // choose i1 to be smaller than i2
                                pp = PP(i, i1, p1, r1,   j, i2, p2, r2);
                            } else {
                                pp = PP(j, i2, p2, r2,   i, i1, p1, r1);
                            }

                            // decide which bin should be used.
                            std::multimap<double, PP> * map1 = &radiusHist[bin1];
                            std::multimap<double, PP> * map2 = &radiusHist[bin2];
                            std::multimap<double, PP> * destMap;
                            if (map1->empty()) {
                                destMap = map1;
                            } else if (map2->empty()) {
                                destMap = map2;
                            } else if (map1->size() < map2->size()) {
                                destMap = map1;
                            } else if (map1->size() > map2->size()) {
                                destMap = map2;
							} else if (map1->rbegin()->first > map2->rbegin()->first) {
                                // heuristic: insert into bin with higher maximum laplacian filter response
                                // (higher probablity of misregistration).
                                destMap = map1;
                            } else {
                                destMap = map2;
                            }
                            // insert
                            destMap->insert(std::make_pair(laplace,pp));
                            // remove last element if too many elements have been gathered
                            if (destMap->size() > pairsPerBin) {
                                destMap->erase((--(destMap->end())));
                            }
//                            nGoodPoints++;
#endif
                        nPoints--;
                    }
                }
            }
        }
    }
    for(unsigned i=0; i < imgs.size(); i++) {
        delete transf[i];
    }
   
}


} // namespace
#endif //_H
