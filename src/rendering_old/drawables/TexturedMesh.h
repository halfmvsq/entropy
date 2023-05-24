#ifndef TEXTURED_MESH_H
#define TEXTURED_MESH_H

#include "rendering_old/drawables/DrawableBase.h"
#include "rendering_old/common/MeshColorLayer.h"
#include "rendering/common/ShaderProviderType.h"
#include "rendering_old/interfaces/ITexturable3D.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include <array>
#include <memory>
#include <utility>


class BlankTextures;
class GLTexture;
class MeshGpuRecord;


/**
 * @brief
 */
class TexturedMesh final :
        public DrawableBase,
        public ITexturable3d,
        public ObjectCounter<TexturedMesh>
{
public:

    TexturedMesh( std::string name,
                  ShaderProgramActivatorType shaderActivator,
                  UniformsProviderType uniformsProvider,
                  std::weak_ptr<BlankTextures> blankTextures,
                  GetterType<MeshGpuRecord*> meshGpuRecordProvider );

    TexturedMesh( const TexturedMesh& ) = delete;
    TexturedMesh& operator=( const TexturedMesh& ) = delete;

    ~TexturedMesh() override = default;

    bool isOpaque() const override;

    DrawableOpacity opacityFlag() const override;

    void setImage3dRecord( std::weak_ptr<ImageRecord> ) override;
    void setParcellationRecord( std::weak_ptr<ParcellationRecord> ) override;
    void setImageColorMapRecord( std::weak_ptr<ImageColorMapRecord> ) override;
    void setLabelTableRecord( std::weak_ptr<LabelTableRecord> ) override;

    std::weak_ptr<ImageRecord> image3dRecord();
    std::weak_ptr<ParcellationRecord> parcelRecord();

    void setTexture2d( std::weak_ptr<GLTexture> );
    void setTexture2dThresholds( glm::vec2 thresholds );

//    void addClippingPlane();
    void setUseOctantClipPlanes( bool set );

    // perm: set i'th layer to be l
    void setLayerPermutation( const std::array< TexturedMeshColorLayer, static_cast<size_t>( TexturedMeshColorLayer::NumLayers ) >& perm );

    void setLayerOpacityMultiplier( TexturedMeshColorLayer, float m );
    float getLayerOpacityMultiplier( TexturedMeshColorLayer ) const;

    void setLayerOpacity( TexturedMeshColorLayer, float a );
    float getLayerOpacity( TexturedMeshColorLayer ) const;

    void enableLayer( TexturedMeshColorLayer );
    void disableLayer( TexturedMeshColorLayer );

    /**
     * @brief Set mesh material color as NON-premultiplied RGB
     * @param color RGB (non-premultiplied)
     */
    void setMaterialColor( const glm::vec3& color );
    glm::vec3 getMaterialColor() const;

    void setMaterialShininess( float );
    float getMaterialShininess() const;

    void setBackfaceCull( bool );
    bool getBackfaceCull() const;

    void setUseAutoHidingMode( bool );

    void setUseImage2dThresholdMode( bool );
    void setUseImage3dThresholdMode( bool );

    void setImage2dThresholdsActive( bool );
    void setImage3dThresholdsActive( bool );

    void setUseXrayMode( bool );
    void setXrayPower( float );


    /**
     * @see http://www.glprogramming.com/red/chapter06.html#name4
     *
     * Offsets the depth values after interpolation from depth values of vertices.
     * Displace the depth values of fragments generated by rendering polygons by a
     * fixed bias plus an amount. The amount is proportional to the maximum absolute
     * value of the depth slope of the polygon, measured and applied in window coordinates.
     * Allows polygons in the same plane to be rendered without interaction.
     * Allows multiple coplanar polygons to be rendered without interaction,
     * if different offset factors are used for each polygon.
     *
     * When enabled, the depth value of each fragment is added to a calculated offset value.
     * The offset is added before the depth test is performed and before the depth value is written
     * into the depth buffer. The offset value o is calculated by: o = m * factor + r * units,
     * where m is the maximum depth slope of the polygon and r is the smallest value guaranteed
     * to produce a resolvable difference in window coordinate depth values.
     * The value r is an implementation-specific constant.
     *
     * For polygons that are parallel to the near and far clipping planes,
     * the depth slope is zero. For the polygons in your scene with a depth slope
     * near zero, only a small, constant offset is needed. To create a small,
     * constant offset, one can pass factor = 0.0 and units = 1.0
     *
     * For polygons that are at a great angle to the clipping planes,
     * the depth slope can be significantly greater than zero, and a larger
     * offset may be needed. Small, non-zero values for factor, such as 0.75 or 1.0,
     * are probably enough to generate distinct depth values and eliminate the
     * unpleasant visual artifacts.
     *
     * Too much offset is less noticeable than too little.
     *
     * Also, since depth values are unevenly transformed into window coordinates
     * when using perspective projection, less offset is needed for polygons that are
     * closer to the near clipping plane, and more offset is needed for polygons that are
     * further away. Once again, experimenting with the value of factor may be warranted.
     */
    void setEnablePolygonOffset( bool enable );
    void setPolygonOffsetValues( float factor, float units );

    void setAmbientLightFactor( float );
    void setDiffuseLightFactor( float );
    void setSpecularLightFactor( float );
    void setAdsLightFactors( float a, float d, float s );


private:

    void doSetupState() override;
    void doRender( const RenderStage& stage ) override;
    void doTeardownState() override;

    void doUpdate( double time, const Viewport&,
                   const camera::Camera&, const CoordinateFrame& crosshairs ) override;

    void initVao();
    void updateLayerOpacities();

    ShaderProgramActivatorType m_shaderProgramActivator;
    UniformsProviderType m_uniformsProvider;

    std::weak_ptr<BlankTextures> m_blankTextures;

    GLVertexArrayObject m_vao;
    std::unique_ptr< GLVertexArrayObject::IndexedDrawParams > m_vaoParams;

    GetterType<MeshGpuRecord*> m_meshGpuRecordProvider;

    std::weak_ptr<GLTexture> m_texture2d;
    std::weak_ptr<ImageRecord> m_image3dRecord;
    std::weak_ptr<ParcellationRecord> m_parcelRecord;
    std::weak_ptr<ImageColorMapRecord> m_imageColorMapRecord;
    std::weak_ptr<LabelTableRecord> m_labelsRecord;

    // m_layerPermutation[i] = l
    // means that the i'th layer is 'l'
    std::array< uint32_t, static_cast<size_t>( TexturedMeshColorLayer::NumLayers ) > m_layerPermutation;

    std::array< float, static_cast<size_t>( TexturedMeshColorLayer::NumLayers ) > m_layerOpacities;
    std::array< float, static_cast<size_t>( TexturedMeshColorLayer::NumLayers ) > m_layerOpacityMultipliers;
    std::array< float, static_cast<size_t>( TexturedMeshColorLayer::NumLayers ) > m_finalLayerOpacities;

    float m_overallOpacity;

    Uniforms m_stdUniforms;
    Uniforms m_initUniforms;
    Uniforms m_peelUniforms;

    glm::mat4 m_clip_O_camera;
    glm::mat4 m_camera_O_world;

    bool m_cameraIsOrthographic;

    glm::vec3 m_worldCameraPos;
    glm::vec3 m_worldCameraDir;
    glm::vec3 m_worldLightPos;
    glm::vec3 m_worldLightDir;

    // Equation of plane with normal n = (A, B, C) and point q = (x0, y0, z0):
    // A*x + B*y + C*z + D = 0
    // D = −A*x0 − B*y0 − C*z0 = -dot(n, q)

    bool m_useOctantClipPlanes;
    std::array< glm::vec4, 3 > m_worldClipPlanes;

    // Material properties
    glm::vec3 m_materialColor;
    float m_materialShininess;

    // ADS light colors
    glm::vec3 m_ambientLightColor;
    glm::vec3 m_diffuseLightColor;
    glm::vec3 m_specularLightColor;

    // ADS factors for normal mode
    float m_ambientLightFactor;
    float m_diffuseLightFactor;
    float m_specularLightFactor;

    // ADS factors for x-ray mode
    float m_xrayAmbientLightFactor;
    float m_xrayDiffuseLightFactor;
    float m_xraySpecularLightFactor;

    bool m_wireframe;
    bool m_backfaceCull;

    bool m_autoHidingMode;

    bool m_image2dThresholdMode;
    bool m_image3dThresholdMode;
    bool m_image2dThresholdActive;
    bool m_image3dThresholdActive;

    bool m_xrayMode;
    float m_xrayPower;

    glm::vec2 m_texture2dThresholds;

//    glm::mat4 m_labelTexCoords_O_view;

    bool m_enablePolygonOffset;
    float m_polygonOffsetFactor;
    float m_polygonOffsetUnits;
};

/** @note On how to avoid z fighting
Draw the first object (the one that should appear behind the other object) by depth testing but not depth writing
Draw the second object by depth testing and depth writing. This won't cause z-fighting since we didn't write any depth in step 1.
Draw the first object by only writing to the depth buffer and not to the color buffer. This makes sure the depth buffer is up to date for any pixels that are covered by object 1 but not by object 2.
Note, the objects need to be drawn consecutively for this to work.
*/

#endif // TEXTURED_MESH_H
