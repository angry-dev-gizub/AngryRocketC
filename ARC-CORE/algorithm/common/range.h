/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file range.h
 *
 * @author Nelson Somé
 *
 * @brief Generic owning range interface.
 *
 * A Range represents a traversal domain beginning at an Iterator and
 * terminating when that Iterator satisfies a Sentinel.
 *
 * A successfully initialized Range owns independent clones of both endpoints.
 * This allows the objects used to create the Range to be modified or destroyed
 * independently after initialization.
 *
 * Range implementations must satisfy the contracts documented by this API.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_RANGE_H_
#define ARC_RANGE_H_

#include <stdbool.h>

#include "iterator/iterator.h"
#include "sentinel.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations. */
typedef struct Allocator Allocator;
typedef struct Range Range;

typedef /** RangeResult */
    /**
     * @brief RangeResult is the set of values that describe whether a Range
     *        operation succeeded or why it failed.
     */
    enum RangeResult {
        /** Operation succeeded. */
        RANGE_SUCCESS = 0,

        /** Range is NULL, invalid, or otherwise unusable. */
        RANGE_ERROR_INVALID_RANGE,

        /** One or more call arguments are invalid. */
        RANGE_ERROR_INVALID_ARGUMENTS,

        /** Required allocator is NULL or otherwise unusable. */
        RANGE_ERROR_INVALID_ALLOCATOR,

        /** Required iterator is NULL or otherwise unusable. */
        RANGE_ERROR_INVALID_ITERATOR,

        /** Required sentinel is NULL or otherwise unusable. */
        RANGE_ERROR_INVALID_SENTINEL,

        /** Iterator and Sentinel expect different element type sizes. */
        RANGE_ERROR_INCOMPATIBLE_TYPES,

        /** An underlying Iterator operation failed. */
        RANGE_ERROR_ITERATOR_ERROR,

        /** An underlying Sentinel operation failed. */
        RANGE_ERROR_SENTINEL_ERROR,
    } RangeResult;

/**
 * @brief Generic owning range of elements.
 *
 * A Range owns an independent beginning Iterator and termination Sentinel.
 * The Iterator stored in 'begin' represents the stable starting position of
 * the Range and should not be used as a working traversal cursor.
 *
 * Algorithms consuming a Range should clone 'begin' and advance the clone,
 * leaving the Range's stored starting position unchanged.
 *
 * @attention Copying a Range directly performs a shallow copy of its owned
 *            Iterator and Sentinel state and may duplicate ownership metadata.
 *            Direct structure copying must therefore not be used to create an
 *            independent Range.
 */
struct Range {
    /** 'true' if the Range was initialized successfully. */
    bool initialized;

    /** Owned Iterator representing the beginning of the Range. */
    Iterator begin;

    /** Owned Sentinel representing the traversal termination boundary. */
    Sentinel end;
};

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
RangeResult range_init(Range* out, const Iterator* begin, const Sentinel* end, Allocator* allocator);

/**
 * @brief Tests whether a Range is valid and initialized.
 *
 * @param[in] range Pointer to the Range to test.
 *
 * @return true if 'range' and both owned endpoints are initialized and valid.
 * @return false otherwise.
 */
bool range_is_valid(const Range* range);

/**
 * @brief Obtains the element type size traversed by a Range.
 *
 * @param[in] range Pointer to an initialized Range.
 *
 * @return The element type size in bytes.
 * @return 0 if 'range' is NULL or invalid.
 */
size_t range_get_type_size(const Range* range);

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
RangeResult range_destroy(Range* range);

/**
 * @brief Converts a numeric RangeResult code into a human-readable string.
 *
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 *
 * @note The returned string is static and must not be freed.
 */
const char* range_result_to_string(RangeResult result);

#ifdef __cplusplus
}
#endif

#endif /* ARC_RANGE_H_ */
