#ifndef GRAPHCUTS_H
#define GRAPHCUTS_H

#include "common/GraphCutsTypes.h"

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
    const GraphCutsNeighborhoodType& hoodType,
    double terminalCapacity,
    const LabelType& fgSeedValue,
    const LabelType& bgSeedValue,
    const glm::ivec3& dims,
    const VoxelDistances& voxelDistances,
    std::function< double (int x, int y, int z, int dx, int dy, int dz) > getImageWeight,
    std::function< LabelType (int x, int y, int z) > getSeedValue,
    std::function< void (int x, int y, int z, const LabelType& value) > setResultSegValue );

bool graphCutsMultiLabelSegmentation(
    const GraphCutsNeighborhoodType& hoodType,
    double terminalCapacity,
    const glm::ivec3& dims,
    const VoxelDistances& voxelDistances,
    std::function< double (int x, int y, int z, int dx, int dy, int dz) > getImageWeight,
    std::function< double (int index1, int index2) > getImageWeight1D,
    std::function< LabelType (int x, int y, int z) > getSeedValue,
    std::function< void (int x, int y, int z, const LabelType& value) > setResultSegValue );

#endif // GRAPHCUTS_H
