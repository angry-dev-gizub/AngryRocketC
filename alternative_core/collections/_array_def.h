/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file _array_def.h
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

#ifndef ARC_ARRAY_DEF_H_
#define ARC_ARRAY_DEF_H_

#include "array.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 *            pointers referring to the Array's internal storage.
 *
 * @note Replacing element bytes without changing Array structure, including
 *       array_set_at() and writes through mutable element/data pointers, does
 *       not invalidate existing borrowed pointers.
 */
struct arc_array_t {
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
    arc_array_growth_factor_t growth_factor;

    /** Allocator used to allocate and release the Array object itself. */
    arc_allocator_t* object_allocator;

    /** Allocator used to allocate and release the internal element storage. */
    arc_allocator_t* storage_allocator;
};

#ifdef __cplusplus
}
#endif

#endif /* ARC_ARRAY_DEF_H_ */
