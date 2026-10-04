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

        /** Invalid arguments were provided to the operation. */
        SENTINEL_ERROR_INVALID_ARGUMENTS,

        /** Sentinel and Iterator belong to incompatible iteration domains. */
        SENTINEL_ERROR_INCOMPATIBLE_DOMAIN,
    } SentinelResult;

/**
 * @brief Callback function type for comparing a sentinel with an iterator.
 *
 * @param[in] sentinel_ctx Sentinel-specific context.
 * @param[in] iterator_ctx Iterator-specific context.
 * @param[out] out Pointer receiving the comparison result. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'out' is NULL or if the
 *         implementation rejects either context.
 * @return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN if the sentinel and iterator
 *         belong to incompatible iteration domains.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - 'out' must not be NULL;
 *            - the behavior for sentinel_ctx = NULL is implementation-specific;
 *            - the behavior for iterator_ctx = NULL is implementation-specific;
 *            - on success, '*out' must be true if the iterator satisfies the
 *              sentinel's termination condition and false otherwise;
 *            - on failure, the value pointed to by 'out' is unspecified.
 */
typedef SentinelResult (*SentinelEqualsFn)(const void* sentinel_ctx, const void* iterator_ctx, bool* out);

/**
 * @brief Generic sentinel interface.
 */
struct SentinelInterface {
    /** Sentinel comparison callback. */
    SentinelEqualsFn equals;
};

/**
 * @brief Sentinel configuration object used to initialize a Sentinel.
 *
 * @attention A valid descriptor must satisfy the following:
 *            - 'type_size' must not be zero;
 *            - 'equals' must not be NULL;
 *            - it is the responsibility of the sentinel implementation to
 *              ensure the validity of the custom termination strategy.
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
 * @brief Generic sentinel value for iterator comparison.
 *
 * A Sentinel represents a traversal termination boundary. It does not need to
 * correspond to a physical end position and may instead represent any
 * implementation-defined stopping condition.
 */
struct Sentinel {
    /** 'true' if the Sentinel was initialized successfully. */
    bool initialized;

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
};

/**
 * @brief Initializes a Sentinel from a descriptor.
 *
 * @param[out] out Pointer to the Sentinel to initialize. Must not be NULL.
 * @param[in] desc Read-only Sentinel configuration descriptor. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'out' or 'desc' is NULL, or if
 *         the descriptor is invalid.
 *
 * @post On success, 'out' is initialized and borrows 'desc->context'.
 *
 * @post On failure, 'out' is reset to an invalid Sentinel state.
 *
 * @attention 'out' must not contain a live initialized Sentinel.
 */
SentinelResult sentinel_init(Sentinel* out, const SentinelDesc* desc);

/**
 * @brief Tests whether a Sentinel is valid.
 *
 * @param[in] sentinel Pointer to the Sentinel to test.
 *
 * @return true if the Sentinel is valid, false otherwise.
 */
bool sentinel_is_valid(const Sentinel* sentinel);

/**
 * @brief Tests whether an iterator has reached a Sentinel boundary.
 *
 * @param[in] sentinel Pointer to an initialized Sentinel. Must not be NULL.
 * @param[in] iterator Pointer to an initialized Iterator. Must not be NULL.
 * @param[out] out Pointer receiving the comparison result. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_SENTINEL if 'sentinel' is NULL or invalid.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'iterator' or 'out' is NULL.
 * @return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN if the Sentinel and Iterator
 *         have different element type sizes or belong to incompatible
 *         iteration domains.
 *
 * @post On success, '*out' is true if the iterator satisfies the Sentinel's
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
 * @brief Converts a numeric sentinel result code into a human-readable string.
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
