#ifndef UI_ISOSURFACE_HEADERS_H
#define UI_ISOSURFACE_HEADERS_H

#include "common/PublicTypes.h"

#include <uuid.h>

class AppData;

void renderIsosurfacesHeader(
        AppData& appData,
        const uuids::uuid& imageUid,
        size_t imageIndex,
        bool isActiveImage );

#endif // UI_ISOSURFACE_HEADERS_H
