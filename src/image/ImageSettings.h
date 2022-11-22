#ifndef IMAGE_SETTINGS_H
#define IMAGE_SETTINGS_H

#include "common/Types.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <ostream>
#include <string>
#include <utility>
#include <vector>


class ImageSettings
{
public:

    explicit ImageSettings() = default;

    /**
     * @brief ImageSettings
     * @param displayName Image display name
     * @param numComponents Number of components per pixel
     * @param componentType Component type
     * @param componentStats Vector of pixel statistics, one per image component
     */
    ImageSettings( std::string displayName,
                   uint32_t numComponents,
                   ComponentType componentType,
                   std::vector< ComponentStats<double> > componentStats );

    ImageSettings( const ImageSettings& ) = default;
    ImageSettings& operator=( const ImageSettings& ) = default;

    ImageSettings( ImageSettings&& ) = default;
    ImageSettings& operator=( ImageSettings&& ) = default;

    ~ImageSettings() = default;


    /// Set the short display name of the image
    void setDisplayName( std::string name );

    /// Get the short display name of the image
    const std::string& displayName() const;


    /// Set the border color
    void setBorderColor( glm::vec3 borderColor );

    /// Get the border color
    const glm::vec3& borderColor() const;


    /// Set whether the image transformation is locked to the reference image
    void setLockedToReference( bool locked );

    /// Get whether the image transformation is locked to the reference image
    bool isLockedToReference() const;


    /// Set whether to display a 3- or 4-component image as color
    void setDisplayImageAsColor( bool doColor );

    /// Get whether to display a 3- or 4-component image as color
    bool displayImageAsColor() const;


    /// Set whether to ignore the alpha component of color images
    void setIgnoreAlpha( bool ignore );

    /// Get whether to ignore the alpha component of color images
    bool ignoreAlpha() const;


    /// Set the interpolation mode for a color image
    void setColorInterpolationMode( InterpolationMode mode );

    /// Get the interpolation mode of a color image
    InterpolationMode colorInterpolationMode() const;


    /// Set whether the image boundary distance map is used to accelerate raycasting rendering of the image
    void setUseDistanceMapForRaycasting( bool use );

    /// Get whether the image boundary distance map is used to accelerate raycasting rendering of the image
    bool useDistanceMapForRaycasting() const;


    /// Set whether the image isosurfaces are visible (global setting for all surfaces)
    void setIsosurfacesVisible( bool visible );

    /// Get whether the image isosurfaces are visible (global setting for all surfaces)
    bool isosurfacesVisible() const;


    /// Set whether to color the image isosurfaces using the image colormap (global setting for all surfaces)
    void setApplyImageColormapToIsosurfaces( bool visible );

    /// Get whether to color the image isosurfaces using the image colormap (global setting for all surfaces)
    bool applyImageColormapToIsosurfaces() const;


    /// Set whether the image isosurfaces are visible in 2D slices (global setting for all surfaces)
    void setShowIsosurfacesIn2d( bool show );

    /// Get whether the image isosurfaces are visible in 2D slices (global setting for all surfaces)
    bool showIsosurfacesIn2d() const;


    /// Set isosurface line width in 2D, as a percentage of the image threshold range (global setting for all surfaces)
    void setIsosurfaceWidthIn2d( double width );

    /// Get isosurface line width in 2D, as a percentage of the image threshold range (global setting for all surfaces)
    double isosurfaceWidthIn2d() const;


    /// Set opacity modulator for all image isosurfaces (global setting for all surfaces)
    void setIsosurfaceOpacityModulator( float opacityMod );

    /// Get opacity modulator for all image isosurfaces (global setting for all surfaces)
    float isosurfaceOpacityModulator() const;



    /// Set/get the dirty flag
    void setDirty( bool set );
    bool isDirty() const;


    /// Get the min/max image intensities for a given component
    std::pair<double, double> minMaxImageRange( uint32_t component ) const;
    std::pair<double, double> minMaxImageRange() const;

    /// Get the min/max window range (in image intensity units) for a given component
    std::pair<double, double> minMaxWindowRange( uint32_t component ) const;
    std::pair<double, double> minMaxWindowRange() const;

    /// Get the min/max threshold range (in image intensity units) for a given component
    std::pair<double, double> minMaxThresholdRange( uint32_t component ) const;
    std::pair<double, double> minMaxThresholdRange() const;


    /// Set lower window value (in image intensity units) for a given component.
    void setWindowLow( uint32_t component, double wLow, bool clampValues = false );
    void setWindowLow( double wLow, bool clampValues = false );

    /// Set upper window value (in image intensity units) for a given component.
    void setWindowHigh( uint32_t component, double wHigh, bool clampValues = false );
    void setWindowHigh( double wHigh, bool clampValues = false );

    /// Set lower and upper window values (in image intensity units) for a given component.
    void setWindowLowHigh( uint32_t component, double wLow, double wHigh, bool clampValues = false );
    void setWindowLowHigh( double wLow, double wHigh, bool clampValues = false );

    /// Get window limits (in image intensity units) for a given component
    std::pair<double, double> windowLowHigh( uint32_t component ) const;
    std::pair<double, double> windowLowHigh() const;

    /// Get window width (in image intensity units) for a given component
    double windowWidth( uint32_t component ) const;
    double windowWidth() const;

    /// Get window center (aka level) (in image intensity units) for a given component
    double windowCenter( uint32_t component ) const;
    double windowCenter() const;



    /// Set low threshold (in image intensity units) for a given component
    void setThresholdLow( uint32_t component, double thresh );
    void setThresholdLow( double thresh );

    /// Set high threshold (in image intensity units) for a given component
    void setThresholdHigh( uint32_t component, double thresh );
    void setThresholdHigh( double thresh );

    /// Get thresholds as GLM vector
    std::pair<double, double> thresholds( uint32_t component ) const;
    std::pair<double, double> thresholds() const;

    /// Get whether the thresholds are active for a given component
    bool thresholdsActive( uint32_t component ) const;
    bool thresholdsActive() const;


    /// Set the image opacity (in [0, 1] range) for a given component
    void setOpacity( uint32_t component, double opacity );
    void setOpacity( double opacity );

    /// Get the image opacity (in [0, 1] range) of a given component
    double opacity( uint32_t component ) const;
    double opacity() const;


    /// Set the visibility for a given component
    void setVisibility( uint32_t component, bool visible );
    void setVisibility( bool visible );

    /// Get the visibility of a given component
    bool visibility( uint32_t component ) const;
    bool visibility() const;


    /// Set the global visibility for all components
    void setGlobalVisibility( bool visible );

    /// Get the global visibility for all components
    bool globalVisibility() const;


    /// Set the global image opacity (in [0, 1] range)
    void setGlobalOpacity( double opacity );

    /// Get the global image opacity (in [0, 1] range)
    double globalOpacity() const;


    /// Set whether edges are shown for a given component
    void setShowEdges( uint32_t component, bool show );
    void setShowEdges( bool show );

    /// Get whether edges are shown for a given component
    bool showEdges( uint32_t component ) const;
    bool showEdges() const;

    /// Set whether edges are thresholded for a given component
    void setThresholdEdges( uint32_t component, bool threshold );
    void setThresholdEdges( bool threshold );

    /// Get whether edges are thresholded for a given component
    bool thresholdEdges( uint32_t component ) const;
    bool thresholdEdges() const;

    /// Set whether edges are computed using Frei-Chen for a given component
    void setUseFreiChen( uint32_t component, bool use );
    void setUseFreiChen( bool use );

    /// Get whether edges are thresholded for a given component
    bool useFreiChen( uint32_t component ) const;
    bool useFreiChen() const;

    /// Set edge magnitude for a given component
    void setEdgeMagnitude( uint32_t component, double mag );
    void setEdgeMagnitude( double mag );

    /// Get edge matnidue for a given component
    double edgeMagnitude( uint32_t component ) const;
    double edgeMagnitude() const;

    /// Set whether edges are computed after applying windowing (width/level) for a given component
    void setWindowedEdges( uint32_t component, bool windowed );
    void setWindowedEdges( bool overlay );

    /// Get whether edges are computed after applying windowing (width/level) for a given component
    bool windowedEdges( uint32_t component ) const;
    bool windowedEdges() const;

    /// Set whether edges are shown as an overlay on the image for a given component
    void setOverlayEdges( uint32_t component, bool overlay );
    void setOverlayEdges( bool overlay );

    /// Get whether edges are shown as an overlay on the image for a given component
    bool overlayEdges( uint32_t component ) const;
    bool overlayEdges() const;

    /// Set whether edges are colormapped for a given component
    void setColormapEdges( uint32_t component, bool showEdges );
    void setColormapEdges( bool showEdges );

    /// Get whether edges are colormapped for a given component
    bool colormapEdges( uint32_t component ) const;
    bool colormapEdges() const;

    /// Set edge color a given component
    void setEdgeColor( uint32_t component, glm::vec3 color );
    void setEdgeColor( glm::vec3 color );

    /// Get whether edges are shown for a given component
    glm::vec3 edgeColor( uint32_t component ) const;
    glm::vec3 edgeColor() const;

    /// Set edge opacity a given component
    void setEdgeOpacity( uint32_t component, double opacity );
    void setEdgeOpacity( double opacity );

    /// Get edge opacity for a given component
    double edgeOpacity( uint32_t component ) const;
    double edgeOpacity() const;


    /// Get the color map index
    void setColorMapIndex( uint32_t component, size_t index );
    void setColorMapIndex( size_t index );

    /// Get the color map index
    size_t colorMapIndex( uint32_t component ) const;
    size_t colorMapIndex() const;


    // Set whether the color map is inverted
    void setColorMapInverted( uint32_t component, bool inverted );
    void setColorMapInverted( bool inverted );

    // Get whether the color map is inverted
    bool isColorMapInverted( uint32_t component ) const;
    bool isColorMapInverted() const;


    /// Get the label table index
    void setLabelTableIndex( uint32_t component, size_t index );
    void setLabelTableIndex( size_t index );

    /// Get the label table index
    size_t labelTableIndex( uint32_t component ) const;
    size_t labelTableIndex() const;


    /// Set the interpolation mode for a given component
    void setInterpolationMode( uint32_t component, InterpolationMode mode );
    void setInterpolationMode( InterpolationMode mode );

    /// Get the interpolation mode of a given component
    InterpolationMode interpolationMode( uint32_t component ) const;
    InterpolationMode interpolationMode() const;


    /// Get window/level slope 'm' and intercept 'b' for a given component.
    /// These are used to map NATIVE (raw) image intensity units 'x' to NORMALIZED units 'y' in the
    /// range [0, 1]: y = m*x + b
    /// after window/level have been applied
    std::pair<double, double> slopeIntercept_normalized_T_native( uint32_t component ) const;
    std::pair<double, double> slopeIntercept_normalized_T_native() const;

    /// Get the slope/intercept mapping from NATIVE intensity to OpenGL TEXTURE intensity
    // std::pair<double, double> slopeIntercept_texture_T_native() const;


    /// Get normalized window/level slope 'm' and intercept 'b' for a given component.
    /// These are used to map image TEXTURE intensity units 'x' to NORMALIZED units 'y' in the
    /// normalized range [0, 1]: y = m*x + b
    /// after window/level have been applied
    std::pair<double, double> slopeIntercept_normalized_T_texture( uint32_t component ) const;
    std::pair<double, double> slopeIntercept_normalized_T_texture() const;


    /// Slope to map TEXTURE intensity to NATIVE intensity, without accounting for window/level.
    float slope_native_T_texture() const;


    glm::dvec2 slopeInterceptVec2_normalized_T_texture( uint32_t component ) const;
    glm::dvec2 slopeInterceptVec2_normalized_T_texture() const;

    glm::dvec2 largestSlopeInterceptTextureVec2( uint32_t component ) const;
    glm::dvec2 largestSlopeInterceptTextureVec2() const;

    /// Get threshold range (in image intensity units) for a given component
    std::pair<double, double> thresholdRange( uint32_t component ) const;
    std::pair<double, double> thresholdRange() const;


    /// Set foreground low threshold (in native image intensity units) for a given component.
    void setForegroundThresholdLow( uint32_t component, double background );
    void setForegroundThresholdLow( double background );

    /// Get foreground low threshold (in native image intensity units) for a given component
    double foregroundThresholdLow( uint32_t component ) const;
    double foregroundThresholdLow() const;


    /// Set foreground high threshold (in native image intensity units) for a given component.
    void setForegroundThresholdHigh( uint32_t component, double background );
    void setForegroundThresholdHigh( double background );

    /// Get foreground thresholds (in native image intensity units) for a given component
    std::pair<double, double> foregroundThresholds( uint32_t component ) const;
    std::pair<double, double> foregroundThresholds() const;


    uint32_t numComponents() const; //!< Number of components per pixel

    /// Get statistics for an image component
    /// The component must be in the range [0, numComponents() - 1].
    const ComponentStats<double>& componentStatistics( uint32_t component ) const;
    const ComponentStats<double>& componentStatistics() const;

    /// Set the active component
    void setActiveComponent( uint32_t component );

    /// Get the active component
    uint32_t activeComponent() const;

    /// Map a native image value to its representation as an OpenGL texture.
    /// This mappings accounts for component type.
    /// @see https://www.khronos.org/opengl/wiki/Normalized_Integer
    double mapNativeIntensityToTexture( double nativeImageValue ) const;

    friend std::ostream& operator<< ( std::ostream&, const ImageSettings& );


private:

    void updateInternals();

    /// @brief Settings for one image component
    struct ComponentSettings
    {
        ComponentSettings() {}

        std::pair<double, double> m_minMaxImageRange{ 0.0, 0.0 }; //!< Min/max image value range
        std::pair<double, double> m_minMaxWindowRange{ 0.0, 0.0 }; //!< Valid window range
        std::pair<double, double> m_minMaxThresholdRange{ 0.0, 0.0 }; //!< Valid threshold range

        /// Low and high limits of the window in native image intensity units
        std::pair<double, double> m_window{ 0.0, 0.0 };

        /// Low and high threshold values in native image intensity units
        std::pair<double, double> m_thresholds{ 0.0, 0.0 };

        /// Native image intensity value thresholds for the image foreground. A Euclidean distance
        /// map is generated from the foreground. This distance map is used to accelerate raycasting
        /// of the image's isosurfaces.
        std::pair<double, double> m_foregroundThresholds{ 0.0, 0.0 };

        /// @note The following slope (m) and intercept (b) are used to map NATIVE image intensity
        /// values (x) into the range [0.0, 1.0], via m*x + b
        double m_slope_native = 0.0; //!< Slope (m) computed from window
        double m_intercept_native = 0.0; //!< Intercept (b) computed from window and level

        /// @note The following slope (m) and intercept (b) are used to map image TEXTURE intensity
        /// values (x) into the range [0.0, 1.0], via m*x + b
        double m_slope_texture = 0.0; //!< Slope computed from window
        double m_intercept_texture = 0.0; //!< Intercept computed from window and level

        /// @note The following values of slope (m) and intercept (b) are used to map image TEXTURE intensity
        /// values (x) into the range [0.0, 1.0], via m*x + b
        /// These values represent the largest window possible
        double m_largest_slope_texture = 0.0; //!< Slope computed from window
        double m_largest_intercept_texture = 0.0; //!< Intercept computed from window and level

        double m_opacity = 0.0; //!< Opacity in range [0.0, 1.0]
        bool m_visible = false; //!< Visibility flag (show/hide the component)

        bool m_showEdges = false; //!< Flag to show edges
        bool m_thresholdEdges = false; //!< Flag to threshold edges
        bool m_useFreiChen = false; //!< Flag to use Frei-Chen filters
        double m_edgeMagnitude = 0.0; //!< Magnitude of edges to show [0.0, 4.0] if thresholding is turned one
        bool m_windowedEdges = false; //!< Flag to compute edges after applying windowing (width/level) to the image
        bool m_overlayEdges = false; //!< Flag to overlay edges atop image (true) or show edges on their own (false)
        bool m_colormapEdges = false; //!< Flag to apply colormap to edges (true) or to render edges with a solid color (false)
        glm::vec3 m_edgeColor = glm::vec3{0.0f}; //!< Edge color (used if not rendering edges using colormap)
        double m_edgeOpacity = 0.0; //!< Edge opacity: only applies when shown as an overlay atop the image

        size_t m_colorMapIndex = 0; //!< Color map index
        bool m_colorMapInverted = false; //!< Whether the color map is inverted

        size_t m_labelTableIndex = 0; //!< Label table index (for segmentation images only)

        /// Interpolation mode
        InterpolationMode m_interpolationMode = InterpolationMode::NearestNeighbor;
    };


    /*** Start settings for all components ***/
    std::string m_displayName; //!< Display name of the image in the UI
    bool m_globalVisibility; //!< Global visibility
    float m_globalOpacity; //!< Global opacity
    glm::vec3 m_borderColor; //!< Border color
    bool m_lockedToReference; //!< Lock this image to the reference image

    // The following settings only apply to images with 3 or 4 components:
    bool m_displayAsColor; //!< Display the image as RGB/RGBA color
    bool m_ignoreAlpha; //!< Ignore the alpha component of the image
    InterpolationMode m_colorInterpolationMode = InterpolationMode::NearestNeighbor; //!< Interpolation mode

    // These apply to the image's isosurfaces:
    bool m_useDistanceMapForRaycasting; //!< Use the distance map to accelerate raycasting of the image
    bool m_isosurfacesVisible; //!< Visibility of image isosurfaces
    bool m_applyImageColormapToIsosurfaces; //!< Color image isosurfaces using the image colormap
    bool m_showIsosurfacesIn2d; //!< Visibility of isosurface edges in 2D image slices
    double m_isosurfaceWidthIn2d; //!< Width of isovalue lines in 2D as a percentage of the image intensity range
    float m_isosurfaceOpacityModulator; //!< Modality of surface opacity for the image
    /*** End settings for all components ***/


    uint32_t m_numComponents; //!< Number of components per pixel
    ComponentType m_componentType; //!< Component type
    std::vector< ComponentStats<double> > m_componentStats; //!< Per-component statistics
    std::vector<ComponentSettings> m_componentSettings; //!< Per-component settings

    uint32_t m_activeComponent; //!< Active component

    bool m_dirty; //!< Flag that the settings have changed since the image was last saved
};


std::ostream& operator<< ( std::ostream&, const ImageSettings& );

#endif // IMAGE_SETTINGS_H
