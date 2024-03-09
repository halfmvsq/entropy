#include "image/Image.h"
#include "image/ImageUtility.h"
#include "image/ImageCastHelper.tpp"
#include "image/ImageUtility.tpp"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <algorithm>
#include <array>
#include <memory>


namespace
{

// Statistics per component are stored as double
using StatsType = double;

// Maximum number of components to load for images with interleaved buffer components
static constexpr uint32_t MAX_INTERLEAVED_COMPS = 4;

}


Image::Image(
    const fs::path& fileName,
    ImageRepresentation imageRep,
    MultiComponentBufferType bufferType )
    :
      m_data_int8(),
      m_data_uint8(),
      m_data_int16(),
      m_data_uint16(),
      m_data_int32(),
      m_data_uint32(),
      m_data_float32(),

      m_imageRep( std::move(imageRep) ),
      m_bufferType( std::move(bufferType) ),

      m_ioInfoOnDisk(),
      m_ioInfoInMemory()
{
    // Read all data from disk to an ITK image with 32-bit floating point pixel components
    using ReadComponentType = float;
    using ComponentImageType = ::itk::Image<ReadComponentType, 3>;

    const itk::ImageIOBase::Pointer imageIo = createStandardImageIo( fileName.c_str() );

    if ( ! imageIo || imageIo.IsNull() )
    {
        spdlog::error( "Error creating ImageIOBase for image from file {}", fileName );
        throw_debug( "Error creating ImageIOBase" )
    }

    if ( ! m_ioInfoOnDisk.set( imageIo ) )
    {
        spdlog::error( "Error setting image IO information for image from file {}", fileName );
        throw_debug( "Error setting image IO information" )
    }

    // The image information in memory may not match the information on disk
    m_ioInfoInMemory = m_ioInfoOnDisk;

    // Source and destination component ITK types
    static constexpr ::itk::IOComponentEnum srcItkCompType = ::itk::IOComponentEnum::FLOAT;
    const ::itk::IOComponentEnum dstItkCompType = m_ioInfoInMemory.m_componentInfo.m_componentType;

    const std::size_t numPixels = m_ioInfoOnDisk.m_sizeInfo.m_imageSizeInPixels;
    const uint32_t numComps = m_ioInfoOnDisk.m_pixelInfo.m_numComponents;
    const bool isVectorImage = ( numComps > 1 );

    spdlog::info( "Attempting to load image from {} with {} pixels and {} components per pixel",
                  fileName, numPixels, numComps );

    if ( isVectorImage )
    {
        // Load multi-component image
        typename itk::ImageBase<3>::Pointer baseImage = readImage<ReadComponentType, 3, true>( fileName );

        if ( ! baseImage )
        {
            spdlog::error( "Unable to read vector ImageBase from file {}", fileName );
            throw_debug( "Unable to read vector image" )
        }

        // Split the base image into component images. Load a maximum of MAX_COMPS components
        // for an image with interleaved component buffers.
        uint32_t numCompsToLoad = numComps;

        if ( MultiComponentBufferType::InterleavedImage == m_bufferType )
        {
            numCompsToLoad = std::min( numCompsToLoad, MAX_INTERLEAVED_COMPS );

            if ( numComps > MAX_INTERLEAVED_COMPS )
            {
                spdlog::warn( "The number of image components ({}) exceeds that maximum that will be loaded ({}) "
                              "because this image uses interleaved buffer format", numComps, MAX_INTERLEAVED_COMPS );
            }
        }

        std::vector< typename ComponentImageType::Pointer > componentImages =
            splitImageIntoComponents<ReadComponentType, 3>( baseImage );

        if ( componentImages.size() < numCompsToLoad )
        {
            spdlog::error( "Only {} component images were loaded, but {} components were expected",
                           componentImages.size(), numCompsToLoad );
            numCompsToLoad = componentImages.size();
        }

        if ( ImageRepresentation::Segmentation == m_imageRep )
        {
            spdlog::warn( "Loading a segmentation image from {} with {} components. "
                          "Only the first component of the segmentation will be used", fileName, numComps );
            numCompsToLoad = 1;
        }

        // Adjust the number of components in the image header
        m_header.setNumComponentsPerPixel( numCompsToLoad );

        if ( 0 == numCompsToLoad )
        {
            spdlog::error( "No components to load for image from file {}", fileName );
            throw_debug( "No components to load for image" )
        }

        // If interleaving vector components, then create a single buffer
        std::unique_ptr<ReadComponentType[]> allComponentBuffers = nullptr;

        if ( MultiComponentBufferType::InterleavedImage == m_bufferType )
        {
            allComponentBuffers = std::make_unique<ReadComponentType[]>( numPixels * numCompsToLoad );

            if ( ! allComponentBuffers )
            {
                spdlog::error( "Null buffer holding all components of image from file {}", fileName );
                throw_debug( "Null image buffer" )
            }
        }

        // Load the buffers from the component images
        for ( uint32_t i = 0; i < numCompsToLoad; ++i )
        {
            if ( ! componentImages[i] )
            {
                spdlog::error( "Null vector image component {} for image from file {}", i, fileName );
                throw_debug( "Null vector component for image" )
            }

            const ReadComponentType* buffer = componentImages[i]->GetBufferPointer();

            if ( ! buffer )
            {
                spdlog::error( "Null buffer of vector image component {} for image from file {}", i, fileName );
                throw_debug( "Null buffer of vector image component" )
            }

            switch ( m_bufferType )
            {
            case MultiComponentBufferType::SeparateImages:
            {
                if ( ImageRepresentation::Segmentation == m_imageRep )
                {
                    if ( ! loadSegBuffer( static_cast<const void*>( buffer ), numPixels, srcItkCompType, dstItkCompType ) )
                    {
                        spdlog::error( "Error loading segmentation image buffer for file {}", fileName );
                        throw_debug( "Error loading segmentation image buffer" )
                    }
                }
                else
                {
                    if ( ! loadImageBuffer( static_cast<const void*>( buffer ), numPixels, srcItkCompType, dstItkCompType ) )
                    {
                        spdlog::error( "Error loading image buffer for file {}", fileName );
                        throw_debug( "Error loading image buffer" )
                    }
                }
                break;
            }
            case MultiComponentBufferType::InterleavedImage:
            {
                // Fill the interleaved buffer
                for ( std::size_t p = 0; p < numPixels; ++p )
                {
                    allComponentBuffers[numComps*p + i] = buffer[p];
                }
                break;
            }
            }
        }

        if ( MultiComponentBufferType::InterleavedImage == m_bufferType )
        {
            const std::size_t numElements = numPixels * numComps;

            if ( ImageRepresentation::Segmentation == m_imageRep )
            {
                if ( ! loadSegBuffer( static_cast<const void*>( allComponentBuffers.get() ),
                                   numElements, srcItkCompType, dstItkCompType ) )
                {
                    spdlog::error( "Error loading segmentation image buffer for file {}", fileName );
                    throw_debug( "Error loading segmentation image buffer" )
                }
            }
            else
            {
                if ( ! loadImageBuffer( static_cast<const void*>( allComponentBuffers.get() ),
                                     numElements, srcItkCompType, dstItkCompType ) )
                {
                    spdlog::error( "Error loading image buffer for file {}", fileName );
                    throw_debug( "Error loading image buffer" )
                }
            }
        }
    }
    else
    {
        // Load scalar, single-component image
        typename itk::ImageBase<3>::Pointer baseImage = readImage<ReadComponentType, 3, false>( fileName );

        if ( ! baseImage )
        {
            spdlog::error( "Unable to read ImageBase from file {}", fileName );
            throw_debug( "Unable to read image" )
        }

        typename ComponentImageType::Pointer image = downcastImageBaseToImage<ReadComponentType, 3>( baseImage );

        if ( ! image )
        {
            spdlog::error( "Null image for file {} following downcast from ImageBase", fileName );
            throw_debug( "Null image" )
        }

        const ReadComponentType* buffer = image->GetBufferPointer();

        if ( ! buffer )
        {
            spdlog::error( "Null buffer of scalar image from file {}", fileName );
            throw_debug( "Null buffer of scalar image" )
        }

        if ( ImageRepresentation::Segmentation == m_imageRep )
        {
            if ( ! loadSegBuffer( static_cast<const void*>( buffer ), numPixels, srcItkCompType, dstItkCompType ) )
            {
                spdlog::error( "Error loading segmentation image buffer for file {}", fileName );
                throw_debug( "Error loading segmentation image buffer" )
            }
        }
        else
        {
            if ( ! loadImageBuffer( static_cast<const void*>( buffer ), numPixels, srcItkCompType, dstItkCompType ) )
            {
                spdlog::error( "Error loading image buffer for file {}", fileName );
                throw_debug( "Error loading image buffer" )
            }
        }
    }

    m_header = ImageHeader(
        m_ioInfoOnDisk, m_ioInfoInMemory, ( MultiComponentBufferType::InterleavedImage == m_bufferType ) );

    m_headerOverrides = ImageHeaderOverrides(
        m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions() );

    m_tx = ImageTransformations(
        m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions() );

    m_settings = ImageSettings(
        getFileName( fileName, false ), m_header.numComponentsPerPixel(),
        m_header.memoryComponentType(), computeImageStatistics<StatsType>( *this ) );
}


Image::Image(
    const ImageHeader& header,
    std::string displayName,
    ImageRepresentation imageRep,
    MultiComponentBufferType bufferType,
    const std::vector<const void*> imageDataComponents )
    :
    m_data_int8(),
    m_data_uint8(),
    m_data_int16(),
    m_data_uint16(),
    m_data_int32(),
    m_data_uint32(),
    m_data_float32(),

    m_imageRep( std::move(imageRep) ),
    m_bufferType( std::move(bufferType) ),

    m_ioInfoOnDisk(),
    m_ioInfoInMemory(),

    m_header( header )
{
    if ( imageDataComponents.empty() )
    {
        spdlog::error( "No image data buffers provided for constructing Image" );
        throw_debug( "No image data buffers provided for constructing Image" );
    }

    // The image does not exist on disk, but we need to fill this out anyway:
    m_ioInfoOnDisk.m_fileInfo.m_fileName = m_header.fileName();
    m_ioInfoOnDisk.m_componentInfo.m_componentType = toItkComponentType( m_header.memoryComponentType() );
    m_ioInfoOnDisk.m_componentInfo.m_componentTypeString = m_header.memoryComponentTypeAsString();

    m_ioInfoInMemory = m_ioInfoOnDisk;

    // Source and destination component ITK types
    using CType = ::itk::IOComponentEnum;
    const CType srcItkCompType = m_ioInfoInMemory.m_componentInfo.m_componentType;
    CType dstItkCompType = ::itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE;

    switch ( srcItkCompType )
    {
    case CType::UCHAR: dstItkCompType = CType::UCHAR; break;
    case CType::CHAR: dstItkCompType = CType::CHAR; break;
    case CType::USHORT: dstItkCompType = CType::USHORT; break;
    case CType::SHORT: dstItkCompType = CType::SHORT; break;
    case CType::UINT: dstItkCompType = CType::UINT; break;
    case CType::INT: dstItkCompType = CType::INT; break;
    case CType::FLOAT: dstItkCompType = CType::FLOAT; break;

    case CType::ULONG:
    case CType::ULONGLONG: dstItkCompType = CType::UINT; break;

    case CType::LONG:
    case CType::LONGLONG: dstItkCompType = CType::INT; break;

    case CType::DOUBLE:
    case CType::LDOUBLE: dstItkCompType = CType::FLOAT; break;

    case CType::UNKNOWNCOMPONENTTYPE:
    {
        spdlog::error( "Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName );
        throw_debug( "Unknown component type in image" )
    }
    }

    const std::size_t numPixels = m_header.numPixels();
    const uint32_t numComps = m_header.numComponentsPerPixel();
    const bool isVectorImage = ( numComps > 1 );

    if ( isVectorImage )
    {
        // Create multi-component image
        uint32_t numCompsToLoad = numComps;

        if ( MultiComponentBufferType::InterleavedImage == m_bufferType )
        {
            // Set a maximum of MAX_COMPS components
            numCompsToLoad = std::min( numCompsToLoad, MAX_INTERLEAVED_COMPS );

            if ( numComps > MAX_INTERLEAVED_COMPS )
            {
                spdlog::warn( "The number of image components ({}) exceeds that maximum that will be created ({}) "
                              "because this image uses interleaved buffer format", numComps, MAX_INTERLEAVED_COMPS );
            }
        }

        if ( ImageRepresentation::Segmentation == m_imageRep )
        {
            spdlog::warn( "Attempting to create a segmentation image with {} components", numComps );
            spdlog::warn( "Only one component of the segmentation image will be created" );
            numCompsToLoad = 1;
        }

        if ( 0 == numCompsToLoad )
        {
            spdlog::error( "No components to create for image from file {}", m_header.fileName() );
            throw_debug( "No components to create for image" )
        }

        // Adjust the number of components in the image header
        m_header.setNumComponentsPerPixel( numCompsToLoad );


        switch ( m_bufferType )
        {
        case MultiComponentBufferType::SeparateImages:
        {
            // Load each component separately:
            if ( imageDataComponents.size() < m_header.numComponentsPerPixel() )
            {
                spdlog::error( "Insufficient number of image data buffers provided: {}", imageDataComponents.size() );
                throw_debug( "Insufficient number of image data buffers were provided" );
            }

            for ( std::size_t c = 0; c < m_header.numComponentsPerPixel(); ++c )
            {
                if ( ImageRepresentation::Segmentation == m_imageRep )
                {
                    if ( ! loadSegBuffer( imageDataComponents[c], numPixels, srcItkCompType, dstItkCompType ) )
                    {
                        throw_debug( "Error loading segmentation image buffer" )
                    }
                }
                else
                {
                    if ( ! loadImageBuffer( imageDataComponents[c], numPixels, srcItkCompType, dstItkCompType ) )
                    {
                        throw_debug( "Error loading image buffer" )
                    }
                }
            }
            break;
        }
        case MultiComponentBufferType::InterleavedImage:
        {
            // Load a single buffer with interleaved components:
            const std::size_t N = numPixels * numComps;

            if ( ImageRepresentation::Segmentation == m_imageRep )
            {
                if ( ! loadSegBuffer( imageDataComponents[0], N, srcItkCompType, dstItkCompType ) )
                {
                    throw_debug( "Error loading segmentation image buffer" )
                }
            }
            else
            {
                if ( ! loadImageBuffer( imageDataComponents[0], N, srcItkCompType, dstItkCompType ) )
                {
                    throw_debug( "Error loading image buffer" )
                }
            }
            break;
        }
        }
    }
    else
    {
        if ( ImageRepresentation::Segmentation == m_imageRep )
        {
            if ( ! loadSegBuffer( imageDataComponents[0], numPixels, srcItkCompType, dstItkCompType ) )
            {
                throw_debug( "Error loading segmentation image buffer" )
            }
        }
        else
        {
            if ( ! loadImageBuffer( imageDataComponents[0], numPixels, srcItkCompType, dstItkCompType ) )
            {
                throw_debug( "Error loading image buffer" )
            }
        }
    }

    m_tx = ImageTransformations(
        m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions() );

    m_headerOverrides = ImageHeaderOverrides(
        m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions() );

    m_settings = ImageSettings(
        std::move( displayName ), m_header.numComponentsPerPixel(),
        m_header.memoryComponentType(), computeImageStatistics<StatsType>( *this ) );
}


bool Image::saveComponentToDisk( uint32_t component, const std::optional<fs::path>& newFileName )
{
    constexpr uint32_t DIM = 3;
    constexpr bool s_isVectorImage = false;

    const fs::path fileName = ( newFileName ) ? *newFileName : m_header.fileName();

    if ( component >= m_header.numComponentsPerPixel() )
    {
        spdlog::error( "Invalid image component {} to save to disk; image has only {} components",
                       component, m_header.numComponentsPerPixel() );
        return false;
    }

    std::array< uint32_t, DIM > dims;
    std::array< double, DIM > origin;
    std::array< double, DIM > spacing;
    std::array< std::array<double, DIM>, DIM > directions;

    for ( uint32_t i = 0; i < DIM; ++i )
    {
        const int ii = static_cast<int>(i);

        dims[i] = m_header.pixelDimensions()[ii];
        origin[i] = static_cast<double>( m_header.origin()[ii] );
        spacing[i] = static_cast<double>( m_header.spacing()[ii] );

        directions[i] = {
            static_cast<double>( m_header.directions()[ii].x ),
            static_cast<double>( m_header.directions()[ii].y ),
            static_cast<double>( m_header.directions()[ii].z )
        };
    }

    bool saved = false;

    switch ( m_header.memoryComponentType() )
    {
    case ComponentType::Int8:
    {
        auto image = makeScalarImage( dims, origin, spacing, directions, m_data_int8[component].data() );
        saved = writeImage<int8_t, DIM, s_isVectorImage>( image, fileName );
        break;
    }
    case ComponentType::UInt8:
    {
        auto image = makeScalarImage( dims, origin, spacing, directions, m_data_uint8[component].data() );
        saved = writeImage<uint8_t, DIM, s_isVectorImage>( image, fileName );
        break;
    }
    case ComponentType::Int16:
    {
        auto image = makeScalarImage( dims, origin, spacing, directions, m_data_int16[component].data() );
        saved = writeImage<int16_t, DIM, s_isVectorImage>( image, fileName );
        break;
    }
    case ComponentType::UInt16:
    {
        auto image = makeScalarImage( dims, origin, spacing, directions, m_data_uint16[component].data() );
        saved = writeImage<uint16_t, DIM, s_isVectorImage>( image, fileName );
        break;
    }
    case ComponentType::Int32:
    {
        auto image = makeScalarImage( dims, origin, spacing, directions, m_data_int32[component].data() );
        saved = writeImage<int32_t, DIM, s_isVectorImage>( image, fileName );
        break;
    }
    case ComponentType::UInt32:
    {
        auto image = makeScalarImage( dims, origin, spacing, directions, m_data_uint32[component].data() );
        saved = writeImage<uint32_t, DIM, s_isVectorImage>( image, fileName );
        break;
    }
    case ComponentType::Float32:
    {
        auto image = makeScalarImage( dims, origin, spacing, directions, m_data_float32[component].data() );
        saved = writeImage<float, DIM, s_isVectorImage>( image, fileName );
        break;
    }
    default:
    {
        saved = false;
        break;
    }
    }

    return saved;
}


bool Image::loadImageBuffer(
    const void* buffer,
    std::size_t numElements,
    const itk::IOComponentEnum& srcComponentType,
    const itk::IOComponentEnum& dstComponentType )
{
    using CType = ::itk::ImageIOBase::IOComponentType;

    bool didCast = false;
    bool warnSizeConversion = false;

    switch ( dstComponentType )
    {
    case CType::UCHAR: m_data_uint8.emplace_back( createBuffer<uint8_t>( buffer, numElements, srcComponentType ) ); break;
    case CType::CHAR: m_data_int8.emplace_back( createBuffer<int8_t>( buffer, numElements, srcComponentType ) ); break;
    case CType::USHORT: m_data_uint16.emplace_back( createBuffer<uint16_t>( buffer, numElements, srcComponentType ) ); break;
    case CType::SHORT: m_data_int16.emplace_back( createBuffer<int16_t>( buffer, numElements, srcComponentType ) ); break;
    case CType::UINT: m_data_uint32.emplace_back( createBuffer<uint32_t>( buffer, numElements, srcComponentType ) ); break;
    case CType::INT: m_data_int32.emplace_back( createBuffer<int32_t>( buffer, numElements, srcComponentType ) ); break;
    case CType::FLOAT: m_data_float32.emplace_back( createBuffer<float>( buffer, numElements, srcComponentType ) ); break;

    case CType::ULONG:
    case CType::ULONGLONG:
    {
        m_data_uint32.emplace_back( createBuffer<uint32_t>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

        didCast = true;
        warnSizeConversion = true;
        break;
    }

    case CType::LONG:
    case CType::LONGLONG:
    {
        m_data_int32.emplace_back( createBuffer<int32_t>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::INT;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

        didCast = true;
        warnSizeConversion = true;
        break;
    }

    case CType::DOUBLE:
    case CType::LDOUBLE:
    {
        m_data_float32.emplace_back( createBuffer<float>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::FLOAT;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

        didCast = true;
        warnSizeConversion = true;
        break;
    }

    case CType::UNKNOWNCOMPONENTTYPE:
    {
        spdlog::error( "Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName );
        return false;
    }
    }

    if ( didCast )
    {
        const std::string newTypeString = itk::ImageIOBase::GetComponentTypeAsString(
            m_ioInfoInMemory.m_componentInfo.m_componentType );

        m_ioInfoInMemory.m_componentInfo.m_componentTypeString = newTypeString;

        m_ioInfoInMemory.m_sizeInfo.m_imageSizeInBytes =
            numElements * m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes;

        spdlog::info( "Cast image pixel component from type {} to {}",
                     m_ioInfoOnDisk.m_componentInfo.m_componentTypeString, newTypeString );

        if ( warnSizeConversion )
        {
            spdlog::warn(
                "Size conversion: "
                "Possible loss of information when casting image pixel component from type {} to {}",
                m_ioInfoOnDisk.m_componentInfo.m_componentTypeString, newTypeString );
        }
    }

    return true;
}


bool Image::loadSegBuffer(
    const void* buffer, std::size_t numElements,
    const itk::IOComponentEnum& srcComponentType,
    const itk::IOComponentEnum& dstComponentType )
{
    using CType = ::itk::ImageIOBase::IOComponentType;

    bool didCast = false;
    bool warnFloatConversion = false;
    bool warnSizeConversion = false;
    bool warnSignConversion = false;

    switch ( dstComponentType )
    {
    // No casting is needed for the cases of unsigned integers with 8, 16, or 32 bytes:
    case CType::UCHAR:
    {
        m_data_uint8.emplace_back( createBuffer<uint8_t>( buffer, numElements, srcComponentType ) );
        break;
    }
    case CType::USHORT:
    {
        m_data_uint16.emplace_back( createBuffer<uint16_t>( buffer, numElements, srcComponentType ) );
        break;
    }
    case CType::UINT:
    {
        m_data_uint32.emplace_back( createBuffer<uint32_t>( buffer, numElements, srcComponentType ) );
        break;
    }

        // Signed 8-, 16-, and 32-bit integers are cast to unsigned 8-, 16-, and 32-bit integers:
    case CType::CHAR:
    {
        m_data_uint8.emplace_back( createBuffer<uint8_t>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UCHAR;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 1;

        didCast = true;
        warnSignConversion = true;
        break;
    }
    case CType::SHORT:
    {
        m_data_uint16.emplace_back( createBuffer<uint16_t>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::USHORT;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 2;

        didCast = true;
        warnSignConversion = true;
        break;
    }
    case CType::INT:
    {
        m_data_uint32.emplace_back( createBuffer<uint32_t>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

        didCast = true;
        warnSignConversion = true;
        break;
    }

        // Unsigned long (64-bit) and long long (128-bit) integers are cast to unsigned 32-bit integers:
    case CType::ULONG:
    case CType::ULONGLONG:
    {
        m_data_uint32.emplace_back( createBuffer<uint32_t>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

        didCast = true;
        warnSizeConversion = true;
        break;
    }

        // Signed long (64-bit) and long long (128-bit) integers are cast to unsigned 32-bit integers:
    case CType::LONG:
    case CType::LONGLONG:
    {
        m_data_uint32.emplace_back( createBuffer<uint32_t>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

        didCast = true;
        warnSizeConversion = true;
        warnSignConversion = true;
        break;
    }

        // Floating-points are cast to unsigned 32-bit integers:
    case CType::FLOAT:
    case CType::DOUBLE:
    case CType::LDOUBLE:
    {
        m_data_uint32.emplace_back( createBuffer<uint32_t>( buffer, numElements, srcComponentType ) );
        m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
        m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

        didCast = true;
        warnFloatConversion = true;
        warnSignConversion = true;
        break;
    }

    case CType::UNKNOWNCOMPONENTTYPE:
    {
        spdlog::error( "Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName );
        return false;
    }
    }

    if ( didCast )
    {
        const std::string newTypeString = itk::ImageIOBase::GetComponentTypeAsString(
            m_ioInfoInMemory.m_componentInfo.m_componentType );

        m_ioInfoInMemory.m_componentInfo.m_componentTypeString = newTypeString;

        m_ioInfoInMemory.m_sizeInfo.m_imageSizeInBytes =
            numElements * m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes;

        spdlog::info(
            "Casted segmentation '{}' pixel component from type {} to {}",
            m_ioInfoOnDisk.m_fileInfo.m_fileName,
            m_ioInfoOnDisk.m_componentInfo.m_componentTypeString, newTypeString );

        if ( warnFloatConversion )
        {
            spdlog::warn(
                "Floating point to integer conversion: "
                "Possible loss of precision and information when casting segmentation pixel component from type {} to {}",
                m_ioInfoOnDisk.m_componentInfo.m_componentTypeString, newTypeString );
        }

        if ( warnSizeConversion )
        {
            spdlog::warn(
                "Size conversion: "
                "Possible loss of information when casting segmentation pixel component from type {} to {}",
                m_ioInfoOnDisk.m_componentInfo.m_componentTypeString, newTypeString );
        }

        if ( warnSignConversion )
        {
            spdlog::warn(
                "Signed to unsigned integer conversion: "
                "Possible loss of information when casting segmentation pixel component from type {} to {}",
                m_ioInfoOnDisk.m_componentInfo.m_componentTypeString, newTypeString );
        }
    }

    return true;
}


const Image::ImageRepresentation& Image::imageRep() const { return m_imageRep; }
const Image::MultiComponentBufferType& Image::bufferType() const { return m_bufferType; }

const ImageHeader& Image::header() const { return m_header; }
ImageHeader& Image::header() { return m_header; }

const ImageTransformations& Image::transformations() const { return m_tx; }
ImageTransformations& Image::transformations() { return m_tx; }

const ImageSettings& Image::settings() const { return m_settings; }
ImageSettings& Image::settings() { return m_settings; }


const void* Image::bufferAsVoid( uint32_t comp ) const
{
    auto F = [this] (uint32_t i) -> const void*
    {
        switch ( m_header.memoryComponentType() )
        {
        case ComponentType::Int8: return static_cast<const void*>( m_data_int8.at(i).data() );
        case ComponentType::UInt8: return static_cast<const void*>( m_data_uint8.at(i).data() );
        case ComponentType::Int16: return static_cast<const void*>( m_data_int16.at(i).data() );
        case ComponentType::UInt16: return static_cast<const void*>( m_data_uint16.at(i).data() );
        case ComponentType::Int32: return static_cast<const void*>( m_data_int32.at(i).data() );
        case ComponentType::UInt32: return static_cast<const void*>( m_data_uint32.at(i).data() );
        case ComponentType::Float32: return static_cast<const void*>( m_data_float32.at(i).data() );
        default: return static_cast<const void*>( nullptr );
        }
    };

    switch ( m_bufferType )
    {
    case MultiComponentBufferType::SeparateImages:
    {
        if ( m_header.numComponentsPerPixel() <= comp )
        {
            return nullptr;
        }
        return F( comp );
    }
    case MultiComponentBufferType::InterleavedImage:
    {
        if ( 1 <= comp )
        {
            return nullptr;
        }
        return F( 0 );
    }
    }

    return nullptr;
}


void* Image::bufferAsVoid( uint32_t comp )
{
    return const_cast<void*>( const_cast<const Image*>(this)->bufferAsVoid( comp ) );
}

std::optional< std::pair< std::size_t, std::size_t > >
Image::getComponentAndOffsetForBuffer( uint32_t comp, int i, int j, int k ) const
{
    const glm::u64vec3 dims = m_header.pixelDimensions();

    const std::size_t index =
        dims.x * dims.y * static_cast<std::size_t>(k) +
        dims.x * static_cast<std::size_t>(j) +
        static_cast<std::size_t>(i);

    return getComponentAndOffsetForBuffer( comp, index );
}

std::optional< std::pair< std::size_t, std::size_t > >
Image::getComponentAndOffsetForBuffer( uint32_t comp, std::size_t index ) const
{
    // 1) component buffer to index
    // 2) offset into that buffer
    std::optional< std::pair< std::size_t, std::size_t > > ret;

    const auto ncomps = m_header.numComponentsPerPixel();

    if ( comp > ncomps )
    {
        // Invalid image component requested
        return std::nullopt;
    }

    // Component to index the image buffer at:
    std::size_t c = 0;

    // Offset into the buffer:
    std::size_t offset = index;

    switch ( m_bufferType )
    {
    case MultiComponentBufferType::SeparateImages:
    {
        c = comp;
        break;
    }
    case MultiComponentBufferType::InterleavedImage:
    {
        // There is just one buffer (0) that holds all components:
        c = 0;

        // Offset into the buffer accounts for the desired component:
        offset = static_cast<std::size_t>( comp + 1 ) * offset;
        break;
    }
    }

    ret = { c, offset };
    return ret;
}

void Image::setUseIdentityPixelSpacings( bool identitySpacings )
{
    m_headerOverrides.m_useIdentityPixelSpacings = identitySpacings;

    m_header.setHeaderOverrides( m_headerOverrides );
    m_tx.setHeaderOverrides( m_headerOverrides );
}

bool Image::getUseIdentityPixelSpacings() const
{
    return m_headerOverrides.m_useIdentityPixelSpacings;
}

void Image::setUseZeroPixelOrigin( bool zeroOrigin )
{
    m_headerOverrides.m_useZeroPixelOrigin = zeroOrigin;

    m_header.setHeaderOverrides( m_headerOverrides );
    m_tx.setHeaderOverrides( m_headerOverrides );
}

bool Image::getUseZeroPixelOrigin() const
{
    return m_headerOverrides.m_useZeroPixelOrigin;
}

void Image::setUseIdentityPixelDirections( bool useIdentity )
{
    m_headerOverrides.m_useIdentityPixelDirections = useIdentity;

    m_header.setHeaderOverrides( m_headerOverrides );
    m_tx.setHeaderOverrides( m_headerOverrides );
}

bool Image::getUseIdentityPixelDirections() const
{
    return m_headerOverrides.m_useIdentityPixelDirections;
}

void Image::setSnapToClosestOrthogonalPixelDirections( bool snap )
{
    m_headerOverrides.m_snapToClosestOrthogonalPixelDirections = snap;

    m_header.setHeaderOverrides( m_headerOverrides );
    m_tx.setHeaderOverrides( m_headerOverrides );
}

bool Image::getSnapToClosestOrthogonalPixelDirections() const
{
    return m_headerOverrides.m_snapToClosestOrthogonalPixelDirections;
}

void Image::setHeaderOverrides( const ImageHeaderOverrides& overrides )
{
    m_headerOverrides = overrides;

    m_header.setHeaderOverrides( m_headerOverrides );
    m_tx.setHeaderOverrides( m_headerOverrides );
}

const ImageHeaderOverrides& Image::getHeaderOverrides() const
{
    return m_headerOverrides;
}

std::ostream& Image::metaData( std::ostream& os ) const
{
    for ( const auto& p : m_ioInfoInMemory.m_metaData )
    {
        os << p.first << ": ";
        // std::visit( [&os] ( const auto& e ) { os << e; }, p.second );
        os << std::endl;
    }
    return os;
}

void Image::updateComponentStats()
{
    m_settings.updateWithNewComponentStatistics(
        computeImageStatistics<StatsType>( *this ), false );
}
