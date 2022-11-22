#include "logic/annotation/AnnotationHelper.h"
#include "logic/annotation/AnnotationGroup.h"
#include "logic/annotation/Polygon.h"

#include "logic/AppData.h"

#include <mapbox/earcut.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <iostream>
#include <list>
#include <utility>


namespace mapbox
{
namespace util
{

template <>
struct nth<0, Polygon::PointType>
{
    inline static auto get( const Polygon::PointType& point )
    {
        return point[0];
    }
};

template <>
struct nth<1, Polygon::PointType>
{
    inline static auto get( const Polygon::PointType& point )
    {
        return point[1];
    }
};

} // namespace util
} // namespace mapbox


void triangulatePolygon( Polygon& polygon )
{
    polygon.setTriangulation( mapbox::earcut<Polygon::IndexType>( polygon.getAllVertices() ) );
}
