#ifndef IMAGE_H
#define IMAGE_H

#include "image/ImageHeader.h"
#include "image/ImageHeaderOverrides.h"
#include "image/ImageIoInfo.h"
#include "image/ImageSettings.h"
#include "image/ImageTransformations.h"

#include <spdlog/spdlog.h>

#include <optional>
#include <ostream>
#include <string>
#include <vector>


/**
 * @brief Encapsulates a 3D medical image with one or more components per pixel
 */
class Image
{
public:

    /// What does the Image represent?
    enum class ImageRepresentation
    {
        Image, //!< An image
        Segmentation //!< A segmentation
    };

    /// How should Image hold data for multi-component images?
    enum class MultiComponentBufferType
    {
        SeparateImages, //!< Each component is a separate image
        InterleavedImage //!< Interleave all components in a single image
    };


    /**
     * @brief Construct Image from a file on disk
     *
     * @param[in] fileName Path to image file
     * @param[in] imageType Indicates whether this is an image or a segmentation
     * @param[in] bufferType Indicates whether multi-component images are loaded as
     * multiple buffers or as a single buffer with interleaved pixel components
     */
    Image( const std::string& fileName,
           ImageRepresentation imageRep,
           MultiComponentBufferType bufferType );

    /**
     * @brief Construct Image from a header and data array
    */
    Image( const ImageHeader& header,
           std::string displayName,
           ImageRepresentation imageRep,
           MultiComponentBufferType bufferType,
           const void* imageData = nullptr );

    Image( const Image& ) = default;
    Image& operator=( const Image& ) = default;

    Image( Image&& ) = default;
    Image& operator=( Image&& ) = default;

    ~Image() = default;


    /** @brief Save an image component to disk. If the image is successfully saved and a
     * new file name is provided, then the image file name is set to the new file name.
     *
     * @param[in] newFileName Optional new file name at which to save the image
     * @param[in] component Component of the image to save
     *
     * @return True iff the image was saved successfully
     */
    bool saveComponentToDisk( uint32_t component, const std::optional<std::string>& newFileName );


    const ImageRepresentation& imageRep() const;
    const MultiComponentBufferType& bufferType() const;


    /** @brief Get a const void pointer to the raw buffer data of an image component.
     *
     * @note If MultiComponentBufferType::InterleavedImage, then the image has only one component (0)
     *
     * @note The component must be in the range [0, header().numComponentsPerPixel() - 1].
     * To read the data, cast this buffer to the appropriate component type obtained via
     * header().componentType().
     *
     * @note A scalar image has a single component (0)
     */
    const void* bufferAsVoid( uint32_t component ) const;


    /// @brief Get a non-const void pointer to the raw buffer data of an image component.
    void* bufferAsVoid( uint32_t component );


    /// @brief Get the value of the buffer at image 1D index
    template<typename T>
    std::optional<T> value( uint32_t component, std::size_t index ) const
    {
        const auto compAndOffset = getComponentAndOffsetForBuffer( component, index );

        if ( ! compAndOffset )
        {
            return std::nullopt;
        }

        const std::size_t c = compAndOffset->first;
        const std::size_t offset = compAndOffset->second;

        switch ( m_header.memoryComponentType() )
        {
        case ComponentType::Int8:    return static_cast<T>( m_data_int8.at(c)[offset] );
        case ComponentType::UInt8:   return static_cast<T>( m_data_uint8.at(c)[offset] );
        case ComponentType::Int16:   return static_cast<T>( m_data_int16.at(c)[offset] );
        case ComponentType::UInt16:  return static_cast<T>( m_data_uint16.at(c)[offset] );
        case ComponentType::Int32:   return static_cast<T>( m_data_int32.at(c)[offset] );
        case ComponentType::UInt32:  return static_cast<T>( m_data_uint32.at(c)[offset] );
        case ComponentType::Float32: return static_cast<T>( m_data_float32.at(c)[offset] );
        default: return std::nullopt;
        }

        return std::nullopt;
    }

    /// @brief Get the value of the buffer at image 3D index (i, j, k)
    template<typename T>
    std::optional<T> value( uint32_t component, int i, int j, int k ) const
    {
        const glm::u64vec3 dims = m_header.pixelDimensions();

        const std::size_t index =
            dims.x * dims.y * static_cast<std::size_t>(k) +
            dims.x * static_cast<std::size_t>(j) +
            static_cast<std::size_t>(i);

        return value<T>( component, index );
    }

    /// @brief Set the value of the buffer at image index (i, j, k)
    template<typename T>
    bool setValue( uint32_t component, int i, int j, int k, T value )
    {
        const auto compAndOffset = getComponentAndOffsetForBuffer( component, i, j, k );

        if ( ! compAndOffset )
        {
            return false;
        }

        const std::size_t c = compAndOffset->first;
        const std::size_t offset = compAndOffset->second;

        switch ( m_header.memoryComponentType() )
        {
        case ComponentType::Int8: m_data_int8.at(c)[offset] = static_cast<int8_t>( value ); return true;
        case ComponentType::UInt8: m_data_uint8.at(c)[offset] = static_cast<uint8_t>( value ); return true;
        case ComponentType::Int16: m_data_int16.at(c)[offset] = static_cast<int16_t>( value ); return true;
        case ComponentType::UInt16: m_data_uint16.at(c)[offset] = static_cast<uint16_t>( value ); return true;
        case ComponentType::Int32: m_data_int32.at(c)[offset] = static_cast<int32_t>( value ); return true;
        case ComponentType::UInt32: m_data_uint32.at(c)[offset] = static_cast<uint32_t>( value ); return true;
        case ComponentType::Float32: m_data_float32.at(c)[offset] = static_cast<float>( value ); return true;
        default: return false;
        }

        return false;
    }

    void setUseIdentityPixelSpacings( bool identitySpacings );
    bool getUseIdentityPixelSpacings() const;

    void setUseZeroPixelOrigin( bool zeroOrigin );
    bool getUseZeroPixelOrigin() const;

    void setUseIdentityPixelDirections( bool identityDirections );
    bool getUseIdentityPixelDirections() const;

    void setSnapToClosestOrthogonalPixelDirections( bool snap );
    bool getSnapToClosestOrthogonalPixelDirections() const;

    void setHeaderOverrides( const ImageHeaderOverrides& overrides );
    const ImageHeaderOverrides& getHeaderOverrides() const;


    /// @brief Get the image header
    const ImageHeader& header() const;
    ImageHeader& header();

    /// @brief Get the image transformations
    const ImageTransformations& transformations() const;
    ImageTransformations& transformations();

    /// @brief Get the image settings
    const ImageSettings& settings() const;
    ImageSettings& settings();

    /// @brief Get the image meta data
    std::ostream& metaData( std::ostream& os ) const;


private:

    template< typename SrcComponentType, typename DestComponentType >
    std::vector<DestComponentType> createBuffer( const SrcComponentType* buffer, std::size_t numElements )
    {
        // Lowest and max values of destination component type, cast to source component type
        static constexpr auto sk_lowestValue = static_cast<SrcComponentType>( std::numeric_limits<DestComponentType>::lowest() );
        static constexpr auto sk_maximumValue = static_cast<SrcComponentType>( std::numeric_limits<DestComponentType>::max() );

        std::vector<DestComponentType> data( numElements, 0 );

        // Clamp values to destination range [lowest, maximum] prior to cast:
        for ( std::size_t i = 0; i < numElements; ++i )
        {
            data[i] = static_cast<DestComponentType>( std::min( std::max( buffer[i], sk_lowestValue ), sk_maximumValue ) );
        }

        return data;
    }

    /// Load a buffer as an image component
    template< typename SrcBufferCompType >
    bool loadImageBuffer( const SrcBufferCompType* buffer, std::size_t numElements )
    {
        using CType = ::itk::ImageIOBase::IOComponentType;

        bool didCast = false;
        bool warnSizeConversion = false;

        switch ( m_ioInfoOnDisk.m_componentInfo.m_componentType )
        {
        case CType::UCHAR:  m_data_uint8.emplace_back( createBuffer<SrcBufferCompType, uint8_t>( buffer, numElements ) ); break;
        case CType::CHAR:   m_data_int8.emplace_back( createBuffer<SrcBufferCompType, int8_t>( buffer, numElements ) ); break;
        case CType::USHORT: m_data_uint16.emplace_back( createBuffer<SrcBufferCompType, uint16_t>( buffer, numElements ) ); break;
        case CType::SHORT:  m_data_int16.emplace_back( createBuffer<SrcBufferCompType, int16_t>( buffer, numElements ) ); break;
        case CType::UINT:   m_data_uint32.emplace_back( createBuffer<SrcBufferCompType, uint32_t>( buffer, numElements ) ); break;
        case CType::INT:    m_data_int32.emplace_back( createBuffer<SrcBufferCompType, int32_t>( buffer, numElements ) ); break;
        case CType::FLOAT:  m_data_float32.emplace_back( createBuffer<SrcBufferCompType, float>( buffer, numElements ) ); break;

        case CType::ULONG:
        case CType::ULONGLONG:
        {
            m_data_uint32.emplace_back( createBuffer<SrcBufferCompType, uint32_t>( buffer, numElements ) );
            m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
            m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

            didCast = true;
            warnSizeConversion = true;
            break;
        }

        case CType::LONG:
        case CType::LONGLONG:
        {
            m_data_int32.emplace_back( createBuffer<SrcBufferCompType, int32_t>( buffer, numElements ) );
            m_ioInfoInMemory.m_componentInfo.m_componentType = CType::INT;
            m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

            didCast = true;
            warnSizeConversion = true;
            break;
        }

        case CType::DOUBLE:
        case CType::LDOUBLE:
        {
            m_data_float32.emplace_back( createBuffer<SrcBufferCompType, float>( buffer, numElements ) );
            m_ioInfoInMemory.m_componentInfo.m_componentType = CType::FLOAT;
            m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

            didCast = true;
            warnSizeConversion = true;
            break;
        }

        case CType::UNKNOWNCOMPONENTTYPE:
        {
            spdlog::error( "Unknown component type in image from file '{}'", m_ioInfoOnDisk.m_fileInfo.m_fileName );
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

    /// Load a buffer as a segmentation component
    template< typename SrcBufferCompType >
    bool loadSegBuffer( const SrcBufferCompType* buffer, std::size_t numElements )
    {
        using CType = ::itk::ImageIOBase::IOComponentType;

        bool didCast = false;
        bool warnFloatConversion = false;
        bool warnSizeConversion = false;
        bool warnSignConversion = false;

        switch ( m_ioInfoOnDisk.m_componentInfo.m_componentType )
        {
        // No casting is needed for the cases of unsigned integers with 8, 16, or 32 bytes:
        case CType::UCHAR:
        {
            m_data_uint8.emplace_back( createBuffer<SrcBufferCompType, uint8_t>( buffer, numElements ) );
            break;
        }
        case CType::USHORT:
        {
            m_data_uint16.emplace_back( createBuffer<SrcBufferCompType, uint16_t>( buffer, numElements ) );
            break;
        }
        case CType::UINT:
        {
            m_data_uint32.emplace_back( createBuffer<SrcBufferCompType, uint32_t>( buffer, numElements ) );
            break;
        }

        // Signed 8-, 16-, and 32-bit integers are cast to unsigned 8-, 16-, and 32-bit integers:
        case CType::CHAR:
        {
            m_data_uint8.emplace_back( createBuffer<SrcBufferCompType, uint8_t>( buffer, numElements ) );
            m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UCHAR;
            m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 1;

            didCast = true;
            warnSignConversion = true;
            break;
        }
        case CType::SHORT:
        {
            m_data_uint16.emplace_back( createBuffer<SrcBufferCompType, uint16_t>( buffer, numElements ) );
            m_ioInfoInMemory.m_componentInfo.m_componentType = CType::USHORT;
            m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 2;

            didCast = true;
            warnSignConversion = true;
            break;
        }
        case CType::INT:
        {
            m_data_uint32.emplace_back( createBuffer<SrcBufferCompType, uint32_t>( buffer, numElements ) );
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
            m_data_uint32.emplace_back( createBuffer<SrcBufferCompType, uint32_t>( buffer, numElements ) );
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
            m_data_uint32.emplace_back( createBuffer<SrcBufferCompType, uint32_t>( buffer, numElements ) );
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
            m_data_uint32.emplace_back( createBuffer<SrcBufferCompType, uint32_t>( buffer, numElements ) );
            m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
            m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;

            didCast = true;
            warnFloatConversion = true;
            warnSignConversion = true;
            break;
        }

        case CType::UNKNOWNCOMPONENTTYPE:
        {
            spdlog::error( "Unknown component type in image from file '{}'",
                           m_ioInfoOnDisk.m_fileInfo.m_fileName );
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


    /// For a given image component and 3D pixel indices, return a pair consisting of:
    /// 1) component buffer to index
    /// 2) offset into that buffer
    std::optional< std::pair< std::size_t, std::size_t > >
    getComponentAndOffsetForBuffer( uint32_t comp, int i, int j, int k ) const;

    /// For a given image component and 1D pixel index, return a pair consisting of:
    /// 1) component buffer to index
    /// 2) offset into that buffer
    std::optional< std::pair< std::size_t, std::size_t > >
    getComponentAndOffsetForBuffer( uint32_t comp, std::size_t index ) const;

    /**
     * @remark If the image has a multi-component pixels, then its components are separated and stored
     * in a vector of images. This is so that the buffer to each image component can be retrieved
     * independently of the others, as required when setting an OpenGL texture. If the components
     * were not separated, then the original buffer would be accessed as a 1-D array with
     * interleaved components: buffer[c + numComponents * ( x + xSize * ( y + ySize * z ) )];
     * where c is the desired component.
    */

    std::vector< std::vector<int8_t> > m_data_int8;
    std::vector< std::vector<uint8_t> > m_data_uint8;
    std::vector< std::vector<int16_t> > m_data_int16;
    std::vector< std::vector<uint16_t> > m_data_uint16;
    std::vector< std::vector<int32_t> > m_data_int32;
    std::vector< std::vector<uint32_t> > m_data_uint32;
    std::vector< std::vector<float> > m_data_float32;

    ImageRepresentation m_imageRep; //!< Is this an image or a segmentation?
    MultiComponentBufferType m_bufferType; //!< How to represent multi-component images?

    ImageIoInfo m_ioInfoOnDisk; //!< Info about image as stored on disk
    ImageIoInfo m_ioInfoInMemory; //!< Info about image as loaded into memory

    ImageHeader m_header;
    ImageHeaderOverrides m_headerOverrides;
    ImageTransformations m_tx;
    ImageSettings m_settings;
};

#endif // IMAGE_H
