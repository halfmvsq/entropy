#ifndef GRAPHCUTS_H
#define GRAPHCUTS_H

#include <uuid.h>

class AppData;
class Rendering;


/// @todo Rendering is only used for updateSegTexture

/**
     * @brief executeGridCutSegmentation
     * @param imageUid
     * @param seedSegUid
     * @param resultSegUid
     * @return
     */
bool graphCutsFgBgSegmentation(
    AppData& appData,
    Rendering& rendering,
    const uuids::uuid& imageUid,
    const uuids::uuid& seedSegUid,
    const uuids::uuid& resultSegUid );

bool graphCutsMultiLabelSegmentation(
    AppData& appData,
    Rendering& rendering,
    const uuids::uuid& imageUid,
    const uuids::uuid& seedSegUid,
    const uuids::uuid& resultSegUid );

#endif // GRAPHCUTS_H
