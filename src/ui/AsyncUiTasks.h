#ifndef ASYNC_UI_TASKS_H
#define ASYNC_UI_TASKS_H

#include <uuid.h>

#include <optional>
#include <string>
#include <tuple>

/**
 * Enumeration of asynchronous tasks created in the UI
 */
enum class AsyncUiTasks
{
    GraphCutsSegmentation,
    IsosurfaceMeshGeneration
};

struct AsyncUiTaskValue
{
    AsyncUiTasks task; //!< Type of task
    std::string description; //!< Description of the task
    uuids::uuid taskUid; //!< UID of the task
    std::optional<uuids::uuid> objectUid = std::nullopt; //!< UID of the object created by the task
    bool success = false; //!< Flag indicating task success (true) or failure (false)
};

#endif // ASYNC_UI_TASKS_H
