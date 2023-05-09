#include "image/Image.h"
#include "image/ImageUtility.h"
#include "image/ImageUtility.tpp"

#include "common/Exception.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>


namespace
{

// Default source component type and value
using DefaultSrcComponentType = float;

}


Image::Image(
    const std::string& fileName,
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
    using ComponentImageType = itk::Image<ReadComponentType, 3>;

    // Statistics per component are stored as double
    using StatsType = double;

    // Maximum number of components to load for images with interleaved buffer components
    static constexpr std::size_t MAX_COMPS = 4;

    const itk::ImageIOBase::Pointer imageIo = createStandardImageIo( fileName.c_str() );

    if ( ! imageIo || imageIo.IsNull() )
    {
        spdlog::error( "Error creating ImageIOBase for image from file '{}'", fileName );
        throw_debug( "Error creating ImageIOBase" )
    }

    if ( ! m_ioInfoOnDisk.set( imageIo ) )
    {
        spdlog::error( "Error setting image IO information for image from file '{}'", fileName );
        throw_debug( "Error setting image IO information" )
    }

    // The image information in memory may not match the information on disk
    m_ioInfoInMemory = m_ioInfoOnDisk;

    const std::size_t numPixels = m_ioInfoOnDisk.m_sizeInfo.m_imageSizeInPixels;
    const std::size_t numComps = m_ioInfoOnDisk.m_pixelInfo.m_numComponents;
    const bool isVectorImage = ( numComps > 1 );

    spdlog::info( "Attempting to load image from '{}' with {} pixels and {} components per pixel",
                  fileName, numPixels, numComps );

    if ( isVectorImage )
    {
        // Load multi-component image
        typename itk::ImageBase<3>::Pointer baseImage = readImage<ReadComponentType, 3, true>( fileName );

        if ( ! baseImage )
        {
            spdlog::error( "Unable to read vector ImageBase from file '{}'", fileName );
            throw_debug( "Unable to read vector image" )
        }

        // Split the base image into component images. Load a maximum of MAX_COMPS components
        // for an image with interleaved component buffers.
        std::size_t numCompsToLoad = numComps;

        if ( MultiComponentBufferType::InterleavedImage == m_bufferType )
        {
            numCompsToLoad = std::min( numCompsToLoad, MAX_COMPS );

            if ( numComps > MAX_COMPS )
            {
                spdlog::warn( "The number of image components ({}) exceeds that maximum that will be loaded ({}) "
                              "because this image uses interleaved buffer format", numComps, MAX_COMPS );
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
            spdlog::warn( "Loading a segmentation image from '{}' with {} components. "
                          "Only the first component of the segmentation will be used",
                          fileName, numComps );
            numCompsToLoad = 1;
        }

        // Adjust the number of components in the image header
        m_header.setNumComponentsPerPixel( numCompsToLoad );

        if ( 0 == numCompsToLoad )
        {
            spdlog::error( "No components to load for image from file '{}'", fileName );
            throw_debug( "No components to load for image" )
        }

        // If interleaving vector components, then create a single buffer
        std::unique_ptr<ReadComponentType[]> allComponentBuffers = nullptr;

        if ( MultiComponentBufferType::InterleavedImage == m_bufferType )
        {
            allComponentBuffers = std::make_unique<ReadComponentType[]>( numPixels * numCompsToLoad );

            if ( ! allComponentBuffers )
            {
                spdlog::error( "Null buffer holding all components of image from file '{}'", fileName );
                throw_debug( "Null image buffer" )
            }
        }

        // Load the buffers from the component images
        for ( std::size_t i = 0; i < numCompsToLoad; ++i )
        {
            if ( ! componentImages[i] )
            {
                spdlog::error( "Null vector image component {} for image from file '{}'", i, fileName );
                throw_debug( "Null vector component for image" )
            }

            const ReadComponentType* buffer = componentImages[i]->GetBufferPointer();

            if ( ! buffer )
            {
                spdlog::error( "Null buffer of vector image component {} for image from file '{}'", i, fileName );
                throw_debug( "Null buffer of vector image component" )
            }

            switch ( m_bufferType )
            {
            case MultiComponentBufferType::SeparateImages:
            {
                if ( ImageRepresentation::Segmentation == m_imageRep )
                {
                    if ( ! loadSegBuffer<ReadComponentType>( buffer, numPixels ) )
                    {
                        spdlog::error( "Error loading segmentation image buffer for file '{}'", fileName );
                        throw_debug( "Error loading segmentation image buffer" )
                    }
                }
                else
                {
                    if ( ! loadImageBuffer<ReadComponentType>( buffer, numPixels ) )
                    {
                        spdlog::error( "Error loading image buffer for file '{}'", fileName );
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
                if ( ! loadSegBuffer<ReadComponentType>( allComponentBuffers.get(), numElements ) )
                {
                    spdlog::error( "Error loading segmentation image buffer for file '{}'", fileName );
                    throw_debug( "Error loading segmentation image buffer" )
                }
            }
            else
            {
                if ( ! loadImageBuffer<ReadComponentType>( allComponentBuffers.get(), numElements ) )
                {
                    spdlog::error( "Error loading image buffer for file '{}'", fileName );
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
            spdlog::error( "Unable to read ImageBase from file '{}'", fileName );
            throw_debug( "Unable to read image" )
        }

        typename ComponentImageType::Pointer image = downcastImageBaseToImage<ReadComponentType, 3>( baseImage );

        if ( ! image )
        {
            spdlog::error( "Null image for file '{}' following downcast from ImageBase", fileName );
            throw_debug( "Null image" )
        }

        const ReadComponentType* buffer = image->GetBufferPointer();

        if ( ! buffer )
        {
            spdlog::error( "Null buffer of scalar image from file '{}'", fileName );
            throw_debug( "Null buffer of scalar image" )
        }

        if ( ImageRepresentation::Segmentation == m_imageRep )
        {
            if ( ! loadSegBuffer<ReadComponentType>( buffer, numPixels ) )
            {
                spdlog::error( "Error loading segmentation image buffer for file '{}'", fileName );
                throw_debug( "Error loading segmentation image buffer" )
            }
        }
        else
        {
            if ( ! loadImageBuffer<ReadComponentType>( buffer, numPixels ) )
            {
                spdlog::error( "Error loading image buffer for file '{}'", fileName );
                throw_debug( "Error loading image buffer" )
            }
        }
    }

    m_header = ImageHeader(
        m_ioInfoOnDisk, m_ioInfoInMemory, ( MultiComponentBufferType::InterleavedImage == m_bufferType ) );

    m_tx = ImageTransformations(
        m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions() );

    m_headerOverrides = ImageHeaderOverrides(
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
    const std::vector<const float*> imageDataComponents )
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
    // Statistics per component are stored as double
    using StatsType = double;

    // Maximum number of components to create for images with interleaved buffers
    static constexpr std::size_t MAX_COMPS = 4;

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

    const std::size_t numPixels = m_header.numPixels();
    const std::size_t numComps = m_header.numComponentsPerPixel();
    const bool isVectorImage = ( numComps > 1 );

    if ( isVectorImage )
    {
        // Create multi-component image
        std::size_t numCompsToLoad = numComps;

        if ( MultiComponentBufferType::InterleavedImage == m_bufferType )
        {
            // Set a maximum of MAX_COMPS components
            numCompsToLoad = std::min( numCompsToLoad, MAX_COMPS );

            if ( numComps > MAX_COMPS )
            {
                spdlog::warn( "The number of image components ({}) exceeds that maximum that will be created ({}) "
                              "because this image uses interleaved buffer format", numComps, MAX_COMPS );
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
            spdlog::error( "No components to create for image from file '{}'", m_header.fileName() );
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
                    if ( ! loadSegBuffer<DefaultSrcComponentType>( imageDataComponents[c], numPixels ) )
                    {
                        throw_debug( "Error loading segmentation image buffer" )
                    }
                }
                else
                {
                    if ( ! loadImageBuffer<DefaultSrcComponentType>( imageDataComponents[c], numPixels ) )
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
                if ( ! loadSegBuffer<DefaultSrcComponentType>( imageDataComponents[0], N ) )
                {
                    throw_debug( "Error loading segmentation image buffer" )
                }
            }
            else
            {
                if ( ! loadImageBuffer<DefaultSrcComponentType>( imageDataComponents[0], N ) )
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
            if ( ! loadSegBuffer<DefaultSrcComponentType>( imageDataComponents[0], numPixels ) )
            {
                throw_debug( "Error loading segmentation image buffer" )
            }
        }
        else
        {
            if ( ! loadImageBuffer<DefaultSrcComponentType>( imageDataComponents[0], numPixels ) )
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


bool Image::saveComponentToDisk( uint32_t component, const std::optional<std::string>& newFileName )
{
    constexpr uint32_t DIM = 3;
    constexpr bool s_isVectorImage = false;

    const std::string fileName = ( newFileName ) ? *newFileName : m_header.fileName();

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
        case ComponentType::Int8:    return static_cast<const void*>( m_data_int8.at(i).data() );
        case ComponentType::UInt8:   return static_cast<const void*>( m_data_uint8.at(i).data() );
        case ComponentType::Int16:   return static_cast<const void*>( m_data_int16.at(i).data() );
        case ComponentType::UInt16:  return static_cast<const void*>( m_data_uint16.at(i).data() );
        case ComponentType::Int32:   return static_cast<const void*>( m_data_int32.at(i).data() );
        case ComponentType::UInt32:  return static_cast<const void*>( m_data_uint32.at(i).data() );
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
