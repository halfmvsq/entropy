#ifndef IMAGE_UTILITY_TPP
#define IMAGE_UTILITY_TPP

#include "common/Exception.hpp"
#include "common/Types.h"

#include "image/Image.h"

#include <itkBinaryThresholdImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkClampImageFilter.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkImageToHistogramFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <itkImportImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkStatisticsImageFilter.h>
#include <itkVectorImage.h>

#include <vtkSmartPointer.h>

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Without undefining min and max, there are some errors compiling in Visual Studio
#undef min
#undef max

#define DEBUG_IMAGE_OUTPUT 0




template< class T >
typename itk::Image<T, 3>::Pointer makeScalarImage(
    const std::array<uint32_t, 3>& imageDims,
    const std::array<double, 3>& imageOrigin,
    const std::array<double, 3>& imageSpacing,
    const std::array< std::array<double, 3>, 3 >&  imageDirection,
    const T* imageData )
{
    using ImportFilterType = itk::ImportImageFilter<T, 3>;

    if ( ! imageData )
    {
        spdlog::error( "Null data array provided when creating new scalar image" );
        return nullptr;
    }

    constexpr bool filterOwnsBuffer = false;

    typename ImportFilterType::IndexType start;
    typename ImportFilterType::SizeType size;
    typename ImportFilterType::DirectionType direction;

    itk::SpacePrecisionType origin[3];
    itk::SpacePrecisionType spacing[3];

    for ( uint32_t i = 0; i < 3; ++i )
    {
        start[i] = 0.0;
        size[i] = imageDims[i];
        origin[i] = imageOrigin[i];
        spacing[i] = imageSpacing[i];

        for ( uint32_t j = 0; j < 3; ++j )
        {
            direction[i][j] = imageDirection[i][j];
        }
    }

    const std::size_t numPixels = size[0] * size[1] * size[2];

    if ( 0 == numPixels )
    {
        spdlog::error( "Cannot create new scalar image with size zero" );
        return nullptr;
    }

    typename  ImportFilterType::RegionType region;
    region.SetIndex( start );
    region.SetSize( size );

    try
    {
        typename ImportFilterType::Pointer importer = ImportFilterType::New();
        importer->SetRegion( region );
        importer->SetOrigin( origin );
        importer->SetSpacing( spacing );
        importer->SetDirection( direction );
        importer->SetImportPointer( const_cast<T*>( imageData ), numPixels, filterOwnsBuffer );
        importer->Update();

        return importer->GetOutput();
    }
    catch ( const std::exception& e )
    {
        spdlog::error( "Exception creating new ITK scalar image from data array: {}", e.what() );
        return nullptr;
    }
}


/**
 * @brief Create a scalar ITK image from an image component
 *
 * @tparam T Component type of output image
 * @param[in] image Image
 * @param[in] component Image component
 *
 * @return Scalar ITK image of the component
 */
template< class T >
typename itk::Image<T, 3>::Pointer createItkImageFromImageComponent(
    const Image& image, uint32_t component )
{
    using OutputImageType = itk::Image<T, 3>;

    const ImageHeader& header = image.header();

    if ( component >= header.numComponentsPerPixel() )
    {
        spdlog::error( "Invalid image component {} to convert to ITK image; image has only {} components",
                      component, header.numComponentsPerPixel() );
        return nullptr;
    }

    std::array< uint32_t, 3 > dims;
    std::array< double, 3 > origin;
    std::array< double, 3 > spacing;
    std::array< std::array<double, 3>, 3 > directions;

    for ( uint32_t i = 0; i < 3; ++i )
    {
        const int ii = static_cast<int>(i);

        dims[i] = header.pixelDimensions()[ii];
        origin[i] = static_cast<double>( header.origin()[ii] );
        spacing[i] = static_cast<double>( header.spacing()[ii] );

        directions[i] = {
            static_cast<double>( header.directions()[ii].x ),
            static_cast<double>( header.directions()[ii].y ),
            static_cast<double>( header.directions()[ii].z )
        };
    }

    switch ( header.memoryComponentType() )
    {
    case ComponentType::Int8:
    {
        using S = int8_t;
        using InputImageType = itk::Image<S, 3>;
        using FilterType = itk::CastImageFilter< InputImageType, OutputImageType >;
        typename FilterType::Pointer caster = FilterType::New();

        InputImageType::Pointer compImage = makeScalarImage(
            dims, origin, spacing, directions,
            reinterpret_cast<const S*>( image.bufferAsVoid( component ) ) );

        if ( ! compImage ) return nullptr;

        caster->SetInput( compImage );
        caster->Update();
        return caster->GetOutput();
    }
    case ComponentType::UInt8:
    {
        using S = uint8_t;
        using InputImageType = itk::Image<S, 3>;
        using FilterType = itk::CastImageFilter< InputImageType, OutputImageType >;
        typename FilterType::Pointer caster = FilterType::New();

        InputImageType::Pointer compImage = makeScalarImage(
            dims, origin, spacing, directions,
            reinterpret_cast<const S*>( image.bufferAsVoid( component ) ) );

        if ( ! compImage ) return nullptr;

        caster->SetInput( compImage );
        caster->Update();
        return caster->GetOutput();
    }
    case ComponentType::Int16:
    {
        using S = int16_t;
        using InputImageType = itk::Image<S, 3>;
        using FilterType = itk::CastImageFilter< InputImageType, OutputImageType >;
        typename FilterType::Pointer caster = FilterType::New();

        InputImageType::Pointer compImage = makeScalarImage(
            dims, origin, spacing, directions,
            reinterpret_cast<const S*>( image.bufferAsVoid( component ) ) );

        if ( ! compImage ) return nullptr;

        caster->SetInput( compImage );
        caster->Update();
        return caster->GetOutput();
    }
    case ComponentType::UInt16:
    {
        using S = uint16_t;
        using InputImageType = itk::Image<S, 3>;
        using FilterType = itk::CastImageFilter< InputImageType, OutputImageType >;
        typename FilterType::Pointer caster = FilterType::New();

        InputImageType::Pointer compImage = makeScalarImage(
            dims, origin, spacing, directions,
            reinterpret_cast<const S*>( image.bufferAsVoid( component ) ) );

        if ( ! compImage ) return nullptr;

        caster->SetInput( compImage );
        caster->Update();
        return caster->GetOutput();
    }
    case ComponentType::Int32:
    {
        using S = int32_t;
        using InputImageType = itk::Image<S, 3>;
        using FilterType = itk::CastImageFilter< InputImageType, OutputImageType >;
        typename FilterType::Pointer caster = FilterType::New();

        InputImageType::Pointer compImage = makeScalarImage(
            dims, origin, spacing, directions,
            reinterpret_cast<const S*>( image.bufferAsVoid( component ) ) );

        if ( ! compImage ) return nullptr;

        caster->SetInput( compImage );
        caster->Update();
        return caster->GetOutput();
    }
    case ComponentType::UInt32:
    {
        using S = uint32_t;
        using InputImageType = itk::Image<S, 3>;
        using FilterType = itk::CastImageFilter< InputImageType, OutputImageType >;
        typename FilterType::Pointer caster = FilterType::New();

        InputImageType::Pointer compImage = makeScalarImage(
            dims, origin, spacing, directions,
            reinterpret_cast<const S*>( image.bufferAsVoid( component ) ) );

        if ( ! compImage ) return nullptr;

        caster->SetInput( compImage );
        caster->Update();
        return caster->GetOutput();
    }
    case ComponentType::Float32:
    {
        using S = float;
        using InputImageType = itk::Image<S, 3>;
        using FilterType = itk::CastImageFilter< InputImageType, OutputImageType >;
        typename FilterType::Pointer caster = FilterType::New();

        InputImageType::Pointer compImage = makeScalarImage(
            dims, origin, spacing, directions,
            reinterpret_cast<const S*>( image.bufferAsVoid( component ) ) );

        if ( ! compImage ) return nullptr;

        caster->SetInput( compImage );
        caster->Update();
        return caster->GetOutput();
    }
    default:
    {
        spdlog::error( "Invalid image component type '{}' upon conversion of component to ITK image",
                      header.memoryComponentTypeAsString() );
        return nullptr;
    }
    }

    return nullptr;
}


/**
 * @brief Compute statistics on one component of an image
 * @tparam T Image component type
 * @tparam U Statistic type
 * @tparam NDim Image dimension
 * @param[in] image Image on which to compute statistics
 * @return Statistics structure for the image component
 */
template< typename T, typename U, uint32_t NDim >
ComponentStats<U> computeImageStatistics( const typename itk::Image<T, NDim>::Pointer image )
{
    using ImageType = itk::Image<T, NDim>;
    using StatisticsFilterType = itk::StatisticsImageFilter<ImageType>;
    using ImageToHistogramFilterType = itk::Statistics::ImageToHistogramFilter<ImageType>;
    using HistogramType = typename ImageToHistogramFilterType::HistogramType;

    auto statsFilter = StatisticsFilterType::New();
    statsFilter->SetInput( image );
    statsFilter->Update();

    static constexpr std::size_t sk_numComponents = 1;
    static constexpr std::size_t sk_numBins = 1001;

    typename HistogramType::SizeType size( sk_numComponents );
    size.Fill( sk_numBins );

    auto histogramFilter = ImageToHistogramFilterType::New();

    // Note: this is another way of setting the min and max histogram bounds:
    // using MeasType = typename ImageToHistogramFilterType::HistogramType::MeasurementVectorType;
    // MeasType lowerBound( sk_numBins );
    // MeasType upperBound( sk_numBins );
    // lowerBound.Fill( statsImageFilter->GetMinimum() );
    // lowerBound.Fill( statsImageFilter->GetMaximum() );
    // histogramFilter->SetHistogramBinMinimum( lowerBound );
    // histogramFilter->SetHistogramBinMaximum( upperBound );

    histogramFilter->SetInput( image );
    histogramFilter->SetAutoMinimumMaximum( true );
    histogramFilter->SetHistogramSize( size );
    histogramFilter->Update();

    ComponentStats<U> stats;
    stats.m_minimum = static_cast<U>( statsFilter->GetMinimum() );
    stats.m_maximum = static_cast<U>( statsFilter->GetMaximum() );
    stats.m_mean = static_cast<U>( statsFilter->GetMean() );
    stats.m_stdDeviation = static_cast<U>( statsFilter->GetSigma() );
    stats.m_variance = static_cast<U>( statsFilter->GetVariance() );
    stats.m_sum = static_cast<U>( statsFilter->GetSum() );

    HistogramType* histogram = histogramFilter->GetOutput();

    if ( ! histogram )
    {
        spdlog::warn( "Filter returned null image histogram" );
        throw_debug( "Unexpected error computing image histogram" )
    }

    auto itr = histogram->Begin();
    const auto end = histogram->End();

    stats.m_histogram.reserve( sk_numBins );

    while ( itr != end )
    {
        stats.m_histogram.push_back( static_cast<double>( itr.GetFrequency() ) );
        ++itr;
    }

    const double B = static_cast<double>( sk_numBins - 1 );

    for ( std::size_t i = 0; i < sk_numBins; ++i )
    {
        stats.m_quantiles[i] = histogram->Quantile( 0, i / B );
    }

    return stats;
}


template< typename T >
std::vector< ComponentStats<T> > computeImageStatistics( const Image& image )
{
    std::vector< ComponentStats<T> > componentStats;

    for ( uint32_t i = 0; i < image.header().numComponentsPerPixel(); ++i )
    {
        switch ( image.header().memoryComponentType() )
        {
        case ComponentType::Int8:
        {
            componentStats.emplace_back( computeImageStatistics<int8_t, T, 3>(
                createItkImageFromImageComponent<int8_t>( image, i ) ) );
            break;
        }
        case ComponentType::UInt8:
        {
            componentStats.emplace_back( computeImageStatistics<uint8_t, T, 3>(
                createItkImageFromImageComponent<uint8_t>( image, i ) ) );
            break;
        }
        case ComponentType::Int16:
        {
            componentStats.emplace_back( computeImageStatistics<int16_t, T, 3>(
                createItkImageFromImageComponent<int16_t>( image, i ) ) );
            break;
        }
        case ComponentType::UInt16:
        {
            componentStats.emplace_back( computeImageStatistics<uint16_t, T, 3>(
                createItkImageFromImageComponent<uint16_t>( image, i ) ) );
            break;
        }
        case ComponentType::Int32:
        {
            componentStats.emplace_back( computeImageStatistics<int32_t, T, 3>(
                createItkImageFromImageComponent<int32_t>( image, i ) ) );
            break;
        }
        case ComponentType::UInt32:
        {
            componentStats.emplace_back( computeImageStatistics<uint32_t, T, 3>(
                createItkImageFromImageComponent<uint32_t>( image, i ) ) );
            break;
        }
        case ComponentType::Float32:
        {
            componentStats.emplace_back( computeImageStatistics<float, T, 3>(
                createItkImageFromImageComponent<float>( image, i ) ) );
            break;
        }
        default:
        {
            spdlog::error( "Invalid component type '{}'",
                componentTypeString( image.header().memoryComponentType() ) );

            throw_debug( "Invalid component type" );
        }
        }
    }

    return componentStats;
}


template< typename T, typename U >
ComponentStats<U> createDefaultImageStatistics( T defaultValue, std::size_t numPixels )
{
    static constexpr std::size_t sk_numBins = 101;

    ComponentStats<U> stats;
    stats.m_minimum = static_cast<U>( defaultValue );
    stats.m_maximum = static_cast<U>( defaultValue );
    stats.m_mean = static_cast<U>( defaultValue );
    stats.m_stdDeviation = static_cast<U>( 0 );
    stats.m_variance = static_cast<U>( 0 );
    stats.m_sum = static_cast<U>( defaultValue * numPixels );
    stats.m_histogram.resize( sk_numBins, 0.0 );

    for ( std::size_t i = 0; i < sk_numBins; ++i )
    {
        stats.m_quantiles[i] = defaultValue;
    }

    return stats;
}


template< class ComponentType, uint32_t NDim >
typename ::itk::Image< ComponentType, NDim >::Pointer
downcastImageBaseToImage( const typename ::itk::ImageBase< NDim >::Pointer& imageBase )
{
    typename ::itk::Image<ComponentType, NDim>::Pointer child =
            dynamic_cast< ::itk::Image<ComponentType, NDim>* >( imageBase.GetPointer() );

    if ( ! child.GetPointer() || child.IsNull() )
    {
        spdlog::error( "Unable to downcast ImageBase to Image with component type {}",
                       typeid( ComponentType ).name() );
        return nullptr;
    }

    return child;
}


template< class ComponentType, uint32_t NDim >
typename ::itk::VectorImage< ComponentType, NDim >::Pointer
downcastImageBaseToVectorImage( const typename ::itk::ImageBase< NDim >::Pointer& imageBase )
{
    typename ::itk::VectorImage< ComponentType, NDim >::Pointer child =
            dynamic_cast< ::itk::VectorImage< ComponentType, NDim > * >( imageBase.GetPointer() );

    if ( ! child.GetPointer() || child.IsNull() )
    {
        spdlog::error( "Unable to downcast ImageBase to VectorImage with component type {}",
                       typeid( ComponentType ).name() );
        return nullptr;
    }

    return child;
}


template< uint32_t NDim >
bool isVectorImage( const typename ::itk::ImageBase<NDim>::Pointer& imageBase )
{
    return ( imageBase && imageBase.IsNotNull() )
            ? ( imageBase->GetNumberOfComponentsPerPixel() > 1 )
            : false;
}


/**
 * @note Data of multi-component (vector) images gets duplicated by this function:
 * one copy pointed to by base class' \c m_imageBasePtr;
 * the other copy pointed to by this class' \c m_splitImagePtrs
 */
template< class ComponentType, uint32_t NDim >
std::vector< typename itk::Image<ComponentType, NDim>::Pointer >
splitImageIntoComponents( const typename ::itk::ImageBase<NDim>::Pointer& imageBase )
{
    using ImageType = itk::Image<ComponentType, NDim>;
    using VectorImageType = itk::VectorImage<ComponentType, NDim>;

    std::vector< typename ImageType::Pointer > splitImages;

    if ( isVectorImage<NDim>( imageBase ) )
    {
        const typename VectorImageType::Pointer vectorImage =
                downcastImageBaseToVectorImage<ComponentType, NDim>( imageBase );

        if ( ! vectorImage || vectorImage.IsNull() )
        {
            spdlog::error( "Error casting ImageBase to vector image" );
            return splitImages;
        }

        const std::size_t numPixels = vectorImage->GetBufferedRegion().GetNumberOfPixels();
        const uint32_t numComponents = vectorImage->GetVectorLength();

        splitImages.resize( numComponents );

        for ( uint32_t i = 0; i < numComponents; ++i )
        {
            splitImages[i] = ImageType::New();
            splitImages[i]->CopyInformation( vectorImage );
            splitImages[i]->SetRegions( vectorImage->GetBufferedRegion() );
            splitImages[i]->Allocate();

            ComponentType* source = vectorImage->GetBufferPointer() + i;
            ComponentType* dest = splitImages[i]->GetBufferPointer();

            const ComponentType* end = dest + numPixels;

            // Copy pixels from component i of vectorImage (the source),
            // which are offset from each other by a stride of numComponents,
            // into pixels of the i'th split image (the destination)
            while ( dest < end )
            {
                *dest = *source;
                ++dest;
                source += numComponents;
            }
        }
    }
    else
    {
        // Image has only one component
        const typename ImageType::Pointer image =
                downcastImageBaseToImage<ComponentType, NDim>( imageBase );

        if ( ! image || image.IsNull() )
        {
            spdlog::error( "Error casting ImageBase to image" );
            return splitImages;
        }

        splitImages.resize( 1 );
        splitImages[0] = image;
    }

    return splitImages;
}


/**
 * @brief Create an Entropy image from an ITK image
 *
 * @todo Finish this function and use it to convert the distance maps (stored as ITK images)
 * into Entropy images.
 *
 * @tparam T Component type of image
 * @param[in] itkImage ITK image
 * @param[in] displayName Image display name
 *
 * @return The created image
 */
template< class T >
Image createImageFromItkImage(
    const typename itk::Image<T, 3>::Pointer itkImage,
    const std::string& displayName )
{
    ImageHeader header;

    Image image( header, displayName,
                 Image::ImageRepresentation::Image,
                 Image::MultiComponentBufferType::SeparateImages,
                 static_cast<void*>( itkImage->GetBufferPointer() ) );

    return image;
}


template< class ComponentType, uint32_t NDim, bool PixelIsVector >
typename itk::ImageBase<NDim>::Pointer readImage( const std::string& fileName )
{
    using ImageType = typename std::conditional< PixelIsVector,
        itk::VectorImage<ComponentType, NDim>,
        itk::Image<ComponentType, NDim> >::type;

    using ReaderType = itk::ImageFileReader<ImageType>;

    try
    {
        auto reader = ReaderType::New();

        if ( ! reader )
        {
            spdlog::error( "Null ITK ImageFileReader on reading image from {}", fileName );
            return nullptr;
        }

        reader->SetFileName( fileName.c_str() );
        reader->Update();
        return static_cast<itk::ImageBase<3>::Pointer>( reader->GetOutput() );
    }
    catch ( const std::exception& e )
    {
        spdlog::error( "Exception reading image from {}: {}", fileName, e.what() );
        return nullptr;
    }
}


template< class T, uint32_t NDim, bool PixelIsVector >
bool writeImage(
    typename itk::Image<T, NDim>::Pointer image,
    const std::string& fileName )
{
    using ImageType = typename std::conditional< PixelIsVector,
        itk::VectorImage<T, NDim>,
        itk::Image<T, NDim> >::type;

    using WriterType = itk::ImageFileWriter<ImageType>;

    if ( ! image )
    {
        spdlog::error( "Null image cannot be written to {}", fileName );
        return false;
    }

    try
    {
        auto writer = WriterType::New();

        if ( ! writer )
        {
            spdlog::error( "Null ITK ImageFileWriter on writing image to '{}'", fileName );
            return false;
        }

        writer->SetFileName( fileName.c_str() );
        writer->SetInput( image );
        writer->Update();
        return true;
    }
    catch ( const std::exception& e )
    {
        spdlog::error( "Exception writing image to '{}': {}", fileName, e.what() );
        return false;
    }
}


/**
 * @brief Compute the signed distance transformation to the boundary of an image.
 * -Voxels inside of the boundary are defined to have negative distance,
 * -Voxels outside of the boundary are defined to have positive distance.
 * -Voxels on the boundary have zero distance.
 *
 * @tparam T Component type of the input image
 * @tparam U Component type of the output distance map image. If U is a signed type,
 * then negative distances are returned for voxels inside the boundary. If U is an
 * unsigned type, then zero distance is returned for voxels inside inside the boundary.
 *
 * @param[in] image Input image
 * @param[in] component Image component
 * @param[in] lowerBoundaryValue Lower value of boundary in input image
 * @param[in] upperBoundaryValue Upper value of boundary in input image
 * @param[in] downsampleFactor Downsampling factor in range (0, 1]. The output distance map
 * will be downsampled by this factor compared to the input image. E.g.: If the factor is 0.25f,
 * then the distance map will be 4x smaller than the input image in each dimension.
 *
 * @return Output distance map image
 */
template< typename T, typename U >
typename itk::Image<U, 3>::Pointer computeEuclideanDistanceMap(
    const typename itk::Image<T, 3>::Pointer image,
    uint32_t component,
    const T& lowerBoundaryValue,
    const T& upperBoundaryValue,
    float downsampleFactor )
{
    using Timer = std::chrono::time_point<std::chrono::system_clock>;

    using InputImageType = itk::Image<T, 3>;
    using DistanceImageType = itk::Image<U, 3>;

    using BoundaryImageType = itk::Image<uint8_t, 3>;
    using FloatImageType = itk::Image<float, 3>;

    typename DistanceImageType::Pointer outputDistanceMap;

    if ( ! image )
    {
        spdlog::error( "Input image is null when computing Euclidean distance transformation" );
        return nullptr;
    }

    float scale = downsampleFactor;

    if ( downsampleFactor <= 0.0f || 1.0f < downsampleFactor )
    {
        spdlog::warn( "Invalid downsampling factor {} provided to Euclidean distance transformation; "
                      "using 1.0 (no downsampling) instead", downsampleFactor );
        scale = 1.0f;
    }


    // Binarize the original image, with values 1 inside and 0 outside
    using ThresholdFilterType = itk::BinaryThresholdImageFilter<InputImageType, InputImageType>;
    typename ThresholdFilterType::Pointer thresholdFilter = ThresholdFilterType::New();

    thresholdFilter->SetInput( image );
    thresholdFilter->SetLowerThreshold( lowerBoundaryValue );
    thresholdFilter->SetUpperThreshold( upperBoundaryValue );
    thresholdFilter->SetOutsideValue( 0 );
    thresholdFilter->SetInsideValue( 1 );


    // Downsample the thresholded boundary image in order to reduce the size of the resulting distance map,
    // especially since the distance map is loaded as a 3D texture on the GPU
    const typename InputImageType::RegionType inputRegion = image->GetLargestPossibleRegion();
    const typename InputImageType::SizeType inputSize = inputRegion.GetSize();
    const typename InputImageType::SpacingType inputSpacing = image->GetSpacing();
    const typename InputImageType::PointType inputOrigin = image->GetOrigin();

    typename InputImageType::SizeType outputSize;
    typename InputImageType::SpacingType outputSpacing;
    typename InputImageType::PointType outputOrigin;

    for ( uint32_t i = 0; i < 3; ++i )
    {
        // 1 is the minimum value for any dimension:
        outputSize[i] = std::max( static_cast<std::size_t>( inputSize[i] * scale ), static_cast<std::size_t>(1ul) );

        // Adjust the scale factor
        scale = std::max( scale, static_cast<float>( outputSize[i] ) / static_cast<float>( inputSize[i] ) );
    }

    for ( uint32_t i = 0; i < 3; ++i )
    {
        outputSpacing[i] = inputSpacing[i] / scale;
        outputOrigin[i] = inputOrigin[i] + 0.5 * ( outputSpacing[i] - inputSpacing[i] );
    }


    using CoordType = double;
    using LinearInterpolatorType = itk::LinearInterpolateImageFunction<InputImageType, CoordType>;
    typename LinearInterpolatorType::Pointer interpolator = LinearInterpolatorType::New();


    // Resample to floating point image type, so that partial voluming can be correctly resolved
    // with a subsequent ceiling filter
    using ResampleFilterType = itk::ResampleImageFilter<InputImageType, FloatImageType>;
    typename ResampleFilterType::Pointer resampleFilter = ResampleFilterType::New();

    resampleFilter->SetInput( thresholdFilter->GetOutput() );
    resampleFilter->SetInterpolator( interpolator );
    resampleFilter->SetSize( outputSize );
    resampleFilter->SetOutputSpacing( outputSpacing );
    resampleFilter->SetOutputOrigin( outputOrigin );
    resampleFilter->SetOutputDirection( image->GetDirection() );
    resampleFilter->SetDefaultPixelValue( 0 );


    // Compute the ceiling of the resampled values, so that any value even slightly larger than
    // zero gets mapped to one (inside the boundary). That way the boundary is never underestimated.
    using CeilFilterType = itk::BinaryThresholdImageFilter<FloatImageType, BoundaryImageType>;
    typename CeilFilterType::Pointer ceilFilter = CeilFilterType::New();

    ceilFilter->SetInput( resampleFilter->GetOutput() );
    ceilFilter->SetLowerThreshold( 0.0f );
    ceilFilter->SetUpperThreshold( 0.0f );

    ceilFilter->SetOutsideValue( 1 );
    ceilFilter->SetInsideValue( 0 );

    const Timer startThreshold = std::chrono::system_clock::now();
    ceilFilter->Update();
    const Timer stopThreshold = std::chrono::system_clock::now();

    spdlog::debug( "Took {} msec to compute image threshold, resampling, and ceiling",
                   std::chrono::duration_cast<std::chrono::milliseconds>( stopThreshold - startThreshold ).count() );


    // Compute the distance map in mm from every voxel to the boundary. Distances are computed for
    // voxels that are both inside and outside the boundary.
    using DistanceFilterType = itk::SignedMaurerDistanceMapImageFilter<BoundaryImageType, FloatImageType>;
    typename DistanceFilterType::Pointer distanceFilter = DistanceFilterType::New();

    distanceFilter->SetInput( ceilFilter->GetOutput() );
    distanceFilter->UseImageSpacingOn();
    distanceFilter->SquaredDistanceOff();

    const Timer startDistance = std::chrono::system_clock::now();
    distanceFilter->Update();
    const Timer stopDistance = std::chrono::system_clock::now();

    spdlog::debug( "Took {} msec to compute distance map to resampled boundary",
                   std::chrono::duration_cast<std::chrono::milliseconds>( stopDistance - startDistance ).count() );


    auto distImage = distanceFilter->GetOutput();

    // If casting to an integral type, then ceil negative values and floor positive values.
    // This is so that distance to the boundary is never overestimated in the returned image.

    if ( std::is_integral<U>() )
    {
        using IteratorType = itk::ImageRegionIterator<FloatImageType>;
        IteratorType it( distImage, distImage->GetLargestPossibleRegion() );

        it.GoToBegin();

        while ( ! it.IsAtEnd() )
        {
            const float d = it.Get();
            it.Set( ( d < 0.0f ) ? std::ceil( d ) : std::floor( d ) );
            ++it;
        }
    }


    // Clamp and cast pixels to the range of the output image type DistanceImageType (U).
    // Note that by default, the clamp bounds equal the range supported by type U.
    using ClampFilterType = itk::ClampImageFilter<FloatImageType, DistanceImageType>;
    typename ClampFilterType::Pointer clampFilter = ClampFilterType::New();

    clampFilter->SetInput( distImage );
    clampFilter->Update();


    if ( DEBUG_IMAGE_OUTPUT )
    {
        const std::string ending = "_" + std::to_string( component ) + ".nii.gz";

        writeImage<T,3,false>( image, "0.image.nii.gz" );
        writeImage<T,3,false>( thresholdFilter->GetOutput(), "1.thresh" + ending );
        writeImage<float,3,false>( resampleFilter->GetOutput(), "2.resample" + ending );
        writeImage<uint8_t,3,false>( ceilFilter->GetOutput(), "3.ceiling" + ending );
        writeImage<float,3,false>( distanceFilter->GetOutput(), "4.distance" + ending );
        writeImage<U,3,false>( clampFilter->GetOutput(), "5.distance_clamp" + ending );
    }

    return clampFilter->GetOutput();
}


template< class ComponentType >
vtkSmartPointer< vtkImageData > convertItkImageToVtkImageData(
    const typename ::itk::Image< ComponentType, 3 >::Pointer image )
{
    if ( image.IsNull() )
    {
        return nullptr;
    }

    using ConversionFilterType = ::itk::ImageToVTKImageFilter< ::itk::Image< ComponentType, 3 > >;
    typename ConversionFilterType::Pointer conversionFilter = ConversionFilterType::New();

    conversionFilter->SetInput( image );

    try
    {
        conversionFilter->Update();
    }
    catch ( const ::itk::ExceptionObject& e )
    {
        spdlog::error( "Exception while converting ITK image to VTK image: {}", e.what() );
        return nullptr;
    }
    catch ( ... )
    {
        spdlog::error( "Exception while converting ITK image to VTK image." );
        return nullptr;
    }

    return static_cast< vtkImageData* >( conversionFilter->GetOutput() );
}


#endif // IMAGE_UTILITY_TPP
