#ifndef UI_ISOSURFACE_HEADERS_H
#define UI_ISOSURFACE_HEADERS_H

#include "common/PublicTypes.h"

#include "ui/AsyncUiTasks.h"

#include <uuid.h>

#include <functional>
#include <future>
#include <string>

class AppData;

void renderIsosurfacesHeader(
        AppData& appData,
        const uuids::uuid& imageUid,
        size_t imageIndex,
        bool isActiveImage,
        std::function< void ( const uuids::uuid& taskUid, std::future<AsyncUiTaskValue> future ) > storeFuture );

#endif // UI_ISOSURFACE_HEADERS_H
