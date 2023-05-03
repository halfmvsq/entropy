#include "mesh/MeshLoading.h"
#include "mesh/MeshCpuRecord.h"
#include "mesh/vtkdetails/MeshGeneration.hpp"

#include "common/MathFuncs.h"
#include "common/Types.h"

#include "image/Image.h"
#include "image/ImageHeader.h"
#include "image/ImageUtility.tpp"

#include "rendering_old/utility/CreateGLObjects.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vnl/vnl_matrix_fixed.h>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <limits>
#include <utility>


namespace meshgen
{

std::unique_ptr<MeshCpuRecord> generateIsosurfaceMeshCpuRecord(
    vtkImageData* imageData,
    const ImageHeader& imageHeader,
    double isoValue )
{
    // Note: triangle strips offer no speed advantage over indexed triangles on modern hardware
    static const MeshPrimitiveType sk_primitiveType = MeshPrimitiveType::Triangles;

    if ( ! imageData )
    {
        spdlog::error( "Error generating iso-surface mesh: Image data is null." );
        return nullptr;
    }

    const vnl_matrix_fixed< double, 3, 3 > imageDirections =
        math::convert::toVnlMatrixFixed( glm::dmat3{ imageHeader.directions() } );

    vtkSmartPointer< vtkPolyData > polyData = nullptr;

    try
    {
        polyData = ::vtkdetails::generateIsoSurfaceMesh(
            imageData, imageDirections, isoValue, sk_primitiveType );
    }
    catch ( const std::exception& e )
    {
        spdlog::error( "Error generating iso-surface mesh: {}", e.what() );
        return nullptr;
    }
    catch ( ... )
    {
        spdlog::error( "Error generating iso-surface mesh" );
        return nullptr;
    }

    if ( ! polyData )
    {
        spdlog::error( "Error generating iso-surface mesh: vtkPolyData is null." );
        return nullptr;
    }

    return std::make_unique<MeshCpuRecord>(
        polyData, MeshInfo( MeshSource::IsoSurface, sk_primitiveType, isoValue ) );
}


std::unique_ptr<MeshCpuRecord> generateLabelMesh(
    vtkImageData* imageData,
    const ImageHeader& imageHeader,
    uint32_t labelIndex )
{
    // Note: triangle strips offer no speed advantage over indexed triangles on modern hardware
    static const MeshPrimitiveType sk_primitiveType = MeshPrimitiveType::Triangles;

    // Parcellation image must have exactly one scalar component
    if ( 1 != imageHeader.numComponentsPerPixel() ||
         PixelType::Scalar != imageHeader.pixelType() )
    {
        spdlog::error( "Error computing mesh at label index {}: "
                       "Pixel type must be single-component scalar.", labelIndex );
        return nullptr;
    }

    // Parcellation pixels are indices that must be of unsigned integer type
    if ( ! isUnsignedIntegerType( imageHeader.memoryComponentType() ) )
    {
        spdlog::error( "Error computing mesh at label index {}: "
                       "Parcellation component type must be unsigned integral.", labelIndex );
        return nullptr;
    }

    if ( ! imageData )
    {
        spdlog::error( "Parcellation image data is null" );
        return nullptr;
    }

    const vnl_matrix_fixed< double, 3, 3 > imageDirections =
        math::convert::toVnlMatrixFixed( glm::dmat3{ imageHeader.directions() } );

    try
    {
        auto polyData = ::vtkdetails::generateLabelMesh(
            imageData, imageDirections, labelIndex, sk_primitiveType );

        if ( ! polyData )
        {
            spdlog::error( "Error computing mesh at label index {} ", labelIndex );
            return nullptr;
        }

        return std::make_unique<MeshCpuRecord>(
                    polyData, MeshInfo( MeshSource::Label, sk_primitiveType, labelIndex ) );
    }
    catch ( const std::exception& e )
    {
        spdlog::error( "Error generating label mesh at index {}: {}", labelIndex, e.what() );
        return nullptr;
    }
    catch ( ... )
    {
        spdlog::error( "Error generating label mesh at index {}", labelIndex);
        return nullptr;
    }
}


std::future< AsyncUiTaskValue > generateIsosurfaceMeshRecord(
    const Image& image,
    uint32_t component,
    double isoValue,
    const uuids::uuid& isosurfaceUid,
    std::function< bool ( std::unique_ptr<MeshRecord> ) > /*meshRecordUpdater*/ )
{
    auto generateMesh = [] (
        const Image& _image,
        uint32_t _component,
        double _isoValue,
        const uuids::uuid& _isosurfaceUid,
        const std::function< void ( bool success ) >& onGenerateDone )
    {
        spdlog::debug( "Start generating mesh for isosurface {} at value {}", _isosurfaceUid, _isoValue );

        AsyncUiTaskValue retval;
        retval.task = AsyncUiTasks::IsosurfaceMeshGeneration;
        retval.description = std::string( "Generate mesh at image isovalue " + std::to_string( _isoValue ) );
        retval.objectUid = _isosurfaceUid;
        retval.success = false;

        // if ( ! _meshRecordUpdater )
        // {
        //     spdlog::error( "Null callback meshRecordUpdater" );
        //     onGenerateDone( false );
        //     return false;
        // }

        // Cast image component to float prior to mesh generation
        const auto itkImage = createItkImageFromImageComponent<float>( _image, _component );
        const auto vtkImageData = convertItkImageToVtkImageData<float>( itkImage );

        if ( ! vtkImageData )
        {
            spdlog::error( "Image has null vtkImageData when generating isosurface" );
            onGenerateDone( false );
            return retval;
        }

        auto cpuRecord = generateIsosurfaceMeshCpuRecord(
            vtkImageData.Get(), _image.header(), _isoValue );

        if ( ! cpuRecord )
        {
            spdlog::error( "Error generating isosurface CPU mesh record" );
            onGenerateDone( false );
            return retval;
        }
        else
        {
            spdlog::debug( "Generated isosurface CPU mesh record" );
        }

        spdlog::debug( "Done generating mesh for isosurface {} at value {}", _isosurfaceUid, _isoValue );

        onGenerateDone( true );

        retval.success = true;
        return retval;
    };

    auto generateDone = []( bool success )
    {
        if ( success )
        {
            spdlog::debug( "Start generating GPU mesh for isosurface" );

        //     auto gpuRecord = gpuhelper::createMeshGpuRecordFromVtkPolyData(
        //         cpuRecord->polyData(), cpuRecord->meshInfo().primitiveType(),
        //         BufferUsagePattern::StreamDraw );

        //     if ( ! gpuRecord )
        //     {
        //         spdlog::error( "Error generating isosurface GPU mesh record" );
        //         onGenerateDone( false );
        //         return false;
        //     }
        //     else
        //     {
        //         spdlog::debug( "Generated isosurface GPU mesh record" );
        //     }

        //     if ( _meshRecordUpdater( std::make_unique<MeshRecord>(
        //             std::move(cpuRecord), std::move(gpuRecord) ) ) )
        //     {
        //         onGenerateDone( true );
        //         return true;
        //     }

            // m_glfw.postEmptyEvent(); // Post an empty event to notify render thread
            spdlog::debug( "Done generating GPU mesh for isosurface" );
        }
        else
        {
            spdlog::error( "Failed to generate GPU mesh for isosurface" );
        }
    };

    return std::async( std::launch::async, generateMesh,
        image, component, isoValue, isosurfaceUid, generateDone );
}


bool writeMeshToFile( const MeshCpuRecord& record, const std::string& fileName )
{
    if ( record.polyData().GetPointer() )
    {
        return ( ::vtkdetails::writePolyData( record.polyData(), fileName.c_str() ) );
    }

    return false;
}

} // namespace meshgen
