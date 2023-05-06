#ifndef GRAPH_CUTS_TYPES_H
#define GRAPH_CUTS_TYPES_H

enum class GraphCutsSegmentationType
{
    Binary,
    MultiLabel
};

enum class GraphCutsNeighborhoodType
{
    Neighbors6, // 6 face neighbors
    Neighbors26 // 26 face, edge, and vertex neighbors
};

#endif // GRAPH_CUTS_TYPES_H