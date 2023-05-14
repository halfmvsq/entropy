#include "logic/segmentation/SegHelpers.h"

#include <glm/glm.hpp>


LabelIndexMaps createLabelIndexMaps(
    const glm::ivec3& dims,
    std::function< LabelType (int x, int y, int z) > getSeedValue )
{
    LabelIndexMaps labelMaps;

    std::size_t labelIndex = 0;

    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                const LabelType seedLabel = getSeedValue(x, y, z);

                // Ignore the background (0) label
                if ( seedLabel > 0 )
                {
                    const auto [iter, inserted] = labelMaps.labelToIndex.emplace( seedLabel, labelIndex );

                    if ( inserted )
                    {
                        labelMaps.indexToLabel.emplace( labelIndex++, seedLabel );
                    }
                }
            }
        }
    }

    return labelMaps;
}
