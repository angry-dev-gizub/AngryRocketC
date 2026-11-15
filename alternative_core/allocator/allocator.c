/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

#include "allocator.h"

/**
 * @brief Checks the validity of a allocator descriptor.
 * 
 * @param[in] desc Read-only configuration of the allocator, must not be NULL.
 * @return true if the configuration is valid, false otherwise.
 */
static bool allocator_desc_is_valid(const arc_allocator_desc_t* desc) {
    if(!desc)
        return false;
    return (desc->iface.allocate != NULL);
}

/**
 * @brief Creates a bitmask based on the availiable functions stored in 'desc'.
 * 
 * @param[in, out] out Pointer to the bitmask to initialize, must not be NULL.
 * @param[in] desc Read-only configuration of the allocator, must not be NULL.
 * @return true if the bitmask is successfully created, false otherwise.
 */
static bool features_from_desc(arc_allocator_feature_flags_t* out, const arc_allocator_desc_t* desc) {
    if(!out || !desc)
        return false;
    *out = 0;
    if(desc->iface.zero_allocate)
        *out |= ARC_ALLOCATOR_FEATURE_ZEROALLOC;
    if(desc->iface.reallocate)
        *out |= ARC_ALLOCATOR_FEATURE_REALLOC;
    if(desc->iface.deallocate)
        *out |= ARC_ALLOCATOR_FEATURE_FREE;
    if(desc->iface.clear)
        *out |= ARC_ALLOCATOR_FEATURE_CLEAR;
    return true;
}

/** Invalid public API arguments are programmer errors, not ordinary failures. */
static void require_allocator(const arc_allocator_t* allocator) {
    if(allocator == NULL || !arc_allocator_is_valid(allocator)) {
        arc_abort("Arc allocator: invalid or uninitialized allocator");
    }
}

/** Required by allocation and reallocation wrappers. */
static void require_out(void** out) {
    if(out == NULL)
        arc_abort("Arc allocator: output parameter is NULL");
}

arc_result_t arc_allocator_init(arc_allocator_t* out, const arc_allocator_desc_t* desc) {
    if(!out)
        arc_abort("Arc allocator: output parameter is NULL");

    *out = (arc_allocator_t){0};

    if(!allocator_desc_is_valid(desc))
        arc_abort("Arc allocator: invalid descriptor");

    arc_allocator_feature_flags_t features;

    if(!features_from_desc(&features, desc))
        arc_abort("Arc allocator: failed to discover features");

    *out = (arc_allocator_t){.supported_features = features, .context = desc->context, .iface = desc->iface

    };

    return arc_result_success();
}

bool arc_allocator_is_valid(const arc_allocator_t* allocator) {
    if(!allocator)
        return false;

    if(!allocator->iface.allocate)
        return false;

    return true;
}

bool arc_allocator_has_features(const arc_allocator_t* allocator, arc_allocator_feature_flags_t required_features) {
    if(!allocator || !arc_allocator_is_valid(allocator))
        arc_abort("Arc allocator: invalid or uninitialized allocator");

    return (allocator->supported_features & required_features) == required_features;
}

arc_result_t arc_allocator_alloc(const arc_allocator_t* allocator, size_t size, void** out) {
    require_allocator(allocator);
    require_out(out);

    *out = NULL;

    if(size == 0)
        return arc_result_success();

    arc_result_t result = allocator->iface.allocate(allocator->context, size, out);

    if(!*out)
        arc_abort("Arc allocator: allocation callback returned NULL on success");

    return result;
}

arc_result_t arc_allocator_zero_alloc(const arc_allocator_t* allocator, size_t num, size_t size, void** out) {
    require_allocator(allocator);
    require_out(out);

    *out = NULL;

    if(num == 0 || size == 0)
        return arc_result_success();

    if(num > SIZE_MAX / size)
        return arc_result_failure(ARC_ALLOCATOR_ERROR_SIZE_OVERFLOW);

    if(!allocator->iface.zero_allocate)
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);

    arc_result_t result = allocator->iface.zero_allocate(allocator->context, num, size, out);

    if(!result.ok) {
        *out = NULL;
        return result;
    }

    if(!*out)
        arc_abort("Arc allocator: zero allocation callback returned NULL on success");

    return result;
}

arc_result_t arc_allocator_realloc(const arc_allocator_t* allocator, void* ptr, size_t new_size, void** out) {
    require_allocator(allocator);
    require_out(out);

    if(!ptr || new_size == 0)
        arc_abort("Arc allocator: invalid reallocation arguments");

    *out = NULL;

    if(!allocator->iface.reallocate)
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);

    arc_result_t result = allocator->iface.reallocate(allocator->context, ptr, new_size, out);

    if(!result.ok) {
        *out = NULL;
        return result;
    }

    if(!*out)
        arc_abort("Arc allocator: reallocation callback returned NULL on success");

    return result;
}

arc_result_t arc_allocator_free(const arc_allocator_t* allocator, void* ptr) {
    require_allocator(allocator);

    if(!allocator->iface.deallocate)
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);

    if(!ptr)
        return arc_result_success();

    return allocator->iface.deallocate(allocator->context, ptr);
}

arc_result_t arc_allocator_clear(const arc_allocator_t* allocator) {
    require_allocator(allocator);

    if(!allocator->iface.clear)
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);

    return allocator->iface.clear(allocator->context);
}
