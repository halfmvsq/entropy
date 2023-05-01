#include "mesh/MeshLoading.h"
#include "mesh/MeshCpuRecord.h"
#include "mesh/vtkdetails/MeshGeneration.hpp"

#include "common/MathFuncs.h"
#include "common/Types.h"

#include "image/ImageHeader.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vnl/vnl_matrix_fixed.h>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <spdlog/spdlog.h>

#include <limits>
#include <utility>


namespace meshgen
{

std::unique_ptr<MeshCpuRecord> generateIsoSurface(
        vtkImageData* imageData,
        const ImageHeader& imageHeader,
        const double isoValue )
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
        const uint32_t labelIndex )
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


bool writeMeshToFile( const MeshCpuRecord& record, const std::string& fileName )
{
    if ( record.polyData().GetPointer() )
    {
        return ( ::vtkdetails::writePolyData( record.polyData(), fileName.c_str() ) );
    }

    return false;
}

} // namespace meshgen
