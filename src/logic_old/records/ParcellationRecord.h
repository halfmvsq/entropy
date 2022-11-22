#ifndef PARCELLATION_RECORD_H
#define PARCELLATION_RECORD_H

#include "logic_old/RenderableRecord.h"
#include "image/Image.h"
#include "rendering_old/records/ImageGpuRecord.h"

/**
 * Record for a parcellation image.
 */
using ParcellationRecord = RenderableRecord< Image, ImageGpuRecord >;

#endif // PARCELLATION_RECORD_H
