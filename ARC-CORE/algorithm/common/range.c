#include "iterator/iterator.h"
#include "sentinel.h"
#include "range.h"

/**
 * @brief Initializes a Range from an Iterator and Sentinel.
 *
 * @param[out] out Pointer to the Range to initialize. Must not be NULL.
 * @param[in] begin Pointer to an initialized Iterator representing the
 *                  beginning of the range. Must not be NULL.
 * @param[in] end Pointer to an initialized Sentinel representing the traversal
 *                termination boundary. Must not be NULL.
 *
 * @return true if successfully initialized, false otherwise.
 *
 * @attention 'begin' and 'end' must have matching element type sizes.
 *
 * @post On success, 'out' borrows 'begin' and 'end'.
 *
 * @post On failure, 'out' is reset to an invalid Range state.
 *
 * @note The Range does not own or modify either object.
 *
 * @attention 'begin' and 'end' must remain valid for the entire lifetime of
 *            the Range.
 */
bool range_init(Range* out, const Iterator* begin, const Sentinel* end) {
    if(!out)
        return false;

    *out = (Range){0};

    if(!begin || !iterator_is_valid(begin))
        return false;

    if(!end || !sentinel_is_valid(end))
        return false;

    if(iterator_get_type_size(begin) != sentinel_get_type_size(end))
        return false;

    *out = (Range){
        .begin = *begin,
        .end   = *end,
    };

    return true;
}