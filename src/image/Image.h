#ifndef IMAGE_H
#define IMAGE_H

#include "image/ImageHeader.h"
#include "image/ImageHeaderOverrides.h"
#include "image/ImageIoInfo.h"
#include "image/ImageSettings.h"
#include "image/ImageTransformations.h"

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

    Image( const ImageHeader& header,
           std::string displayName,
           ImageRepresentation imageRep,
           MultiComponentBufferType bufferType );

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

    /// @brief Get the value of the buffer at image index (i, j, k) as a double type
    std::optional<double> valueAsDouble( uint32_t component, int i, int j, int k ) const;

    /// @brief Get the value of the buffer at image index (i, j, k) as a int64_t type
    std::optional<int64_t> valueAsInt64( uint32_t component, int i, int j, int k ) const;

    /// @brief Set the value of the buffer at image index (i, j, k) as a int64_t type
    bool setValue( uint32_t component, int i, int j, int k, int64_t value );

    /// @brief Set the value of the buffer at image index (i, j, k) as a double type
    bool setValue( uint32_t component, int i, int j, int k, double value );


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

    /// Load a buffer as an image component
    void loadImageBuffer( const float* buffer, size_t numElements );

    /// Load a buffer as a segmentation component
    void loadSegBuffer( const float* buffer, size_t numElements );

    /// For a given image component and indices, return a pair consisting of:
    /// 1) component buffer to index
    /// 2) offset into that buffer
    std::optional< std::pair< size_t, size_t > >
    getComponentAndOffsetForBuffer( uint32_t comp, int i, int j, int k ) const;

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
