#include "logic/segmentation/GraphCuts.h"

#include "image/Image.h"

#include "logic/app/Data.h"

#include "rendering/Rendering.h"

// #include <GridCut/GridGraph_3D_6C.h>
#include <GridCut/GridGraph_3D_26C.h>
#include <AlphaExpansion/AlphaExpansion_3D_26C.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <glm/glm.hpp>

#include <unordered_set>


namespace
{

using LabelType = int64_t;

// Distances between voxel neighbors.
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


VoxelDistances computeVoxelDistances( const glm::vec3& spacing, bool normalized )
{
    VoxelDistances v;

    const double L = ( normalized ) ? glm::length( spacing ) : 1.0f;

    v.distXYZ = glm::length( spacing ) / L;

    v.distX = glm::length( glm::vec3{spacing.x, 0, 0} ) / L;
    v.distY = glm::length( glm::vec3{0, spacing.y, 0} ) / L;
    v.distZ = glm::length( glm::vec3{0, 0, spacing.z} ) / L;

    v.distXY = glm::length( glm::vec3{spacing.x, spacing.y, 0} ) / L;
    v.distXZ = glm::length( glm::vec3{spacing.x, 0, spacing.z} ) / L;
    v.distYZ = glm::length( glm::vec3{0, spacing.y, spacing.z} ) / L;

    return v;
}


/**
 * @brief Compute the number of non-zero labels in a segmentation
 * @param[in] seg Segmentation image
 * @return Number of non-zero voxels/labels
 */
std::size_t computeNumLabels( const Image& seg )
{
    std::unordered_set<LabelType> labels;

    const glm::uvec3 dims = seg.header().pixelDimensions();

    for ( std::size_t z = 0; z < dims.z; ++z ) {
        for ( std::size_t y = 0; y < dims.y; ++y ) {
            for ( std::size_t x = 0; x < dims.x; ++x )
            {
                const LabelType s = seg.valueAsInt64(0, x, y, z).value_or(0);

                if ( s > 0 )
                {
                    labels.insert( s );
                }
            }
        }
    }

    return labels.size();
}

} // anonymous


bool graphCutsFgBgSegmentation(
    AppData& appData,
    Rendering& rendering,
    const uuids::uuid& imageUid,
    const uuids::uuid& seedSegUid,
    const uuids::uuid& resultSegUid )
{
    using namespace std::chrono;

    // Type used for the graph cuts to represent:
    // -capcities of edges between nodes and terminals
    // -capacities of edges between nodes and their neighbors
    // -total flow
    using T = float;

    using Grid = GridGraph_3D_26C<T, T, T>;


    static constexpr LabelType FG_SEED = 1; // Foreground segmentation value
    static constexpr LabelType BG_SEED = 2; // Background segmentation value

    const Image* image = appData.image( imageUid );
    const Image* seedSeg = appData.seg( seedSegUid );
    Image* resultSeg = appData.seg( resultSegUid );

    if ( ! image )
    {
        spdlog::error( "Null image {} to segment using graph cuts", imageUid );
        return false;
    }

    if ( ! seedSeg )
    {
        spdlog::error( "Null seed segmentation {} for graph cuts", seedSegUid );
        return false;
    }

    if ( ! resultSeg )
    {
        spdlog::error( "Null result segmentation {} for graph cuts", resultSegUid );
        return false;
    }

    const glm::ivec3 dims{ image->header().pixelDimensions() };
    const glm::vec3 spacing{ image->header().spacing() };

    if ( image->header().pixelDimensions() != seedSeg->header().pixelDimensions() )
    {
        spdlog::error( "Dimensions of image {} ({}) and seed segmentation {} ({}) do not match",
                      imageUid, glm::to_string(dims), seedSegUid,
                      glm::to_string( seedSeg->header().pixelDimensions() ) );
        return false;
    }

    if ( image->header().pixelDimensions() != resultSeg->header().pixelDimensions() )
    {
        spdlog::error( "Dimensions of image {} ({}) and result segmentation {} ({}) do not match",
                      imageUid, glm::to_string(dims), resultSegUid,
                      glm::to_string( resultSeg->header().pixelDimensions() ) );
        return false;
    }

    const T amplitude = static_cast<T>( appData.settings().graphCutsWeightsAmplitude() );
    const T sigma = static_cast<T>( appData.settings().graphCutsWeightsSigma() );

    auto weight = [&amplitude, &sigma] (double a) -> T
    {
        return static_cast<T>( amplitude * std::exp( -0.5 * std::pow(a / sigma, 2.0) ) );
    };

    spdlog::info( "Executing graph cuts on image {} with seeds {}", imageUid, seedSegUid );
    spdlog::info( "Amplitude = {}, sigma = {}", amplitude, sigma );

    spdlog::debug( "Start creating grid" );
    auto start = high_resolution_clock::now();

    auto grid = std::make_unique<Grid>( dims.x, dims.y, dims.z );

    spdlog::debug( "Done creating grid" );
    auto stop = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>( stop - start );
    spdlog::debug( "Grid creation time: {} msec", duration.count() );

    if ( ! grid )
    {
        spdlog::error( "Null grid for graph cuts segmentation" );
        return false;
    }

    const uint32_t imComp = image->settings().activeComponent();

    const VoxelDistances dists = computeVoxelDistances( spacing, true );

    auto getWeight = [&weight, &image, &imComp]
        (int x, int y, int z, int dx, int dy, int dz, double dist) -> T
    {
        const auto a = image->valueAsDouble(imComp, x, y, z);
        const auto b = image->valueAsDouble(imComp, x + dx, y + dy, z + dz);

        if ( a && b ) { return static_cast<T>( weight((*a) - (*b)) / dist ); }
        else { return static_cast<T>(0); }
    };

    // Set symmetric capacities for edges from X to X + dX and from X + dX to X.
    auto setCaps = [&grid, &getWeight] (int x, int y, int z, int dx, int dy, int dz, double dist)
    {
        const T cap = getWeight(x, y, z, dx, dy, dz, dist);
        grid->set_neighbor_cap( grid->node_id(x, y, z), dx, dy, dz, cap );
        grid->set_neighbor_cap( grid->node_id(x + dx, y + dy, z + dz), -dx, -dy, -dz, cap );
    };

    spdlog::debug( "Start filling grid" );
    start = high_resolution_clock::now();

    for ( int z = 0; z < dims.z; ++z )
    {
        const bool ZL = ( z > 0 );
        const bool ZH = ( z < ( dims.z - 1 ) );

        for ( int y = 0; y < dims.y; ++y )
        {
            const bool YL = ( y > 0 );
            const bool YH = ( y < ( dims.y - 1 ) );

            for ( int x = 0; x < dims.x; ++x )
            {
                const bool XL = ( x > 0 );
                const bool XH = ( x < ( dims.x - 1 ) );

                const LabelType seed = seedSeg->valueAsInt64(imComp, x, y, z).value_or(0);

                grid->set_terminal_cap( grid->node_id(x, y, z),
                                       (seed == BG_SEED) ? amplitude : 0,
                                       (seed == FG_SEED) ? amplitude : 0 );

                // 6 face neighbors:
                if (XH) { setCaps(x, y, z, 1, 0, 0, dists.distX); }
                if (YH) { setCaps(x, y, z, 0, 1, 0, dists.distY); }
                if (ZH) { setCaps(x, y, z, 0, 0, 1, dists.distZ); }

                // 12 edge neighbors:
                if (XH && YH) { setCaps(x, y, z,  1,  1,  0, dists.distXY); }
                if (XL && YH) { setCaps(x, y, z, -1,  1,  0, dists.distXY); }
                if (XH && ZH) { setCaps(x, y, z,  1,  0,  1, dists.distXZ); }
                if (XL && ZH) { setCaps(x, y, z, -1,  0,  1, dists.distXZ); }
                if (YH && ZH) { setCaps(x, y, z,  0,  1,  1, dists.distYZ); }
                if (YL && ZH) { setCaps(x, y, z,  0, -1,  1, dists.distYZ); }

                // 8 vertex neighbors:
                if (XH && YH && ZH) { setCaps(x, y, z,  1,  1,  1, dists.distXYZ); }
                if (XL && YH && ZH) { setCaps(x, y, z, -1,  1,  1, dists.distXYZ); }
                if (XH && YL && ZH) { setCaps(x, y, z,  1, -1,  1, dists.distXYZ); }
                if (XH && YH && ZL) { setCaps(x, y, z,  1,  1, -1, dists.distXYZ); }
            }
        }
    }
    spdlog::debug( "Done filling grid" );
    stop = high_resolution_clock::now();

    duration = duration_cast<milliseconds>( stop - start );
    spdlog::debug( "Grid fill time: {} msec", duration.count() );


    spdlog::debug( "Start computing max flow" );
    start = high_resolution_clock::now();
    {
        grid->compute_maxflow();
    }
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>( stop - start );

    spdlog::debug( "Done computing max flow" );
    spdlog::debug( "Graph cuts execution time: {} msec", duration.count() );


    spdlog::debug( "Start reading back segmentation results" );
    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                const LabelType seg = static_cast<LabelType>(
                    grid->get_segment( grid->node_id(x, y, z) ) ? 1 : 0 );

                resultSeg->setValue(imComp, x, y, z, seg);

                // Fill in background seeds again
                // const std::optional<LabelType> optSeed = seedSeg->valueAsInt64( 0, x, y, z );
                // const LabelType seed = optSeed ? *optSeed : 0;

                // if ( 2 == seed && 0 == seg )
                // {
                //     resultSeg->setValue( 0, x, y, z, seed );
                // }
            }
        }
    }
    spdlog::debug( "Done reading back segmentation results" );


    const glm::uvec3 dataOffset = glm::uvec3{ 0 };
    const glm::uvec3 dataSize = glm::uvec3{ resultSeg->header().pixelDimensions() };

    spdlog::debug( "Start updating segmentation texture" );

    rendering.updateSegTexture(
        resultSegUid, resultSeg->header().memoryComponentType(),
        dataOffset, dataSize,
        resultSeg->bufferAsVoid( 0 ) );

    spdlog::debug( "Done updating segmentation texture" );

    return true;
}


bool graphCutsMultiLabelSegmentation(
    AppData& appData,
    Rendering& rendering,
    const uuids::uuid& imageUid,
    const uuids::uuid& seedSegUid,
    const uuids::uuid& resultSegUid )
{
    using namespace std::chrono;

    // Type used for the alpha expansion algorithm to represent:
    // -data and smoothness costs
    // -resulting energy
    using T = float;
    using Expansion = AlphaExpansion_3D_26C<LabelType, T, T>;

    const Image* image = appData.image( imageUid );
    const Image* seedSeg = appData.seg( seedSegUid );
    Image* resultSeg = appData.seg( resultSegUid );

    if ( ! image )
    {
        spdlog::error( "Null image {} to segment using graph cuts", imageUid );
        return false;
    }

    if ( ! seedSeg )
    {
        spdlog::error( "Null seed segmentation {} for graph cuts", seedSegUid );
        return false;
    }

    if ( ! resultSeg )
    {
        spdlog::error( "Null result segmentation {} for graph cuts", resultSegUid );
        return false;
    }

    const uint32_t imComp = image->settings().activeComponent();

    const glm::ivec3 dims{ image->header().pixelDimensions() };
    const glm::vec3 spacing{ image->header().spacing() };

    if ( image->header().pixelDimensions() != seedSeg->header().pixelDimensions() )
    {
        spdlog::error( "Dimensions of image {} ({}) and seed segmentation {} ({}) do not match",
                      imageUid, glm::to_string(dims), seedSegUid,
                      glm::to_string( seedSeg->header().pixelDimensions() ) );
        return false;
    }

    if ( image->header().pixelDimensions() != resultSeg->header().pixelDimensions() )
    {
        spdlog::error( "Dimensions of image {} ({}) and result segmentation {} ({}) do not match",
                      imageUid, glm::to_string(dims), resultSegUid,
                      glm::to_string( resultSeg->header().pixelDimensions() ) );
        return false;
    }

    const T amplitude = static_cast<T>( appData.settings().graphCutsWeightsAmplitude() );
    const T sigma = static_cast<T>( appData.settings().graphCutsWeightsSigma() );

    const VoxelDistances dists = computeVoxelDistances( spacing, true );

    auto weight = [&amplitude, &sigma] (double a) -> T
    {
        return static_cast<T>( amplitude * std::exp( -0.5 * std::pow(a / sigma, 2.0) ) );
    };

    auto getWeight = [&weight, &image, &imComp]
        (int x, int y, int z, int dx, int dy, int dz, double dist) -> T
    {
        const auto a = image->valueAsDouble(imComp, x, y, z);
        const auto b = image->valueAsDouble(imComp, x + dx, y + dy, z + dz);

        if ( a && b ) { return static_cast<T>( weight((*a) - (*b)) / dist ); }
        else { return static_cast<T>(0); }
    };

    spdlog::debug( "Start creating expansion" );

    const std::size_t numLabels = computeNumLabels( *seedSeg );

    std::vector<T> dataCosts( dims.x * dims.y * dims.z * numLabels, 0 );

    auto getIndex = [&dims] (int x, int y, int z) -> std::size_t
    {
        return z * (dims.x * dims.y) + y * dims.x + x;
    };

    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                const LabelType seed = seedSeg->valueAsInt64(imComp, x, y, z).value_or(0);

                for ( std::size_t label = 0; label < numLabels; ++label )
                {
                    dataCosts[getIndex(x, y, z) * numLabels + label] =
                        ( seed == static_cast<LabelType>(label + 1) ) ? 0 : amplitude;
                }
            }
        }
    }

    // For 3D grids with 26 connected neighboring system,
    // there are thirteen smoothness tables for each pixel.
    static constexpr std::size_t numSmoothnessTables = 13;

    T** smoothnessCosts = new T*[numSmoothnessTables * dims.x * dims.y * dims.z];

    // Set smoothness constraints for the 6 face-, 12 edge-, and 8 vertex-neighbors.
    // The order is specified in the AlphaExpansion documentation.

    for ( int z = 0; z < dims.z; ++z )
    {
//        const bool ZL = ( z > 0 );
        const bool ZH = ( z < ( dims.z - 1 ) );

        for ( int y = 0; y < dims.y; ++y )
        {
            const bool YL = ( y > 0 );
            const bool YH = ( y < ( dims.y - 1 ) );

            for ( int x = 0; x < dims.x; ++x )
            {
                const bool XL = ( x > 0 );
                const bool XH = ( x < ( dims.x - 1 ) );

                const std::size_t index = getIndex(x, y, z);

                for ( std::size_t t = 0; t < numSmoothnessTables; ++t )
                {
                    smoothnessCosts[numSmoothnessTables * index + t] = new T[numLabels * numLabels];
                }

                for ( std::size_t label = 0; label < numLabels; ++label )
                {
                    for ( std::size_t otherLabel = 0; otherLabel < numLabels; ++otherLabel )
                    {
                        const size_t l = otherLabel * numLabels + label;
                        const bool B = ( label != otherLabel );

                        smoothnessCosts[numSmoothnessTables * index +  0][l] = (XH && B)             ? getWeight(x, y, z,  1,  0,  0, dists.distX)   : 0;
                        smoothnessCosts[numSmoothnessTables * index +  1][l] = (YH && B)             ? getWeight(x, y, z,  0,  1,  0, dists.distY)   : 0;
                        smoothnessCosts[numSmoothnessTables * index +  2][l] = (XH && YL && B)       ? getWeight(x, y, z,  1, -1,  0, dists.distXY)  : 0;
                        smoothnessCosts[numSmoothnessTables * index +  3][l] = (XH && YH && B)       ? getWeight(x, y, z,  1,  1,  0, dists.distXY)  : 0;
                        smoothnessCosts[numSmoothnessTables * index +  4][l] = (ZH && B)             ? getWeight(x, y, z,  0,  0,  1, dists.distZ)   : 0;
                        smoothnessCosts[numSmoothnessTables * index +  5][l] = (YL && ZH && B)       ? getWeight(x, y, z,  0, -1,  1, dists.distYZ)  : 0;
                        smoothnessCosts[numSmoothnessTables * index +  6][l] = (YH && ZH && B)       ? getWeight(x, y, z,  0,  1,  1, dists.distYZ)  : 0;
                        smoothnessCosts[numSmoothnessTables * index +  7][l] = (XL && ZH && B)       ? getWeight(x, y, z, -1,  0,  1, dists.distXZ)  : 0;
                        smoothnessCosts[numSmoothnessTables * index +  8][l] = (XL && YL && ZH && B) ? getWeight(x, y, z, -1, -1,  1, dists.distXYZ) : 0;
                        smoothnessCosts[numSmoothnessTables * index +  9][l] = (XL && YH && ZH && B) ? getWeight(x, y, z, -1,  1,  1, dists.distXYZ) : 0;
                        smoothnessCosts[numSmoothnessTables * index + 10][l] = (XH && ZH && B)       ? getWeight(x, y, z,  1,  0,  1, dists.distXZ)  : 0;
                        smoothnessCosts[numSmoothnessTables * index + 11][l] = (XH && YL && ZH && B) ? getWeight(x, y, z,  1, -1,  1, dists.distXYZ) : 0;
                        smoothnessCosts[numSmoothnessTables * index + 12][l] = (XH && YH && ZH && B) ? getWeight(x, y, z,  1,  1,  1, dists.distXYZ) : 0;
                    }
                }
            }
        }
    }

    //    auto smoothnessCostFn = [] ( int X, int Y, int LX, int LY )
    //    {

    //    };

    auto expansion = std::make_unique<Expansion>(
        dims.x, dims.y, dims.z, numLabels,
        dataCosts.data(), smoothnessCosts );

    spdlog::debug( "Done creating expansion" );

    spdlog::debug( "Start performing expansion" );
    expansion->perform();
    spdlog::debug( "Done performing expansion" );


    spdlog::debug( "Start reading back segmentation results" );
    LabelType* labeling = expansion->get_labeling();

    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                resultSeg->setValue( imComp, x, y, z, labeling[getIndex(x, y, z)] + 1 );
            }
        }
    }
    spdlog::debug( "Done reading back segmentation results" );


    const glm::uvec3 dataOffset = glm::uvec3{ 0 };
    const glm::uvec3 dataSize = glm::uvec3{ resultSeg->header().pixelDimensions() };

    spdlog::debug( "Start updating segmentation texture" );
    rendering.updateSegTexture(
        resultSegUid, resultSeg->header().memoryComponentType(),
        dataOffset, dataSize,
        resultSeg->bufferAsVoid( 0 ) );
    spdlog::debug( "Done updating segmentation texture" );


    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                const std::size_t index = getIndex(x, y, z);

                for ( std::size_t t = 0; t < numSmoothnessTables; ++t )
                {
                    delete[] smoothnessCosts[numSmoothnessTables * index + t];
                    smoothnessCosts[numSmoothnessTables * index + t] = nullptr;
                }
            }
        }
    }

    delete[] smoothnessCosts;
    smoothnessCosts = nullptr;

    return true;
}
