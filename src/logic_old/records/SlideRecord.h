#ifndef SLIDE_RECORD_H
#define SLIDE_RECORD_H

#include "logic_old/RenderableRecord.h"
#include "slideio/SlideCpuRecord.h"
#include "rendering_old/records/SlideGpuRecord.h"

/**
 * Record for a microscopy slide.
 */
using SlideRecord = RenderableRecord< slideio::SlideCpuRecord, SlideGpuRecord >;

#endif // SLIDE_RECORD_H
