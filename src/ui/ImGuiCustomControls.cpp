#include "ui/ImGuiCustomControls.h"

#include <imgui/imgui_internal.h>

// File browser for ImGui:
// We use the version in src/imgui, since there are modifications to our local version
// (compared the version on Github) that allow it to work on macOS.
#include "imgui/imgui-filebrowser/imfilebrowser.h"

#include <glm/glm.hpp>

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


namespace
{

static const ImGuiDataTypeInfo GDataTypeInfo[] =
    {
        { sizeof(char),             "S8",   "%d",   "%d"    },  // ImGuiDataType_S8
        { sizeof(unsigned char),    "U8",   "%u",   "%u"    },
        { sizeof(short),            "S16",  "%d",   "%d"    },  // ImGuiDataType_S16
        { sizeof(unsigned short),   "U16",  "%u",   "%u"    },
        { sizeof(int),              "S32",  "%d",   "%d"    },  // ImGuiDataType_S32
        { sizeof(unsigned int),     "U32",  "%u",   "%u"    },
#ifdef _MSC_VER
        { sizeof(ImS64),            "S64",  "%I64d","%I64d" },  // ImGuiDataType_S64
        { sizeof(ImU64),            "U64",  "%I64u","%I64u" },
#else
        { sizeof(ImS64),            "S64",  "%lld", "%lld"  },  // ImGuiDataType_S64
        { sizeof(ImU64),            "U64",  "%llu", "%llu"  },
#endif
        { sizeof(float),            "float", "%.3f","%f"    },  // ImGuiDataType_Float (float are promoted to double in va_arg)
        { sizeof(double),           "double","%f",  "%lf"   },  // ImGuiDataType_Double
};

}


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
    const std::vector<glm::vec4>& colors,
    bool inverted,
    bool quantize,
    int quantizationLevels,
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

//    const int startIndex = ( inverted ? colors.size() - 1 : 0 );
//    const int dir = ( inverted ? -1 : 1 );

    /*
    auto renderLines = [&] ( const float alpha )
    {
        const float step = static_cast<float>( colors.size() ) / static_cast<float>( width );

        for ( int i = 0; i < width; ++i )
        {
            // 0 to numColors - 1
            int idx = startIndex + dir * i * static_cast<int>( step );

//            if ( quantize )
//            {
//                const float L = static_cast<float>(quantizationLevels);

//                float idx_cont = float(idx) / float( 4 * ( numColors - 1 ) );
//                idx_cont = std::min( std::max( std::floor( L * idx_cont ) / (L - 1.0f), 0.0f ), 1.0f );

//                idx = static_cast<int>( idx_cont * 4*(numColors-1) );
//            }

            drawList->AddLine(
                ImVec2( posMin.x + static_cast<float>(i), posMin.y ),
                ImVec2( posMin.x + static_cast<float>(i), posMax.y ),
                IM_COL32( 255.0f * colors[idx].r,
                          255.0f * colors[idx].g,
                          255.0f * colors[idx].b,
                          255.0f * alpha * colors[idx].a ) );
        }
    };
*/

    auto renderFilledRects = [&] ( const float alpha )
    {
        const float step = static_cast<float>( width ) / static_cast<float>( colors.size() );

        for ( std::size_t i = 0; i < colors.size(); ++i )
        {
            const float minX = posMin.x + static_cast<float>(i) * step;

            std::size_t index = i;

            if ( quantize )
            {
                // Index normalized from 0 to 1:
                const float normIndex = static_cast<float>(i) / static_cast<float>( colors.size() - 1 );

                // Index quantized from 0 to size - 1:
                index = static_cast<std::size_t>(
                    std::min( std::max( std::floor( quantizationLevels * normIndex ) / ( quantizationLevels - 1 ), 0.0f ), 1.0f ) *
                    ( colors.size() - 1 ) );
            }

            if ( inverted )
            {
                index = colors.size() - 1 - index;
            }

            drawList->AddRectFilled(
                ImVec2( minX, posMin.y ),
                ImVec2( minX + step, posMax.y ),

                IM_COL32( 255.0f * colors[index].r,
                          255.0f * colors[index].g,
                          255.0f * colors[index].b,
                          255.0f * alpha * colors[index].a ) );
        }
    };

    renderFilledRects( ImGui::GetStyle().Alpha );
    drawList->AddRect( posMin, posMax, IM_COL32( 128.0f, 128.0f, 128.0f, 255.0f ), 0.0f, 0, 0.5f );

    return true;
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


bool SliderScalarN_multiComp(
    const char* label,
    ImGuiDataType data_type,
    void* v, int components,
    const void** v_min, const void** v_max,
    const char** format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    BeginGroup();
    PushID(label);
    PushMultiItemsWidths(components, CalcItemWidth());
    size_t type_size = GDataTypeInfo[data_type].Size;
    for (int i = 0; i < components; i++)
    {
        PushID(i);
        if (i > 0)
            SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= SliderScalar("", data_type, v, v_min[i], v_max[i], format[i], flags);
        PopID();
        PopItemWidth();
        v = (void*)((char*)v + type_size);
    }
    PopID();

    const char* label_end = FindRenderedTextEnd(label);
    if (label != label_end)
    {
        SameLine(0, g.Style.ItemInnerSpacing.x);
        TextEx(label, label_end);
    }

    EndGroup();
    return value_changed;
}


} // namespace ImGui
