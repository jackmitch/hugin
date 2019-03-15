// -*- c-basic-offset: 4 -*-

/** @file StitchingWatershed.h
 *
 *  @brief stitching images using the watershed algorithm
 *
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
 
#include <vigra/seededregiongrowing.hxx>
#include <vigra/convolution.hxx>
#include "vigra_ext/BlendPoisson.h"
#ifdef HAVE_OPENMP
#include <omp.h>
#endif
#include "openmp_vigra.h"

namespace vigra_ext
{
    namespace detail
    {
        // some helper functions 
        struct BuildSeed
        {
            template <class PixelType>
            PixelType operator()(PixelType const& v1, PixelType const& v2) const
            {
                return (v1 & 1) | (v2 & 2);
            }
        };

        template <typename t>
        inline double square(t x)
        {
            return x * x;
        }

        struct BuildDiff
        {
            template <class PixelType>
            double operator()(PixelType const& v1, PixelType const& v2) const
            {
                return std::abs(static_cast<double>(v1 - v2));
            };
            template <class PixelType>
            double operator()(vigra::RGBValue<PixelType> const& v1, vigra::RGBValue<PixelType> const& v2) const
            {
                return sqrt(square(v1.red() - v2.red()) + square(v1.green() - v2.green()) + square(v1.blue() - v2.blue()));
            };
        };

        struct CombineMasks
        {
            template <class PixelType>
            PixelType operator()(PixelType const& v1, PixelType const& v2) const
            {
                if ((v1 & 2) & v2)
                {
                    return vigra::NumericTraits<PixelType>::max();
                }
                else
                {
                    return vigra::NumericTraits<PixelType>::zero();
                };
            };
        };

        struct CombineMasksForPoisson
        {
            template <class PixelType>
            PixelType operator()(PixelType const& v1, PixelType const& v2) const
            {
                if ((v2 & 2) & v1)
                {
                    return 5;
                }
                else
                {
                    if (v1 > 0)
                    {
                        return v2;
                    }
                    else
                    {
                        return vigra::NumericTraits<PixelType>::zero();
                    };
                };
            };
        };

        template <class ImageType>
        ImageType ResizeImage(const ImageType& image, const vigra::Size2D& newSize)
        {
            ImageType newImage(std::max(image.size().width(), newSize.width()), std::max(image.size().height(), newSize.height()));
            vigra::omp::copyImage(vigra::srcImageRange(image), vigra::destImage(newImage));
            return newImage;
        };
    }; // namespace detail
    
    template <class ImageType, class MaskType>
    void MergeImages(ImageType& image1, MaskType& mask1, const ImageType& image2, const MaskType& mask2, const vigra::Diff2D offset, const bool wrap, const bool hardSeam)
    {
        const vigra::Point2D offsetPoint(offset);
        const vigra::Rect2D offsetRect(offsetPoint, mask2.size());
        //increase image size if necessary
        if (image1.width() < offsetRect.lowerRight().x || image1.height() < offsetRect.lowerRight().y)
        {
            image1 = detail::ResizeImage(image1, vigra::Size2D(offsetRect.lowerRight()));
            mask1 = detail::ResizeImage(mask1, image1.size());
        }
        // generate seed mask
        vigra::BImage labels(image2.size());
        // create a seed mask
        // value 0: pixel is not contained in image 1 or 2
        // value 1: pixel contains only information from image 1
        // value 2: pixel contains only information from image 2
        // value 3: pixel contains information from image 1 and 2
        vigra::omp::combineTwoImages(vigra::srcImageRange(mask1, offsetRect), vigra::srcImage(mask2), vigra::destImage(labels), detail::BuildSeed());
        // find bounding rectangles for all values
        vigra::ArrayOfRegionStatistics<vigra::FindBoundingRectangle> roi(3);
        vigra::inspectTwoImages(vigra::srcIterRange<vigra::Diff2D>(vigra::Diff2D(0, 0), labels.size()), vigra::srcImage(labels), roi);
        // handle some special cases
        if (roi.regions[3].size().area() == 0)
        {
            // images do not overlap, simply copy image2 into image1
            vigra::copyImageIf(vigra::srcImageRange(image2), vigra::srcImage(mask2), vigra::destImage(image1, offsetPoint, image1.accessor()));
            // now merge masks
            vigra::copyImageIf(vigra::srcImageRange(mask2), vigra::srcImage(mask2), vigra::destImage(mask1, offsetPoint, mask1.accessor()));
            return;
        };
        if (roi.regions[2].size().area() == 0)
        {
            // image 2 is fully overlapped by image 1
            // we don't need to do anything
            return;
        };
        if (roi.regions[1].size().area() == 0)
        {
            // image 1 is fully overlapped by image 2
            // copy image 2 into output
            vigra::copyImageIf(vigra::srcImageRange(image2), vigra::srcImage(mask2), vigra::destImage(image1, offsetPoint, image1.accessor()));
            // now merge masks
            vigra::copyImageIf(vigra::srcImageRange(mask2), vigra::srcImage(mask2), vigra::destImage(mask1, offsetPoint, mask1.accessor()));
            return;
        }
        const double smoothRadius = std::max(1.0, std::max(roi.regions[3].size().width(), roi.regions[3].size().height()) / 1000.0);
        const bool doWrap = wrap && (roi.regions[3].size().width() == image1.width());
        // build seed map
        vigra::omp::transformImage(vigra::srcImageRange(labels), vigra::destImage(labels), vigra::functor::Arg1() % vigra::functor::Param(3));
        // build difference, only consider overlapping area
        // increase size by 1 pixel in each direction if possible
        vigra::Point2D p1(roi.regions[3].upperLeft);
        if (p1.x > 0)
        {
            --(p1.x);
        };
        if (p1.y > 0)
        {
            --(p1.y);
        };
        vigra::Point2D p2(roi.regions[3].lowerRight);
        if (p2.x + 1 < image2.width())
        {
            ++(p2.x);
        };
        if (p2.y + 1 < image2.height())
        {
            ++(p2.y);
        };
        vigra::DImage diff(p2 - p1);
        const vigra::Rect2D rect1(offsetPoint + p1, diff.size());
        // build difference map
        vigra::omp::combineTwoImages(vigra::srcImageRange(image1, rect1), vigra::srcImage(image2, p1), vigra::destImage(diff), detail::BuildDiff());
        // scale to 0..255 to faster watershed
        vigra::FindMinMax<double> diffMinMax;
        vigra::inspectImage(vigra::srcImageRange(diff), diffMinMax);
        diffMinMax.max = std::min<double>(diffMinMax.max, 0.25f * vigra::NumericTraits<typename vigra::NumericTraits<typename ImageType::PixelType>::ValueType>::max());
        vigra::BImage diffByte(diff.size());
        vigra::omp::transformImage(vigra::srcImageRange(diff), vigra::destImage(diffByte), vigra::functor::Param(255) - vigra::functor::Param(255.0f / diffMinMax.max)*vigra::functor::Arg1());
        diff.resize(0, 0);
        // run watershed algorithm
        vigra::ArrayOfRegionStatistics<vigra::SeedRgDirectValueFunctor<vigra::UInt8> > stats(3);
        if (doWrap)
        {
            // handle wrapping
            const int oldWidth = labels.width();
            const int oldHeight = labels.height();
            vigra::BImage labelsWrapped(oldWidth * 2, oldHeight);
            vigra::omp::copyImage(vigra::srcImageRange(labels), vigra::destImage(labelsWrapped));
            vigra::omp::copyImage(labels.upperLeft(), labels.lowerRight(), labels.accessor(), labelsWrapped.upperLeft() + vigra::Diff2D(oldWidth, 0), labelsWrapped.accessor());
            vigra::BImage diffWrapped(oldWidth * 2, diffByte.height());
            vigra::omp::copyImage(vigra::srcImageRange(diffByte), vigra::destImage(diffWrapped));
            vigra::omp::copyImage(diffByte.upperLeft(), diffByte.lowerRight(), diffByte.accessor(), diffWrapped.upperLeft() + vigra::Diff2D(oldWidth, 0), diffWrapped.accessor());
            // apply gaussian smoothing with size depending radius
            // we need a minimum size of window to apply gaussianSmoothing
            if (diffWrapped.width() > 3 * smoothRadius && diffWrapped.height() > 3 * smoothRadius)
            {
                vigra::gaussianSmoothing(vigra::srcImageRange(diffWrapped), vigra::destImage(diffWrapped), smoothRadius);
            };
            vigra::fastSeededRegionGrowing(vigra::srcImageRange(diffWrapped), vigra::destImage(labelsWrapped, p1), stats, vigra::CompleteGrow, vigra::FourNeighborCode(), 255);
            vigra::omp::copyImage(labelsWrapped.upperLeft() + vigra::Diff2D(oldWidth / 2, 0), labelsWrapped.upperLeft() + vigra::Diff2D(oldWidth, oldHeight), labelsWrapped.accessor(),
                labels.upperLeft() + vigra::Diff2D(oldWidth / 2, 0), labels.accessor());
            vigra::omp::copyImage(labelsWrapped.upperLeft() + vigra::Diff2D(oldWidth, 0), labelsWrapped.upperLeft() + vigra::Diff2D(oldWidth + oldWidth / 2, oldHeight), labelsWrapped.accessor(),
                labels.upperLeft(), labels.accessor());
        }
        else
        {
            // apply gaussian smoothing with size depending radius
            // we need a minimum size of window to apply gaussianSmoothing
            if (diffByte.width() > 3 * smoothRadius && diffByte.height() > 3 * smoothRadius)
            {
                vigra::gaussianSmoothing(vigra::srcImageRange(diffByte), vigra::destImage(diffByte), smoothRadius);
            };
            vigra::fastSeededRegionGrowing(vigra::srcImageRange(diffByte), vigra::destImage(labels, p1), stats, vigra::CompleteGrow, vigra::FourNeighborCode(), 255);
        };
        // now we can merge the images
        // merging the mask is straightforward
        vigra::initImageIf(vigra::destImageRange(mask1, offsetRect), vigra::srcImage(mask2), vigra::NumericTraits<typename MaskType::value_type>::max());
        if (hardSeam)
        {
            // the watershed algorithm could also reached area where no informations are available
            vigra::omp::combineTwoImages(vigra::srcImageRange(labels), vigra::srcImage(mask2), vigra::destImage(labels), detail::CombineMasks());
            // now we can merge the images
            vigra::copyImageIf(vigra::srcImageRange(image2), vigra::srcImage(labels), vigra::destImage(image1, offsetPoint));
        }
        else
        {
            // find all boundaries in new mask
            // first filter out unused pixel the watershed algorithm has also processed
            vigra::omp::combineTwoImages(vigra::srcImageRange(mask1, offsetRect), vigra::srcImage(labels), vigra::destImage(labels), detail::CombineMasksForPoisson());
            // labels has now the following values:
            // 0: no image here
            // 1: use information from image 1
            // 5: use information from image 2
            // mark edges in labels for solving Poisson equation with different boundary conditions
            vigra::ImagePyramid<vigra::Int8Image> seams;
            const int minLength = 8;
            vigra_ext::poisson::BuildSeamPyramid(labels, seams, minLength);
            // create gradient map
            typedef typename vigra::NumericTraits<typename ImageType::PixelType>::RealPromote ImageRealPixelType;
            vigra::BasicImage<ImageRealPixelType> gradient(image2.size());
            vigra::BasicImage<ImageRealPixelType> target(image2.size());
            // build gradient map with special handling of both boundary conditions
            vigra_ext::poisson::BuildGradientMap(image1, image2, mask2, seams[0], gradient, offsetPoint, doWrap);
            // we start with the values of the image2 as begin
            vigra::omp::copyImageIf(vigra::srcImageRange(image2), vigra::srcImage(seams[0], vigra_ext::poisson::MaskGreaterAccessor<vigra::Int8>(2)), vigra::destImage(target));
            // solve poisson equation
            vigra_ext::poisson::Multigrid(target, gradient, seams, minLength, 0.01f, 500, doWrap);
            // copy result back into output
            vigra::omp::copyImageIf(vigra::srcImageRange(target), vigra::srcImage(seams[0], vigra_ext::poisson::MaskGreaterAccessor<vigra::Int8>(2)), vigra::destImage(image1, offsetPoint));
        };
    };

}
