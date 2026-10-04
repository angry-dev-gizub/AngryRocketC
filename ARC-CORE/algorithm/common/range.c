/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include "allocator/allocator.h"
#include "range.h"

/**
 * @brief Initializes an owning Range from an Iterator and Sentinel.
 *
 * Independent endpoint contexts are created using 'allocator'.
 *
 * @param[out] out Pointer to the Range to initialize. Must not be NULL.
 * @param[in] begin Pointer to an initialized Iterator representing the beginning
 *                  of the Range. Must not be NULL and must support cloning.
 * @param[in] end Pointer to an initialized Sentinel representing the termination
 *                boundary. Must not be NULL and must support cloning.
 * @param[in] allocator Allocator used to clone endpoint contexts. Must not be
 *                      NULL and must remain valid until the Range is destroyed.
 *
 * @return RANGE_SUCCESS on success.
 * @return RANGE_ERROR_INVALID_ARGUMENTS if 'out' is NULL.
 * @return RANGE_ERROR_INVALID_ALLOCATOR if 'allocator' is NULL or not initialized.
 * @return RANGE_ERROR_INVALID_ITERATOR if 'begin' is NULL or not initialized.
 * @return RANGE_ERROR_INVALID_SENTINEL if 'end' is NULL or not initialized.
 * @return RANGE_ERROR_INCOMPATIBLE_TYPES if the endpoints report different
 *         element type sizes.
 * @return RANGE_ERROR_ITERATOR_ERROR if cloning the beginning Iterator fails.
 * @return RANGE_ERROR_SENTINEL_ERROR if cloning the Sentinel fails.
 *
 * @post On success:
 *       - 'out' is initialized;
 *       - 'out' owns independent clones of 'begin' and 'end';
 *       - modifying or destroying the source endpoint wrappers does not affect
 *         the Range;
 *       - 'allocator' must remain valid until range_destroy() succeeds.
 *
 * @post On failure:
 *       - 'out' is not a valid usable Range;
 *       - any endpoint resources that can be released successfully are released;
 *       - if rollback destruction fails, 'out' retains ownership of the
 *         unreleased endpoint so that range_destroy() can be used to retry
 *         cleanup.
 *
 * @attention 'out' must not contain any live owned endpoint state.
 *            An existing or partially constructed Range must be successfully
 *            destroyed before being reused as output.
 *
 * @attention The underlying traversal domain referenced by the cloned endpoint
 *            contexts must remain valid for the lifetime of the Range.
 */
RangeResult range_init(Range* out, const Iterator* begin, const Sentinel* end, Allocator* allocator) {
    if(!out)
        return RANGE_ERROR_INVALID_ARGUMENTS;

    *out = (Range){0};

    if(!begin || !iterator_is_valid(begin))
        return RANGE_ERROR_INVALID_ITERATOR;

    if(!end || !sentinel_is_valid(end))
        return RANGE_ERROR_INVALID_SENTINEL;

    if(!allocator || !allocator->initialized)
        return RANGE_ERROR_INVALID_ALLOCATOR;

    if(iterator_get_type_size(begin) != sentinel_get_type_size(end))
        return RANGE_ERROR_INCOMPATIBLE_TYPES;

    if(iterator_clone(begin, &out->begin, allocator) != ITERATOR_SUCCESS)
        return RANGE_ERROR_ITERATOR_ERROR;

    if(sentinel_clone(end, &out->end, allocator) != SENTINEL_SUCCESS) {
        if(iterator_destroy(&out->begin) != ITERATOR_SUCCESS)
            return RANGE_ERROR_ITERATOR_ERROR;

        *out = (Range){0};
        return RANGE_ERROR_SENTINEL_ERROR;
    }

    out->initialized = true;
    return RANGE_SUCCESS;
}

/**
 * @brief Tests whether a Range is valid and initialized.
 *
 * @param[in] range Pointer to the Range to test.
 *
 * @return true if 'range' and both owned endpoints are initialized and valid.
 * @return false otherwise.
 */
bool range_is_valid(const Range* range) {
    if(!range || !range->initialized)
        return false;

    if(!iterator_is_valid(&range->begin))
        return false;

    if(!sentinel_is_valid(&range->end))
        return false;

    return true;
}

/**
 * @brief Obtains the element type size traversed by a Range.
 *
 * @param[in] range Pointer to an initialized Range.
 *
 * @return The element type size in bytes.
 * @return 0 if 'range' is NULL or invalid.
 */
size_t range_get_type_size(const Range* range) {
    if(!range || !range_is_valid(range))
        return (size_t)0;

    return iterator_get_type_size(&range->begin);
}

/**
 * @brief Destroys a Range and releases its owned endpoint contexts.
 *
 * The owned Iterator and Sentinel are destroyed using their own stored cloning
 * allocators.
 *
 * @param[in, out] range Range to destroy. Must not be NULL.
 *
 * @return RANGE_SUCCESS if all owned endpoint resources are released.
 * @return RANGE_ERROR_INVALID_RANGE if 'range' is NULL or contains no live
 *         endpoint state to destroy.
 * @return RANGE_ERROR_ITERATOR_ERROR if Iterator destruction fails.
 * @return RANGE_ERROR_SENTINEL_ERROR if Sentinel destruction fails.
 *
 * @post On success, the Range is reset to an invalid zero state.
 *
 * @attention If destruction of one endpoint succeeds and destruction of the
 *            other fails, the successfully destroyed endpoint remains invalid
 *            while ownership of the failed endpoint remains in the Range.
 *            range_destroy() may be called again to retry cleanup of the
 *            remaining live endpoint.
 */
RangeResult range_destroy(Range* range) {
    if(!range)
        return RANGE_ERROR_INVALID_RANGE;

    bool has_begin = iterator_is_valid(&range->begin);
    bool has_end   = sentinel_is_valid(&range->end);

    if(!has_begin && !has_end) {
        *range = (Range){0};
        return RANGE_ERROR_INVALID_RANGE;
    }

    if(has_begin) {
        if(iterator_destroy(&range->begin) != ITERATOR_SUCCESS)
            return RANGE_ERROR_ITERATOR_ERROR;
    }

    if(has_end) {
        if(sentinel_destroy(&range->end) != SENTINEL_SUCCESS)
            return RANGE_ERROR_SENTINEL_ERROR;
    }

    *range = (Range){0};

    return RANGE_SUCCESS;
}

/**
 * @brief Converts a numeric RangeResult code into a human-readable string.
 *
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 *
 * @note The returned string is static and must not be freed.
 */
const char* range_result_to_string(RangeResult result) {
    switch(result) {
        case RANGE_SUCCESS:
            return "no error";
        case RANGE_ERROR_INVALID_RANGE:
            return "null or invalid range";
        case RANGE_ERROR_INVALID_ITERATOR:
            return "null or invalid iterator endpoint";
        case RANGE_ERROR_INVALID_SENTINEL:
            return "null or invalid sentinel endpoint";
        case RANGE_ERROR_INVALID_ALLOCATOR:
            return "null or invalid allocator";
        case RANGE_ERROR_INCOMPATIBLE_TYPES:
            return "iterator and sentinel type sizes do not match";
        case RANGE_ERROR_ITERATOR_ERROR:
            return "iterator endpoint operation failed";
        case RANGE_ERROR_SENTINEL_ERROR:
            return "sentinel endpoint operation failed";
        default:
            return NULL;
    }
}
