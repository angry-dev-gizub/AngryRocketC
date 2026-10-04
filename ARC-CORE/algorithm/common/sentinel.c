/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include "allocator/allocator.h"
#include "iterator/iterator.h"
#include "sentinel.h"

/**
 * @brief Checks the validity of a sentinel descriptor.
 *
 * @param[in] desc Read-only configuration of the sentinel, must not be NULL.
 * @return true if the configuration is valid, false otherwise.
 */
static bool sentinel_desc_is_valid(const SentinelDesc* desc) {
    if(!desc)
        return false;

    if(desc->type_size == 0)
        return false;

    if(!desc->iface.equals)
        return false;

    bool has_clone   = (desc->iface.clone != NULL);
    bool has_destroy = (desc->iface.destroy != NULL);

    if(has_clone != has_destroy)
        return false;

    return true;
}

/**
 * @brief Initializes a borrowed Sentinel from a descriptor.
 *
 * @param[out] out Pointer to the Sentinel to initialize. Must not be NULL.
 * @param[in] desc Read-only Sentinel configuration descriptor. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'out' or 'desc' is NULL, or if
 *         the descriptor is invalid.
 *
 * @post On success:
 *       - 'out' is initialized;
 *       - 'out' borrows 'desc->context';
 *       - 'out->owns_context' is false;
 *       - no allocation is performed by this function.
 *
 * @post On failure, 'out' is reset to an invalid Sentinel state.
 *
 * @attention 'out' must not contain a live initialized Sentinel.
 */
SentinelResult sentinel_init(Sentinel* out, const SentinelDesc* desc) {
    if(!out)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = (Sentinel){0};

    if(!desc || !sentinel_desc_is_valid(desc))
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = (Sentinel){
        .initialized       = true,
        .owns_context      = false,
        .type_size         = desc->type_size,
        .context           = desc->context,
        .iface             = desc->iface,
        .cloning_allocator = NULL,
    };

    return SENTINEL_SUCCESS;
}

/**
 * @brief Tests whether a Sentinel is valid and initialized.
 *
 * @param[in] sentinel Pointer to the Sentinel to test.
 *
 * @return true if the Sentinel is valid and initialized, false otherwise.
 */
bool sentinel_is_valid(const Sentinel* sentinel) {
    if(!sentinel)
        return false;

    return sentinel->initialized;
}

/**
 * @brief Tests whether an Iterator has reached a Sentinel boundary.
 *
 * @param[in] sentinel Pointer to an initialized Sentinel. Must not be NULL.
 * @param[in] iterator Pointer to an initialized Iterator. Must not be NULL.
 * @param[out] out Pointer receiving the comparison result. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_SENTINEL if 'sentinel' is NULL or invalid.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'iterator' is NULL or not
 *         initialized, or if 'out' is NULL.
 * @return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN if the Sentinel and Iterator have
 *         different element type sizes or belong to incompatible traversal domains.
 * @return Any error returned by the implementation-specific comparison callback.
 *
 * @post On success, '*out' is true if the Iterator satisfies the Sentinel's
 *       termination condition and false otherwise.
 *
 * @note The generic Sentinel layer validates element-size compatibility before
 *       invoking the implementation-specific comparison callback.
 */
SentinelResult sentinel_equals(const Sentinel* sentinel, const Iterator* iterator, bool* out) {
    if(!sentinel || !sentinel_is_valid(sentinel) || !sentinel->iface.equals)
        return SENTINEL_ERROR_INVALID_SENTINEL;

    if(!iterator || !iterator_is_valid(iterator) || !out)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    if(iterator_get_type_size(iterator) != sentinel_get_type_size(sentinel))
        return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN;

    return sentinel->iface.equals(sentinel->context, iterator->context, out);
}

/**
 * @brief Obtains the element type size expected by a Sentinel.
 *
 * @param[in] sentinel Pointer to an initialized Sentinel.
 *
 * @return The expected element type size in bytes.
 * @return 0 if 'sentinel' is NULL or not initialized.
 */
size_t sentinel_get_type_size(const Sentinel* sentinel) {
    if(!sentinel || !sentinel_is_valid(sentinel))
        return (size_t)0;

    return sentinel->type_size;
}

/**
 * @brief Creates an independent owned clone of a Sentinel.
 *
 * The cloned Sentinel represents the same termination boundary and traversal
 * domain as the source Sentinel.
 *
 * @param[in] sentinel Initialized Sentinel to clone. Must not be NULL.
 * @param[out] out Sentinel receiving the independent clone. Must not be NULL.
 * @param[in] allocator Allocator used to allocate the cloned context. Must not
 *                      be NULL and must remain valid until the clone is destroyed.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_SENTINEL if 'sentinel' is NULL or invalid.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'out' is NULL or aliases
 *         'sentinel'.
 * @return SENTINEL_ERROR_INVALID_ALLOCATOR if 'allocator' is NULL or not initialized.
 * @return SENTINEL_ERROR_UNSUPPORTED_OPERATION if cloning is not supported.
 * @return Any error returned by the implementation-specific cloning callback.
 *
 * @post On success:
 *       - 'out' is initialized;
 *       - 'out' represents the same termination boundary and traversal domain;
 *       - 'out' owns an independent context;
 *       - 'out' stores 'allocator' for later destruction.
 *
 * @post On failure, if 'out' is non-NULL and does not alias 'sentinel', it is
 *       reset to an invalid Sentinel state.
 *
 * @attention A successfully cloned Sentinel must eventually be passed to
 *            sentinel_destroy().
 *
 * @attention 'out' must not contain a live initialized Sentinel.
 * @attention 'out' must not point to the same Sentinel as 'sentinel'.
 */
SentinelResult sentinel_clone(const Sentinel* sentinel, Sentinel* out, Allocator* allocator) {
    if(!out)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    if(out == sentinel)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = (Sentinel){0};

    if(!sentinel || !sentinel_is_valid(sentinel))
        return SENTINEL_ERROR_INVALID_SENTINEL;

    if(!allocator || !allocator->initialized)
        return SENTINEL_ERROR_INVALID_ALLOCATOR;

    if(!sentinel->iface.clone)
        return SENTINEL_ERROR_UNSUPPORTED_OPERATION;

    void* clone_ctx       = NULL;
    SentinelResult result = sentinel->iface.clone(sentinel->context, &clone_ctx, allocator);
    if(result != SENTINEL_SUCCESS)
        return result;

    *out = (Sentinel){
        .initialized       = true,
        .owns_context      = true,
        .type_size         = sentinel->type_size,
        .context           = clone_ctx,
        .iface             = sentinel->iface,
        .cloning_allocator = allocator,
    };

    return SENTINEL_SUCCESS;
}

/**
 * @brief Destroys a Sentinel.
 *
 * If the Sentinel owns its context, the context is released using the
 * implementation-specific destruction callback and the allocator originally
 * supplied to sentinel_clone().
 *
 * If the Sentinel does not own its context, only the Sentinel wrapper is
 * invalidated and the borrowed context is left untouched.
 *
 * @param[in, out] sentinel Sentinel to destroy. Must not be NULL and must be initialized.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_SENTINEL if 'sentinel' is NULL or invalid.
 * @return SENTINEL_ERROR_INVALID_ALLOCATOR if an owned Sentinel's stored
 *         cloning allocator is NULL or no longer initialized.
 * @return SENTINEL_ERROR_UNSUPPORTED_OPERATION if an owned Sentinel does not
 *         provide the required destruction callback.
 * @return Any error returned by the implementation-specific destruction callback.
 *
 * @post On success, the Sentinel is reset to an invalid state.
 *
 * @post On failure, an owned Sentinel retains ownership of its context and
 *       remains unchanged.
 *
 * @note Destroying an initialized Sentinel that does not own its context is a
 *       successful operation and does not affect the borrowed context.
 */
SentinelResult sentinel_destroy(Sentinel* sentinel) {
    if(!sentinel || !sentinel_is_valid(sentinel))
        return SENTINEL_ERROR_INVALID_SENTINEL;

    if(!sentinel->owns_context) {
        *sentinel = (Sentinel){0};
        return SENTINEL_SUCCESS;
    }

    if(!sentinel->cloning_allocator || !sentinel->cloning_allocator->initialized)
        return SENTINEL_ERROR_INVALID_ALLOCATOR;

    if(!sentinel->iface.destroy)
        return SENTINEL_ERROR_UNSUPPORTED_OPERATION;

    SentinelResult result = sentinel->iface.destroy(sentinel->context, sentinel->cloning_allocator);
    if(result != SENTINEL_SUCCESS)
        return result;

    *sentinel = (Sentinel){0};
    return SENTINEL_SUCCESS;
}

/**
 * @brief Converts a numeric SentinelResult code into a human-readable string.
 *
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 *
 * @note The returned string is static and must not be freed.
 */
const char* sentinel_result_to_string(SentinelResult result) {
    switch(result) {
        case SENTINEL_SUCCESS:
            return "no error";
        case SENTINEL_ERROR_INVALID_SENTINEL:
            return "null or invalid sentinel";
        case SENTINEL_ERROR_INVALID_ARGUMENTS:
            return "invalid call argument";
        case SENTINEL_ERROR_INCOMPATIBLE_DOMAIN:
            return "incompatible iteration domain";
        case SENTINEL_ERROR_UNSUPPORTED_OPERATION:
            return "unsupported operation";
        case SENTINEL_ERROR_ALLOCATOR_ERROR:
            return "allocator error";
        default:
            return NULL;
    }
}
