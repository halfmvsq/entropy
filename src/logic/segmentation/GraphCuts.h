#ifndef GRAPHCUTS_H
#define GRAPHCUTS_H

#include <uuid.h>
#include <glm/fwd.hpp>
#include <functional>


using LabelType = int64_t;

// Distances between voxel neighbors
struct VoxelDistances
{
    float distX;
    float distY;
    float distZ;

    float distXY;
    float distXZ;
    float distYZ;

    float distXYZ;
};


bool graphCutsBinarySegmentation(
    double terminalCapacity,
    const glm::ivec3& dims,
    const VoxelDistances& voxelDistances,
    std::function< double (int x, int y, int z, int dx, int dy, int dz) > getImageWeight,
    std::function< LabelType (int x, int y, int z) > getSeedValue,
    std::function< void (int x, int y, int z, const LabelType& value) > setResultSegValue );

bool graphCutsMultiLabelSegmentation(
    double terminalCapacity,
    const glm::ivec3& dims,
    const VoxelDistances& voxelDistances,
    std::function< double (int x, int y, int z, int dx, int dy, int dz) > getImageWeight,
    std::function< LabelType (int x, int y, int z) > getSeedValue,
    std::function< void (int x, int y, int z, const LabelType& value) > setResultSegValue );

#endif // GRAPHCUTS_H
