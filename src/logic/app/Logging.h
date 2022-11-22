#ifndef LOGGING_H
#define LOGGING_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>


class Logging
{
public:

    Logging() = default;
    ~Logging() = default;

    void setup();

    /**
     * @brief Set logging level for the console sink
     * @param[in] level Desired logging level. One of the following:
     *      SPDLOG_LEVEL_TRACE    0
     *      SPDLOG_LEVEL_DEBUG    1
     *      SPDLOG_LEVEL_INFO     2
     *      SPDLOG_LEVEL_WARN     3
     *      SPDLOG_LEVEL_ERROR    4
     *      SPDLOG_LEVEL_CRITICAL 5
     *      SPDLOG_LEVEL_OFF      6
     */
    void setConsoleSinkLevel( spdlog::level::level_enum level );

private:

    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> m_console_sink;
};

#endif // LOGGING_H
