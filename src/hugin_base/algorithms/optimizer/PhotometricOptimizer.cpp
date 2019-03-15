// -*- c-basic-offset: 4 -*-
/** @file PhotometricOptimizer.cpp
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id: PhotometricOptimizer.cpp 1998 2007-05-10 06:26:46Z dangelo $
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

#include "PhotometricOptimizer.h"

#include <fstream>
#include <foreign/levmar/levmar.h>
#include <photometric/ResponseTransform.h>
#include <algorithms/basic/LayerStacks.h>

#ifdef DEBUG
#define DEBUG_LOG_VIG 1
#endif


namespace HuginBase {
    
/** expects the abs(error) values */
inline double weightHuber(double x, double sigma)
{
    if (x > sigma) {
        x = sqrt(sigma* (2*x - sigma));
    }
    return x;
}



PhotometricOptimizer::OptimData::OptimData(const PanoramaData & pano, const OptimizeVector & optvars,
                                           const std::vector<vigra_ext::PointPairRGB> & data,
                                           double mEstimatorSigma, bool symmetric,
                                           int maxIter, AppBase::ProgressDisplay* progress)
  : m_pano(pano), m_data(data), huberSigma(mEstimatorSigma), symmetricError(symmetric),
    m_maxIter(maxIter), m_progress(progress)
{
    assert(pano.getNrOfImages() == optvars.size());
        
    for (unsigned i=0; i < pano.getNrOfImages(); i++) {
        m_imgs.push_back(pano.getSrcImage(i));
    }

    std::vector<std::set<std::string> > usedVars(pano.getNrOfImages());

    // create variable map with param <-> var assignments
    for (unsigned i=0; i < optvars.size(); i++) 
    {
        const std::set<std::string> vars = optvars[i];
        const SrcPanoImage & img_i = pano.getImage(i);
        for (std::set<std::string>::const_iterator it = vars.begin();
             it != vars.end(); ++it)
        {
            VarMapping var;
            var.type = *it;
            //check if variable is yet included
            if(set_contains(usedVars[i],var.type))
                continue;
            var.imgs.insert(i);
            usedVars[i].insert(var.type);
            //now check all linked images and add image nr
#define CheckLinked(name)\
            if(img_i.name##isLinked())\
            {\
                for(unsigned j=i+1;j<pano.getNrOfImages();j++)\
                    if(img_i.name##isLinkedWith(pano.getImage(j)))\
                    {\
                        var.imgs.insert(j);\
                        usedVars[j].insert(var.type);\
                    };\
            }

            if(var.type=="Eev")
            {
                CheckLinked(ExposureValue)
            };
            if(var.type=="Er")
            {
                CheckLinked(WhiteBalanceRed)
            };
            if(var.type=="Eb")
            {
                CheckLinked(WhiteBalanceBlue)
            };
            if(var.type[0]=='R')
            {
                CheckLinked(EMoRParams)
            };
            if(var.type=="Va" || var.type=="Vb" || var.type=="Vc" || var.type=="Vd")
            {
                CheckLinked(RadialVigCorrCoeff)
            }
            if(var.type=="Vx" || var.type=="Vy")
            {
                CheckLinked(RadialVigCorrCenterShift)
            };
#undef CheckLinked
            m_vars.push_back(var);
        }
    }
}

void PhotometricOptimizer::OptimData::ToX(double * x)
{
    for (size_t i=0; i < m_vars.size(); i++)
    {
        assert(m_vars[i].imgs.size() > 0);
            // get corresponding image number
        unsigned j = *(m_vars[i].imgs.begin());
            // get value
        x[i] = m_imgs[j].getVar(m_vars[i].type);
        // TODO: transform some variables, such as the vignetting center!
    }
}


void PhotometricOptimizer::OptimData::FromX(double * x)
{
    for (size_t i=0; i < m_vars.size(); i++)
    {
        // TODO: transform some variables, such as the vignetting center!
        assert(m_vars[i].imgs.size() > 0);
            // copy value int all images
        for (std::set<unsigned>::const_iterator it = m_vars[i].imgs.begin();
             it != m_vars[i].imgs.end(); ++it)
        {
            m_imgs[*it].setVar(m_vars[i].type, x[i]);
        }
    }
}



void PhotometricOptimizer::photometricError(double *p, double *x, int m, int n, void * data)
{
#ifdef DEBUG_LOG_VIG
    static int iter = 0;
#endif
    typedef Photometric::ResponseTransform<vigra::RGBValue<double> > RespFunc;
    typedef Photometric::InvResponseTransform<vigra::RGBValue<double>, vigra::RGBValue<double> > InvRespFunc;

    int xi = 0 ;

    OptimData * dat = static_cast<OptimData*>(data);
    dat->FromX(p);
#ifdef DEBUG_LOG_VIG
    std::ostringstream oss;
    oss << "vig_log_" << iter;
    iter++;
    std::ofstream log(oss.str().c_str());
    log << "VIGparams = [";
    for (int i = 0; i < m; i++) {
        log << p[i] << " ";
    }
    log << " ]; " << std::endl;
    // TODO: print parameters of images.
    std::ofstream script("vig_test.pto");
    OptimizeVector optvars(dat->m_pano.getNrOfImages());
    UIntSet imgs = dat->m_pano.getActiveImages();
    dat->m_pano.printPanoramaScript(script, optvars, dat->m_pano.getOptions(), imgs, false, "");
#endif

    size_t nImg = dat->m_imgs.size();
    std::vector<RespFunc> resp(nImg);
    std::vector<InvRespFunc> invResp(nImg);
    for (size_t i=0; i < nImg; i++) {
        resp[i] = RespFunc(dat->m_imgs[i]);
        invResp[i] = InvRespFunc(dat->m_imgs[i]);
        // calculate the monotonicity error
        double monErr = 0;
        if (dat->m_imgs[i].getResponseType() == SrcPanoImage::RESPONSE_EMOR) {
            // calculate monotonicity error
            int lutsize = resp[i].m_lutR.size();
            for (int j=0; j < lutsize-1; j++)
            {
                double d = resp[i].m_lutR[j] - resp[i].m_lutR[j+1];
                if (d > 0) {
                    monErr += d*d*lutsize;
                }
            }
        }
        x[xi++] = monErr;
		// enforce a montonous response curves
		resp[i].enforceMonotonicity();
		invResp[i].enforceMonotonicity();
    }

#ifdef DEBUG
    double sqerror=0;
#endif
    // loop over all points to calculate the error
#ifdef DEBUG_LOG_VIG
    log << "VIGval = [ ";
#endif

    for (std::vector<vigra_ext::PointPairRGB>::const_iterator it = dat->m_data.begin();
         it != dat->m_data.end(); ++it)
    {
        vigra::RGBValue<double> l2 = invResp[it->imgNr2](it->i2, it->p2);
        vigra::RGBValue<double> i2ini1 = resp[it->imgNr1](l2, it->p1);
        vigra::RGBValue<double> error = it->i1 - i2ini1;


        // if requested, calcuate the error in image 2 as well.
        //TODO: weighting dependent on the pixel value? check if outside of i2 range?
        vigra::RGBValue<double> l1 = invResp[it->imgNr1](it->i1, it->p1);
        vigra::RGBValue<double> i1ini2 = resp[it->imgNr2](l1, it->p2);
        vigra::RGBValue<double> error2 = it->i2 - i1ini2;

#ifdef DEBUG
        for (int i=0; i < 3; i++) {
            sqerror += error[i]*error[i];
            sqerror += error2[i]*error2[i];
        }
#endif

        // use huber robust estimator
        if (dat->huberSigma > 0) {
            for (int i=0; i < 3; i++) {
                x[xi++] = weightHuber(fabs(error[i]), dat->huberSigma);
                x[xi++] = weightHuber(fabs(error2[i]), dat->huberSigma);
            }
        } else {
            x[xi++] = error[0];
            x[xi++] = error[1];
            x[xi++] = error[2];
            x[xi++] = error2[0];
            x[xi++] = error2[1];
            x[xi++] = error2[2];
        }

#ifdef DEBUG_LOG_VIG
        log << it->i1.green()  << " "<< l1.green()  << " " << i1ini2.green() << "   " 
             << it->i2.green()  << " "<< l2.green()  << " " << i2ini1.green() << ";  " << std::endl;
#endif

    }
#ifdef DEBUG_LOG_VIG
    log << std::endl << "VIGerr = [";
    for (int i = 0; i < n; i++) {
        log << x[i] << std::endl;
    }
    log << " ]; " << std::endl;
#endif
#ifdef DEBUG
    DEBUG_DEBUG("squared error: " << sqerror);
#endif
}

int PhotometricOptimizer::photometricVis(double *p, double *x, int m, int n, int iter, double sqerror, void * data)
{
    OptimData * dat = static_cast<OptimData*>(data);
    char tmp[200];
    tmp[199] = 0;
    double error = sqrt(sqerror/n)*255;
    snprintf(tmp,199, "Iteration: %d, error: %f", iter, error);
    return dat->m_progress->updateDisplay(std::string(tmp)) ? 1 : 0 ;
}

void PhotometricOptimizer::optimizePhotometric(PanoramaData & pano, const OptimizeVector & vars,
                                               const std::vector<vigra_ext::PointPairRGB> & correspondences,
                                               const float imageStepSize,
                                               AppBase::ProgressDisplay* progress,
                                               double & error)
{

    OptimizeVector photometricVars;
    // keep only the photometric variables
    unsigned int optCount=0;
    for (OptimizeVector::const_iterator it=vars.begin(); it != vars.end(); ++it)
    {
        std::set<std::string> cvars;
        for (std::set<std::string>::const_iterator itv = (*it).begin();
             itv != (*it).end(); ++itv)
        {
            if ((*itv)[0] == 'E' || (*itv)[0] == 'R' || (*itv)[0] == 'V') {
                cvars.insert(*itv);
            }
        }
        photometricVars.push_back(cvars);
        optCount+=cvars.size();
    }
    //if no variables to optimize return
    if(optCount==0)
    {
        return;
    };

    int nMaxIter = 250;
    OptimData data(pano, photometricVars, correspondences, 5 * imageStepSize, false, nMaxIter, progress);

    double info[LM_INFO_SZ];

    // parameters
    int m=data.m_vars.size();
    vigra::ArrayVector<double> p(m, 0.0);

    // vector for errors
    int n=2*3*correspondences.size()+pano.getNrOfImages();
    vigra::ArrayVector<double> x(n, 0.0);

    data.ToX(p.begin());
#ifdef DEBUG
    printf("Parameters before optimisation: ");
    for(int i=0; i<m; ++i)
        printf("%.7g ", p[i]);
    printf("\n");
#endif

    // TODO: setup optimisation options with some good defaults.
    double optimOpts[5];
    
    optimOpts[0] = LM_INIT_MU;  // init mu
    // stop thresholds
    optimOpts[1] = LM_STOP_THRESH;   // ||J^T e||_inf
    optimOpts[2] = LM_STOP_THRESH;   // ||Dp||_2
    optimOpts[3] = std::pow(imageStepSize*0.1f, 2);   // ||e||_2
    // difference mode
    optimOpts[4] = LM_DIFF_DELTA;
    
    dlevmar_dif(&photometricError, &photometricVis, &(p[0]), &(x[0]), m, n, nMaxIter, optimOpts, info, NULL,NULL, &data);  // no jacobian

    // copy to source images (data.m_imgs)
    data.FromX(p.begin());
    // calculate error at solution
    data.huberSigma = 0;
    photometricError(&(p[0]), &(x[0]), m, n, &data);
    error = 0;
    for (int i=0; i<n; i++) {
        error += x[i]*x[i];
    }
    error = sqrt(error/n);

#ifdef DEBUG
    printf("Levenberg-Marquardt returned in %g iter, reason %g\nSolution: ", info[5], info[6]);
    for(int i=0; i<m; ++i)
        printf("%.7g ", p[i]);
    printf("\n\nMinimization info:\n");
    for(int i=0; i<LM_INFO_SZ; ++i)
        printf("%g ", info[i]);
    printf("\n");
#endif

    // copy settings to panorama
    for (unsigned i=0; i<pano.getNrOfImages(); i++) {
        pano.setSrcImage(i, data.m_imgs[i]);
    }
}

bool IsHighVignetting(std::vector<double> vigCorr)
{
    SrcPanoImage srcImage;
    srcImage.setRadialVigCorrCoeff(vigCorr);
    srcImage.setSize(vigra::Size2D(500, 500));
    Photometric::ResponseTransform<double> transform(srcImage);
    for (size_t x = 0; x < 250; x += 10)
    {
        const double vigFactor = transform.calcVigFactor(hugin_utils::FDiff2D(x, x));
        if (vigFactor<0.7 || vigFactor>1.1)
        {
            return true;
        };
    };
    return false;
};

bool CheckStrangeWB(PanoramaData & pano)
{
    for(size_t i=0; i<pano.getNrOfImages(); i++)
    {
        if(pano.getImage(i).getWhiteBalanceBlue()>3)
        {
            return true;
        };
        if(pano.getImage(i).getWhiteBalanceRed()>3)
        {
            return true;
        };
    };
    return false;
};

void SmartPhotometricOptimizer::smartOptimizePhotometric(PanoramaData & pano, PhotometricOptimizeMode mode,
                                                          const std::vector<vigra_ext::PointPairRGB> & correspondences,
                                                          const float imageStepSize, 
                                                          AppBase::ProgressDisplay* progress,
                                                          double & error)
{
    PanoramaOptions opts = pano.getOptions();
    UIntSet images;
    fill_set(images, 0, pano.getNrOfImages()-1);
    std::vector<UIntSet> stacks = getHDRStacks(pano, images, pano.getOptions());
    const bool singleStack = (stacks.size() == 1);

    int vars = 0;
    if (mode == OPT_PHOTOMETRIC_LDR || mode == OPT_PHOTOMETRIC_LDR_WB)
    {
        // optimize exposure
        vars = OPT_EXP;
        optimizePhotometric(pano, 
                            createOptVars(pano, vars, opts.colorReferenceImage),
                            correspondences, imageStepSize, progress, error);
    };

    if(!singleStack)
    {
        //optimize vignetting only if there are more than 1 image stack
        // for a single stack vignetting can't be calculated by this optimization
        vars |= OPT_VIG;
        VariableMapVector oldVars = pano.getVariables();
        optimizePhotometric(pano, 
                            createOptVars(pano, vars, opts.colorReferenceImage),
                            correspondences, imageStepSize, progress, error);
        // check if vignetting is plausible
        if(IsHighVignetting(pano.getImage(0).getRadialVigCorrCoeff()))
        {
            vars &= ~OPT_VIG;
            pano.updateVariables(oldVars);
        };
    };

    // now take response curve into account
    vars |= OPT_RESP;
    // also WB if desired
    if (mode == OPT_PHOTOMETRIC_LDR_WB || mode == OPT_PHOTOMETRIC_HDR_WB)
    {
        vars |= OPT_WB;
    };
    VariableMapVector oldVars = pano.getVariables();
    optimizePhotometric(pano, 
                        createOptVars(pano, vars, opts.colorReferenceImage),
                        correspondences, imageStepSize, progress, error);
    // now check the results
    const bool hasHighVignetting = IsHighVignetting(pano.getImage(0).getRadialVigCorrCoeff());
    // @TODO check also response curve, what parameters are considered invalid?
    const bool hasHighWB = CheckStrangeWB(pano);
    if(hasHighVignetting)
    {
        vars &= ~OPT_VIG;
    };
    if(hasHighWB)
    {
        vars &= ~OPT_WB;
    };
    if(hasHighVignetting || hasHighWB)
    {
        // we got strange results, optimize again with less parameters
        pano.updateVariables(oldVars);
        if(vars>0)
        {
            optimizePhotometric(pano, 
                                createOptVars(pano, vars, opts.colorReferenceImage),
                                correspondences, imageStepSize, progress, error);
        };
    };
}


bool PhotometricOptimizer::runAlgorithm()
{
    optimizePhotometric(o_panorama, 
                        o_vars, o_correspondences, o_imageStepSize,
                        getProgressDisplay(), o_resultError);
    
    // optimizePhotometric does not tell us if it's cancelled
    if (getProgressDisplay()->wasCancelled())
    {
        cancelAlgorithm();
    }
    
    return wasCancelled(); // let's hope so.
}

bool SmartPhotometricOptimizer::runAlgorithm()
{
    smartOptimizePhotometric(o_panorama,
                             o_optMode,
                             o_correspondences, o_imageStepSize,
                             getProgressDisplay(), o_resultError);
    
    // smartOptimizePhotometric does not tell us if it's cancelled
    if (getProgressDisplay()->wasCancelled())
    {
        cancelAlgorithm();
    };
    
    return !wasCancelled(); // let's hope so.
}

} //namespace
