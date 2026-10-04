/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file sentinel.h
 *
 * @author Nelson Somé
 *
 * @brief Generic sentinel interface.
 *
 * A Sentinel represents a traversal termination boundary that can be compared
 * against a compatible Iterator.
 *
 * Sentinels may either borrow implementation-specific context or own an
 * independently cloned context.
 *
 * Sentinel implementations must satisfy the contracts documented by this API.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_SENTINEL_H_
#define ARC_SENTINEL_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations. */
typedef struct Allocator Allocator;
typedef struct Iterator Iterator;
typedef struct SentinelDesc SentinelDesc;
typedef struct SentinelInterface SentinelInterface;
typedef struct Sentinel Sentinel;

typedef /** SentinelResult */
    /**
     * @brief SentinelResult is the set of values that describe whether a sentinel
     *        operation succeeded or why it failed.
     */
    enum SentinelResult {
        /** Operation succeeded. */
        SENTINEL_SUCCESS = 0,

        /** Sentinel is NULL or invalid. */
        SENTINEL_ERROR_INVALID_SENTINEL,

        /** Required allocator is NULL or otherwise unusable. */
        SENTINEL_ERROR_INVALID_ALLOCATOR,

        /** Invalid arguments were provided to the operation. */
        SENTINEL_ERROR_INVALID_ARGUMENTS,

        /** Requested operation is not supported by this Sentinel. */
        SENTINEL_ERROR_UNSUPPORTED_OPERATION,

        /** Sentinel and Iterator belong to incompatible iteration domains. */
        SENTINEL_ERROR_INCOMPATIBLE_DOMAIN,

        /** An allocator operation required by the Sentinel failed. */
        SENTINEL_ERROR_ALLOCATOR_ERROR,
    } SentinelResult;

/**
 * @brief Callback function type for comparing a Sentinel with an Iterator.
 *
 * @param[in] sentinel_ctx Sentinel-specific context.
 * @param[in] iterator_ctx Iterator-specific context.
 * @param[out] out Pointer receiving the comparison result. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'out' is NULL or if the
 *         implementation rejects either context.
 * @return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN if the Sentinel and Iterator
 *         belong to incompatible iteration domains.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - 'out' must not be NULL;
 *            - the behavior for sentinel_ctx = NULL is implementation-specific;
 *            - the behavior for iterator_ctx = NULL is implementation-specific;
 *            - on success, '*out' must be true if the Iterator satisfies the
 *              Sentinel's termination condition and false otherwise;
 *            - on failure, the value pointed to by 'out' is unspecified.
 */
typedef SentinelResult (*SentinelEqualsFn)(const void* sentinel_ctx, const void* iterator_ctx, bool* out);

/**
 * @brief Sentinel context cloning callback.
 *
 * Creates an independent copy of a Sentinel-specific context.
 *
 * @param[in] context Sentinel context to clone.
 * @param[out] out Pointer receiving the cloned context. Must not be NULL.
 * @param[in] allocator Allocator used to create the cloned context. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS for invalid arguments.
 * @return SENTINEL_ERROR_ALLOCATOR_ERROR if allocation fails.
 * @return Any other implementation-specific SentinelResult.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - 'out' must not be NULL;
 *            - 'allocator' must not be NULL;
 *            - on success, '*out' owns an independent context;
 *            - modifying the cloned context must not modify the state represented
 *              by 'context';
 *            - the clone must represent the same termination boundary and
 *              traversal domain as 'context';
 *            - on failure, '*out' must be set to NULL;
 *            - any resources allocated before failure must be released before
 *              returning.
 *
 * @note The behavior for context = NULL is implementation-specific.
 *
 * @note A successfully cloned context must later be released using the
 *       corresponding SentinelDestroyFn and the same allocator.
 */
typedef SentinelResult (*SentinelCloneFn)(const void* context, void** out, Allocator* allocator);

/**
 * @brief Sentinel context destruction callback.
 *
 * Releases a Sentinel-specific context previously created by the corresponding
 * SentinelCloneFn.
 *
 * @param[in, out] context Sentinel context to destroy.
 * @param[in] allocator Allocator originally used to create the context.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return Any implementation-specific SentinelResult on failure.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - 'allocator' must be the allocator originally used to create the
 *              cloned context;
 *            - on success, all resources owned by the context are released;
 *            - after successful destruction, 'context' must no longer be used;
 *            - on failure, ownership remains with the caller and the context
 *              must remain valid for another destruction attempt.
 *
 * @note The behavior for context = NULL is implementation-specific.
 */
typedef SentinelResult (*SentinelDestroyFn)(void* context, Allocator* allocator);

/**
 * @brief Generic Sentinel interface.
 */
struct SentinelInterface {
    /** Sentinel comparison callback. Must not be NULL. */
    SentinelEqualsFn equals;

    /** Sentinel context cloning callback. May be NULL if cloning is unsupported. */
    SentinelCloneFn clone;

    /** Sentinel context destruction callback. May be NULL if cloning is unsupported. */
    SentinelDestroyFn destroy;
};

/**
 * @brief Sentinel configuration object used to initialize a Sentinel.
 *
 * @attention A valid descriptor must satisfy the following:
 *            - 'type_size' must not be zero;
 *            - 'equals' must not be NULL;
 *            - 'clone' and 'destroy' must either both be NULL or both be non-NULL;
 *            - it is the responsibility of the Sentinel implementation to ensure
 *              the validity of the custom termination strategy.
 */
struct SentinelDesc {
    /** Sentinel-specific state. May be NULL. */
    void* context;

    /**
     * Size in bytes of the element type expected by this Sentinel.
     *
     * The value is fixed at initialization and must not be zero.
     */
    size_t type_size;

    /** Sentinel interface. */
    SentinelInterface iface;
};

/**
 * @brief Generic Sentinel value for Iterator comparison.
 *
 * A Sentinel represents a traversal termination boundary. It does not need to
 * correspond to a physical end position and may instead represent any
 * implementation-defined stopping condition.
 *
 * @attention Copying a Sentinel directly performs a shallow copy. If the
 *            Sentinel owns its context, shallow copying duplicates ownership
 *            metadata and may result in multiple Sentinel objects attempting
 *            to release the same context.
 *
 *            Use sentinel_clone() when an independent Sentinel is required.
 */
struct Sentinel {
    /** 'true' if the Sentinel was initialized successfully. */
    bool initialized;

    /** 'true' if the Sentinel owns its context and must release it. */
    bool owns_context;

    /**
     * Size in bytes of the element type expected by this Sentinel.
     *
     * The value is fixed at initialization and must not be zero.
     */
    size_t type_size;

    /** Sentinel-specific state. May be NULL. */
    void* context;

    /** Sentinel interface. */
    SentinelInterface iface;

    /**
     * Allocator used to create an owned cloned context.
     *
     * NULL when the Sentinel does not own its context.
     */
    Allocator* cloning_allocator;
};

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
SentinelResult sentinel_init(Sentinel* out, const SentinelDesc* desc);

/**
 * @brief Tests whether a Sentinel is valid and initialized.
 *
 * @param[in] sentinel Pointer to the Sentinel to test.
 *
 * @return true if the Sentinel is valid and initialized, false otherwise.
 */
bool sentinel_is_valid(const Sentinel* sentinel);

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
SentinelResult sentinel_equals(const Sentinel* sentinel, const Iterator* iterator, bool* out);

/**
 * @brief Obtains the element type size expected by a Sentinel.
 *
 * @param[in] sentinel Pointer to an initialized Sentinel.
 *
 * @return The expected element type size in bytes.
 * @return 0 if 'sentinel' is NULL or not initialized.
 */
size_t sentinel_get_type_size(const Sentinel* sentinel);

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
SentinelResult sentinel_clone(const Sentinel* sentinel, Sentinel* out, Allocator* allocator);

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
SentinelResult sentinel_destroy(Sentinel* sentinel);

/**
 * @brief Converts a numeric SentinelResult code into a human-readable string.
 *
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 *
 * @note The returned string is static and must not be freed.
 */
const char* sentinel_result_to_string(SentinelResult result);

#ifdef __cplusplus
}
#endif

#endif /* ARC_SENTINEL_H_ */
