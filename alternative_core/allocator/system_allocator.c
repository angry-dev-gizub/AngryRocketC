/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

#include <stdlib.h>
#include <stdint.h>

#include "allocator.h"

/**
 * @brief Allocates memory using the system heap.
 */
static arc_result_t system_alloc(void* context, size_t size, void** out) {
    (void)context;

    if(!out)
        arc_abort("Arc system allocator: output parameter is NULL");

    *out = NULL;

    if(size == 0)
        return arc_result_success();

    *out = malloc(size);

    if(!*out)
        return arc_result_failure(ARC_ALLOCATOR_ERROR_OUT_OF_MEMORY);

    return arc_result_success();
}

/**
 * @brief Allocates zero-initialized memory using the system heap.
 */
static arc_result_t system_zero_alloc(void* context, size_t num, size_t size, void** out) {
    (void)context;

    if(!out)
        arc_abort("Arc system allocator: output parameter is NULL");

    *out = NULL;

    if(num == 0 || size == 0)
        return arc_result_success();

    if(num > SIZE_MAX / size)
        return arc_result_failure(ARC_ALLOCATOR_ERROR_SIZE_OVERFLOW);

    *out = calloc(num, size);

    if(!*out)
        return arc_result_failure(ARC_ALLOCATOR_ERROR_OUT_OF_MEMORY);

    return arc_result_success();
}

/**
 * @brief Resizes a system-heap allocation.
 */
static arc_result_t system_realloc(void* context, void* ptr, size_t new_size, void** out) {
    (void)context;

    if(!out)
        arc_abort("Arc system allocator: output parameter is NULL");

    *out = NULL;

    if(!ptr || new_size == 0)
        arc_abort("Arc system allocator: invalid reallocation arguments");

    void* resized = realloc(ptr, new_size);

    if(!resized)
        return arc_result_failure(ARC_ALLOCATOR_ERROR_OUT_OF_MEMORY);

    *out = resized;

    return arc_result_success();
}

/**
 * @brief Releases a system-heap allocation.
 */
static arc_result_t system_free(void* context, void* ptr) {
    (void)context;

    free(ptr);

    return arc_result_success();
}

static const arc_allocator_interface_t g_system_alloc_iface = {
    .allocate      = system_alloc,
    .zero_allocate = system_zero_alloc,
    .reallocate    = system_realloc,
    .deallocate    = system_free,
    .clear         = NULL,
};

static const arc_allocator_t g_system_allocator = {
    .context = NULL,
    .supported_features =
        (ARC_ALLOCATOR_FEATURE_ZEROALLOC | ARC_ALLOCATOR_FEATURE_REALLOC | ARC_ALLOCATOR_FEATURE_FREE),
    .iface = g_system_alloc_iface,
};

const arc_allocator_t* arc_get_system_allocator(void) {
    return &g_system_allocator;
}
