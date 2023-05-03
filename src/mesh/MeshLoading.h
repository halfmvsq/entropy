#ifndef MESH_LOADER_H
#define MESH_LOADER_H

#include "mesh/MeshCpuRecord.h"
#include "ui/AsyncUiTasks.h"

#include <functional>
#include <future>
#include <memory>
#include <string>

class Image;

namespace meshgen
{

std::future< AsyncUiTaskValue > generateIsosurfaceMeshCpuRecord(
        const Image& image,
        uint32_t component,
        double isoValue,
        const uuids::uuid& isosurfaceUid,
        std::function< bool ( const uuids::uuid& isosurfaceUid, std::unique_ptr<MeshCpuRecord> ) > meshCpuRecordUpdater );

/// @todo Put this function here
//std::map< int64_t, double >
//generateImageHistogramAtLabelValues(
//        vtkImageData* imageData,
//        const std::unordered_set<int64_t>& labelValues );

bool writeMeshToFile( const MeshCpuRecord&, const std::string& fileName );

} // namespace meshgen

#endif // MESH_LOADER_H
