#include "ui/ImGuiCustomControls.h"

// #define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>

// File browser for ImGui:
// We use the version in src/imgui, since there are modifications to our local version
// (compared the version on Github) that allow it to work on macOS.
#include "imgui/imgui-filebrowser/imfilebrowser.h"


// On Apple platforms, we must use the alternative ghc::filesystem,
// because it is not fully implemented or supported prior to macOS 10.15.
#if !defined(__APPLE__)
#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include)
#if __has_include(<filesystem>)
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif
#endif

#ifndef GHC_USE_STD_FS
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#endif


namespace ImGui
{

/**
 * @copyright Copyright (c) 2018-2020 Michele Morrone
 * All rights reserved.
 * https://michelemorrone.eu - https://BrutPitt.com
 * twitter: https://twitter.com/BrutPitt - github: https://github.com/BrutPitt
 * mailto:brutpitt@gmail.com - mailto:me@michelemorrone.eu
 * This software is distributed under the terms of the BSD 2-Clause license
 *
 * @note Minor modifications have been made to this function for Entropy
 */
bool paletteButton(
        const char* label,
        int numCol,
        const float* buff,
        bool inverted,
        const ImVec2& size )
{
    ImGuiWindow* window = GetCurrentWindow();

    if ( ! window ) return false;
    if ( window->SkipItems ) return false;
    if ( ! GImGui ) return false;

    const ImGuiStyle& style = GImGui->Style;

    const ImGuiID id = window->GetID( label );

    const float lineH = ( window->DC.CurrLineSize.y <= 0 )
            ? ( ( window->DC.PrevLineSize.y <= 0 )
                ? size.y
                : window->DC.PrevLineSize.y )
            : ( window->DC.CurrLineSize.y );

    const ImRect bb( ImVec2( window->DC.CursorPos.x + style.FramePadding.x,
                             window->DC.CursorPos.y ),
                     ImVec2( window->DC.CursorPos.x + size.x - 2.0f * style.FramePadding.x,
                             window->DC.CursorPos.y + lineH ) );

    ItemSize( bb );

    if ( ! ItemAdd( bb, id ) ) return false;

    const float borderY = ( lineH < size.y ) ? 0.0f : 0.5f * ( lineH - size.y );

    const ImVec2 posMin = ImVec2( bb.Min.x, bb.Min.y + borderY );
    const ImVec2 posMax = ImVec2( bb.Max.x, bb.Max.y - borderY );
    const int width = static_cast<int>( posMax.x - posMin.x );

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const int N = ( inverted ? 4 * numCol : 0 );
    const int m = ( inverted ? -1 : 1 );

    auto renderLines = [&] ( const float alpha )
    {
        const float step = static_cast<float>( numCol ) / static_cast<float>( width );

        for ( int i = 0; i < width; ++i )
        {
            const float ii = static_cast<float>( i );
            const int idx = N + m * 4 * i * static_cast<int>( step );

            drawList->AddLine( ImVec2( posMin.x + ii, posMin.y ),
                               ImVec2( posMin.x + ii, posMax.y ),
                               IM_COL32( 255.0f * buff[idx],
                                         255.0f * buff[idx + 1],
                                         255.0f * buff[idx + 2],
                                         255.0f * alpha * buff[idx + 3] ) );
        }
    };

    auto renderFilledRects = [&] ( const float alpha )
    {
        const float step = static_cast<float>( width ) / static_cast<float>( numCol );

        for ( int i = 0; i < numCol; ++i )
        {
            const float ii = static_cast<float>( i );
            const int idx = N + m * 4 * i;

            drawList->AddRectFilled(
                        ImVec2( posMin.x + ii * step, posMin.y ),
                        ImVec2( posMin.x + ( ii + 1.0f ) * step, posMax.y ),
                        IM_COL32( 255.0f * buff[idx + 0],
                                  255.0f * buff[idx + 1],
                                  255.0f * buff[idx + 2],
                                  255.0f * alpha * buff[idx + 3] ) );
        }
    };

    if ( numCol / 2 >= width )
    {
        renderLines( ImGui::GetStyle().Alpha );
    }
    else
    {
        renderFilledRects( ImGui::GetStyle().Alpha );
    }

    return false;
}


std::optional< std::string > renderFileButtonDialogAndWindow(
        const char* buttonText,
        const char* dialogTitle,
        const std::vector< std::string > dialogFilters )
{
    static ImGui::FileBrowser saveDialog(
                ImGuiFileBrowserFlags_EnterNewFilename |
                ImGuiFileBrowserFlags_CloseOnEsc |
                ImGuiFileBrowserFlags_CreateNewDir );

    saveDialog.SetTitle( dialogTitle );
    saveDialog.SetTypeFilters( dialogFilters );

    if ( ImGui::Button( buttonText ) )
    {
        saveDialog.Open();
    }

    saveDialog.Display();

    if ( saveDialog.HasSelected() )
    {
        const fs::path selectedFile = saveDialog.GetSelected();
        saveDialog.ClearSelected();
        return selectedFile.string();
    }

    return std::nullopt;
}

} // namespace ImGui
