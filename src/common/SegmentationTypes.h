#ifndef SEGMENTATION_TYPES_H
#define SEGMENTATION_TYPES_H

#include <stdint.h>

using LabelType = int64_t;

enum class SeedSegmentationType
{
    Binary,
    MultiLabel
};

enum class GraphNeighborhoodType
{
    Neighbors6, // 6 face neighbors
    Neighbors26 // 26 face, edge, and vertex neighbors
};

#endif // SEGMENTATION_TYPES_H
