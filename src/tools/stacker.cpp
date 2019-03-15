// -*- c-basic-offset: 4 -*-

/** @file stacker.cpp
*
*  @brief program to merge images in a stack
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

#include <stdio.h>
#include <iostream>
#include <getopt.h>
#include <hugin_utils/utils.h>
#include <hugin_utils/stl_utils.h>
#include <vigra/imageinfo.hxx>
#include <vigra/codec.hxx>
#include <vigra/stdimage.hxx>
#include <vigra_ext/impexalpha.hxx>
#include <vigra/tiff.hxx>
#include <vigra_ext/tiffUtils.h>
#include <vigra_ext/openmp_vigra.h>

/** set compression for jpeg or tiff */
void SetCompression(vigra::ImageExportInfo& output, const std::string& compression)
{
    const std::string ext(hugin_utils::toupper(hugin_utils::getExtension(output.getFileName())));
    if (!compression.empty())
    {
        if (ext == "JPEG" || ext == "JPG")
        {
            output.setCompression(std::string("JPEG QUALITY=" + compression).c_str());
        }
        else
        {
            output.setCompression(compression.c_str());
        };
    };
};

/** save image, when possible with alpha channel, take care of formats which does not support alpha channels */
template <class ImageType, class MaskType>
bool SaveImage(ImageType& image, MaskType& mask, vigra::ImageExportInfo& exportImageInfo, std::string filetype, std::string pixelType)
{
    typedef typename vigra::NumericTraits<typename ImageType::value_type>::isScalar scalar;
    const int numberChannels = scalar().asBool ? 1 : 3;
    exportImageInfo.setPixelType(pixelType.c_str());
    if (vigra::isBandNumberSupported(filetype, numberChannels +1))
    {
        try
        {
            vigra::exportImageAlpha(vigra::srcImageRange(image), vigra::srcImage(mask), exportImageInfo);
        }
        catch (std::exception& e)
        {
            std::cerr << "ERROR: Could not save " << exportImageInfo.getFileName() << std::endl
                << "Cause: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    else
    {
        if (vigra::isBandNumberSupported(filetype, numberChannels))
        {
            std::cout << "Warning: Filetype " << filetype << " does not support alpha channels." << std::endl
                << "Saving image without alpha channel." << std::endl;
            try
            {
                vigra::exportImage(vigra::srcImageRange(image), exportImageInfo);
            }
            catch (std::exception& e)
            {
                std::cerr << "ERROR: Could not save " << exportImageInfo.getFileName() << std::endl
                    << "Cause: " << e.what() << std::endl;
                return false;
            };
            return true;
        }
        else
        {
            std::cerr << "ERROR: Output filetype " << filetype << " does not support " << numberChannels << " channels." << std::endl
                << "Can't save image." << std::endl;
        };
    };
    return false;
};

/** save final image, take care of some supported pixel types and convert when necessary to smaller pixel type */
template <class ImageType, class MaskType>
bool SaveFinalImage(ImageType& image, MaskType& mask, const std::string& inputPixelType, vigra::ImageExportInfo& output)
{
    VIGRA_UNIQUE_PTR<vigra::Encoder> encoder(vigra::encoder(output));
    if (vigra::isPixelTypeSupported(encoder->getFileType(), inputPixelType))
    {
        return SaveImage(image, mask, output, encoder->getFileType(), inputPixelType);
    }
    else
    {
        if (vigra::isPixelTypeSupported(encoder->getFileType(), "UINT8"))
        {
            // transform to UINT8
            output.setForcedRangeMapping(0, vigra::NumericTraits<typename vigra::NumericTraits<typename ImageType::PixelType>::ValueType>::max(), 0, 255);
            return SaveImage(image, mask, output, encoder->getFileType(), "UINT8");
        }
        else
        {
            std::cerr << "ERROR: Output file type " << encoder->getFileType() << " does not support" << std::endl
                << "requested pixeltype " << inputPixelType << "." << std::endl
                << "Save output in other file format." << std::endl;
        };
    };
    return false;
};

/** prints help screen */
static void usage(const char* name)
{
    std::cout << name << ": stack images" << std::endl
        << name << " version " << hugin_utils::GetHuginVersion() << std::endl
        << std::endl
        << "Usage:  " << name << " [options] images" << std::endl
        << std::endl
        << "     --output=FILE            Set the filename for the output file." << std::endl
        << "     --compression=value      Compression of the output files" << std::endl
        << "                                For jpeg output: 0-100" << std::endl
        << "                                For tiff output: PACKBITS, DEFLATE, LZW" << std::endl
        << "     --mode=STRING            Select the mode for stacking" << std::endl
        << "                              Possible names are described below." << std::endl
        << std::endl
        << "        min|minimum|darkest   Select the darkest pixel" << std::endl
        << "        max|maximum|brightest Select the brightest pixel" << std::endl
        << "        avg|average|mean      Calculate the mean for each position" << std::endl
        << "        median                Calculate the median for each position" << std::endl
        << "        winsor                Calculate the Winsor trimmed mean" << std::endl
        << "                              for each position. The parameter can be" << std::endl
        << "                              set with --winsor-trim=NUMBER (default: 0.2)" << std::endl
        << "        sigma                 Calculate the sigma clipped mean for" << std::endl
        << "                              each position. Fine-tune with" << std::endl
        << "                              --max-sigma=NUMBER (default: 2) and" << std::endl
        << "                              --max-iterations=NUMBER (default: 5)" << std::endl
        << std::endl
        << "     --mask-input             Mask input images" << std::endl
        << "                              Only pixel which differ more than" << std::endl
        << "                              mask-sigma * standard deviation" << std::endl
        << "                              are visible in output" << std::endl
        << "                              available for modes median|winsor|clip" << std::endl
        << "     --mask-suffix=STRING     Suffix for the masked input images" << std::endl
        << "                              (default: _mask)" << std::endl
        << "     --mask-sigma=NUMBER      Sigma parameter for input images masking" << std::endl
        << "                              (default: 2)" << std::endl
        << "     --multi-layer-output     Output layered TIFF instead of single images" << std::endl
        << "                              (has only effect with --mask-input)" << std::endl
        << "     --bigtiff                Write output in BigTIFF format" << std::endl
        << "                              (only with TIFF output)" << std::endl
        << "     -h, --help               Shows this help" << std::endl
        << std::endl;
};

static struct GeneralParameters
{
public:
    // for output file
    std::string outputFilename;
    std::string compression;
    // stacking mode
    std::string stackMode;
    // for sigma clipping
    double sigma = 2;
    int maxIterations = 5;
    // for Winsor trimmed mean
    double winsorTrim = 0.2;
    // for masking input images
    double maskSigma = 2;
    // should masked input images saved
    bool maskInput = false;
    std::string maskSuffix = "_mask";
    bool multiLayer = false;
    bool useBigTIFF = false;
} Parameters;

class InputImage
{
public:
    explicit InputImage(const std::string filename) : m_info(filename.c_str())
    {
        m_filename = filename;
        m_canvassize=m_info.getCanvasSize();
        if (m_canvassize.area() == 0)
        {
            // not all images contains the canvas size/full image size
            // in this case take also the position into account to get full image size
            m_canvassize =vigra::Size2D(m_info.width(), m_info.height());
        };
        m_offsetX = m_info.getPosition().x;
        m_offsetY = m_info.getPosition().y;
        m_decoder=vigra::decoder(m_info);
        m_hasAlpha = m_info.numExtraBands() == 1;
        m_width=m_decoder->getWidth();
        m_height=m_decoder->getHeight();
        m_bands=m_decoder->getNumBands();
        m_offset=m_decoder->getOffset();
        m_x = 0;
        m_y = 0;
    };
    ~InputImage()
    {
        m_decoder->abort();
    };
    const std::string getPixelType() const { return m_info.getPixelType(); };
    const bool isColor() const { return m_info.isColor(); };
    const bool isGrayscale() const { return m_info.isGrayscale(); };
    const int numBands() const { return m_bands; };
    const int numExtraBands() const { return m_info.numExtraBands(); }
    const int numPixelSamples() const { return m_bands - m_info.numExtraBands(); };
    const float getXResolution() const { return m_info.getXResolution(); };
    const float getYResolution() const { return m_info.getYResolution(); };
    const vigra::ImageImportInfo::ICCProfile getICCProfile() const { return m_info.getICCProfile(); };
    const std::string getFilename() const { return m_filename; };
    const vigra::Rect2D getROI() const { return vigra::Rect2D(vigra::Point2D(m_offsetX,m_offsetY), vigra::Size2D(m_width, m_height)); };
    const vigra::Size2D getCanvasSize() const { return m_canvassize; };
    const std::string getMaskFilename() const { return hugin_utils::stripExtension(m_filename) + Parameters.maskSuffix + ".tif"; };
    const vigra::ImageImportInfo& getImageImportInfo() const { return m_info; };
    void readLine(const int y)
    {
        if (y < m_offsetY + static_cast<int>(m_height))
        {
            if (y < m_offsetY)
            {
                m_noData = true;
            }
            else
            {
                m_noData = false;
                m_decoder->nextScanline();
                ++m_y;
                while (y+1 < static_cast<int>(m_y) + m_offsetY)
                {
                    m_decoder->nextScanline();
                    ++m_y;
                }
            }
        }
        else
        {
            m_noData = true;
        };
    };
    template<class ValueType>
    void getValue(const int x, vigra::RGBValue<ValueType>& value, ValueType& mask)
    {
        if (m_noData)
        {
            mask = vigra::NumericTraits<ValueType>::zero();
        }
        else
        {
            if (x < m_offsetX)
            {
                mask = vigra::NumericTraits<ValueType>::zero();
            }
            else
            {
                if (x < m_offsetX + static_cast<int>(m_width))
                {
                    const ValueType* band0= static_cast<const ValueType*>(m_decoder->currentScanlineOfBand(0));
                    const ValueType* band1 = static_cast<const ValueType*>(m_decoder->currentScanlineOfBand(1));
                    const ValueType* band2 = static_cast<const ValueType*>(m_decoder->currentScanlineOfBand(2));
                    band0 += m_offset*(x - m_offsetX);
                    band1 += m_offset*(x - m_offsetX);
                    band2 += m_offset*(x - m_offsetX);
                    if (m_bands == 4)
                    {
                        const ValueType* band3 = static_cast<const ValueType*>(m_decoder->currentScanlineOfBand(3));
                        band3 += m_offset*(x - m_offsetX);
                        mask = *band3;
                    }
                    else
                    {
                        mask = vigra::NumericTraits<ValueType>::max();
                    };
                    value = vigra::RGBValue<ValueType>(*band0, *band1, *band2);
                }
                else
                {
                    mask = vigra::NumericTraits<ValueType>::zero();
                };
            };
        };
    };
    template<class ValueType>
    void getValue(const int x, ValueType& value, ValueType& mask)
    {
        if (m_noData)
        {
            mask = vigra::NumericTraits<ValueType>::zero();
        }
        else
        {
            if (x < m_offsetX)
            {
                mask = vigra::NumericTraits<ValueType>::zero();
            }
            else
            {
                if (x < m_offsetX + static_cast<int>(m_width))
                {
                    const ValueType* band0 = static_cast<const ValueType*>(m_decoder->currentScanlineOfBand(0));
                    band0 += m_offset*(x - m_offsetX);
                    if (m_bands == 2)
                    {
                        const ValueType* band1 = static_cast<const ValueType*>(m_decoder->currentScanlineOfBand(1));
                        band1 += m_offset*(x - m_offsetX);
                        mask = *band1;
                    }
                    else
                    {
                        mask = vigra::NumericTraits<ValueType>::max();
                    };
                    value = *band0;
                }
                else
                {
                    mask = vigra::NumericTraits<ValueType>::zero();
                };
            };
        };
    };

private:
    std::string m_filename;
    vigra::ImageImportInfo m_info;
    vigra::Size2D m_canvassize;
    int m_offsetX, m_offsetY;
    unsigned m_x, m_y, m_width, m_height, m_offset, m_bands;
    VIGRA_UNIQUE_PTR<vigra::Decoder> m_decoder;
    bool m_hasAlpha, m_noData;
};

template<class ValueType>
void getMean(const std::vector<ValueType>& values, ValueType& val)
{
    size_t n = 0;
    typedef vigra::NumericTraits<ValueType> RealTraits;
    typename RealTraits::RealPromote mean = RealTraits::zero();
    for (auto& x : values)
    {
        ++n;
        mean += (x - mean) / n;
    };
    val = RealTraits::fromRealPromote(mean);
};

template<class ValueType>
void getMeanSigma(const std::vector<ValueType>& values, ValueType& val, typename vigra::NumericTraits<ValueType>::RealPromote& sigma)
{
    typedef vigra::NumericTraits<ValueType> RealTraits;
    typedef typename RealTraits::RealPromote RealType;
    size_t n = 0;
    RealType mean = RealTraits::zero();
    RealType m2 = RealTraits::zero();
    for (auto& x : values)
    {
        ++n;
        const RealType delta = x - mean;
        mean += delta / n;
        const RealType delta2 = x - mean;
        m2 += delta*delta2;
    };
    val = RealTraits::fromRealPromote(mean);
    if (n >= 2)
    {
        sigma = sqrt(m2 / n);
    }
    else
    {
        sigma = vigra::NumericTraits<RealType>::zero();
    };
};

template<class ValueType>
class MinStacker
{
public:
    void reset() { m_min = vigra::NumericTraits<ValueType>::max(); };
    void operator()(const ValueType& val, vigra::VigraTrueType) { if (val < m_min) m_min = val; };
    void operator()(const ValueType& val, vigra::VigraFalseType) { if (val.luminance() < m_min.luminance()) m_min = val; }
    void operator()(const ValueType& val)
    {
        typedef typename vigra::NumericTraits<ValueType>::isScalar is_scalar;
        operator()(val, is_scalar());
    };
    void getResult(ValueType& val) { val = m_min; };
    bool IsValid() { return m_min != vigra::NumericTraits<ValueType>::max(); };
private:
    ValueType m_min;
};

template<class ValueType>
class MaxStacker
{
public:
    virtual void reset() { m_max = vigra::NumericTraits<ValueType>::min(); };
    void operator()(const ValueType& val, vigra::VigraTrueType) { if (val > m_max) m_max = val; };
    void operator()(const ValueType& val, vigra::VigraFalseType) { if (val.luminance() > m_max.luminance()) m_max = val;}
    virtual void operator()(const ValueType& val)
    {
        typedef typename vigra::NumericTraits<ValueType>::isScalar is_scalar;
        operator()(val, is_scalar());
    };
    virtual void getResult(ValueType& val) { val = m_max; };
    virtual bool IsValid() { return m_max != vigra::NumericTraits<ValueType>::min(); };
private:
    ValueType m_max;
};

template<class ValueType>
class AverageStacker
{
public:
    virtual void reset() { m_sum = vigra::NumericTraits<ValueType>::zero(); m_count = 0; };
    virtual void operator()(const ValueType& val) { m_sum += val; ++m_count;};
    virtual void getResult(ValueType& val) { val = m_sum / m_count; };
    virtual bool IsValid() { return m_count > 0; };

private:
    typename vigra::NumericTraits<ValueType>::RealPromote m_sum;
    size_t m_count;
};

template<class ValueType>
class MedianStacker
{
public:
    void reset() { m_values.clear(); };
    void operator()(const ValueType& val) { m_values.push_back(val); };
    void getResult(ValueType& val)
    {
        sort();
        if (m_values.size() % 2 == 1)
        {
            val = m_values[(m_values.size() - 1) / 2];
        }
        else
        {
            const int index = m_values.size() / 2;
            val = 0.5 * (m_values[index - 1] + m_values[index]);
        };
    };
    bool IsValid() { return !m_values.empty();};
    void getResultAndSigma(ValueType& val, typename vigra::NumericTraits<ValueType>::RealPromote& sigma)
    {
        sort();
        if (m_values.size() % 2 == 1)
        {
            val = m_values[(m_values.size() - 1) / 2];
        }
        else
        {
            const int index = m_values.size() / 2;
            val = 0.5 * (m_values[index - 1] + m_values[index]);
        };
        ValueType mean;
        getMeanSigma(m_values, mean, sigma);
    };
    const std::string getName() const { return "median"; };
protected:
    // sort gray scale
    void sort(vigra::VigraTrueType)
    {
        std::sort(m_values.begin(), m_values.end());
    };
    // sort color values
    void sort(vigra::VigraFalseType)
    {
        std::sort(m_values.begin(), m_values.end(),
            [](const ValueType & a, const ValueType & b) {return a.luminance() < b.luminance(); });
    };
    // generic sort
    void sort()
    {
        typedef typename vigra::NumericTraits<ValueType>::isScalar is_scalar;
        sort(is_scalar());
    };

    std::vector<ValueType> m_values;
};

template<class ValueType>
class WinsorMeanStacker : public MedianStacker<ValueType>
{
public:
    virtual void getResult(ValueType& val)
    {
        this->sort();
        const int indexTrim = hugin_utils::floori(Parameters.winsorTrim * this->m_values.size());
        for (size_t i = 0; i < indexTrim; ++i)
        {
            this->m_values[i] = this->m_values[indexTrim];
        }
        for (size_t i = this->m_values.size() - indexTrim; i < this->m_values.size(); ++i)
        {
            this->m_values[i] = this->m_values[this->m_values.size() - indexTrim - 1];
        };
        getMean(this->m_values, val);
    };
    virtual void getResultAndSigma(ValueType& val, typename vigra::NumericTraits<ValueType>::RealPromote& sigma)
    {
        this->sort();
        const int indexTrim = hugin_utils::floori(Parameters.winsorTrim * this->m_values.size());
        for (size_t i = 0; i < indexTrim; ++i)
        {
            this->m_values[i] = this->m_values[indexTrim];
        }
        for (size_t i = this->m_values.size() - indexTrim; i < this->m_values.size(); ++i)
        {
            this->m_values[i] = this->m_values[this->m_values.size() - indexTrim - 1];
        };
        getMeanSigma(this->m_values, val, sigma);
    };
    const std::string getName() const { return "Winsor clipped mean"; };
};

template<class ValueType>
class SigmaMeanStacker
{
public:
    virtual void reset() { m_values.clear(); m_sortValues.clear(); };
    void operator()(const ValueType& val, vigra::VigraTrueType) { m_values.push_back(val); m_sortValues.push_back(val); };
    void operator()(const ValueType& val, vigra::VigraFalseType) { m_values.push_back(val); m_sortValues.push_back(val.luminance()); };
    virtual void operator()(const ValueType& val)
    {
        typedef typename vigra::NumericTraits<ValueType>::isScalar is_scalar;
        operator()(val, is_scalar());
    };
    virtual void getResult(ValueType& val)
    {
        size_t iteration = 0;
        while (iteration < Parameters.maxIterations)
        {
            double mean, sigma;
            getMeanSigma(m_sortValues, mean, sigma);
            const size_t oldSize = m_sortValues.size();
            for (int i = m_sortValues.size() - 1; i >= 0 && m_sortValues.size() > 1; --i)
            {
                // check if values are in range
                if (abs(m_sortValues[i] - mean) > Parameters.sigma * sigma)
                {
                    m_sortValues.erase(m_sortValues.begin() + i);
                    m_values.erase(m_values.begin() + i);
                };
            };
            if (m_sortValues.size() == oldSize)
            {
                // no values outside range, return mean value
                getMean(m_values, val);
                return;
            };
            ++iteration;
        };
        getMean(m_values, val);
    };
    virtual void getResultAndSigma(ValueType& val, typename vigra::NumericTraits<ValueType>::RealPromote& sigma)
    {
        size_t iteration = 0;
        while (iteration < Parameters.maxIterations)
        {
            double grayMean, graySigma;
            getMeanSigma(m_sortValues, grayMean, graySigma);
            const size_t oldSize = m_sortValues.size();
            for (int i = m_sortValues.size() - 1; i >= 0 && m_sortValues.size() > 1; --i)
            {
                // check if values are in range
                if (abs(m_sortValues[i] - grayMean) > Parameters.sigma * graySigma)
                {
                    m_sortValues.erase(m_sortValues.begin() + i);
                    m_values.erase(m_values.begin() + i);
                };
            };
            if (m_sortValues.size() == oldSize)
            {
                // no values outside range, return mean value
                getMeanSigma(m_values, val, sigma);
                return;
            };
            ++iteration;
        };
        getMeanSigma(m_values, val, sigma);
    };
    virtual bool IsValid() { return !m_values.empty(); };
    const std::string getName() const { return "sigma clipped mean"; };

private:
    std::vector<ValueType> m_values;
    std::vector<double> m_sortValues;
};

bool CheckInput(const std::vector<InputImage*>& images, vigra::Rect2D& outputROI, vigra::Size2D& canvasSize)
{
    if (images.empty())
    {
        return false;
    };
    // get ROI and canvas size which contains all images
    outputROI = images[0]->getROI();
    canvasSize = images[0]->getCanvasSize();
    for (size_t i = 1; i < images.size(); ++i)
    {
        outputROI |= images[i]->getROI();
        if (images[i]->getCanvasSize().width() > canvasSize.width())
        {
            canvasSize.setWidth(images[i]->getCanvasSize().width());
        };
        if (images[i]->getCanvasSize().height() > canvasSize.height())
        {
            canvasSize.setHeight(images[i]->getCanvasSize().height());
        };
    };
    if (outputROI.area() == 0)
    {
        std::cerr << "ERROR: You can't stack non-overlapping images." << std::endl;
        return false;
    };
    return true;
}

/** loads images line by line and merge into final image, save the result */
template <class PixelType, class Functor>
bool StackImages(std::vector<InputImage*>& images, Functor& stacker)
{
    typedef typename vigra::NumericTraits<PixelType>::ValueType ChannelType;
    vigra::Rect2D outputROI;
    vigra::Size2D canvasSize;
    if (!CheckInput(images, outputROI, canvasSize))
    {
        return false;
    }
    // prepare output
    vigra::ImageExportInfo exportImageInfo(Parameters.outputFilename.c_str(), Parameters.useBigTIFF ? "w8" : "w");
    exportImageInfo.setXResolution(images[0]->getXResolution());
    exportImageInfo.setYResolution(images[0]->getYResolution());
    exportImageInfo.setPosition(outputROI.upperLeft());
    exportImageInfo.setCanvasSize(canvasSize);
    exportImageInfo.setICCProfile(images[0]->getICCProfile());
    SetCompression(exportImageInfo, Parameters.compression);
    vigra::BasicImage<PixelType> output(outputROI.size());
    vigra::BImage mask(output.size(),vigra::UInt8(0));
    // loop over all lines
    for (size_t y = outputROI.top(); y < outputROI.bottom(); ++y)
    {
        // load next line
#pragma omp parallel for
        for (int i = 0; i < images.size(); ++i)
        {
            images[i]->readLine(y);
        };
        // process current line
#pragma omp parallel for schedule(static, 100)
        for (int x = outputROI.left(); x < outputROI.right(); ++x)
        {
            // we need a private copy for each thread
            Functor privateStacker(stacker);
            privateStacker.reset();
            for (size_t i = 0; i < images.size(); ++i)
            {
                PixelType value;
                ChannelType maskValue;
                images[i]->getValue(x, value, maskValue);
                if (maskValue > 0)
                {
                    privateStacker(value);
                }
            };
            if (privateStacker.IsValid())
            {
                privateStacker.getResult(output(x - outputROI.left(), y - outputROI.top()));
                mask(x-outputROI.left(), y-outputROI.top()) = 255;
            };
        };
    };
    std::cout << "Write result to " << Parameters.outputFilename << std::endl;
    return SaveFinalImage(output, mask, images[0]->getPixelType(), exportImageInfo);
};

template <class PixelType>
class FilterMask
{
public:
    typedef typename vigra::NumericTraits<PixelType>::RealPromote realPixelType;
    typedef vigra::TinyVector<realPixelType, 2> first_argument_type;
    typedef PixelType second_argument_type;
    typedef vigra::UInt8 third_argument_type;
    typedef vigra::UInt8 result_type;
    vigra::UInt8 operator()(const vigra::TinyVector<realPixelType, 2>& limits, const PixelType& color, const vigra::UInt8& mask, vigra::VigraFalseType) const
    {
        if (mask > 0 && (color.red() < limits[0].red() || color.red()>limits[1].red() ||
            color.green() < limits[0].green() || color.green() > limits[1].green() ||
            color.blue() < limits[0].blue() || color.blue() > limits[1].blue()))
        {
            return 255;
        }
        else
        {
            return 0;
        };
    };

    vigra::UInt8 operator()(const vigra::TinyVector<realPixelType, 2>& limits, const PixelType& gray, const vigra::UInt8& mask, vigra::VigraTrueType) const
    {
        if (mask > 0 && (gray < limits[0] || gray>limits[1]))
        {
            return 255;
        }
        else
        {
            return 0;
        };
    };

    vigra::UInt8 operator()(const vigra::TinyVector<realPixelType, 2>& limits, const PixelType& pixel, const vigra::UInt8& mask) const
    {
        typedef typename vigra::NumericTraits<PixelType>::isScalar is_scalar;
        return (*this)(limits, pixel, mask, is_scalar());
    }

};

template <class PixelType, class Functor>
bool StackImagesAndMask(std::vector<InputImage*>& images, Functor& stacker)
{
    typedef typename vigra::NumericTraits<PixelType>::ValueType ChannelType;
    vigra::Rect2D outputROI;
    vigra::Size2D canvasSize;
    if (!CheckInput(images, outputROI, canvasSize))
    {
        return false;
    }
    // prepare output
    vigra::ImageExportInfo exportImageInfo(Parameters.outputFilename.c_str(), Parameters.useBigTIFF ? "w8" : "w");
    exportImageInfo.setXResolution(images[0]->getXResolution());
    exportImageInfo.setYResolution(images[0]->getYResolution());
    exportImageInfo.setPosition(outputROI.upperLeft());
    exportImageInfo.setCanvasSize(canvasSize);
    exportImageInfo.setICCProfile(images[0]->getICCProfile());
    SetCompression(exportImageInfo, Parameters.compression);
    // for multi-layer output
    vigra::TiffImage* tiffImage;
    vigra::BasicImage<PixelType> output(outputROI.size());
    vigra::BImage mask(output.size(), vigra::UInt8(0));
    vigra::BasicImage<vigra::TinyVector<typename vigra::NumericTraits<PixelType>::RealPromote, 2>> limits(output.size());
    // loop over all lines
    for (size_t y = outputROI.top(); y < outputROI.bottom(); ++y)
    {
        // load next line
#pragma omp parallel for
        for (int i = 0; i < images.size(); ++i)
        {
            images[i]->readLine(y);
        };
        // process current line
#pragma omp parallel for schedule(static, 100)
        for (int x = outputROI.left(); x < outputROI.right(); ++x)
        {
            // we need a private copy for each thread
            Functor privateStacker(stacker);
            privateStacker.reset();
            for (size_t i = 0; i < images.size(); ++i)
            {
                PixelType value;
                ChannelType maskValue;
                images[i]->getValue(x, value, maskValue);
                if (maskValue > 0)
                {
                    privateStacker(value);
                }
            };
            if (privateStacker.IsValid())
            {
                PixelType mean;
                typename vigra::NumericTraits<PixelType>::RealPromote sigma;
                privateStacker.getResultAndSigma(mean, sigma);
                output(x - outputROI.left(), y - outputROI.top()) = mean;
                mask(x - outputROI.left(), y - outputROI.top()) = 255;
                limits(x - outputROI.left(), y - outputROI.top()) = vigra::TinyVector<PixelType, 2>(mean - Parameters.maskSigma*sigma, mean + Parameters.maskSigma*sigma);
            };
        };
    };
    std::cout << "Write result to " << Parameters.outputFilename << std::endl;
    if (Parameters.multiLayer)
    {
        tiffImage = TIFFOpen(Parameters.outputFilename.c_str(), "w");
        vigra_ext::createTiffDirectory(tiffImage, stacker.getName(), stacker.getName(), 
            Parameters.compression.empty() ? "LZW" : Parameters.compression, 0, images.size() + 1,
            outputROI.upperLeft(), canvasSize, images[0]->getICCProfile());
        vigra_ext::createAlphaTiffImage(vigra::srcImageRange(output), vigra::maskImage(mask), tiffImage);
        TIFFFlush(tiffImage);
    }
    else
    {
        if (!SaveFinalImage(output, mask, images[0]->getPixelType(), exportImageInfo))
        {
            return false;
        };
    };
    // we don't need median image any more
    output.resize(0, 0);
    std::cout << "Masking input images with sigma=" << Parameters.maskSigma;
    if (Parameters.multiLayer)
    {
        std::cout << std::endl;
    }
    else
    {
        std::cout << " and suffix " << Parameters.maskSuffix << ".tif" << std::endl;
    };
    for (size_t i = 0; i < images.size(); ++i)
    {
        std::cout << "Masking " << images[i]->getFilename();
        if (Parameters.multiLayer)
        {
            std::cout << std::endl;
        }
        else
        {
            std::cout << " -> " << images[i]->getMaskFilename() << std::endl;
        };
        vigra::BasicImage<PixelType> image(images[i]->getROI().size());
        vigra::BImage mask(image.size(), 255);
        if (images[i]->numExtraBands() == 1)
        {
            vigra::importImageAlpha(images[i]->getImageImportInfo(), vigra::destImage(image), vigra::destImage(mask));
        }
        else
        {
            vigra::importImage(images[i]->getImageImportInfo(), vigra::destImage(image));
        };
        vigra::Rect2D roi = images[i]->getROI();
        roi.moveBy(-outputROI.upperLeft());
        vigra::combineThreeImages(vigra::srcImageRange(limits, roi), vigra::srcImage(image), vigra::srcImage(mask), vigra::destImage(mask), FilterMask<PixelType>());
        if (hugin_utils::FileExists(images[i]->getMaskFilename()))
        {
            std::cout << "Masked file \"" << images[i]->getMaskFilename() << "\" already exists." << std::endl
                << "Processing aborted." << std::endl;
            return false;
        }
        if (Parameters.multiLayer)
        {
            vigra_ext::createTiffDirectory(tiffImage, images[i]->getFilename(), images[i]->getFilename(), 
                Parameters.compression.empty() ? "LZW" : Parameters.compression, i+1, images.size() + 1,
                images[i]->getROI().upperLeft(), images[i]->getCanvasSize(), images[i]->getICCProfile());
            vigra_ext::createAlphaTiffImage(vigra::srcImageRange(image), vigra::maskImage(mask), tiffImage);
            TIFFFlush(tiffImage);
        }
        else
        {
            vigra::ImageExportInfo exportMaskImage(images[i]->getMaskFilename().c_str(), Parameters.useBigTIFF ? "w8" : "w");
            exportMaskImage.setXResolution(images[i]->getXResolution());
            exportMaskImage.setYResolution(images[i]->getYResolution());
            exportMaskImage.setPosition(images[i]->getROI().upperLeft());
            exportMaskImage.setCanvasSize(images[i]->getCanvasSize());
            exportMaskImage.setICCProfile(images[i]->getICCProfile());
            exportMaskImage.setPixelType(images[i]->getPixelType().c_str());
            exportMaskImage.setCompression("LZW");
            try
            {
                vigra::exportImageAlpha(vigra::srcImageRange(image), vigra::srcImage(mask), exportMaskImage);
            }
            catch (std::exception& e)
            {
                std::cerr << "Could not save masked images \"" << exportMaskImage.getFileName() << "\"." << std::endl
                    << "Error code: " << e.what() << std::endl
                    << "Processing aborted." << std::endl;
                return false;
            }
        };
    };
    if (Parameters.multiLayer)
    {
        TIFFClose(tiffImage);
    }
    return true;
};

void CleanUp(std::vector<InputImage*>& images)
{
    for (auto& img : images)
    {
        delete img;
    };
};

template <class PixelType>
bool main_stacker(std::vector<InputImage*>& images)
{
    if (Parameters.stackMode == "min" || Parameters.stackMode == "minimum" || Parameters.stackMode == "darkest")
    {
        std::cout << "Merging stack with minimum operator." << std::endl;
        MinStacker<PixelType> stacker;
        return StackImages<PixelType>(images, stacker);
    }
    else
    {
        if (Parameters.stackMode == "max" || Parameters.stackMode == "maximum" || Parameters.stackMode == "brightest")
        {
            std::cout << "Merging stack with maximum operator." << std::endl;
            MaxStacker<PixelType> stacker;
            return StackImages<PixelType>(images, stacker);
        }
        else
        {
            if (Parameters.stackMode == "avg" || Parameters.stackMode == "average" || Parameters.stackMode == "mean")
            {
                std::cout << "Merging stack with average operator." << std::endl;
                AverageStacker<PixelType> stacker;
                return StackImages<PixelType>(images, stacker);
            }
            else
            {
                if (Parameters.stackMode == "median")
                {
                    MedianStacker<PixelType> stacker;
                    std::cout << "Merging stack with median operator." << std::endl;
                    if (Parameters.maskInput)
                    {
                        return StackImagesAndMask<PixelType>(images, stacker);
                    }
                    else
                    {
                        return StackImages<PixelType>(images, stacker);
                    };
                }
                else
                {
                    if (Parameters.stackMode == "winsor")
                    {
                        WinsorMeanStacker<PixelType> stacker;
                        std::cout << "Merging stack with Winsor clipping operator (trim=" << Parameters.winsorTrim << ")." << std::endl;
                        if (Parameters.maskInput)
                        {
                            return StackImagesAndMask<PixelType>(images, stacker);
                        }
                        else
                        {
                            return StackImages<PixelType>(images, stacker);
                        };
                    }
                    else
                    {
                        if (Parameters.stackMode == "sigma")
                        {
                            SigmaMeanStacker<PixelType> stacker;
                            std::cout << "Merging stack with sigma clipping operator (max sigma=" << Parameters.sigma << ", max " << Parameters.maxIterations << " iterations)." << std::endl;
                            if (Parameters.maskInput)
                            {
                                return StackImagesAndMask<PixelType>(images, stacker);
                            }
                            else
                            {
                                return StackImages<PixelType>(images, stacker);
                            };
                        }
                        else
                        {
                            std::cerr << "ERROR: " << "\"" << Parameters.stackMode << "\" is not a valid stack mode." << std::endl
                                << "        Allowed values are min|max|average|median|winsor|sigma" << std::endl;
                            return false;
                        }
                    };
                };
            };
        };
    };
};

int main(int argc, char* argv[])
{
    // parse arguments
    const char* optstring = "o:h";

    enum
    {
        OPT_COMPRESSION = 1000,
        OPT_STACKMODE,
        OPT_WINSOR_TRIM,
        OPT_SIGMA_MAX,
        OPT_MAX_ITER,
        OPT_MASK_INPUT,
        OPT_MASK_SUFFIX,
        OPT_MASK_SIGMA,
        OPT_MULTILAYER,
        OPT_BIGTIFF
    };
    static struct option longOptions[] =
    {
        { "output", required_argument, NULL, 'o' },
        { "compression", required_argument, NULL, OPT_COMPRESSION },
        { "mode", required_argument, NULL, OPT_STACKMODE },
        { "winsor-trim", required_argument, NULL, OPT_WINSOR_TRIM},
        { "max-sigma", required_argument, NULL, OPT_SIGMA_MAX},
        { "max-iterations", required_argument, NULL, OPT_MAX_ITER},
        { "mask-input", no_argument, NULL, OPT_MASK_INPUT},
        { "mask-suffix", required_argument, NULL, OPT_MASK_SUFFIX},
        { "mask-sigma", required_argument, NULL, OPT_MASK_SIGMA },
        { "multi-layer-output", no_argument, NULL, OPT_MULTILAYER },
        { "bigtiff", no_argument, NULL, OPT_BIGTIFF },
        { "help", no_argument, NULL, 'h' },
        0
    };

    int c;
    while ((c = getopt_long(argc, argv, optstring, longOptions, nullptr)) != -1)
    {
        switch (c)
        {
            case 'o':
                Parameters.outputFilename = optarg;
                break;
            case 'h':
                usage(hugin_utils::stripPath(argv[0]).c_str());
                return 0;
                break;
            case OPT_COMPRESSION:
                Parameters.compression = hugin_utils::toupper(optarg);
                break;
            case OPT_STACKMODE:
                Parameters.stackMode=optarg;
                Parameters.stackMode = hugin_utils::tolower(Parameters.stackMode);
                break;
            case OPT_WINSOR_TRIM:
                {
                    std::string text(optarg);
                    int pos = text.find("%");
                    if (pos != std::string::npos)
                    {
                        text = text.substr(0, pos);
                        if (!hugin_utils::stringToDouble(text, Parameters.winsorTrim))
                        {
                            std::cerr << hugin_utils::stripPath(argv[0]) << ": No valid number for Winsor trim factor given." << std::endl;
                            return 1;
                        };
                        Parameters.winsorTrim /= 100.0;
                    }
                    else
                    {
                        if (!hugin_utils::stringToDouble(text, Parameters.winsorTrim))
                        {
                            std::cerr << hugin_utils::stripPath(argv[0]) << ": No valid number for Winsor trim factor given." << std::endl;
                            return 1;
                        };
                    };
                    if (Parameters.winsorTrim<0.01 || Parameters.winsorTrim>0.49)
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": Winsor trim factor " << Parameters.winsorTrim << " not in valid range (0.01 - 0.49)." << std::endl;
                        return 1;
                    };
                };
                break;
            case OPT_SIGMA_MAX:
                    {
                        std::string text(optarg);
                        if (!hugin_utils::stringToDouble(text, Parameters.sigma))
                        {
                            std::cerr << hugin_utils::stripPath(argv[0]) << ": No valid number for maximal sigma value." << std::endl;
                            return 1;
                        };
                        if (Parameters.sigma<0.01)
                        {
                            std::cerr << hugin_utils::stripPath(argv[0]) << ": Maximal sigma value have to be positive." << std::endl;
                            return 1;
                        };
                    };
                    break;
            case OPT_MAX_ITER:
                {
                    std::string text(optarg);
                    if (!hugin_utils::stringToInt(text, Parameters.maxIterations))
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": No valid number for maximal iterations." << std::endl;
                        return 1;
                    };
                    if (Parameters.maxIterations<1)
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": Maximal iterations values have to be at least 1." << std::endl;
                        return 1;
                    };
                };
                break;
            case OPT_MASK_INPUT:
                Parameters.maskInput = true;
                break;
            case OPT_MASK_SUFFIX:
                Parameters.maskSuffix = optarg;
                break;
            case OPT_MASK_SIGMA:
                {
                    std::string text(optarg);
                    if (!hugin_utils::stringToDouble(text, Parameters.maskSigma))
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": No valid number for maximal sigma value." << std::endl;
                        return 1;
                    };
                    if (Parameters.maskSigma<0.01)
                    {
                        std::cerr << hugin_utils::stripPath(argv[0]) << ": Maximal sigma value have to be positive." << std::endl;
                        return 1;
                    };
                };
                break;
            case OPT_MULTILAYER:
                Parameters.multiLayer = true;
                break;
            case OPT_BIGTIFF:
                Parameters.useBigTIFF = true;
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
    };

    unsigned nFiles = argc - optind;
    if (nFiles < 1)
    {
        std::cerr << hugin_utils::stripPath(argv[0]) << ": at least one image need to be specified" << std::endl;
        return 1;
    }

    // extract file names
    std::vector<std::string> files;
    for (size_t i = 0; i < nFiles; i++)
    {
        std::string currentFile(argv[optind + i]);
        // check file existence
        if (hugin_utils::FileExists(currentFile) && vigra::isImage(currentFile.c_str()))
        {
            files.push_back(currentFile);
        };
    }

    if (files.empty())
    {
        std::cerr << "ERROR: " << hugin_utils::stripPath(argv[0]) << " needs at least one image." << std::endl;
        return 1;
    };

    if (Parameters.outputFilename.empty())
    {
        Parameters.outputFilename = "final.tif";
    };
    // if no extension is given assume TIF format
    hugin_utils::EnforceExtension(Parameters.outputFilename, "tif");
    if (!hugin_utils::IsFileTypeSupported(Parameters.outputFilename))
    {
        std::cerr << "ERROR: Extension \"" << hugin_utils::getExtension(Parameters.outputFilename) << "\" is unknown." << std::endl;
        return 1;
    };
    const std::string extension = hugin_utils::tolower(hugin_utils::getExtension(Parameters.outputFilename));
    if (Parameters.multiLayer && extension != "tif" && extension != "tiff")
    {
        std::cerr << "ERROR: Multi layer output expects a tiff file as output." << std::endl
            << "       Other image formates are not compatible with this option." << std::endl;
        return 1;
    };
    bool success = false;
    std::vector<InputImage*> images;
    for (size_t i = 0; i < files.size(); ++i)
    {
        images.push_back(new InputImage(files[i]));
    };
    if (!images[0]->isColor() && !images[0]->isGrayscale())
    {
        std::cerr << "ERROR: Only RGB and grayscale images are supported." << std::endl
            << "       Image \"" << images[0]->getFilename() << "\" has " << images[0]->numPixelSamples() << " channels per pixel." << std::endl;
        CleanUp(images);
        return 1;
    }
    if (images[0]->numExtraBands() > 1)
    {
        std::cerr << "ERROR: Images with several alpha channels are not supported." << std::endl
            << "       Image \"" << images[0]->getFilename() << "\" has " << images[0]->numExtraBands() << " extra channels." << std::endl;
        CleanUp(images);
        return 1;
    }
    const std::string pixeltype(images[0]->getPixelType());
    //check, that image information matches
    for (size_t i = 1; i < files.size(); ++i)
    {
        if (!images[i]->isColor() && !images[i]->isGrayscale())
        {
            std::cerr << "ERROR: Only RGB and grayscale images are supported." << std::endl
                << "       Image \"" << images[i]->getFilename() << "\" has " << images[i]->numPixelSamples() << " channels per pixel." << std::endl;
            CleanUp(images);
            return 1;
        };
        if (images[i]->numExtraBands() > 1)
        {
            std::cerr << "ERROR: Images with several alpha channels are not supported." << std::endl
                << "       Image \"" << images[i]->getFilename() << "\" has " << images[i]->numExtraBands() << " extra channels." << std::endl;
            CleanUp(images);
            return 1;
        };
        if (images[0]->isColor() != images[i]->isColor())
        {
            std::cerr << "ERROR: You can't merge color and grayscale images." << std::endl;
            CleanUp(images);
            return 1;
        };
        if (images[0]->numPixelSamples() != images[i]->numPixelSamples())
        {
            std::cerr << "ERROR: You can't merge image with different number of channels." << std::endl
                << "       Image \"" << images[0]->getFilename() << "\" has " << images[0]->numBands() << " channels," << std::endl
                << "       but image \"" << images[i]->getFilename() << "\" has " << images[i]->numBands() << " channels." << std::endl;
            CleanUp(images);
            return 1;
        };
        if (pixeltype!=images[i]->getPixelType())
        {
            std::cerr << "ERROR: You can't merge images with different pixel types." << std::endl
                << "       Image \"" << images[0]->getFilename() << "\" has pixel type " << images[0]->getPixelType() << "," << std::endl
                << "       but image \"" << images[i]->getFilename() << "\" has pixel type " << images[i]->getPixelType() << "." << std::endl;
            CleanUp(images);
            return 1;
        };
    };

    if (images[0]->isColor())
    {
        if (pixeltype == "UINT8")
        {
            success = main_stacker<vigra::RGBValue<vigra::UInt8>>(images);
        }
        else if (pixeltype == "UINT16")
        {
            success = main_stacker<vigra::RGBValue<vigra::UInt16>>(images);
        }
        else
        {
            std::cerr << " ERROR: unsupported pixel type: " << pixeltype << std::endl;
        };
    }
    else
    {
        //grayscale images
        if (pixeltype == "UINT8")
        {
           success = main_stacker<vigra::UInt8>(images);
        }
        else if (pixeltype == "UINT16")
        {
            success = main_stacker<vigra::UInt16>(images);
        }
        else
        {
            std::cerr << " ERROR: unsupported pixel type: " << pixeltype << std::endl;
        };
    };

    CleanUp(images);
    if (success)
    {
        return 0;
    };
    return 1;
}
