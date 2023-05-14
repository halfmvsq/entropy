#ifndef SEGMENTATION_HELPERS_H
#define SEGMENTATION_HELPERS_H

#include "common/SegmentationTypes.h"

#include <glm/fwd.hpp>

#include <functional>
#include <map>


struct LabelIndexMaps
{
    /// Map from segmentation label to label index
    std::map<LabelType, std::size_t> labelToIndex;

    /// Map from label index to segmentation label
    std::map<std::size_t, LabelType> indexToLabel;
};


/**
 * @brief Compute the number of non-zero labels in a segmentation
 */
LabelIndexMaps createLabelIndexMaps(
    const glm::ivec3& dims,
    std::function< LabelType (int x, int y, int z) > getSeedValue );

#endif // SEGMENTATION_HELPERS_H
