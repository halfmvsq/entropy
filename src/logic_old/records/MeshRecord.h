#ifndef MESH_RECORD_H
#define MESH_RECORD_H

#include "logic_old/RenderableRecord.h"
#include "mesh/MeshCpuRecord.h"
#include "rendering_old/records/MeshGpuRecord.h"

using MeshRecord = RenderableRecord< MeshCpuRecord, MeshGpuRecord >;

#endif // MESH_RECORD_H
