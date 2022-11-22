#ifndef IMGUI_WRAPPER_H
#define IMGUI_WRAPPER_H

#include "common/PublicTypes.h"

#include <glm/fwd.hpp>
#include <uuid.h>
#include <functional>

class AppData;
class CallbackHandler;
struct GLFWwindow;


/**
 * @brief A simple wrapper for ImGui
 */
class ImGuiWrapper
{
public:

    ImGuiWrapper( GLFWwindow* window, AppData& appData, CallbackHandler& callbackHandler );
    ~ImGuiWrapper();

    void setCallbacks(
            std::function< void (void) > readjustViewport,
            std::function< void ( const uuids::uuid& viewUid ) > recenterView,
            AllViewsRecenterType recenterCurrentViews,
            std::function< bool ( void ) > getOverlayVisibility,
            std::function< void ( bool ) > setOverlayVisibility,
            std::function< void ( void ) > updateAllImageUniforms,
            std::function< void ( const uuids::uuid& viewUid ) > updateImageUniforms,
            std::function< void ( const uuids::uuid& viewUid ) > updateImageInterpolationMode,
            std::function< void ( size_t tableIndex ) > updateLabelColorTableTexture,
            std::function< void ( const uuids::uuid& imageUid, size_t labelIndex ) > moveCrosshairsToSegLabelCentroid,
            std::function< void ()> updateMetricUniforms,
            std::function< glm::vec3 () > getWorldDeformedPos,
            std::function< std::optional<glm::vec3> ( size_t imageIndex ) > getSubjectPos,
            std::function< std::optional<glm::ivec3> ( size_t imageIndex ) > getVoxelPos,
            std::function< void ( size_t imageIndex, const glm::vec3& subjectPos ) > setSubjectPos,
            std::function< void ( size_t imageIndex, const glm::ivec3& voxelPos ) > setVoxelPos,
            std::function< std::vector< double > ( size_t imageIndex, bool getOnlyActiveComponent ) > getImageValues,
            std::function< std::optional<int64_t> ( size_t imageIndex ) > getSegLabel,
            std::function< std::optional<uuids::uuid>( const uuids::uuid& matchingImageUid, const std::string& segDisplayName ) > createBlankSeg,
            std::function< bool ( const uuids::uuid& segUid ) > clearSeg,
            std::function< bool ( const uuids::uuid& segUid ) > removeSeg,
            std::function< bool ( const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const uuids::uuid& resultSegUid ) > m_executeGridCutsSeg,
            std::function< bool ( const uuids::uuid& imageUid, bool locked ) > setLockManualImageTransformation,
            std::function< void () > paintActiveSegmentationWithActivePolygon );

    void render();


private:

    void initializeData();

    void annotationToolbar( const std::function< void () > paintActiveAnnotation );

    AppData& m_appData;
    CallbackHandler& m_callbackHandler;

    // Callbacks:
    std::function< void (void) > m_readjustViewport = nullptr;
    std::function< void ( const uuids::uuid& viewUid ) > m_recenterView = nullptr;
    AllViewsRecenterType m_recenterAllViews = nullptr;
    std::function< bool ( void ) > m_getOverlayVisibility = nullptr;
    std::function< void ( bool ) > m_setOverlayVisibility = nullptr;
    std::function< void ( void ) > m_updateAllImageUniforms = nullptr;
    std::function< void ( const uuids::uuid& viewUid ) > m_updateImageUniforms = nullptr;
    std::function< void ( const uuids::uuid& viewUid ) > m_updateImageInterpolationMode = nullptr;
    std::function< void ( size_t tableIndex ) > m_updateLabelColorTableTexture = nullptr;
    std::function< void ( const uuids::uuid& imageUid, size_t labelIndex ) > m_moveCrosshairsToSegLabelCentroid = nullptr;
    std::function< void ( void ) > m_updateMetricUniforms = nullptr;
    std::function< glm::vec3 () > m_getWorldDeformedPos = nullptr;
    std::function< std::optional<glm::vec3> ( size_t imageIndex ) > m_getSubjectPos = nullptr;
    std::function< std::optional<glm::ivec3> ( size_t imageIndex ) > m_getVoxelPos = nullptr;
    std::function< void ( size_t imageIndex, const glm::vec3& subjectPos ) > m_setSubjectPos = nullptr;
    std::function< void ( size_t imageIndex, const glm::ivec3& voxelPos ) > m_setVoxelPos = nullptr;
    std::function< std::vector< double > ( size_t imageIndex, bool getOnlyActiveComponent ) > m_getImageValues = nullptr;
    std::function< std::optional<int64_t> ( size_t imageIndex ) > m_getSegLabel = nullptr;
    std::function< std::optional<uuids::uuid>( const uuids::uuid& matchingImageUid, const std::string& segDisplayName ) > m_createBlankSeg = nullptr;
    std::function< bool ( const uuids::uuid& segUid ) > m_clearSeg = nullptr;
    std::function< bool ( const uuids::uuid& segUid ) > m_removeSeg = nullptr;
    std::function< bool ( const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const uuids::uuid& resultSegUid ) > m_executeGridCutsSeg = nullptr;
    std::function< bool ( const uuids::uuid& imageUid, bool locked ) > m_setLockManualImageTransformation = nullptr;
    std::function< void () > m_paintActiveSegmentationWithActivePolygon = nullptr;
};

#endif // IMGUI_WRAPPER_H
