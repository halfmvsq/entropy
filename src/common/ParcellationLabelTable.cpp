#include "common/ParcellationLabelTable.h"
#include "common/Exception.hpp"
#include "common/MathFuncs.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <sstream>
#include <utility>


namespace
{

static constexpr size_t sk_seed = 1234567890;

static const auto sk_hueMinMax = std::make_pair( 0.0f, 360.0f );
static const auto sk_satMinMax = std::make_pair( 0.5f, 1.0f );
static const auto sk_valMinMax = std::make_pair( 0.5f, 1.0f );

}


ParcellationLabelTable::ParcellationLabelTable( size_t labelCount, size_t maxLabelCount )
    :
      m_colors_RGBA_F32(),
      m_properties(),
      m_maxLabelCount( maxLabelCount )
{
    static const std::vector<float> sk_startAngles{
        0.0f, 120.0f, 240.0f, 60.0f, 180.0f, 300.0f };

    if ( labelCount < 7 )
    {
        throw_debug( "Parcellation must have at least 7 labels" )
    }

    if ( labelCount > maxLabelCount )
    {
        throw_debug( "Label count exceeds maximum" )
    }

    std::vector< glm::vec3 > rgbValues;

    // The first label (0) is always black:
    rgbValues.push_back( glm::vec3{ 0.0f, 0.0f, 0.0f } );

    // Insert the six primary colors for labels 1-7:
    for ( float s : sk_startAngles )
    {
        rgbValues.push_back( glm::rgbColor( glm::vec3{ s, 1.0f, 1.0f } ) );
    }

    const std::vector< glm::vec3 > hsvSamples = math::generateRandomHsvSamples(
                labelCount - 7, sk_hueMinMax, sk_satMinMax, sk_valMinMax, sk_seed );

    std::transform( std::begin( hsvSamples ), std::end( hsvSamples ),
                    std::back_inserter( rgbValues ),
                    glm::rgbColor< float, glm::precision::defaultp > );


    m_colors_RGBA_F32.resize( labelCount );

    for ( size_t i = 0; i < labelCount; ++i )
    {
        LabelProperties props;

        std::ostringstream ss;

        if ( 0 == i )
        {
            // Label index 0 is always used as the background label,
            // so it is fully transparent and not visible in 2D/3D views
            ss << "Background";
            props.m_alpha = 0.0f;
            props.m_visible = false;
            props.m_showMesh = false;
        }
        else
        {
            ss << "Region " << i;
            props.m_alpha = 1.0f;
            props.m_visible = true;
            props.m_showMesh = false;
        }

        props.m_name = ss.str();
        props.m_color = rgbValues[i];

        m_properties.emplace_back( std::move( props ) );

        // Update the color in m_colors_RGBA_F32
        updateColorRGBA( i );
    }
}


glm::vec4 ParcellationLabelTable::color_RGBA_premult_F32( size_t index ) const
{
    checkLabelIndex( index );
    return m_colors_RGBA_F32.at( index );
}


size_t ParcellationLabelTable::numLabels() const
{
    return m_colors_RGBA_F32.size();
}

size_t ParcellationLabelTable::maxNumLabels() const
{
    return m_maxLabelCount;
}


size_t ParcellationLabelTable::numColorBytes_RGBA_F32() const
{
    return m_colors_RGBA_F32.size() * sizeof( glm::vec4 );
}


const float* ParcellationLabelTable::colorData_RGBA_premult_F32() const
{
    return glm::value_ptr( m_colors_RGBA_F32[0] );
}


tex::SizedInternalBufferTextureFormat ParcellationLabelTable::bufferTextureFormat_RGBA_F32()
{
    static const tex::SizedInternalBufferTextureFormat sk_format =
            tex::SizedInternalBufferTextureFormat::RGBA32F;

    return sk_format;
}


const std::string& ParcellationLabelTable::getName( size_t index ) const
{
    checkLabelIndex( index );
    return m_properties.at( index ).m_name;
}


void ParcellationLabelTable::setName( size_t index, std::string name )
{
    checkLabelIndex( index );
    m_properties[index].m_name = std::move( name );
}


bool ParcellationLabelTable::getVisible( size_t index ) const
{
    checkLabelIndex( index );
    return m_properties.at( index ).m_visible;
}


void ParcellationLabelTable::setVisible( size_t index, bool show )
{
    checkLabelIndex( index );
    m_properties[index].m_visible = show;
    updateColorRGBA( index );
}


bool ParcellationLabelTable::getShowMesh( size_t index ) const
{
    checkLabelIndex( index );
    return m_properties.at( index ).m_showMesh;
}


void ParcellationLabelTable::setShowMesh( size_t index, bool show )
{
    checkLabelIndex( index );
    m_properties[index].m_showMesh = show;
}


glm::vec3 ParcellationLabelTable::getColor( size_t index ) const
{
    checkLabelIndex( index );
    return m_properties.at( index ).m_color;
}


void ParcellationLabelTable::setColor( size_t index, const glm::vec3& color )
{
    checkLabelIndex( index );

    m_properties[index].m_color = color;
    updateColorRGBA( index );
}


float ParcellationLabelTable::getAlpha( size_t index ) const
{
    checkLabelIndex( index );
    return m_properties.at( index ).m_alpha;
}


void ParcellationLabelTable::setAlpha( size_t index, float alpha )
{
    checkLabelIndex( index );

    if ( alpha < 0.0f || 1.0f < alpha )
    {
        return;
    }

    m_properties[index].m_alpha = alpha;
    updateColorRGBA( index );
}


std::vector<size_t> ParcellationLabelTable::addLabels( size_t count )
{
    std::vector<size_t> newIndices;

    if ( 0 == count )
    {
        return newIndices; // None to add
    }

    if ( numLabels() + count > maxNumLabels() )
    {
        spdlog::error( "Unable to add {} new labels: exceeds maximum number of labels allowed ({}) "
                       "for this parcellation label table", count, maxNumLabels() );

        return newIndices;
    }

    // Generate random colors
    const size_t last = m_colors_RGBA_F32.size();
    const size_t seed = sk_seed + last;

    const std::vector< glm::vec3 > hsvSamples = math::generateRandomHsvSamples(
                count, sk_hueMinMax, sk_satMinMax, sk_valMinMax, seed );

    std::vector< glm::vec3 > rgbValues;

    std::transform( std::begin( hsvSamples ), std::end( hsvSamples ),
                    std::back_inserter( rgbValues ),
                    glm::rgbColor< float, glm::precision::defaultp > );

    m_colors_RGBA_F32.resize( last + count );

    for ( size_t i = last; i < last + count; ++i )
    {
        newIndices.push_back( i );

        LabelProperties props;

        std::ostringstream ss;
        ss << "Region " << i;

        props.m_alpha = 1.0f;
        props.m_visible = true;
        props.m_showMesh = false;
        props.m_name = ss.str();
        props.m_color = rgbValues[i - last];

        m_properties.emplace_back( std::move( props ) );

        // Update the color in m_colors_RGBA_F32
        updateColorRGBA( i );
    }

    return newIndices;
}


void ParcellationLabelTable::updateColorRGBA( size_t index )
{
    checkLabelIndex( index );

    // Modulate opacity by visibility:
    const float a = getAlpha( index ) * ( getVisible( index ) ? 1.0f : 0.0f );

    // Pre-multiplied RGBA:
    m_colors_RGBA_F32[index] = a * glm::vec4{ getColor( index ), 1.0f };
}


void ParcellationLabelTable::checkLabelIndex( size_t index ) const
{
    if ( index >= m_properties.size() )
    {
        std::ostringstream ss;
        ss << "Invalid label index " << index << std::ends;
        throw_debug( ss.str() )
    }
}
