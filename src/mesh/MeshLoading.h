#ifndef MESH_LOADER_H
#define MESH_LOADER_H

#include "logic/records/MeshRecord.h"
#include "ui/AsyncUiTasks.h"

#include <functional>
#include <future>
#include <memory>
#include <string>

class Image;
class ImageHeader;
class vtkImageData;


namespace meshgen
{

std::unique_ptr<MeshCpuRecord> generateIsosurfaceMeshCpuRecord(
        vtkImageData* imageData,
        const ImageHeader& imageHeader,
        double isoValue );

std::unique_ptr<MeshCpuRecord> generateLabelMeshCpuRecord(
        vtkImageData* imageData,
        const ImageHeader& imageHeader,
        uint32_t labelIndex );

std::future< AsyncUiTaskValue > generateIsosurfaceMeshRecord(
        const Image& image,
        uint32_t component,
        double isoValue,
        const uuids::uuid& isosurfaceUid,
        std::function< bool ( std::unique_ptr<MeshRecord> ) > meshRecordUpdater );

/// @todo Put this function here
//std::map< int64_t, double >
//generateImageHistogramAtLabelValues(
//        vtkImageData* imageData,
//        const std::unordered_set<int64_t>& labelValues );

bool writeMeshToFile( const MeshCpuRecord&, const std::string& fileName );

} // namespace meshgen

#endif // MESH_LOADER_H
