#include "image/ImageUtility.h"

#include "common/MathFuncs.h"

#include <spdlog/spdlog.h>

#include <itkImageIOFactory.h>

#include <filesystem>
#include <limits>
#include <vector>


namespace
{

#if __APPLE__

#include <set>

std::vector<std::string> splitPath( const std::string& pathString, const std::set<char> delimiters )
{
    std::vector<std::string> result;

    char const* pch = pathString.c_str();
    char const* start = pch;

    for ( ; *pch; ++pch )
    {
        if ( delimiters.find( *pch ) != std::end( delimiters ) )
        {
            if ( start != pch )
            {
                std::string s( start, pch );
                result.push_back( s );
            }
            else
            {
                result.push_back( "" );
            }
            start = pch + 1;
        }
    }

    result.push_back( start );
    return result;
}

#endif

} // anonymous


std::string getFileName( const std::string& filePath, bool withExtension )
{
    if ( ! withExtension )
    {
#if __APPLE__
        static const std::set<char> delims{ '/' };
        std::vector<std::string> pSplit = splitPath( filePath, delims );
        return pSplit.back();
#else
        const std::filesystem::path p( filePath );

        // Check if path has a stem (i.e. filename without extension)
        if ( p.has_stem() )
        {
            // Remove one more stem (e.g. in case the file name is like *.nii.gz)
            const auto stem1 = p.stem();
            const auto stem2 = ( stem1.has_stem() ) ? stem1.stem() : stem1;

            // Return the stem (file name without extension) from path
            return stem2.string();
        }
        else
        {
            return filePath;
        }
#endif
    }
    else
    {
#if __APPLE__
        static const std::set<char> delims{ '/' };
        std::vector<std::string> pSplit = splitPath( filePath, delims );
        return pSplit.back();
#else
        // Return the file name with extension from path
        const std::filesystem::path p( filePath );
        return p.filename().string();
#endif
    }
}


PixelType fromItkPixelType( const ::itk::IOPixelEnum& pixelType )
{
    switch ( pixelType )
    {
    case itk::IOPixelEnum::UNKNOWNPIXELTYPE: return PixelType::Undefined;
    case itk::IOPixelEnum::SCALAR: return PixelType::Scalar;
    case itk::IOPixelEnum::RGB: return PixelType::RGB;
    case itk::IOPixelEnum::RGBA: return PixelType::RGBA;
    case itk::IOPixelEnum::OFFSET: return PixelType::Offset;
    case itk::IOPixelEnum::VECTOR: return PixelType::Vector;
    case itk::IOPixelEnum::POINT: return PixelType::Point;
    case itk::IOPixelEnum::COVARIANTVECTOR: return PixelType::CovariantVector;
    case itk::IOPixelEnum::SYMMETRICSECONDRANKTENSOR: return PixelType::SymmetricSecondRankTensor;
    case itk::IOPixelEnum::DIFFUSIONTENSOR3D: return PixelType::DiffusionTensor3D;
    case itk::IOPixelEnum::COMPLEX: return PixelType::Complex;
    case itk::IOPixelEnum::FIXEDARRAY: return PixelType::FixedArray;
    case itk::IOPixelEnum::ARRAY: return PixelType::Array;
    case itk::IOPixelEnum::MATRIX: return PixelType::Matrix;
    case itk::IOPixelEnum::VARIABLELENGTHVECTOR: return PixelType::VariableLengthVector;
    case itk::IOPixelEnum::VARIABLESIZEMATRIX: return PixelType::VariableSizeMatrix;
    }

    return PixelType::Undefined;
}


ComponentType fromItkComponentType( const ::itk::IOComponentEnum& componentType )
{
    switch ( componentType )
    {
    case itk::IOComponentEnum::UCHAR: return ComponentType::UInt8;
    case itk::IOComponentEnum::CHAR: return ComponentType::Int8;
    case itk::IOComponentEnum::USHORT: return ComponentType::UInt16;
    case itk::IOComponentEnum::SHORT: return ComponentType::Int16;
    case itk::IOComponentEnum::UINT: return ComponentType::UInt32;
    case itk::IOComponentEnum::INT: return ComponentType::Int32;
    case itk::IOComponentEnum::FLOAT: return ComponentType::Float32;
    case itk::IOComponentEnum::LONG: return ComponentType::Long;
    case itk::IOComponentEnum::ULONG: return ComponentType::ULong;
    case itk::IOComponentEnum::LONGLONG: return ComponentType::LongLong;
    case itk::IOComponentEnum::ULONGLONG: return ComponentType::ULongLong;
    case itk::IOComponentEnum::DOUBLE: return ComponentType::Float64;
    case itk::IOComponentEnum::LDOUBLE: return ComponentType::LongDouble;

    default:
    case itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE: return ComponentType::Undefined;
    }
}


::itk::IOComponentEnum toItkComponentType( const ComponentType& componentType )
{
    switch ( componentType )
    {
    case ComponentType::Int8: return ::itk::IOComponentEnum::CHAR;
    case ComponentType::UInt8: return ::itk::IOComponentEnum::UCHAR;
    case ComponentType::Int16: return ::itk::IOComponentEnum::SHORT;
    case ComponentType::UInt16: return ::itk::IOComponentEnum::USHORT;
    case ComponentType::Int32: return ::itk::IOComponentEnum::INT;
    case ComponentType::UInt32: return ::itk::IOComponentEnum::UINT;
    case ComponentType::Float32: return ::itk::IOComponentEnum::FLOAT;
    case ComponentType::Float64: return ::itk::IOComponentEnum::DOUBLE;
    case ComponentType::Long: return ::itk::IOComponentEnum::LONG;
    case ComponentType::ULong: return ::itk::IOComponentEnum::ULONG;
    case ComponentType::LongLong: return ::itk::IOComponentEnum::LONGLONG;
    case ComponentType::ULongLong: return ::itk::IOComponentEnum::ULONG;
    case ComponentType::LongDouble: return ::itk::IOComponentEnum::LDOUBLE;

    default:
    case ComponentType::Undefined: return ::itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE;
    }
}


std::pair< itk::CommonEnums::IOComponent, std::string >
sniffComponentType( const char* fileName )
{
    static const std::pair< itk::CommonEnums::IOComponent, std::string > UNKNOWN{
        itk::CommonEnums::IOComponent::UNKNOWNCOMPONENTTYPE, "UNKNOWNCOMPONENTTYPE" };

    try
    {
        const itk::ImageIOBase::Pointer imageIO =
                itk::ImageIOFactory::CreateImageIO( fileName, itk::ImageIOFactory::ReadMode );

        if ( ! imageIO || imageIO.IsNull() )
        {
            // None of the registered ImageIO classes can read the file
            spdlog::error( "ITK image I/O factory could not create the I/O object for image {}", fileName );
            return UNKNOWN;
        }

        imageIO->SetFileName( fileName );
        imageIO->ReadImageInformation();

        const auto compType = imageIO->GetComponentType();
        return { compType, imageIO->GetComponentTypeAsString( compType ) };
    }
    catch ( const ::itk::ExceptionObject& e )
    {
        spdlog::error( "Exception while creating ImageIOBase for image {}: {}", fileName, e.what() );
        return UNKNOWN;
    }
    catch ( ... )
    {
        spdlog::error( "Unknown exception while creating ImageIOBase for image {}", fileName );
        return UNKNOWN;
    }
}


typename itk::ImageIOBase::Pointer createStandardImageIo( const char* fileName )
{
    try
    {
        const itk::ImageIOBase::Pointer imageIo =
            itk::ImageIOFactory::CreateImageIO( fileName, itk::ImageIOFactory::ReadMode );

        if ( ! imageIo || imageIo.IsNull() )
        {
            // None of the registered ImageIO classes can read the file
            spdlog::error( "ITK image I/O factory could not create the I/O object for image {}", fileName );
            return nullptr;
        }

        imageIo->SetFileName( fileName );
        imageIo->ReadImageInformation();
        return imageIo;
    }
    catch ( const ::itk::ExceptionObject& e )
    {
        spdlog::error( "Exception while creating ImageIOBase for image {}: {}", fileName, e.what() );
        return nullptr;
    }
    catch ( ... )
    {
        spdlog::error( "Unknown exception while creating ImageIOBase for image {}", fileName );
        return nullptr;
    }
}


std::pair< double, double > componentRange( const ComponentType& componentType )
{
    switch ( componentType )
    {
    case ComponentType::Int8: return { std::numeric_limits<int8_t>::lowest(), std::numeric_limits<int8_t>::max() };
    case ComponentType::UInt8: return { std::numeric_limits<uint8_t>::lowest(), std::numeric_limits<uint8_t>::max() };
    case ComponentType::Int16: return { std::numeric_limits<int16_t>::lowest(), std::numeric_limits<int16_t>::max() };
    case ComponentType::UInt16: return { std::numeric_limits<uint16_t>::lowest(), std::numeric_limits<uint16_t>::max() };
    case ComponentType::Int32: return { std::numeric_limits<int32_t>::lowest(), std::numeric_limits<int32_t>::max() };
    case ComponentType::UInt32: return { std::numeric_limits<uint32_t>::lowest(), std::numeric_limits<uint32_t>::max() };
    case ComponentType::Float32: return { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max() };
    default: return { 0.0, 0.0 };
    }
}


std::pair<glm::vec3, glm::vec3> computeWorldMinMaxCornersOfImage( const Image& image )
{
    const auto& world_T_subject = image.transformations().worldDef_T_subject();
    const auto& subjectCorners = image.header().subjectBBoxCorners();

    std::array< glm::vec3, 8 > worldCorners;

    std::transform( std::begin( subjectCorners ), std::end( subjectCorners ),
                    std::begin( worldCorners ),
    [ &world_T_subject ]( const glm::dvec3& v )
    {
        const glm::vec4 worldCorner = world_T_subject * glm::vec4{ v, 1.0f };
        return glm::vec3{ worldCorner } / worldCorner.w;
    } );

    return math::computeMinMaxCornersOfAABBox( worldCorners );
}
