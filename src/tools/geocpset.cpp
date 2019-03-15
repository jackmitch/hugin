// -*- c-basic-offset: 4 -*-

/** @file geocpset.cpp
 *
 *  @brief program to set "geometric" control points
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

#include <fstream>
#include <sstream>
#include <getopt.h>
#include <panodata/Panorama.h>
#include <algorithms/basic/CalculateOverlap.h>
#include <algorithms/nona/ComputeImageROI.h>

// check if given point x,y is inside both images img1 and img2,
// using PTools::Tranform object to prevent creating of this transform for each point
bool CheckAndAddPoint(HuginBase::Panorama& pano, size_t img1, size_t img2, HuginBase::PTools::Transform& transform1, HuginBase::PTools::Transform& transform2, double x, double y)
{
    double x1, y1, x2, y2;
    if(!transform1.transformImgCoord(x1, y1, x, y))
    {
        return false;
    };
    if(!pano.getImage(img1).isInside(vigra::Point2D(x1,y1), true))
    {
        return false;
    };
    if(!transform2.transformImgCoord(x2, y2, x, y))
    {
        return false;
    };
    if(!pano.getImage(img2).isInside(vigra::Point2D(x2,y2), true))
    {
        return false;
    };
    pano.addCtrlPoint(HuginBase::ControlPoint(img1, x1, y1, img2, x2, y2));
    return true;
};

//helper class for sort
bool sortByDistance(hugin_utils::FDiff2D p1, hugin_utils::FDiff2D p2)
{
    return p1.squareLength()<p2.squareLength();
};

// add geometric control point to overlap of images img1 and img2
void AddGeometricControlPoint(HuginBase::Panorama& pano, size_t img1, size_t img2)
{
    HuginBase::PanoramaOptions opts = pano.getOptions();
    //reset ROI to prevent unwanted clipping in this algorithm
    opts.setROI(vigra::Rect2D(0, 0, opts.getWidth(), opts.getHeight()));
    vigra::Rect2D rect1=estimateOutputROI(pano, opts, img1);
    vigra::Rect2D rect2=estimateOutputROI(pano, opts, img2);
    //get union of both outputs
    rect1=rect1 & rect2;
    if(rect1.area()>0)
    {
        HuginBase::PTools::Transform transform1, transform2;
        transform1.createTransform(pano.getImage(img1), opts);
        transform2.createTransform(pano.getImage(img2), opts);

        hugin_utils::FDiff2D mid=(rect1.upperLeft()+rect1.lowerRight())/2.0;
        //create grid of points to check
        std::vector<hugin_utils::FDiff2D> points;
        for(int dx=-5; dx<=5; dx++)
        {
            for(int dy=-5; dy<=5; dy++)
            {
                points.push_back(hugin_utils::FDiff2D(dx*rect1.width()/10.0, dy*rect1.height()/10.0));
            };
        };
        //sort by distance
        sort(points.begin(), points.end(),sortByDistance);
        //now check all points in the grid
        for(size_t i=0; i<points.size(); i++)
        {
            if(CheckAndAddPoint(pano, img1, img2, transform1, transform2, mid.x+points[i].x, mid.y+points[i].y))
            {
                return;
            };
        };
    };
};

// only add control points for images without control points
void SetGeometricControlPointsUnconnected(HuginBase::Panorama& pano, const int minOverlap)
{
    //first test: have image control points?
    HuginBase::CPVector cp = pano.getCtrlPoints();
    HuginBase::UIntSet imgsWithCp;
    HuginBase::UIntSet imgsWithoutCp;
    for(size_t i=0; i<pano.getNrOfImages(); i++)
    {
        bool hasControlPoints=false;
        for (HuginBase::CPVector::const_iterator it = cp.begin(); it != cp.end(); ++it)
        {
            if ((it->image1Nr == i || it->image2Nr == i) && (it->mode == HuginBase::ControlPoint::X_Y))
            {
                hasControlPoints=true;
                break;
            };
        };
        if(hasControlPoints)
        {
            imgsWithCp.insert(i);
        }
        else
        {
            imgsWithoutCp.insert(i);
        };
    };
    // now test if images without control points have a linked position with an image with control point
    HuginBase::UIntSet imgsToTest;
    for (HuginBase::UIntSet::const_iterator img = imgsWithoutCp.begin(); img != imgsWithoutCp.end(); ++img)
    {
        const HuginBase::SrcPanoImage& img1 = pano.getImage(*img);
        bool connected=false;
        if(img1.YawisLinked())
        {
            for (HuginBase::UIntSet::const_iterator img2 = imgsWithCp.begin(); img2 != imgsWithCp.end(); ++img2)
            {
                if(img1.YawisLinkedWith(pano.getImage(*img2)))
                {
                    imgsWithCp.insert(*img);
                    connected=true;
                    break;
                };
            };
        };
        if(!connected)
        {
            imgsToTest.insert(*img);
        };
    };

    // have we found unconnected images?
    if(imgsToTest.size()==0)
    {
        std::cout << "No unconnected images found." << std::endl
             << "No control point added." << std::endl;
        return;
    };
    std::cout << std::endl << "Found " << imgsToTest.size() << " unconnected images." << std::endl;

    // now find overlapping images
    HuginBase::CalculateImageOverlap overlap(&pano);
    overlap.limitToImages(imgsToTest);
    overlap.calculate(10);
    std::vector<HuginBase::UIntSet> checkedImgPairs(pano.getNrOfImages());
    for (HuginBase::UIntSet::const_iterator img = imgsToTest.begin(); img != imgsToTest.end(); ++img)
    {
        HuginBase::UIntSet overlappingImgs;
        // search overlapping images, take linked positions into account
        for(size_t i=0; i<pano.getNrOfImages(); i++)
        {
            if(i==*img)
            {
                continue;
            };
            if(overlap.getOverlap(*img, i)>minOverlap/100.0f)
            {
                //ignore overlap for linked images
                bool ignoreImage=false;
                const HuginBase::SrcPanoImage& img2 = pano.getImage(i);
                if(img2.YawisLinked())
                {
                    for (HuginBase::UIntSet::const_iterator it = overlappingImgs.begin(); it != overlappingImgs.end(); ++it)
                    {
                        if(img2.YawisLinkedWith(pano.getImage(*it)))
                        {
                            ignoreImage=true;
                            break;
                        }
                    };
                };
                if(set_contains(checkedImgPairs[*img], i) || set_contains(checkedImgPairs[i], *img))
                {
                    ignoreImage=true;
                };
                if(!ignoreImage)
                {
                    overlappingImgs.insert(i);
                    checkedImgPairs[*img].insert(i);
                    checkedImgPairs[i].insert(*img);
                };
            };
        };
        // now add control points
        for (HuginBase::UIntSet::const_iterator overlapImg = overlappingImgs.begin(); overlapImg != overlappingImgs.end(); ++overlapImg)
        {
            AddGeometricControlPoint(pano, *img, *overlapImg);
        };
    };
    std::cout << "Added " << pano.getCtrlPoints().size() - cp.size() << " control points." << std::endl;
};

// only add control points for images without control points
void SetGeometricControlPointsOverlap(HuginBase::Panorama& pano, const int minOverlap)
{
    HuginBase::CPVector cp = pano.getCtrlPoints();
    // find overlapping images
    HuginBase::CalculateImageOverlap overlap(&pano);
    overlap.calculate(10);
    for(size_t i=0; i<pano.getNrOfImages()-1; i++)
    {
        HuginBase::UIntSet overlappingImgs;
        const HuginBase::SrcPanoImage& img1 = pano.getImage(i);
        // search overlapping images, take linked positions into account
        for(size_t j=i+1; j<pano.getNrOfImages(); j++)
        {
            //skip linked images
            if(img1.YawisLinked())
            {
                if(img1.YawisLinkedWith(pano.getImage(j)))
                {
                    continue;
                };
            };
            if(overlap.getOverlap(i, j)>=minOverlap/100.0f)
            {
                // we have an overlap, now check if there are control points
                bool hasControlPoints=false;
                for (HuginBase::CPVector::const_iterator it = cp.begin(); it != cp.end(); ++it)
                {
                    if(((it->image1Nr==i && it->image2Nr==j) ||
                            (it->image1Nr==j && it->image2Nr==i) ) &&
                            (it->mode == HuginBase::ControlPoint::X_Y))
                    {
                        hasControlPoints=true;
                        break;
                    };
                };
                if(!hasControlPoints)
                {
                    //ignore overlap for linked images
                    bool ignoreImage=false;
                    const HuginBase::SrcPanoImage& img2 = pano.getImage(j);
                    if(img2.YawisLinked())
                    {
                        for (HuginBase::UIntSet::const_iterator it = overlappingImgs.begin(); it != overlappingImgs.end(); ++it)
                        {
                            if(img2.YawisLinkedWith(pano.getImage(*it)))
                            {
                                ignoreImage=true;
                                break;
                            };
                        };
                    };
                    if(!ignoreImage)
                    {
                        overlappingImgs.insert(j);
                    };
                };
            };
        };
        // now add control points
        for (HuginBase::UIntSet::const_iterator overlapImg = overlappingImgs.begin(); overlapImg != overlappingImgs.end(); ++overlapImg)
        {
            AddGeometricControlPoint(pano, i, *overlapImg);
        };
    };
    std::cout << std::endl << "Added " << pano.getCtrlPoints().size() - cp.size() << " control points." << std::endl;
};

static void usage(const char* name)
{
    std::cout << name << ": set geometric control points" << std::endl
         << name << " version " << hugin_utils::GetHuginVersion() << std::endl
         << std::endl
         << "Usage:  " << name << " [options] input.pto" << std::endl
         << std::endl
         << "  Options:" << std::endl
         << "     -o, --output=file.pto  Output Hugin PTO file. Default: <filename>_geo.pto" << std::endl
         << "     -e, --each-overlap     By default, geocpset will only work on the overlap" << std::endl
         << "                            of unconnected images. With this switch it will" << std::endl
         << "                            work on all overlaps without control points." << std::endl
         << "     --minimum-overlap=NUM  Take only these images into account where the" << std::endl
         << "                            overlap is bigger than NUM percent (default 10)." << std::endl
         << "     -h, --help             Shows this help" << std::endl
         << std::endl;
}

int main(int argc, char* argv[])
{
    // parse arguments
    const char* optstring = "o:eh";
    enum
    {
        MINOVERLAP=1000
    };

    static struct option longOptions[] =
    {
        {"output", required_argument, NULL, 'o' },
        {"each-overlap", no_argument, NULL, 'e' },
        {"min-overlap", required_argument, NULL, MINOVERLAP},
        {"help", no_argument, NULL, 'h' },
        0
    };

    int c;
    bool eachOverlap=false;
    int minOverlap=10;
    std::string output;
    while ((c = getopt_long (argc, argv, optstring, longOptions,nullptr)) != -1)
    {
        switch (c)
        {
            case 'o':
                output = optarg;
                break;
            case 'e':
                eachOverlap = true;
                break;
            case MINOVERLAP:
                minOverlap=atoi(optarg);
                if(minOverlap<1 || minOverlap>99)
                {
                    std::cerr << hugin_utils::stripPath(argv[0]) << ": Invalid minimum overlap: " << optarg << std::endl
                         << "Minimum overlap have to be between 1 and 99." << std::endl;
                    return 1;
                };
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
    };

    std::string input=argv[optind];
    // read panorama
    HuginBase::Panorama pano;
    std::ifstream prjfile(input.c_str());
    if (!prjfile.good())
    {
        std::cerr << "could not open script : " << input << std::endl;
        return 1;
    }
    pano.setFilePrefix(hugin_utils::getPathPrefix(input));
    AppBase::DocumentData::ReadWriteError err = pano.readData(prjfile);
    if (err != AppBase::DocumentData::SUCCESSFUL)
    {
        std::cerr << "error while parsing panos tool script: " << input << std::endl
            << "DocumentData::ReadWriteError code: " << err << std::endl;
        return 1;
    }

    if(pano.getNrOfImages()==0)
    {
        std::cerr << "error: project file does not contains any image" << std::endl
            << "aborting processing" << std::endl;
        return 1;
    };
    if(pano.getNrOfImages()==1)
    {
        std::cerr << "error: project file contains only one image" << std::endl
            << "aborting processing" << std::endl;
        return 1;
    };

    std::cout << "Adding geometric control points..." << std::endl;
    if(eachOverlap)
    {
        SetGeometricControlPointsOverlap(pano, minOverlap);
    }
    else
    {
        SetGeometricControlPointsUnconnected(pano, minOverlap);
    };

    //write output
    HuginBase::UIntSet imgs;
    fill_set(imgs, 0, pano.getNrOfImages()-1);
    // Set output .pto filename if not given
    if (output=="")
    {
        output=input.substr(0,input.length()-4).append("_geo.pto");
    }
    std::ofstream of(output.c_str());
    pano.printPanoramaScript(of, pano.getOptimizeVector(), pano.getOptions(), imgs, false, hugin_utils::getPathPrefix(input));

    std::cout << std::endl << "Written output to " << output << std::endl;
    return 0;
}
