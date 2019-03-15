// -*- c-basic-offset: 4 -*-
/** @file CalculateCPStatistics.h
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id$
 *
 * !! from Panorama.h 1947 
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

#ifndef _BASICALGORITHMS_CALCULATECPSTSTISTICS_H
#define _BASICALGORITHMS_CALCULATECPSTSTISTICS_H

#include <hugin_shared.h>
#include <algorithms/PanoramaAlgorithm.h>



namespace HuginBase {


/// just some common implementation; probably not so useful
class IMPEX CalculateCPStatistics : public PanoramaAlgorithm
{

    protected:
        ///
        CalculateCPStatistics(PanoramaData& panorama, const int& imgNr=-1)
            : PanoramaAlgorithm(panorama), o_imageNumber(imgNr), o_resultMin(0), o_resultMax(0), o_resultMean(0), o_resultVar(0)
        {};
        
    public:
        ///
        virtual ~CalculateCPStatistics() {};
        
        
    public:
        ///
        virtual bool modifiesPanoramaData() const
            { return false; }
            
        ///
        virtual bool runAlgorithm() =0;
          
        
    public:
        ///
        virtual double getResultMin()
        {
            // [TODO] if(!hasRunSuccessfully()) DEBUG;
            return o_resultMin;
        }
        
        ///
        virtual double getResultMax()
        {
            // [TODO] if(!hasRunSuccessfully()) DEBUG;
            return o_resultMax;
        }
        
        ///
        virtual double getResultMean()
        {
            // [TODO] if(!hasRunSuccessfully()) DEBUG;
            return o_resultMean;
        }
    
        ///
        virtual double getResultVariance()
        {
            // [TODO] if(!hasRunSuccessfully()) DEBUG;
            return o_resultVar;
        }
        
    protected:
        int o_imageNumber;
        double o_resultMin, o_resultMax, o_resultMean, o_resultVar;
};
    
    
    
    
class IMPEX CalculateCPStatisticsError : public CalculateCPStatistics
{

    public:
        ///
        CalculateCPStatisticsError(PanoramaData& panorama, const int& imgNr=-1)
         : CalculateCPStatistics(panorama, imgNr)
        {};
        CalculateCPStatisticsError(PanoramaData& panorama, const bool onlyActive, const bool ignoreLineCps=false)
            :CalculateCPStatistics(panorama, -1), m_onlyActiveImages(onlyActive), m_ignoreLineCps(ignoreLineCps)
        {};
        
        ///
        virtual ~CalculateCPStatisticsError() {};
          
        
    public:
        ///
        static void calcCtrlPntsErrorStats(const PanoramaData& pano,
                                           double & min, double & max, double & mean,
                                           double & var,
                                           const int& imgNr=-1, 
                                           const bool onlyActive=false,
                                           const bool ignoreLineCp=false);
        
        
    public:
        ///
        virtual bool runAlgorithm()
        {
            calcCtrlPntsErrorStats(o_panorama, 
                                   o_resultMin, o_resultMax, o_resultMean,
                                   o_resultVar,
                                   o_imageNumber, m_onlyActiveImages, m_ignoreLineCps);
            return true; // let's hope so.
        }
    private:
        bool m_onlyActiveImages = false;
        bool m_ignoreLineCps = false;
};


class IMPEX CalculateCPStatisticsRadial : public CalculateCPStatistics
{
    
    public:
        ///
        CalculateCPStatisticsRadial(PanoramaData& panorama, const int& imgNr=-1)
            : CalculateCPStatistics(panorama, imgNr), o_resultQ10(0), o_resultQ90(0)
        {};
        
        ///
        virtual ~CalculateCPStatisticsRadial() {};
        
        
    public:
        ///
        static void calcCtrlPntsRadiStats(const PanoramaData& pano,
                                          double & min, double & max, double & mean, double & var,
                                          double & q10, double & q90, 
                                          const int& imgNr=-1);
        
        ///
        virtual double getResultPercentile10()
        {
            // [TODO] if(!hasRunSuccessfully()) DEBUG;
            return o_resultQ10;
        }
        
        ///
        virtual double getResultPercentile90()
        {
            // [TODO] if(!hasRunSuccessfully()) DEBUG;
            return o_resultQ90;
        }
        
        
    public:
        ///
        virtual bool runAlgorithm()
        {
                calcCtrlPntsRadiStats(o_panorama, 
                                      o_resultMin, o_resultMax, o_resultMean, o_resultVar,
                                      o_resultQ10, o_resultQ90,
                                      o_imageNumber);
                return true; // let's hope so.
        }
        
        
    protected:
        double o_resultQ10, o_resultQ90;
        
};

} //namespace
#endif //_H
