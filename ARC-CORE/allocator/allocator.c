/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include "allocator.h"

/**
 * @brief Creates a failure-state allocation result.
 */
static AllocResult alloc_result_failure(AllocError error) {
    AllocResult result = {.ok = false, .as.error = error};

    return result;
}

/**
 * @brief Checks the validity of a allocator descriptor.
 * 
 * @param[in] desc Read-only configuration of the allocator, must not be NULL.
 * @return true if the configuration is valid, false otherwise.
 */
static bool allocator_desc_is_valid(const AllocatorDesc* desc) {
    if(!desc)
        return false;
    /**
     * for now we only check if the descriptor contains mem_alloc, but
     * this might evolve into more complex validity rules.
     */
    return (desc->iface.mem_alloc != NULL);
}

/**
 * @brief Creates a bitmask based on the availiable functions stored in 'desc'.
 * 
 * @param[in, out] out Pointer to the bitmask to initialize, must not be NULL.
 * @param[in] desc Read-only configuration of the allocator, must not be NULL.
 * @return true if the bitmask is successfully created, false otherwise.
 */
static bool features_from_desc(AllocatorFeatureFlags* out, const AllocatorDesc* desc) {
    if(!out || !desc)
        return false;
    *out = 0;
    if(desc->iface.mem_zero_alloc)
        *out |= ALLOCATOR_FEATURE_ZERO_ALLOC;
    if(desc->iface.mem_aligned_alloc)
        *out |= ALLOCATOR_FEATURE_ALIGNED_ALLOC;
    if(desc->iface.mem_realloc)
        *out |= ALLOCATOR_FEATURE_REALLOC;
    if(desc->iface.mem_free)
        *out |= ALLOCATOR_FEATURE_FREE;
    if(desc->iface.mem_aligned_free)
        *out |= ALLOCATOR_FEATURE_ALIGNED_FREE;
    if(desc->iface.mem_clear)
        *out |= ALLOCATOR_FEATURE_CLEAR;
    return true;
}

/**
 * @brief Initializes an allocator.
 * 
 * @param[in, out] out Pointer to the allocator to initialize, must not be NULL.
 * @param[in] desc Read-only configuration of the allocator, must not be NULL.
 * 
 * @return true if successfully initialized, false otherwise.
 * 
 * @note On failure, the allocator's 'initialized' flag is set to false.
 *       Trying to use the allocator after failure results in an invalid allocator error.
 */
bool allocator_init(Allocator* out, const AllocatorDesc* desc) {
    if(!out)
        return false;

    *out = (Allocator){0};

    if(!allocator_desc_is_valid(desc))
        return false;

    AllocatorFeatureFlags features;

    if(!features_from_desc(&features, desc))
        return false;

    *out = (Allocator){
        .initialized        = true,
        .supported_features = features,
        .context            = desc->context,
        .iface              = desc->iface

    };

    return true;
}

/**
 * @brief Checks whether an allocator supports all requested features.
 *
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] required_features Bitmask of features that must be supported.
 *
 * @return true if all requested features are supported, false otherwise.
 */
bool allocator_has_features(const Allocator* allocator, AllocatorFeatureFlags required_features) {
    if(!allocator || !allocator->initialized)
        return false;

    return (allocator->supported_features & required_features) == required_features;
}

/**
 * @brief Allocates a memory block of 'size' bytes.
 * 
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] size The size in bytes to allocate.
 * @return The allocation result of the operation.
 */
AllocResult allocator_alloc(const Allocator* allocator, size_t size) {
    if(!allocator || !allocator->initialized)
        return alloc_result_failure(ALLOC_ERROR_INVALID_ALLOCATOR);
    if(!allocator->iface.mem_alloc)
        return alloc_result_failure(ALLOC_ERROR_INVALID_ALLOCATOR);
    return allocator->iface.mem_alloc(allocator->context, size);
}

/**
 * @brief Allocates a zero-initialized memory block of 'num * size' bytes.
 * 
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] num The number of elements to allocate.
 * @param[in] size The size in bytes of a single element.
 * @return The allocation result of the operation.
 *
 * @note If the allocator does not provide a zero-initialized allocation callback,
 *       the operation fails with ALLOC_ERROR_UNSUPPORTED_OPERATION.
 */
AllocResult allocator_zero_alloc(const Allocator* allocator, size_t num, size_t size) {
    if(!allocator || !allocator->initialized)
        return alloc_result_failure(ALLOC_ERROR_INVALID_ALLOCATOR);
    if(!allocator->iface.mem_zero_alloc)
        return alloc_result_failure(ALLOC_ERROR_UNSUPPORTED_OPERATION);
    return allocator->iface.mem_zero_alloc(allocator->context, num, size);
}

/**
 * @brief Resizes an already allocated memory block.
 *
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] ptr Pointer to the allocated block to resize.
 * @param[in] new_size The new size in bytes of the block.
 * @return The allocation result of the operation.
 *
 * @note If the allocator does not provide a reallocation callback, the operation
 *       fails with ALLOC_ERROR_UNSUPPORTED_OPERATION.
 */
AllocResult allocator_realloc(const Allocator* allocator, void* ptr, size_t new_size) {
    if(!allocator || !allocator->initialized)
        return alloc_result_failure(ALLOC_ERROR_INVALID_ALLOCATOR);
    if(!allocator->iface.mem_realloc)
        return alloc_result_failure(ALLOC_ERROR_UNSUPPORTED_OPERATION);
    return allocator->iface.mem_realloc(allocator->context, ptr, new_size);
}

/**
 * @brief Allocates an aligned memory block of 'size' bytes.
 * 
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] alignment Requested alignment.
 * @param[in] size The size in bytes to allocate.
 * @return The allocation result of the operation.
 *
 * @note If the allocator does not provide an aligned allocation callback, the
 *       operation fails with ALLOC_ERROR_UNSUPPORTED_OPERATION.
 */
AllocResult allocator_aligned_alloc(const Allocator* allocator, size_t alignment, size_t size) {
    if(!allocator || !allocator->initialized)
        return alloc_result_failure(ALLOC_ERROR_INVALID_ALLOCATOR);
    if(!allocator->iface.mem_aligned_alloc)
        return alloc_result_failure(ALLOC_ERROR_UNSUPPORTED_OPERATION);
    return allocator->iface.mem_aligned_alloc(allocator->context, alignment, size);
}

/**
 * @brief Releases an allocated block of memory.
 * 
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] ptr Pointer to the allocated memory block to release.
 * @return The release result of the operation.
 *
 * @note If the allocator does not provide a free callback, the operation returns
 *       RELEASE_ERROR_UNSUPPORTED_OPERATION.
 */
ReleaseResult allocator_free(const Allocator* allocator, void* ptr) {
    if(!allocator || !allocator->initialized)
        return RELEASE_ERROR_INVALID_ALLOCATOR;
    if(!allocator->iface.mem_free)
        return RELEASE_ERROR_UNSUPPORTED_OPERATION;
    allocator->iface.mem_free(allocator->context, ptr);
    return RELEASE_SUCCESS;
}

/**
 * @brief Releases a block of memory that has been allocated with a specific alignment.
 * 
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] ptr Pointer to the allocated memory block to release.
 * @return The release result of the operation.
 *
 * @note If the allocator does not provide an aligned free callback, the operation
 *       returns RELEASE_ERROR_UNSUPPORTED_OPERATION.
 */
ReleaseResult allocator_aligned_free(const Allocator* allocator, void* ptr) {
    if(!allocator || !allocator->initialized)
        return RELEASE_ERROR_INVALID_ALLOCATOR;
    if(!allocator->iface.mem_aligned_free)
        return RELEASE_ERROR_UNSUPPORTED_OPERATION;
    allocator->iface.mem_aligned_free(allocator->context, ptr);
    return RELEASE_SUCCESS;
}

/**
 * @brief Releases all memory managed by an allocator.
 * 
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @return The release result of the operation.
 *
 * @note If the allocator does not provide a clear callback, the operation returns
 *       RELEASE_ERROR_UNSUPPORTED_OPERATION.
 */
ReleaseResult allocator_clear(const Allocator* allocator) {
    if(!allocator || !allocator->initialized)
        return RELEASE_ERROR_INVALID_ALLOCATOR;
    if(!allocator->iface.mem_clear)
        return RELEASE_ERROR_UNSUPPORTED_OPERATION;
    allocator->iface.mem_clear(allocator->context);
    return RELEASE_SUCCESS;
}

/**
 * @brief Converts a numeric allocation error code into a human-readable string.
 * 
 * @param[in] error The error code to convert.
 * @return A human-readable description of the error code, or NULL for invalid inputs.
 * 
 * @note The returned string is static and must not be freed.
 */
const char* alloc_error_to_string(AllocError error) {
    switch(error) {
        case ALLOC_ERROR_UNKNOWN:
            return "unknown error";
        case ALLOC_ERROR_OUT_OF_MEMORY:
            return "out of memory";
        case ALLOC_ERROR_INVALID_ALLOCATOR:
            return "invalid or uninitialized allocator";
        case ALLOC_ERROR_INVALID_ALIGNMENT:
            return "invalid alignment";
        case ALLOC_ERROR_INVALID_ARGUMENTS:
            return "invalid call arguments";
        case ALLOC_ERROR_UNSUPPORTED_OPERATION:
            return "operation not supported";
        default:
            return NULL;
    }
}

/**
 * @brief Converts a numeric release result code into a human-readable string.
 * 
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 * 
 * @note The returned string is static and must not be freed.
 */
const char* release_result_to_string(ReleaseResult result) {
    switch(result) {
        case RELEASE_SUCCESS:
            return "no error";
        case RELEASE_ERROR_INVALID_ALLOCATOR:
            return "invalid or uninitialized allocator";
        case RELEASE_ERROR_UNSUPPORTED_OPERATION:
            return "operation not supported";
        default:
            return NULL;
    }
}
