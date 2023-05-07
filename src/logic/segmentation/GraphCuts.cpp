#include "logic/segmentation/GraphCuts.h"

#include <GridCut/GridGraph_3D_6C.h>
#include <GridCut/GridGraph_3D_26C.h>

#include <AlphaExpansion/AlphaExpansion_3D_6C.h>
#include <AlphaExpansion/AlphaExpansion_3D_26C.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <glm/glm.hpp>

#include <unordered_set>

namespace
{
    
/**
 * @brief Compute the number of non-zero labels in a segmentation
 * @param[in] seg Segmentation image
 * @return Number of non-zero voxels/labels
 */
std::size_t computeNumLabels(
    const glm::ivec3& dims,
    std::function< LabelType (int x, int y, int z) > getSeedValue )
{
    std::unordered_set<LabelType> labels;

    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                const LabelType s = getSeedValue(x, y, z);

                if ( s > 0 )
                {
                    labels.insert(s);
                }
            }
        }
    }

    return labels.size();
}


template<typename type_tcap, typename type_ncap, typename type_flow>
class GridGraph3D_Base
{
public:

    virtual ~GridGraph3D_Base() = default;
    virtual int node_id( int x, int y, int z ) const = 0;
    virtual void set_terminal_cap( int node_id, type_tcap cap_source, type_tcap cap_sink ) = 0;
    virtual void set_neighbor_cap( int node_id, int offset_x, int offset_y, int offset_z, type_ncap cap ) = 0;
    virtual void compute_maxflow() = 0;
    virtual int get_segment( int node_id ) const = 0;
};

template<typename type_tcap, typename type_ncap, typename type_flow>
class GridGraph3D_6_Neighbors : public GridGraph3D_Base<type_tcap, type_ncap, type_flow>
{
public:

    GridGraph3D_6_Neighbors( int width, int height, int depth )
        : m_grid( std::make_unique< GridGraph_3D_6C<type_tcap, type_ncap, type_flow> >( 
            width, height, depth) ) {}

    ~GridGraph3D_6_Neighbors() override = default;

    int node_id( int x, int y, int z ) const override { return m_grid->node_id(x, y, z); }

    void set_terminal_cap( int node_id, type_tcap cap_source, type_tcap cap_sink ) override
    { m_grid->set_terminal_cap(node_id, cap_source, cap_sink); }

    void set_neighbor_cap( int node_id, int offset_x, int offset_y, int offset_z, type_ncap cap ) override
    { m_grid->set_neighbor_cap(node_id, offset_x, offset_y, offset_z, cap); }

    void compute_maxflow() override { m_grid->compute_maxflow(); }
    int get_segment( int node_id ) const override { return m_grid->get_segment(node_id); }

private:

    std::unique_ptr< GridGraph_3D_6C<type_tcap, type_ncap, type_flow> > m_grid;
};

template<typename type_tcap, typename type_ncap, typename type_flow>
class GridGraph3D_26_Neighbors : public GridGraph3D_Base<type_tcap, type_ncap, type_flow>
{
public:

    GridGraph3D_26_Neighbors( int width, int height, int depth )
        : m_grid( std::make_unique< GridGraph_3D_26C<type_tcap, type_ncap, type_flow> >(
            width, height, depth) ) {}

    ~GridGraph3D_26_Neighbors() override = default;

    int node_id( int x, int y, int z ) const override { return m_grid->node_id(x, y, z); }

    void set_terminal_cap( int node_id, type_tcap cap_source, type_tcap cap_sink ) override
    { m_grid->set_terminal_cap(node_id, cap_source, cap_sink); }

    void set_neighbor_cap( int node_id, int offset_x, int offset_y, int offset_z, type_ncap cap ) override
    { m_grid->set_neighbor_cap(node_id, offset_x, offset_y, offset_z, cap); }

    void compute_maxflow() override { m_grid->compute_maxflow(); }
    int get_segment( int node_id ) const override { return m_grid->get_segment(node_id); }

private:

    std::unique_ptr< GridGraph_3D_26C<type_tcap, type_ncap, type_flow> > m_grid;
};

template<typename type_label, typename type_cost, typename type_energy>
class AlphaExpansion3D_Base
{
public:

    virtual ~AlphaExpansion3D_Base() = default;
    virtual void perform() = 0;
    virtual void perform_random() = 0;
    virtual type_label* get_labeling() = 0;
};

template<typename type_label, typename type_cost, typename type_energy>
class AlphaExpansion3D_6_Neighbors : public AlphaExpansion3D_Base<type_label, type_cost, type_energy>
{
public:

    AlphaExpansion3D_6_Neighbors( int width, int height, int depth, int nLabels, type_cost *data, type_cost **smooth )
        : m_expansion( std::make_unique< AlphaExpansion_3D_6C<type_label, type_cost, type_energy> >(
            width, height, depth, nLabels, data, smooth ) ) {}

    ~AlphaExpansion3D_6_Neighbors() override = default;

    void perform() override { m_expansion->perform(); }
    void perform_random() override { m_expansion->perform_random(); }
    type_label* get_labeling() override { return m_expansion->get_labeling(); }

private:

    std::unique_ptr< AlphaExpansion_3D_6C<type_label, type_cost, type_energy> > m_expansion;
};

template<typename type_label, typename type_cost, typename type_energy>
class AlphaExpansion3D_26_Neighbors : public AlphaExpansion3D_Base<type_label, type_cost, type_energy>
{
public:

    AlphaExpansion3D_26_Neighbors( int width, int height, int depth, int nLabels, type_cost *data, type_cost **smooth )
        : m_expansion( std::make_unique< AlphaExpansion_3D_26C<type_label, type_cost, type_energy> >(
            width, height, depth, nLabels, data, smooth ) ) {}

    ~AlphaExpansion3D_26_Neighbors() override = default;

    void perform() override { m_expansion->perform(); }
    void perform_random() override { m_expansion->perform_random(); }
    type_label* get_labeling() override { return m_expansion->get_labeling(); }

private:

    std::unique_ptr< AlphaExpansion_3D_26C<type_label, type_cost, type_energy> > m_expansion;
};

} // anonymous


bool graphCutsBinarySegmentation(
    const GraphCutsNeighborhoodType& hoodType,
    double terminalCapacity,
    const LabelType& fgSeedValue,
    const LabelType& bgSeedValue,
    const glm::ivec3& dims,
    const VoxelDistances& voxelDistances,
    std::function< double (int x, int y, int z, int dx, int dy, int dz) > getImageWeight,
    std::function< LabelType (int x, int y, int z) > getSeedValue,
    std::function< void (int x, int y, int z, const LabelType& value) > setResultSegValue )
{
    using namespace std::chrono;

    // Type used for the graph cuts to represent:
    // -capcities of edges between nodes and terminals
    // -capacities of edges between nodes and their neighbors
    // -total flow
    using T = float;

    spdlog::debug( "Start creating grid" );
    auto start = high_resolution_clock::now();

    std::unique_ptr< GridGraph3D_Base<T, T, T> > grid = nullptr;

    switch ( hoodType )
    {
    case GraphCutsNeighborhoodType::Neighbors6:
    {
        grid = std::make_unique< GridGraph3D_6_Neighbors<T, T, T> >( dims.x, dims.y, dims.z );
        break;
    }
    case GraphCutsNeighborhoodType::Neighbors26:
    {
        grid = std::make_unique< GridGraph3D_26_Neighbors<T, T, T> >( dims.x, dims.y, dims.z );
        break;
    }
    }

    spdlog::debug( "Done creating grid" );
    auto stop = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>( stop - start );
    spdlog::debug( "Grid creation time: {} msec", duration.count() );

    if ( ! grid )
    {
        spdlog::error( "Null grid for graph cuts segmentation" );
        return false;
    }

    // Set symmetric capacities for edges from X to X + dX and from X + dX to X
    auto setCaps = [&grid, &getImageWeight]
        (int x, int y, int z, int dx, int dy, int dz, double dist)
    {
        const T cap = static_cast<T>( getImageWeight(x, y, z, dx, dy, dz) / dist );

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

                const LabelType seed = getSeedValue(x, y, z);

                grid->set_terminal_cap( grid->node_id(x, y, z),
                    (seed == bgSeedValue) ? terminalCapacity : 0,
                    (seed == fgSeedValue) ? terminalCapacity : 0 );

                // 6 face neighbors:
                if (XH) { setCaps(x, y, z, 1, 0, 0, voxelDistances.distX); }
                if (YH) { setCaps(x, y, z, 0, 1, 0, voxelDistances.distY); }
                if (ZH) { setCaps(x, y, z, 0, 0, 1, voxelDistances.distZ); }

                if ( GraphCutsNeighborhoodType::Neighbors26 == hoodType )
                {
                    // 12 edge neighbors:
                    if (XH && YH) { setCaps(x, y, z,  1,  1,  0, voxelDistances.distXY); }
                    if (XL && YH) { setCaps(x, y, z, -1,  1,  0, voxelDistances.distXY); }
                    if (XH && ZH) { setCaps(x, y, z,  1,  0,  1, voxelDistances.distXZ); }
                    if (XL && ZH) { setCaps(x, y, z, -1,  0,  1, voxelDistances.distXZ); }
                    if (YH && ZH) { setCaps(x, y, z,  0,  1,  1, voxelDistances.distYZ); }
                    if (YL && ZH) { setCaps(x, y, z,  0, -1,  1, voxelDistances.distYZ); }

                    // 8 vertex neighbors:
                    if (XH && YH && ZH) { setCaps(x, y, z,  1,  1,  1, voxelDistances.distXYZ); }
                    if (XL && YH && ZH) { setCaps(x, y, z, -1,  1,  1, voxelDistances.distXYZ); }
                    if (XH && YL && ZH) { setCaps(x, y, z,  1, -1,  1, voxelDistances.distXYZ); }
                    if (XH && YH && ZL) { setCaps(x, y, z,  1,  1, -1, voxelDistances.distXYZ); }
                }
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
                    grid->get_segment( grid->node_id(x, y, z) )
                        ? fgSeedValue : bgSeedValue );

                setResultSegValue(x, y, z, seg);
            }
        }
    }
    spdlog::debug( "Done reading back segmentation results" );

    return true;
}


bool graphCutsMultiLabelSegmentation(
    const GraphCutsNeighborhoodType& hoodType,
    double terminalCapacity,
    const glm::ivec3& dims,
    const VoxelDistances& voxelDistances,
    std::function< double (int x, int y, int z, int dx, int dy, int dz) > getImageWeight,
    std::function< LabelType (int x, int y, int z) > getSeedValue,
    std::function< void (int x, int y, int z, const LabelType& value) > setResultSegValue )
{
    using namespace std::chrono;

    // Type used for the alpha expansion algorithm to represent:
    // -data and smoothness costs
    // -resulting energy
    using T = float;

    const std::size_t numLabels = computeNumLabels(dims, getSeedValue);

    spdlog::debug( "Start creating expansion" );

    std::vector<T> dataCosts( dims.x * dims.y * dims.z * numLabels, 0 );

    auto getIndex = [&dims] (int x, int y, int z) -> std::size_t
    {
        return z * (dims.x * dims.y) + y * dims.x + x;
    };

    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                const LabelType seed = getSeedValue(x, y, z);

                for ( std::size_t label = 0; label < numLabels; ++label )
                {
                    dataCosts[getIndex(x, y, z) * numLabels + label] =
                        ( seed == static_cast<LabelType>(label + 1) ) ? 0 : terminalCapacity;
                }
            }
        }
    }

    // For 3D grids with 26 connected neighboring system,
    // there are thirteen smoothness tables for each pixel.
    const std::size_t numSmoothnessTables =
        ( GraphCutsNeighborhoodType::Neighbors6 == hoodType ) ? 3 : 13;

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

                        if ( GraphCutsNeighborhoodType::Neighbors6 == hoodType )
                        {
                            smoothnessCosts[numSmoothnessTables * index +  0][l] = (XH && B) ? getImageWeight(x, y, z,  1,  0,  0) / voxelDistances.distX : 0;
                            smoothnessCosts[numSmoothnessTables * index +  1][l] = (YH && B) ? getImageWeight(x, y, z,  0,  1,  0) / voxelDistances.distY : 0;
                            smoothnessCosts[numSmoothnessTables * index +  2][l] = (ZH && B) ? getImageWeight(x, y, z,  0,  0,  1) / voxelDistances.distZ : 0;
                        }
                        else
                        {
                            smoothnessCosts[numSmoothnessTables * index +  0][l] = (XH && B)             ? getImageWeight(x, y, z,  1,  0,  0) / voxelDistances.distX   : 0;
                            smoothnessCosts[numSmoothnessTables * index +  1][l] = (YH && B)             ? getImageWeight(x, y, z,  0,  1,  0) / voxelDistances.distY   : 0;
                            smoothnessCosts[numSmoothnessTables * index +  2][l] = (XH && YL && B)       ? getImageWeight(x, y, z,  1, -1,  0) / voxelDistances.distXY  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  3][l] = (XH && YH && B)       ? getImageWeight(x, y, z,  1,  1,  0) / voxelDistances.distXY  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  4][l] = (ZH && B)             ? getImageWeight(x, y, z,  0,  0,  1) / voxelDistances.distZ   : 0;
                            smoothnessCosts[numSmoothnessTables * index +  5][l] = (YL && ZH && B)       ? getImageWeight(x, y, z,  0, -1,  1) / voxelDistances.distYZ  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  6][l] = (YH && ZH && B)       ? getImageWeight(x, y, z,  0,  1,  1) / voxelDistances.distYZ  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  7][l] = (XL && ZH && B)       ? getImageWeight(x, y, z, -1,  0,  1) / voxelDistances.distXZ  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  8][l] = (XL && YL && ZH && B) ? getImageWeight(x, y, z, -1, -1,  1) / voxelDistances.distXYZ : 0;
                            smoothnessCosts[numSmoothnessTables * index +  9][l] = (XL && YH && ZH && B) ? getImageWeight(x, y, z, -1,  1,  1) / voxelDistances.distXYZ : 0;
                            smoothnessCosts[numSmoothnessTables * index + 10][l] = (XH && ZH && B)       ? getImageWeight(x, y, z,  1,  0,  1) / voxelDistances.distXZ  : 0;
                            smoothnessCosts[numSmoothnessTables * index + 11][l] = (XH && YL && ZH && B) ? getImageWeight(x, y, z,  1, -1,  1) / voxelDistances.distXYZ : 0;
                            smoothnessCosts[numSmoothnessTables * index + 12][l] = (XH && YH && ZH && B) ? getImageWeight(x, y, z,  1,  1,  1) / voxelDistances.distXYZ : 0;
                        }
                    }
                }
            }
        }
    }

    std::unique_ptr< AlphaExpansion3D_Base<LabelType, T, T> > expansion = nullptr;

    switch ( hoodType )
    {
    case GraphCutsNeighborhoodType::Neighbors6:
    {
        expansion = std::make_unique< AlphaExpansion3D_6_Neighbors<LabelType, T, T> >(
            dims.x, dims.y, dims.z, numLabels, dataCosts.data(), smoothnessCosts );
        break;
    }
    case GraphCutsNeighborhoodType::Neighbors26:
    {
        expansion = std::make_unique< AlphaExpansion3D_26_Neighbors<LabelType, T, T> >(
            dims.x, dims.y, dims.z, numLabels, dataCosts.data(), smoothnessCosts );
        break;
    }
    }
    
    spdlog::debug( "Done creating expansion" );

    spdlog::debug( "Start performing expansion" );
    expansion->perform_random();
    spdlog::debug( "Done performing expansion" );

    spdlog::debug( "Start reading back segmentation results" );
    LabelType* labeling = expansion->get_labeling();

    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                setResultSegValue(x, y, z, labeling[getIndex(x, y, z)] + 1);
            }
        }
    }
    spdlog::debug( "Done reading back segmentation results" );

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
