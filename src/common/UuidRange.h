#ifndef UUID_RANGE_H
#define UUID_RANGE_H

#include <uuid.h>
#include <boost/range/any_range.hpp>

/// Convenience alias for a forward-traversable range of UIDs.
using uuid_range_t = boost::any_range<
    const uuids::uuid,
    boost::forward_traversal_tag,
    const uuids::uuid&,
    std::ptrdiff_t >;

#endif // UUID_RANGE
