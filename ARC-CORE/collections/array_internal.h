/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file array_internal.h
 *
 * @author Nelson Somé
 *
 * @brief Internal file for Array.
 *
 * The Array API provides contiguous storage for a sequence of fixed-size
 * elements.
 *
 * An array has:
 * - a logical size representing the number of currently stored elements;
 * - a capacity representing the number of elements that can be stored without
 *   changing the storage capacity;
 * - a fixed element size shared by every element stored in the array;
 * - a growth strategy defining whether and how its capacity may grow.
 *
 * @note This file is part of Arc Core.
 * 
 * @attention This header is for Arc Core internal use only and must not be
 *            included by public API users.
 */

#ifndef ARC_ARRAY_INTERNAL_H_
#define ARC_ARRAY_INTERNAL_H_

#include <stdbool.h>
#include <stddef.h>

#include "array.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations. */
typedef struct Sentinel Sentinel;

/**
 * @brief Array object representing a contiguous sequence of fixed-size elements.
 *
 * The Array object is allocated using 'object_allocator' and its internal
 * storage is allocated using 'storage_allocator'.
 *
 * The Array does not own the allocator objects and does not manage their
 * lifetime. The allocators must remain valid for the entire lifetime of the
 * Array.
 *
 * @attention Successful structural mutations invalidate all previously obtained
 *            iterators, ranges, sentinels, and pointers referring to the Array's
 *            traversal state or internal storage.
 *
 * @note Replacing element bytes without changing Array structure, including
 *       array_set_at() and writes through mutable element/data pointers, does
 *       not invalidate existing iterators, ranges, sentinels, or borrowed pointers.
 */
struct Array {
    /** Pointer to the internal storage of the Array. */
    void* data;

    /** Current logical size of the Array in number of elements. */
    size_t size;

    /** Fixed size in bytes of each element stored in the Array. */
    size_t element_size;

    /** Current storage capacity of the Array in number of elements. */
    size_t capacity;

    /** Base capacity of the Array in number of elements. */
    size_t base_capacity;

    /** Growth factor of the Array. */
    ArrayGrowthFactor growth_factor;

    /** Allocator used to allocate and release the Array object itself. */
    Allocator* object_allocator;

    /** Allocator used to allocate and release the internal element storage. */
    Allocator* storage_allocator;
};


/**
 * @brief Context structure for Array iterators.
 *
 * The iterator context is allocated using the Array's object allocator and
 * owned by the Iterator. It is freed when the Iterator is destroyed.
 *
 * The context contains a pointer to the Array being traversed, which is
 * borrowed and not owned by the Iterator.
 */
typedef struct ArrayIteratorContext {
    /** Pointer to the Array being traversed. */
    const Array* array;

    /** Start index of the traversal domain. */
    size_t start_index;

    /** End index of the traversal domain. */
    size_t end_index;

    /** Current position of the iterator within the traversal domain. */
    size_t position;

    /** Flag indicating whether the traversal is in reverse order. */
    bool reverse;
} ArrayIteratorContext;

/**
 * @brief Context structure for Array sentinels.
 *
 * The sentinel context is allocated using the Array's object allocator and
 * owned by the Sentinel. It is freed when the Sentinel is destroyed.
 *
 * The context contains a pointer to the Array being traversed, which is
 * borrowed and not owned by the Sentinel.
 */
typedef struct ArraySentinelContext {
    /** Pointer to the Array being traversed. */
    const Array* array;

    /** Start index of the traversal domain. */
    size_t start_index;

    /** End index of the traversal domain. */
    size_t end_index;

    /** Current position of the sentinel within the traversal domain. */
    size_t position;

    /** Flag indicating whether the traversal is in reverse order. */
    bool reverse;
} ArraySentinelContext;

/**
 * @brief Checks whether an Array is valid and initialized.
 *
 * @param[in] array Pointer to the Array to check.
 * @return true if the Array is valid and initialized, false otherwise.
 */
bool array_is_valid(const Array* array);

/**
 * @brief Initializes an Array-backed Iterator from an Array iterator context.
 *
 * The resulting Iterator owns an independent copy of the supplied context.
 *
 * @param[out] out Iterator to initialize.
 * @param[in] context Array iterator state to copy.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @attention 'out' must not contain a live initialized Iterator.
 */
ArrayResult array_iterator_init(Iterator* out, const ArrayIteratorContext* context);

/**
 * @brief Initializes an Array-backed Sentinel from an Array sentinel context.
 *
 * The resulting Sentinel owns an independent copy of the supplied context.
 *
 * @param[out] out Sentinel to initialize.
 * @param[in] context Array sentinel state to copy.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @attention 'out' must not contain a live initialized Sentinel.
 */
ArrayResult array_sentinel_init(Sentinel* out, const ArraySentinelContext* context);

#ifdef __cplusplus
}
#endif

#endif /* ARC_ARRAY_INTERNAL_H_ */