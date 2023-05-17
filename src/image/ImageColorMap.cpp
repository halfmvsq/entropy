#include "image/ImageColorMap.h"
#include "common/Exception.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <boost/tokenizer.hpp>

#include <algorithm>
#include <cmath>
#include <set>


ImageColorMap::ImageColorMap(
    std::string name,
    std::string technicalName,
    std::string description,
    std::vector< glm::vec3 > colors )
    :
    m_name( std::move( name ) ),
    m_technicalName( std::move( technicalName ) ),
    m_description( std::move( description ) ),
    m_preview( 0 ),
    m_interpolationMode( InterpolationMode::Linear )
{
    if ( colors.empty() )
    {
        throw_debug( "Empty color map" )
    }

    for ( const auto& x : colors )
    {
        m_colors_RGBA_F32.push_back( glm::vec4{ x.r, x.g, x.b, 1.0f } );
    }
}

ImageColorMap::ImageColorMap(
    std::string name,
    std::string technicalName,
    std::string description,
    std::vector< glm::vec4 > colors )
    :
    m_name( std::move( name ) ),
    m_technicalName( std::move( technicalName ) ),
    m_description( std::move( description ) ),
    m_colors_RGBA_F32( std::move( colors ) ),
    m_preview( 0 )
{
    if ( m_colors_RGBA_F32.empty() )
    {
        throw_debug( "Empty color map" )
    }
}


void ImageColorMap::setPreviewMap( std::vector< glm::vec4 > preview )
{
    m_preview = std::move( preview );
}


bool ImageColorMap::hasPreviewMap() const
{
    return ( ! m_preview.empty() );
}


size_t ImageColorMap::numPreviewMapColors() const
{
    return m_preview.size();
}


const float* ImageColorMap::getPreviewMap() const
{
    return glm::value_ptr( m_preview[0] );
}


const std::string& ImageColorMap::name() const
{
    return m_name;
}


const std::string& ImageColorMap::technicalName() const
{
    return m_technicalName;
}


const std::string& ImageColorMap::description() const
{
    return m_description;
}


size_t ImageColorMap::numColors() const
{
    return m_colors_RGBA_F32.size();
}


glm::vec4 ImageColorMap::color_RGBA_F32( size_t index ) const
{
    if ( index >= m_colors_RGBA_F32.size() )
    {
        std::ostringstream ss;
        ss << "Invalid color map index " << index << std::ends;
        throw_debug( ss.str() )
    }

    return m_colors_RGBA_F32.at( index );
}


size_t ImageColorMap::numBytes_RGBA_F32() const
{
    return m_colors_RGBA_F32.size() * sizeof( glm::vec4 );
}


const float* ImageColorMap::data_RGBA_F32() const
{
    return glm::value_ptr( m_colors_RGBA_F32[0] );
}


glm::vec2 ImageColorMap::slopeIntercept( bool inverted ) const
{
    const float N = static_cast<float>( numColors() );
    const float slope = ( inverted ? -( N - 1.0f ) / N : ( N - 1.0f ) / N );
    const float intercept = ( inverted ? ( N - 0.5f ) / N : 0.5f / N );
    return glm::vec2{ slope, intercept };
}


void ImageColorMap::cyclicRotate( float fraction )
{
    const float f = ( fraction < 0.0f ) ? 1.0f - fraction : fraction;
    const int middle = static_cast<int>( f * static_cast<float>( m_colors_RGBA_F32.size() ) );

    std::rotate( std::begin( m_colors_RGBA_F32 ),
                 std::begin( m_colors_RGBA_F32 ) + middle,
                 std::end( m_colors_RGBA_F32 ) );
}


void ImageColorMap::reverse()
{
    std::reverse( std::begin( m_colors_RGBA_F32 ), std::end( m_colors_RGBA_F32 ) );
}


tex::SizedInternalFormat ImageColorMap::textureFormat_RGBA_F32()
{
    static const tex::SizedInternalFormat sk_format = tex::SizedInternalFormat::RGBA32F;
    return sk_format;
}


ImageColorMap ImageColorMap::loadImageColorMap( std::istringstream& csv )
{
    /// @todo Throws are not needed here. Just return flag that colormap could not be loaded.

    auto predicate = [] ( const auto& c ) -> bool
    {
        static const std::set<char> allowedChars{ ' ', '-', '_', '(', ')' };
        const auto e = std::end( allowedChars );
        return ! ( std::isalnum( c ) || e != allowedChars.find( c ) );
    };

    // Read names and description from first three lines
    std::string briefName; // line 1
    std::string technicalName; // line 2
    std::string description; // line 3

    std::string line;  

    if ( std::getline( csv, line ) )
    {
        briefName = line;
        briefName.erase( std::remove_if( std::begin(briefName), std::end(briefName), predicate ), std::end(briefName) );

    }
    else
    {
        spdlog::error( "Could not extract brief name of colormap from CSV" );
        throw_debug( "Invalid colormap: could not load name" )
    }

    if ( std::getline( csv, line ) )
    {
        technicalName = line;
        technicalName.erase( std::remove_if( std::begin(technicalName), std::end(technicalName), predicate ), std::end(technicalName) );
    }
    else
    {
        spdlog::error( "Could not extract technical name of colormap '{}'", briefName );
        throw_debug( "Invalid colormap: could not load technical name" )
    }

    if ( std::getline( csv, line ) )
    {
        description = line;
        description.erase( std::remove_if( std::begin(description), std::end(description), predicate ), std::end(description) );
    }
    else
    {
        spdlog::error( "Could not extract description of colormap '{}'", briefName );
        throw_debug( "Invalid colormap: could not load description" )
    }

    // Read a color from each line of the file
    std::vector< glm::vec4 > colors;
    size_t count = 0;

    while ( getline( csv, line ) )
    {
        boost::tokenizer< boost::escaped_list_separator<char> > tok( line );
        std::vector< std::string > c;
        c.assign( tok.begin(), tok.end() );

        if ( 3 == c.size() )
        {
            // Assume alpha component is 1:
            const float r = std::stof( c[0], nullptr );
            const float g = std::stof( c[1], nullptr );
            const float b = std::stof( c[2], nullptr );
            colors.push_back( glm::vec4{ r, g, b, 1 } );
        }
        else if ( 4 == c.size() )
        {
            // Pre-multiple the alpha component:
            const float r = std::stof( c[0], nullptr );
            const float g = std::stof( c[1], nullptr );
            const float b = std::stof( c[2], nullptr );
            const float a = std::stof( c[3], nullptr );
            colors.push_back( a * glm::vec4{ r, g, b, 1 } );
        }
        else
        {
            spdlog::error( "Invalid color map \"{}\": Color {} has {} components", briefName, count, c.size() );
            throw_debug( "Invalid colormap: missing color components" )
        }

        ++count;
    }

    if ( colors.empty() )
    {
        spdlog::error( "Invalid color map '{}' has no colors", briefName );
        throw_debug( "Invalid colormap: no colors" )
    }

    spdlog::trace( "Loaded image color map \"{}\" (\"{}\") with {} colors", briefName, technicalName, colors.size() );

    return ImageColorMap( std::move( briefName ), std::move( technicalName ), std::move( description ), std::move( colors ) );
}


ImageColorMap ImageColorMap::createLinearImageColorMap(
        const glm::vec3& startColor,
        const glm::vec3& endColor,
        size_t numSteps,
        std::string briefName,
        std::string description,
        std::string technicalName )
{
    const size_t N = ( numSteps >= 2 ? numSteps : 2 );

    // Number of pixels in preview image of the color map
    static constexpr int sk_previewSize = 64;

    // Linearly interpolate between start and end colors
    std::vector< glm::vec3 > colors( N );

    const float Nm1 = static_cast<float>( N - 1 );

    for ( size_t i = 0; i < N; ++i )
    {
        colors[i] = startColor + static_cast<float>( i ) / Nm1 * endColor;
    }

    ImageColorMap map( std::move( briefName ), std::move( technicalName ),
                       std::move( description ), std::move( colors ) );

    std::vector< glm::vec4 > previewColors( sk_previewSize );
    for ( uint32_t i = 0; i < sk_previewSize; ++i )
    {
        previewColors[i] = glm::vec4{ glm::vec3{ static_cast<float>( i ) / sk_previewSize }, 1.0f };
    }

    map.setPreviewMap( previewColors );

    return map;
}


void ImageColorMap::setInterpolationMode( const ImageColorMap::InterpolationMode& mode )
{
    m_interpolationMode = mode;
}

ImageColorMap::InterpolationMode ImageColorMap::interpolationMode() const
{
    return m_interpolationMode;
}
