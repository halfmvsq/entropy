#include <spdlog/spdlog.h>

#define MyAssert(_EXPR) do { spdlog::warn( "ImGui assert: {}", _EXPR ) } while(0);

#define IM_ASSERT(_EXPR)  MyAssert(_EXPR)