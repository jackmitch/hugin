// -*- c-basic-offset: 4 ; tab-width: 4 -*-
/*
* Copyright (C) 2007-2008 Anael Orlinski
*
* This file is part of Panomatic.
*
* Panomatic is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* Panomatic is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Panomatic; if not, write to the Free Software
* <http://www.gnu.org/licenses/>.
*/

#include "PanoDetector.h"
#include <iostream>
#include <fstream>
#include <sstream>

#include <time.h>

#include "Utils.h"
#include "Tracer.h"
#include "hugin_base/hugin_utils/platform.h"

#include <algorithms/nona/ComputeImageROI.h>
#include <algorithms/basic/CalculateOptimalScale.h>
#include <algorithms/basic/LayerStacks.h>
#include <nona/RemappedPanoImage.h>
#include <nona/ImageRemapper.h>

//for multi row strategy
#include <panodata/StandardImageVariableGroups.h>
#include <panodata/Panorama.h>
#include <algorithms/optimizer/ImageGraph.h>
#include <algorithms/optimizer/PTOptimizer.h>
#include <algorithms/basic/CalculateOverlap.h>

#include "ImageImport.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <hugin_config.h>

#ifndef srandom
#define srandom srand
#endif

#ifdef HAVE_OPENMP
#include <omp.h>
#endif

#define TRACE_IMG(X) {if (_panoDetector.getVerbose() == 1) {TRACE_INFO("i" << _imgData._number << " : " << X << std::endl);}}
#define TRACE_PAIR(X) {if (_panoDetector.getVerbose() == 1){ TRACE_INFO("i" << _matchData._i1->_number << " <> " \
                "i" << _matchData._i2->_number << " : " << X << std::endl);}}

std::string includeTrailingPathSep(std::string path)
{
    std::string pathWithSep(path);
#ifdef _WIN32
    if(pathWithSep[pathWithSep.length()-1]!='\\' || pathWithSep[pathWithSep.length()-1]!='/')
    {
        pathWithSep.append("\\");
    }
#else
    if(pathWithSep[pathWithSep.length()-1]!='/')
    {
        pathWithSep.append("/");
    }
#endif
    return pathWithSep;
};

std::string getKeyfilenameFor(std::string keyfilesPath, std::string filename)
{
    std::string newfilename;
    if(keyfilesPath.empty())
    {
        //if no path for keyfiles is given we are saving into the same directory as image file
        newfilename=hugin_utils::stripExtension(filename);
    }
    else
    {
        newfilename = includeTrailingPathSep(keyfilesPath);
        newfilename.append(hugin_utils::stripPath(hugin_utils::stripExtension(filename)));
    };
    newfilename.append(".key");
    return newfilename;
};

PanoDetector::PanoDetector() :
    _writeAllKeyPoints(false), _verbose(1),
    _sieve1Width(10), _sieve1Height(10), _sieve1Size(100),
    _kdTreeSearchSteps(200), _kdTreeSecondDistance(0.25),
    _minimumMatches(6), _ransacMode(HuginBase::RANSACOptimizer::AUTO), _ransacIters(1000), _ransacDistanceThres(50),
    _sieve2Width(5), _sieve2Height(5), _sieve2Size(1),
    _matchingStrategy(ALLPAIRS), _linearMatchLen(1),
    _test(false), _cores(0), _downscale(true), _cache(false), _cleanup(false),
    _celeste(false), _celesteThreshold(0.5), _celesteRadius(20), 
    _keypath(""), _outputFile("default.pto"), _outputGiven(false), svmModel(NULL)
{
    _panoramaInfo = new HuginBase::Panorama();
}

PanoDetector::~PanoDetector()
{
    delete _panoramaInfo;
}

bool PanoDetector::checkData()
{
    // test linear match data
    if (_linearMatchLen < 1)
    {
        std::cout << "Linear match length must be at least 1." << std::endl;
        return false;
    }

    // check the test mode
    if (_test)
    {
        if (_filesData.size() != 2)
        {
            std::cout << "In test mode you must provide exactly 2 images." << std::endl;
            return false;
        }
    }

    return true;
}

void PanoDetector::printDetails()
{
    std::cout << "Input file           : " << _inputFile << std::endl;
    if (_keyPointsIdx.size() != 0)
    {
        std::cout << "Output file(s)       : keyfile(s) for images";
        for (unsigned int i = 0; i < _keyPointsIdx.size(); ++i)
        {
            std::cout << " " << _keyPointsIdx[i] << std::endl;
        }
    }
    else
    {
        if(_writeAllKeyPoints)
        {
            std::cout << "Output file(s)       : keyfiles for all images in project" << std::endl;
        }
        else
        {
            std::cout << "Output file          : " << _outputFile << std::endl;
        };
    }
    if(_keypath.size()>0)
    {
        std::cout <<     "Path to keyfiles     : " << _keypath << std::endl;
    };
    if(_cleanup)
    {
        std::cout << "Cleanup temporary files." << std::endl;
    };
    if(_cache)
    {
        std::cout << "Automatically cache keypoints files to disc." << std::endl;
    };
#ifdef HAVE_OPENMP
    std::cout << "Number of threads  : " << (_cores>0 ? _cores : omp_get_max_threads()) << std::endl << std::endl;
#endif
    std::cout << "Input image options" << std::endl;
    std::cout << "  Downscale to half-size : " << (_downscale?"yes":"no") << std::endl;
    if(_celeste)
    {
        std::cout << "Celeste options" << std::endl;
        std::cout << "  Threshold : " << _celesteThreshold << std::endl;
        std::cout << "  Radius : " << _celesteRadius << std::endl;
    }
    std::cout << "Sieve 1 Options" << std::endl;
    std::cout << "  Width : " << _sieve1Width << std::endl;
    std::cout << "  Height : " << _sieve1Height << std::endl;
    std::cout << "  Size : " << _sieve1Size << std::endl;
    std::cout << "  ==> Maximum keypoints per image : " << _sieve1Size* _sieve1Height* _sieve1Width << std::endl;
    std::cout << "KDTree Options" << std::endl;
    std::cout << "  Search steps : " << _kdTreeSearchSteps << std::endl;
    std::cout << "  Second match distance : " << _kdTreeSecondDistance << std::endl;
    std::cout << "Matching Options" << std::endl;
    switch(_matchingStrategy)
    {
        case ALLPAIRS:
            std::cout << "  Mode : All pairs" << std::endl;
            break;
        case LINEAR:
            std::cout << "  Mode : Linear match with length of " << _linearMatchLen << " image" << std::endl;
            break;
        case MULTIROW:
            std::cout << "  Mode : Multi row" << std::endl;
            break;
        case PREALIGNED:
            std::cout << "  Mode : Prealigned positions" << std::endl;
            break;
    };
    std::cout << "  Distance threshold : " << _ransacDistanceThres << std::endl;
    std::cout << "RANSAC Options" << std::endl;
    std::cout << "  Mode : ";
    switch (_ransacMode)
    {
        case HuginBase::RANSACOptimizer::AUTO:
            std::cout << "auto" << std::endl;
            break;
        case HuginBase::RANSACOptimizer::HOMOGRAPHY:
            std::cout << "homography" << std::endl;
            break;
        case HuginBase::RANSACOptimizer::RPY:
            std::cout << "roll, pitch, yaw" << std::endl;
            break;
        case HuginBase::RANSACOptimizer::RPYV:
            std::cout << "roll, pitch, yaw, fov" << std::endl;
            break;
        case HuginBase::RANSACOptimizer::RPYVB:
            std::cout << "roll, pitch, yaw, fov, distortion" << std::endl;
            break;
    }
    std::cout << "  Iterations : " << _ransacIters << std::endl;
    std::cout << "  Distance threshold : " << _ransacDistanceThres << std::endl;
    std::cout << "Minimum matches per image pair: " << _minimumMatches << std::endl;
    std::cout << "Sieve 2 Options" << std::endl;
    std::cout << "  Width : " << _sieve2Width << std::endl;
    std::cout << "  Height : " << _sieve2Height << std::endl;
    std::cout << "  Size : " << _sieve2Size << std::endl;
    std::cout << "  ==> Maximum matches per image pair : " << _sieve2Size* _sieve2Height* _sieve2Width << std::endl;
}

void PanoDetector::printFilenames()
{
    std::cout << std::endl << "Project contains the following images:" << std::endl;
    for(unsigned int i=0; i<_panoramaInfo->getNrOfImages(); i++)
    {
        std::string name(_panoramaInfo->getImage(i).getFilename());
        if(name.compare(0,_prefix.length(),_prefix)==0)
        {
            name=name.substr(_prefix.length(),name.length()-_prefix.length());
        }
        std::cout << "Image " << i << std::endl << "  Imagefile: " << name << std::endl;
        bool writeKeyfileForImage=false;
        if(_keyPointsIdx.size()>0)
        {
            for(unsigned j=0; j<_keyPointsIdx.size() && !writeKeyfileForImage; j++)
            {
                writeKeyfileForImage=_keyPointsIdx[j]==i;
            };
        };
        if(_cache || _filesData[i]._hasakeyfile || writeKeyfileForImage)
        {
            name=_filesData[i]._keyfilename;
            if(name.compare(0,_prefix.length(),_prefix)==0)
            {
                name=name.substr(_prefix.length(),name.length()-_prefix.length());
            }
            std::cout << "  Keyfile  : " << name;
            if(writeKeyfileForImage)
            {
                std::cout << " (will be generated)" << std::endl;
            }
            else
            {
                std::cout << (_filesData[i]._hasakeyfile?" (will be loaded)":" (will be generated)") << std::endl;
            };
        };
        std::cout << "  Remapped : " << (_filesData[i].NeedsRemapping()?"yes":"no") << std::endl;
    };
};

class Runnable
{
public:
    Runnable() {};
    virtual void run() = 0;
    virtual ~Runnable() {};
};

// definition of a runnable class for image data
class ImgDataRunnable : public Runnable
{
public:
    ImgDataRunnable(PanoDetector::ImgData& iImageData, const PanoDetector& iPanoDetector) : Runnable(),
        _panoDetector(iPanoDetector), _imgData(iImageData) {};

    virtual void run()
    {
        TRACE_IMG("Analyzing image...");
        if (!PanoDetector::AnalyzeImage(_imgData, _panoDetector))
        {
            return;
        }
        PanoDetector::FindKeyPointsInImage(_imgData, _panoDetector);
        PanoDetector::FilterKeyPointsInImage(_imgData, _panoDetector);
        PanoDetector::MakeKeyPointDescriptorsInImage(_imgData, _panoDetector);
        PanoDetector::RemapBackKeypoints(_imgData, _panoDetector);
        PanoDetector::BuildKDTreesInImage(_imgData, _panoDetector);
        PanoDetector::FreeMemoryInImage(_imgData, _panoDetector);
    }
private:
    const PanoDetector&			_panoDetector;
    PanoDetector::ImgData&		_imgData;
};

// definition of a runnable class for writeKeyPoints
class WriteKeyPointsRunnable : public Runnable
{
public:
    WriteKeyPointsRunnable(PanoDetector::ImgData& iImageData, const PanoDetector& iPanoDetector) :
        _panoDetector(iPanoDetector), _imgData(iImageData) {};

    virtual void run()
    {
        TRACE_IMG("Analyzing image...");
        if (!PanoDetector::AnalyzeImage(_imgData, _panoDetector))
        {
            return;
        }
        PanoDetector::FindKeyPointsInImage(_imgData, _panoDetector);
        PanoDetector::FilterKeyPointsInImage(_imgData, _panoDetector);
        PanoDetector::MakeKeyPointDescriptorsInImage(_imgData, _panoDetector);
        PanoDetector::RemapBackKeypoints(_imgData, _panoDetector);
        PanoDetector::FreeMemoryInImage(_imgData, _panoDetector);
    }
private:
    const PanoDetector&			_panoDetector;
    PanoDetector::ImgData&		_imgData;
};

// definition of a runnable class for keypoints data
class LoadKeypointsDataRunnable : public Runnable
{
public:
    LoadKeypointsDataRunnable(PanoDetector::ImgData& iImageData, const PanoDetector& iPanoDetector) :
        _panoDetector(iPanoDetector), _imgData(iImageData) {};

    virtual void run()
    {
        TRACE_IMG("Loading keypoints...");
        PanoDetector::LoadKeypoints(_imgData, _panoDetector);
        PanoDetector::BuildKDTreesInImage(_imgData, _panoDetector);
    }

private:
    const PanoDetector&			_panoDetector;
    PanoDetector::ImgData&		_imgData;
};

// definition of a runnable class for MatchData
class MatchDataRunnable : public Runnable
{
public:
    MatchDataRunnable(PanoDetector::MatchData& iMatchData, const PanoDetector& iPanoDetector) :
        _panoDetector(iPanoDetector), _matchData(iMatchData) {};

    virtual void run()
    {
        //TRACE_PAIR("Matching...");
        if(_matchData._i1->_kp.size()>0 && _matchData._i2->_kp.size()>0)
        {
            PanoDetector::FindMatchesInPair(_matchData, _panoDetector);
            PanoDetector::RansacMatchesInPair(_matchData, _panoDetector);
            PanoDetector::FilterMatchesInPair(_matchData, _panoDetector);
            TRACE_PAIR("Found " << _matchData._matches.size() << " matches");
        };
    }
private:
    const PanoDetector&			_panoDetector;
    PanoDetector::MatchData&	_matchData;
};

bool PanoDetector::LoadSVMModel()
{
    std::string model_file = ("celeste.model");
    std::ifstream test(model_file.c_str());
    if (!test.good())
    {
        std::string install_path_model=hugin_utils::GetDataDir();
        install_path_model.append(model_file);
        std::ifstream test2(install_path_model.c_str());
        if (!test2.good())
        {
            std::cout << std::endl << "Couldn't open SVM model file " << model_file << std::endl;
            std::cout << "Also tried " << install_path_model << std::endl << std::endl;
            return false;
        };
        model_file = install_path_model;
    }
    if(!celeste::loadSVMmodel(svmModel,model_file))
    {
        svmModel=NULL;
        return false;
    };
    return true;
};

typedef std::vector<Runnable*> RunnableVector;

void RunQueue(std::vector<Runnable*>& queue)
{
#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < queue.size(); ++i)
    {
        queue[i]->run();
    };
    // now clear queue
    while (!queue.empty())
    {
        delete queue.back();
        queue.pop_back();
    };
};

void PanoDetector::run()
{
    // init the random time generator
    srandom((unsigned int)time(NULL));

    // Load the input project file
    if(!loadProject())
    {
        return;
    };
    if(_writeAllKeyPoints)
    {
        for(unsigned int i=0; i<_panoramaInfo->getNrOfImages(); i++)
        {
            _keyPointsIdx.push_back(i);
        };
    };

    if(_cleanup)
    {
        CleanupKeyfiles();
        return;
    };

    //checking, if memory allows running desired number of threads
    unsigned long maxImageSize=0;
    bool withRemap=false;
    for (ImgDataIt_t aB = _filesData.begin(); aB != _filesData.end(); ++aB)
    {
        if(!aB->second._hasakeyfile)
        {
            maxImageSize=std::max<unsigned long>(aB->second._detectWidth*aB->second._detectHeight,maxImageSize);
            if(aB->second.NeedsRemapping())
            {
                withRemap=true;
            };
        };
    };
#ifdef HAVE_OPENMP
    if (_cores == 0)
    {
        setCores(omp_get_max_threads());
    };
    if (maxImageSize != 0)
    {
        unsigned long long maxCores;
        //determinded factors by testing of some projects
        //the memory usage seems to be very high
        //if the memory usage could be decreased these numbers can be decreased
        if(withRemap)
        {
            maxCores=utils::getTotalMemory()/(maxImageSize*75);
        }
        else
        {
            maxCores=utils::getTotalMemory()/(maxImageSize*50);
        };
        if(maxCores<1)
        {
            maxCores=1;
        }
        if(maxCores<_cores)
        {
            if(getVerbose()>0)
            {
                std::cout << "\nThe available memory does not allow running " << _cores << " threads parallel.\n"
                            << "Running cpfind with " << maxCores << " threads.\n";
            };
            setCores(maxCores);
        };
    };
    omp_set_num_threads(_cores);
#endif
    RunnableVector queue;
    svmModel = NULL;
    if(_celeste)
    {
        TRACE_INFO("\nLoading Celeste model file...\n");
        if(!LoadSVMModel())
        {
            setCeleste(false);
        };
    };

    //print some more information about the images
    if (_verbose > 0)
    {
        printFilenames();
    }

    // 2. run analysis of images or keypoints
#if _WIN32
    //multi threading of image loading results sometime in a race condition
    //try to prevent this by initialisation of codecManager before
    //running multi threading part
    std::string s=vigra::impexListExtensions();
#endif
    if (_keyPointsIdx.size() != 0)
    {
        if (_verbose > 0)
        {
            TRACE_INFO(std::endl << "--- Analyze Images ---" << std::endl);
        }
        for (unsigned int i = 0; i < _keyPointsIdx.size(); ++i)
        {
            queue.push_back(new WriteKeyPointsRunnable(_filesData[_keyPointsIdx[i]], *this));
        };
    }
    else
    {
        TRACE_INFO(std::endl << "--- Analyze Images ---" << std::endl);
        if (getMatchingStrategy() == MULTIROW)
        {
            // when using multirow, don't analyse stacks with linked positions
            buildMultiRowImageSets();
            HuginBase::UIntSet imagesToAnalyse;
            imagesToAnalyse.insert(_image_layer.begin(), _image_layer.end());
            for (size_t i = 0; i < _image_stacks.size(); i++)
            {
                imagesToAnalyse.insert(_image_stacks[i].begin(), _image_stacks[i].end());
            }
            for (HuginBase::UIntSet::const_iterator it = imagesToAnalyse.begin(); it != imagesToAnalyse.end(); ++it)
            {
                if (_filesData[*it]._hasakeyfile)
                {
                    queue.push_back(new LoadKeypointsDataRunnable(_filesData[*it], *this));
                }
                else
                {
                    queue.push_back(new ImgDataRunnable(_filesData[*it], *this));
                };
            };
        }
        else
        {
            for (ImgDataIt_t aB = _filesData.begin(); aB != _filesData.end(); ++aB)
            {
                if (aB->second._hasakeyfile)
                {
                    queue.push_back(new LoadKeypointsDataRunnable(aB->second, *this));
                }
                else
                {
                    queue.push_back(new ImgDataRunnable(aB->second, *this));
                }
            }
        };
    }
    RunQueue(queue);

    if(svmModel!=NULL)
    {
        celeste::destroySVMmodel(svmModel);
    };

    // check if the load of images succeed.
    if (!checkLoadSuccess())
    {
        TRACE_INFO("One or more images failed to load. Exiting.");
        return;
    }

    if(_cache)
    {
        TRACE_INFO(std::endl << "--- Cache keyfiles to disc ---" << std::endl);
        for (ImgDataIt_t aB = _filesData.begin(); aB != _filesData.end(); ++aB)
        {
            if (!aB->second._hasakeyfile)
            {
                TRACE_INFO("i" << aB->second._number << " : Caching keypoints..." << std::endl);
                writeKeyfile(aB->second);
            };
        };
    };

    // Detect matches if writeKeyPoints wasn't set
    if(_keyPointsIdx.size() == 0)
    {
        switch (getMatchingStrategy())
        {
            case ALLPAIRS:
            case LINEAR:
                {
                    std::vector<HuginBase::UIntSet> imgPairs(_panoramaInfo->getNrOfImages());
                    if(!match(imgPairs))
                    {
                        return;
                    };
                };
                break;
            case MULTIROW:
                if(!matchMultiRow())
                {
                    return;
                };
                break;
            case PREALIGNED:
                {
                    //check, which image pairs are already connected by control points
                    std::vector<HuginBase::UIntSet> connectedImages(_panoramaInfo->getNrOfImages());
                    HuginBase::CPVector cps=_panoramaInfo->getCtrlPoints();
                    for(HuginBase::CPVector::const_iterator it=cps.begin();it!=cps.end(); ++it)
                    {
                        if((*it).mode==HuginBase::ControlPoint::X_Y)
                        {
                            connectedImages[(*it).image1Nr].insert((*it).image2Nr);
                            connectedImages[(*it).image2Nr].insert((*it).image1Nr);
                        };
                    };
                    //build dummy map
                    std::vector<size_t> imgMap(_panoramaInfo->getNrOfImages());
                    for(size_t i=0; i<_panoramaInfo->getNrOfImages(); i++)
                    {
                        imgMap[i]=i;
                    };
                    //and the final matching step
                    if(!matchPrealigned(_panoramaInfo, connectedImages, imgMap))
                    {
                        return;
                    };
                }
                break;
        };
    }

    // 5. write output
    if (_keyPointsIdx.size() != 0)
    {
        //Write all keyfiles
        TRACE_INFO(std::endl<< "--- Write Keyfiles output ---" << std::endl << std::endl);
        for (unsigned int i = 0; i < _keyPointsIdx.size(); ++i)
        {
            writeKeyfile(_filesData[_keyPointsIdx[i]]);
        };
        if(_outputGiven)
        {
            std::cout << std::endl << "Warning: You have given the --output switch." << std::endl
                 << "This switch is not compatible with the --writekeyfile or --kall switch." << std::endl
                 << "If you want to generate the keyfiles and" << std::endl
                 << "do the matching in the same run use the --cache switch instead." << std::endl << std::endl;
        };
    }
    else
    {
        /// Write output project
        TRACE_INFO(std::endl<< "--- Write Project output ---" << std::endl);
        writeOutput();
        TRACE_INFO("Written output to " << _outputFile << std::endl << std::endl);
    };
}

bool PanoDetector::match(std::vector<HuginBase::UIntSet> &checkedPairs)
{
    // 3. prepare matches
    RunnableVector queue;
    MatchData_t matchesData;
    unsigned int aLen = _filesData.size();
    if (getMatchingStrategy()==LINEAR)
    {
        aLen = _linearMatchLen;
    }

    if (aLen >= _filesData.size())
    {
        aLen = _filesData.size() - 1;
    }

    for (unsigned int i1 = 0; i1 < _filesData.size(); ++i1)
    {
        unsigned int aEnd = i1 + 1 + aLen;
        if (_filesData.size() < aEnd)
        {
            aEnd = _filesData.size();
        }

        for (unsigned int i2 = (i1+1); i2 < aEnd; ++i2)
        {
            if(set_contains(checkedPairs[i1], i2))
            {
                continue;
            };
            // create a new entry in the matches map
            matchesData.push_back(MatchData());

            MatchData& aM = matchesData.back();
            aM._i1 = &(_filesData[i1]);
            aM._i2 = &(_filesData[i2]);

            checkedPairs[i1].insert(i2);
            checkedPairs[i2].insert(i1);
        }
    }
    // 4. find matches
    TRACE_INFO(std::endl<< "--- Find pair-wise matches ---" << std::endl);
    for (size_t i = 0; i < matchesData.size(); ++i)
    {
        queue.push_back(new MatchDataRunnable(matchesData[i], *this));
    };
    RunQueue(queue);

    // Add detected matches to _panoramaInfo
    for (size_t i = 0; i < matchesData.size(); ++i)
    {
        const MatchData& aM = matchesData[i];
        for (size_t j = 0; j < aM._matches.size(); ++j)
        {
            const lfeat::PointMatchPtr& aPM = aM._matches[j];
            _panoramaInfo->addCtrlPoint(HuginBase::ControlPoint(aM._i1->_number, aPM->_img1_x, aPM->_img1_y,
                aM._i2->_number, aPM->_img2_x, aPM->_img2_y));
        };
    };
    return true;
};

bool PanoDetector::loadProject()
{
    std::ifstream ptoFile(_inputFile.c_str());
    if (ptoFile.bad())
    {
        std::cerr << "ERROR: could not open file: '" << _inputFile << "'!" << std::endl;
        return false;
    }
    _prefix=hugin_utils::getPathPrefix(_inputFile);
    if(_prefix.empty())
    {
        // Get the current working directory:
        char* buffer;
#ifdef _WIN32
#define getcwd _getcwd
#endif
        if((buffer=getcwd(NULL,0))!=NULL)
        {
            _prefix.append(buffer);
            free(buffer);
            _prefix=includeTrailingPathSep(_prefix);
        }
    };
    _panoramaInfo->setFilePrefix(_prefix);
    AppBase::DocumentData::ReadWriteError err = _panoramaInfo->readData(ptoFile);
    if (err != AppBase::DocumentData::SUCCESSFUL)
    {
        std::cerr << "ERROR: couldn't parse panos tool script: '" << _inputFile << "'!" << std::endl;
        return false;
    }

    // Create a copy of panoramaInfo that will be used to define
    // image options
    _panoramaInfoCopy=_panoramaInfo->duplicate();

    // Add images found in the project file to _filesData
    const unsigned int nImg = _panoramaInfo->getNrOfImages();
    unsigned int imgWithKeyfile=0;
    for (unsigned int imgNr = 0; imgNr < nImg; ++imgNr)
    {
        // insert the image in the map
        _filesData.insert(std::make_pair(imgNr, ImgData()));

        // get the data
        ImgData& aImgData = _filesData[imgNr];

        // get a copy of image info
        HuginBase::SrcPanoImage img = _panoramaInfoCopy.getSrcImage(imgNr);

        // set the name
        aImgData._name = img.getFilename();

        // modify image position in the copy
        img.setYaw(0);
        img.setRoll(0);
        img.setPitch(0);
        img.setX(0);
        img.setY(0);
        img.setZ(0);
        img.setActive(true);
        _panoramaInfoCopy.setImage(imgNr,img);

        // Number pointing to image info in _panoramaInfo
        aImgData._number = imgNr;

        bool needsremap=(img.getHFOV()>=65 && img.getProjection() != HuginBase::SrcPanoImage::FISHEYE_STEREOGRAPHIC);
        // set image detection size
        if(needsremap)
        {
            _filesData[imgNr]._detectWidth = std::max(img.getSize().width(),img.getSize().height());
            _filesData[imgNr]._detectHeight = std::max(img.getSize().width(),img.getSize().height());
            _filesData[imgNr].SetSizeMode(ImgData::REMAPPED);
        }
        else
        {
            _filesData[imgNr]._detectWidth = img.getSize().width();
            _filesData[imgNr]._detectHeight = img.getSize().height();
        };

        // don't downscale if downscale image is smaller than 1000 pixel
        if (_downscale && std::min(img.getSize().width(), img.getSize().height()) > 2000)
        {
            _filesData[imgNr]._detectWidth >>= 1;
            _filesData[imgNr]._detectHeight >>= 1;
            if (!_filesData[imgNr].NeedsRemapping())
            {
                _filesData[imgNr].SetSizeMode(ImgData::DOWNSCALED);
            };
        }

        // set image remapping options
        if (aImgData.GetSizeMode() == ImgData::REMAPPED)
        {
            aImgData._projOpts.setProjection(HuginBase::PanoramaOptions::STEREOGRAPHIC);
            aImgData._projOpts.setHFOV(250);
            aImgData._projOpts.setVFOV(250);
            aImgData._projOpts.setWidth(250);
            aImgData._projOpts.setHeight(250);

            // determine size of output image.
            // The old code did not work with images with images with a FOV
            // approaching 180 degrees
            vigra::Rect2D roi=estimateOutputROI(_panoramaInfoCopy,aImgData._projOpts,imgNr);
            double scalefactor = std::max((double)_filesData[imgNr]._detectWidth / roi.width(),
                                     (double)_filesData[imgNr]._detectHeight / roi.height() );

            // resize output canvas
            vigra::Size2D canvasSize((int)aImgData._projOpts.getWidth() * scalefactor,
                                     (int)aImgData._projOpts.getHeight() * scalefactor);
            aImgData._projOpts.setWidth(canvasSize.width(), false);
            aImgData._projOpts.setHeight(canvasSize.height());

            // set roi to cover the remapped input image
            roi *= scalefactor;
            _filesData[imgNr]._detectWidth = roi.width();
            _filesData[imgNr]._detectHeight = roi.height();
            aImgData._projOpts.setROI(roi);
        }

        // Specify if the image has an associated keypoint file

        aImgData._keyfilename = getKeyfilenameFor(_keypath,aImgData._name);
        aImgData._hasakeyfile = hugin_utils::FileExists(aImgData._keyfilename);
        if(aImgData._hasakeyfile)
        {
            imgWithKeyfile++;
        };
    }
    //update masks, convert positive masks into negative masks
    //because positive masks works only if the images are on the final positions
    _panoramaInfoCopy.updateMasks(true);

    //if all images has keyfile, we don't need to load celeste model file
    if(nImg==imgWithKeyfile)
    {
        _celeste=false;
    };
    return true;
}

bool PanoDetector::checkLoadSuccess()
{
    if(!_keyPointsIdx.empty())
    {
        for (unsigned int i = 0; i < _keyPointsIdx.size(); ++i)
        {
            if(_filesData[_keyPointsIdx[i]]._loadFail)
            {
                return false;
            };
        };
    }
    else
    {
        for (unsigned int aFileN = 0; aFileN < _filesData.size(); ++aFileN)
        {
            if(_filesData[aFileN]._loadFail)
            {
                return false;
            };
        };
    };
    return true;
}

void PanoDetector::CleanupKeyfiles()
{
    for (ImgDataIt_t aB = _filesData.begin(); aB != _filesData.end(); ++aB)
    {
        if (aB->second._hasakeyfile)
        {
            remove(aB->second._keyfilename.c_str());
        };
    };
};

void PanoDetector::buildMultiRowImageSets()
{
    std::vector<HuginBase::UIntVector> stacks = HuginBase::getSortedStacks(_panoramaInfo);
    //get image with median exposure for search with cp generator
    for (size_t i = 0; i < stacks.size(); ++i)
    {
        size_t index = 0;
        if (_panoramaInfo->getImage(*(stacks[i].begin())).getExposureValue() != _panoramaInfo->getImage(*(stacks[i].rbegin())).getExposureValue())
        {
            index = stacks[i].size() / 2;
        };
        _image_layer.insert(stacks[i][index]);
        if (stacks[i].size()>1)
        {
            //build list for stacks, consider only unlinked stacks
            if (!_panoramaInfo->getImage(*(stacks[i].begin())).YawisLinked())
            {
                _image_stacks.push_back(stacks[i]);
            };
        };
    };
};

bool PanoDetector::matchMultiRow()
{
    RunnableVector queue;
    MatchData_t matchesData;
    //step 1
    std::vector<HuginBase::UIntSet> checkedImagePairs(_panoramaInfo->getNrOfImages());
    for (size_t i = 0; i < _image_stacks.size();i++)
    {
        //build match list for stacks
        for(unsigned int j=0; j<_image_stacks[i].size()-1; j++)
        {
            const size_t img1 = _image_stacks[i][j];
            const size_t img2 = _image_stacks[i][j + 1];
            matchesData.push_back(MatchData());
            MatchData& aM=matchesData.back();
            if (img1 < img2)
            {
                aM._i1 = &(_filesData[img1]);
                aM._i2 = &(_filesData[img2]);
            }
            else
            {
                aM._i1 = &(_filesData[img2]);
                aM._i2 = &(_filesData[img1]);
            };
            checkedImagePairs[img1].insert(img2);
            checkedImagePairs[img2].insert(img1);
        };
    };
    //build match data list for image pairs
    if(_image_layer.size()>1)
    {
        for (HuginBase::UIntSet::const_iterator it = _image_layer.begin(); it != _image_layer.end(); ++it)
        {
            const size_t img1 = *it;
            HuginBase::UIntSet::const_iterator it2 = it;
            ++it2;
            if (it2 != _image_layer.end())
            {
                const size_t img2 = *it2;
                matchesData.push_back(MatchData());
                MatchData& aM = matchesData.back();
                if (img1 < img2)
                {
                    aM._i1 = &(_filesData[img1]);
                    aM._i2 = &(_filesData[img2]);
                }
                else
                {
                    aM._i1 = &(_filesData[img2]);
                    aM._i2 = &(_filesData[img1]);
                };
                checkedImagePairs[img1].insert(img2);
                checkedImagePairs[img2].insert(img1);
            };
        };
    };
    TRACE_INFO(std::endl<< "--- Find matches ---" << std::endl);
    for (size_t i = 0; i < matchesData.size(); ++i)
    {
        queue.push_back(new MatchDataRunnable(matchesData[i], *this));
    };
    RunQueue(queue);

    // Add detected matches to _panoramaInfo
    for (size_t i = 0; i < matchesData.size(); ++i)
    {
        const MatchData& aM = matchesData[i];
        for (size_t j = 0; j < aM._matches.size(); ++j)
        {
            const lfeat::PointMatchPtr& aPM = aM._matches[j];
            _panoramaInfo->addCtrlPoint(HuginBase::ControlPoint(aM._i1->_number, aPM->_img1_x, aPM->_img1_y,
                aM._i2->_number, aPM->_img2_x, aPM->_img2_y));
        };
    };

    // step 2: connect all image groups
    queue.clear();
    matchesData.clear();
    HuginBase::Panorama mediumPano = _panoramaInfo->getSubset(_image_layer);
    HuginGraph::ImageGraph graph(mediumPano);
    const HuginGraph::ImageGraph::Components comps = graph.GetComponents();
    const size_t n = comps.size();
    if(n>1)
    {
        std::vector<unsigned int> ImagesGroups;
        for(size_t i=0; i<n; i++)
        {
            HuginBase::UIntSet::iterator imgIt = _image_layer.begin();
            std::advance(imgIt, *(comps[i].begin()));
            ImagesGroups.push_back(*imgIt);
            if(comps[i].size()>1)
            {
                imgIt = _image_layer.begin();
                std::advance(imgIt, *(comps[i].rbegin()));
                ImagesGroups.push_back(*imgIt);
            }
        };
        for(unsigned int i=0; i<ImagesGroups.size()-1; i++)
        {
            for(unsigned int j=i+1; j<ImagesGroups.size(); j++)
            {
                size_t img1=ImagesGroups[i];
                size_t img2=ImagesGroups[j];
                //skip already checked image pairs
                if(set_contains(checkedImagePairs[img1],img2))
                {
                    continue;
                };
                matchesData.push_back(MatchData());
                MatchData& aM = matchesData.back();
                if (img1 < img2)
                {
                    aM._i1 = &(_filesData[img1]);
                    aM._i2 = &(_filesData[img2]);
                }
                else
                {
                    aM._i1 = &(_filesData[img2]);
                    aM._i2 = &(_filesData[img1]);
                };
                checkedImagePairs[img1].insert(img2);
                checkedImagePairs[img2].insert(img1);
            };
        };
        TRACE_INFO(std::endl<< "--- Find matches in images groups ---" << std::endl);
        for (size_t i = 0; i < matchesData.size(); ++i)
        {
            queue.push_back(new MatchDataRunnable(matchesData[i], *this));
        };
        RunQueue(queue);

        for (size_t i = 0; i < matchesData.size(); ++i)
        {
            const MatchData& aM = matchesData[i];
            for (size_t j = 0; j < aM._matches.size(); ++j)
            {
                const lfeat::PointMatchPtr& aPM = aM._matches[j];
                _panoramaInfo->addCtrlPoint(HuginBase::ControlPoint(aM._i1->_number, aPM->_img1_x, aPM->_img1_y,
                    aM._i2->_number, aPM->_img2_x, aPM->_img2_y));
            };
        };
    };
    // step 3: now connect all overlapping images
    queue.clear();
    matchesData.clear();
    HuginBase::Panorama optPano=_panoramaInfo->getSubset(_image_layer);
    HuginGraph::ImageGraph graph2(optPano);
    if(graph2.IsConnected())
    {
        if(_image_layer.size()>2)
        {
            //reset translation parameters
            HuginBase::VariableMapVector varMapVec = optPano.getVariables();
            for(size_t i=0; i<varMapVec.size(); i++)
            {
                map_get(varMapVec[i], "TrX").setValue(0);
                map_get(varMapVec[i], "TrY").setValue(0);
                map_get(varMapVec[i], "TrZ").setValue(0);
            };
            optPano.updateVariables(varMapVec);
            //next steps happens only when all images are connected;
            //now optimize panorama
            HuginBase::PanoramaOptions opts = optPano.getOptions();
            opts.setProjection(HuginBase::PanoramaOptions::EQUIRECTANGULAR);
            opts.optimizeReferenceImage=0;
            // calculate proper scaling, 1:1 resolution.
            // Otherwise optimizer distances are meaningless.
            opts.setWidth(30000, false);
            opts.setHeight(15000);

            optPano.setOptions(opts);
            int w = hugin_utils::roundi(HuginBase::CalculateOptimalScale::calcOptimalScale(optPano) * opts.getWidth());
            opts.setWidth(w);
            opts.setHeight(w/2);
            optPano.setOptions(opts);

            //generate optimize vector, optimize only yaw and pitch
            HuginBase::OptimizeVector optvars;
            for (unsigned i=0; i < optPano.getNrOfImages(); i++)
            {
                std::set<std::string> imgopt;
                if(i==opts.optimizeReferenceImage)
                {
                    //optimize only anchors pitch, not yaw
                    imgopt.insert("p");
                }
                else
                {
                    imgopt.insert("p");
                    imgopt.insert("y");
                };
                optvars.push_back(imgopt);
            }
            optPano.setOptimizeVector(optvars);

            // remove vertical and horizontal control points
            HuginBase::CPVector cps = optPano.getCtrlPoints();
            HuginBase::CPVector newCP;
            for (HuginBase::CPVector::const_iterator it = cps.begin(); it != cps.end(); ++it)
            {
                if (it->mode == HuginBase::ControlPoint::X_Y)
                {
                    newCP.push_back(*it);
                }
            }
            optPano.setCtrlPoints(newCP);

            if (getVerbose() < 2)
            {
                PT_setProgressFcn(ptProgress);
                PT_setInfoDlgFcn(ptinfoDlg);
            };
            //in a first step do a pairwise optimisation
            HuginBase::AutoOptimise::autoOptimise(optPano, false);
            //now the final optimisation
            HuginBase::PTools::optimize(optPano);
            if (getVerbose() < 2)
            {
                PT_setProgressFcn(NULL);
                PT_setInfoDlgFcn(NULL);
            };

            //now match overlapping images
            std::vector<size_t> imgMap(_image_layer.begin(), _image_layer.end());
            if(!matchPrealigned(&optPano, checkedImagePairs, imgMap, false))
            {
                return false;
            };
        };
    }
    else
    {
        if(!match(checkedImagePairs))
        {
            return false;
        };
    };
    return true;
};

bool PanoDetector::matchPrealigned(HuginBase::Panorama* pano, std::vector<HuginBase::UIntSet> &connectedImages, std::vector<size_t> imgMap, bool exactOverlap)
{
    RunnableVector queue;
    MatchData_t matchesData;
    HuginBase::Panorama tempPano = pano->duplicate();
    if(!exactOverlap)
    {
        // increase hfov by 25 % to handle narrow overlaps (or even no overlap) better
        HuginBase::VariableMapVector varMapVec = tempPano.getVariables();
        for(size_t i=0; i<tempPano.getNrOfImages(); i++)
        {
            HuginBase::Variable& hfovVar = map_get(varMapVec[i], "v");
            hfovVar.setValue(std::min(360.0, 1.25 * hfovVar.getValue()));
        };
        tempPano.updateVariables(varMapVec);
    };
    HuginBase::CalculateImageOverlap overlap(&tempPano);
    overlap.calculate(10);
    for(size_t i=0; i<tempPano.getNrOfImages()-1; i++)
    {
        for(size_t j=i+1; j<tempPano.getNrOfImages(); j++)
        {
            if(set_contains(connectedImages[imgMap[i]],imgMap[j]))
            {
                continue;
            };
            if(overlap.getOverlap(i,j)>0)
            {
                matchesData.push_back(MatchData());
                MatchData& aM = matchesData.back();
                aM._i1 = &(_filesData[imgMap[i]]);
                aM._i2 = &(_filesData[imgMap[j]]);
                connectedImages[imgMap[i]].insert(imgMap[j]);
                connectedImages[imgMap[j]].insert(imgMap[i]);
            };
        };
    };

    TRACE_INFO(std::endl<< "--- Find matches for overlapping images ---" << std::endl);
    for (size_t i = 0; i < matchesData.size(); ++i)
    {
        queue.push_back(new MatchDataRunnable(matchesData[i], *this));
    };
    RunQueue(queue);

    // Add detected matches to _panoramaInfo
    for (size_t i = 0; i < matchesData.size(); ++i)
    {
        const MatchData& aM = matchesData[i];
        for (size_t j = 0; j < aM._matches.size(); ++j)
        {
            const lfeat::PointMatchPtr& aPM = aM._matches[j];
            _panoramaInfo->addCtrlPoint(HuginBase::ControlPoint(aM._i1->_number, aPM->_img1_x, aPM->_img1_y,
                aM._i2->_number, aPM->_img2_x, aPM->_img2_y));
        };
    };

    return true;
};
