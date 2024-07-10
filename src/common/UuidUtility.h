#ifndef UUID_UTILITY_H
#define UUID_UTILITY_H

#include <uuid.h>
#include <boost/range/any_range.hpp>

uuids::uuid generateRandomUuid();

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template <> struct fmt::formatter<uuids::uuid> : ostream_formatter{};
#endif

#endif // UUID_UTILITY_H
