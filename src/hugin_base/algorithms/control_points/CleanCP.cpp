// -*- c-basic-offset: 4 -*-
/**  @file CleanCP.cpp
 *
 *  @brief algorithms for remove control points by statistic method
 *  
 *  the algorithm is based on ptoclean by Bruno Postle
 *
 *  @author Thomas Modes
 *
 *  $Id$
 *
 */
 
 /*  This is free software; you can redistribute it and/or
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

#include "CleanCP.h"
#include <algorithms/optimizer/PTOptimizer.h>
#include "algorithms/basic/CalculateCPStatistics.h"
#include "hugin_base/panotools/PanoToolsUtils.h"

namespace HuginBase {

UIntSet getCPoutsideLimit_pair(Panorama pano, AppBase::ProgressDisplay& progress, double n)
{
    CPVector allCP=pano.getCtrlPoints();
    unsigned int nrImg=pano.getNrOfImages();
    PanoramaOptions opts=pano.getOptions();
    //set projection to equrectangular for optimisation
    opts.setProjection(PanoramaOptions::EQUIRECTANGULAR);
    pano.setOptions(opts);
    UIntSet CPtoRemove;

    // do optimisation of all images pair
    // after it remove cp with errors > median/mean + n*sigma
    for (unsigned int image1=0; image1<nrImg-1; image1++)
    {
        const SrcPanoImage& img=pano.getImage(image1);
        for (unsigned int image2=image1+1; image2<nrImg; image2++)
        {
            //do not check linked image pairs
            if(img.YawisLinkedWith(pano.getImage(image2)))
                continue;
            UIntSet Images;
            Images.clear();
            Images.insert(image1);
            Images.insert(image2);
            Panorama clean=pano.getSubset(Images);
            // pictures should contain at least 2 control points
            if(clean.getNrOfCtrlPoints()>1)
            {
                // remove all horizontal and vertical control points
                CPVector cpl = clean.getCtrlPoints();
                CPVector newCP;
                for (CPVector::const_iterator it = cpl.begin(); it != cpl.end(); ++it) 
                    if (it->mode == ControlPoint::X_Y)
                        newCP.push_back(*it);
                clean.setCtrlPoints(newCP);

                // we need at least 3 cp to optimize 3 variables: yaw, pitch and roll
                if(clean.getNrOfCtrlPoints()>3)
                {
                    //optimize position and hfov
                    OptimizeVector optvec;
                    std::set<std::string> imgopt;
                    //imgopt.insert("v");
                    optvec.push_back(imgopt);
                    imgopt.insert("r");
                    imgopt.insert("p");
                    imgopt.insert("y");
                    optvec.push_back(imgopt);
                    clean.setOptimizeVector(optvec);
                    PTools::optimize(clean);
                    cpl.clear();
                    cpl=clean.getCtrlPoints();
                    //calculate statistic and determine limit
                    double min,max,mean,var;
                    CalculateCPStatisticsError::calcCtrlPntsErrorStats(clean,min,max,mean,var);
                    // if the standard deviation is bigger than the value, assume we have a lot of
                    // false cp, in this case take the mean value directly as limit
                    double limit = (sqrt(var) > mean) ? mean : (mean + n*sqrt(var));

                    //identify cp with big error
                    unsigned int index=0;
                    unsigned int cpcounter=0;
                    for (CPVector::const_iterator it = allCP.begin(); it != allCP.end(); ++it)
                    {
                        if(it->mode == ControlPoint::X_Y)
                            if((it->image1Nr==image1 && it->image2Nr==image2) ||
                                (it->image1Nr==image2 && it->image2Nr==image1))
                            {
                                if (cpl[index].error>limit)
                                    CPtoRemove.insert(cpcounter);
                                index++;
                            };
                        cpcounter++;
                    };
                };
            };
            if (!progress.updateDisplayValue())
            {
                return CPtoRemove;
            };
        };
    };

    return CPtoRemove;
};

UIntSet getCPoutsideLimit(Panorama pano, double n, bool skipOptimisation, bool includeLineCp)
{
    UIntSet CPtoRemove;
    if(skipOptimisation)
    {
        //calculate current cp errors
        HuginBase::PTools::calcCtrlPointErrors(pano);
    }
    else
    {
        //optimize pano, after optimization pano contains the cp errors of the optimized project
        SmartOptimise::smartOptimize(pano);
    };
    CPVector allCP=pano.getCtrlPoints();
    if(!includeLineCp)
    {
        //remove all horizontal and vertical CP for calculation of mean and sigma
        CPVector CPxy;
        for (CPVector::const_iterator it = allCP.begin(); it != allCP.end(); ++it)
        {
            if(it->mode == ControlPoint::X_Y)
                CPxy.push_back(*it);
        };
        pano.setCtrlPoints(CPxy);
    };
    //calculate mean and sigma
    double min,max,mean,var;
    CalculateCPStatisticsError::calcCtrlPntsErrorStats(pano,min,max,mean,var);
    if(!includeLineCp)
    {
        pano.setCtrlPoints(allCP);
    };
    // if the standard deviation is bigger than the value, assume we have a lot of
    // false cp, in this case take the mean value directly as limit
    double limit = (sqrt(var) > mean) ? mean : (mean + n*sqrt(var));

    //now determine all control points with error > limit 
    unsigned int index=0;
    for (CPVector::const_iterator it = allCP.begin(); it != allCP.end(); ++it)
    {
        if(it->error > limit)
        {
            if(includeLineCp)
            {
                // all cp are treated the same
                // no need for further checks
                CPtoRemove.insert(index);
            }
            else
            {
                //check only normal cp
                if(it->mode == ControlPoint::X_Y)
                {
                    CPtoRemove.insert(index);
                };
            };
        };
        index++;
    };

    return CPtoRemove;
};

UIntSet getCPinMasks(HuginBase::Panorama pano)
{
    HuginBase::UIntSet cps;
    HuginBase::CPVector cpList=pano.getCtrlPoints();
    if(cpList.size()>0)
    {
        for(unsigned int i=0;i<cpList.size();i++)
        {
            HuginBase::ControlPoint cp=cpList[i];
            // ignore line control points
            if(cp.mode!=HuginBase::ControlPoint::X_Y)
                continue;
            bool insideMask=false;
            // check first image
            // remark: we could also use pano.getImage(cp.image1Nr).isInside(vigra::Point2D(cp.x1,cp.y1))
            //   this would also check the crop rectangles/circles
            //   but it would require that the pano is correctly align, otherwise the positive masks
            //   would not correctly checked
            HuginBase::MaskPolygonVector masks=pano.getImage(cp.image1Nr).getMasks();
            if(masks.size()>0)
            {
                unsigned int j=0;
                while((!insideMask) && (j<masks.size()))
                {
                    insideMask=masks[j].isInside(hugin_utils::FDiff2D(cp.x1,cp.y1));
                    j++;
                };
            };
            // and now the second
            if(!insideMask)
            {
                masks=pano.getImage(cp.image2Nr).getMasks();
                if(masks.size()>0)
                {
                    unsigned int j=0;
                    while((!insideMask) && (j<masks.size()))
                    {
                        insideMask=masks[j].isInside(hugin_utils::FDiff2D(cp.x2,cp.y2));
                        j++;
                    };
                };
            };
            if(insideMask)
                cps.insert(i);
        };
    }
    return cps;
};

}  // namespace
