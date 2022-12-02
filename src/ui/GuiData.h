#ifndef GUI_DATA_H
#define GUI_DATA_H

#include <glm/vec2.hpp>
#include <imgui/imgui.h>
#include <uuid.h>

#include <string>
#include <unordered_map>


/**
 * @brief Data for the user interface
 */
struct GuiData
{
    /// Global setting to turn on/off rendering of the UI windows
    bool m_renderUiWindows = false;

    /// Global setting to turn on/off rendering of the UI overlays (crosshairs, anatomical labels)
    bool m_renderUiOverlays = false;

    // Flags to show specific UI windows
    bool m_showImagePropertiesWindow = true; //!< Show image properties window
    bool m_showSegmentationsWindow = false; //!< Show segmentations window
    bool m_showLandmarksWindow = false; //!< Show landmarks window
    bool m_showAnnotationsWindow = false; //!< Show annotations window
    bool m_showIsosurfacesWindow = false; //!< Show isosurfaces window
    bool m_showSettingsWindow = false; //!< Show settings window
    bool m_showInspectionWindow = true; //!< Show cursor inspection window
    bool m_showOpacityBlenderWindow = false; //!< Show opacity blender window
    bool m_showImGuiDemoWindow = false; //!< Show ImGui demo window
    bool m_showImPlotDemoWindow = false; //!< Show ImPlot demo window

    /// Flag to show dialog confirming closing of the application window.
    /// This is set the false until the user requests to close the window.
    bool m_showConfirmCloseAppPopup = false;

    /// Map of imageUid to boolean of whether its image color map window is shown.
    /// (The color map window is shown as a popup from the Image Properties window)
    std::unordered_map< uuids::uuid, bool > m_showImageColormapWindow;

    // Show the color map windows for the difference, cross-correlation, and histogram metric views
    bool m_showDifferenceColormapWindow = false; //!< Show difference colormap window
    bool m_showCorrelationColormapWindow = false; //!< Show correlation colormap window
    bool m_showJointHistogramColormapWindow = false; //!< Show joint histogram colormap window

    void setCoordsPrecisionFormat();
    void setTxPrecisionFormat();

    /// Precision format string used for spatial coordinates
    std::string m_coordsPrecisionFormat = "%0.3f";
    uint32_t m_coordsPrecision = 3;

    /// Precision format string used for image transformations
    std::string m_txPrecisionFormat = "%0.3f";
    uint32_t m_txPrecision = 3;

    /// Precision format string used for image values
    std::string m_imageValuePrecisionFormat = "%0.3f";
    uint32_t m_imageValuePrecision = 3;

    // Pointers to fonts allocated by ImGui
    // ImFont* m_cousineFont = nullptr; //!< Main ImGui font
    // ImFont* m_forkAwesomeFont = nullptr; //!< Icons font
    std::unordered_map< std::string, ImFont* > m_fonts;

    // Font data, using raw pointers used because ImGui takes ownership and deletes them.
    // char* m_cousineFontData = nullptr;
    // char* m_forkAwesomeFontData = nullptr;
    // std::unordered_map< std::string, char* > m_fontData;

    bool m_showMainMenuBar = true;
    glm::vec2 m_mainMenuBarDims{ 0.0f, 0.0f };

    bool m_showModeToolbar = true;
    bool m_isModeToolbarHorizontal = false;
    int m_modeToolbarCorner = 1;
    glm::vec2 m_modeToolbarDockDims{ 0.0f, 0.0f };

    bool m_showSegToolbar = false;
    bool m_isSegToolbarHorizontal = false;
    int m_segToolbarCorner = 0;
    glm::vec2 m_segToolbarDockDims{ 0.0f, 0.0f };

    // Corners: -1 custom, 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right
};

#endif // GUI_DATA_H
