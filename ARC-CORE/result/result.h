/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file result.h
 *
 * @author Nelson Somé
 *
 * @brief Generic operation result representation.
 *
 * Provides a common result model for Arc Core operations.
 *
 * A Result represents either:
 * - a successful operation;
 * - a failed operation without residual ownership;
 * - a failed operation that transfers ownership of one residual resource to
 *   the caller.
 *
 * Errors are identified by an ErrorDomain and a domain-specific numeric code.
 *
 * Residues represent resources that remain owned after an operation fails and
 * cannot complete its cleanup before returning.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_RESULT_H_
#define ARC_RESULT_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations. */
typedef struct Allocator Allocator;
typedef struct Error Error;
typedef struct Residue Residue;
typedef struct Result Result;

typedef /** ErrorDomain */
    /**
     * @brief Identifies the subsystem from which an Error originates.
     *
     * The numeric Error code stored in Error is interpreted according to the
     * corresponding domain.
     *
     * ERROR_DOMAIN_NONE represents the absence of an Error and is used by
     * successful Results.
     */
    enum ErrorDomain {
        /** No Error is present. */
        ERROR_DOMAIN_NONE = 0,

        /** Error originated from the Allocator subsystem. */
        ERROR_DOMAIN_ALLOCATOR,

        /** Error originated from the Iterator subsystem. */
        ERROR_DOMAIN_ITERATOR,

        /** Error originated from the Sentinel subsystem. */
        ERROR_DOMAIN_SENTINEL,

        /** Error originated from the Range subsystem. */
        ERROR_DOMAIN_RANGE,

        /** Error originated from the Array subsystem. */
        ERROR_DOMAIN_ARRAY,

        /** Additional domains may be introduced as Arc Core grows. */
    } ErrorDomain;

typedef /** ResidueKind */
    /**
     * @brief Identifies the kind of residual resource owned by a failed Result.
     *
     * A Result may contain at most one residual resource.
     *
     * RESIDUE_KIND_NONE represents the absence of residual ownership.
     */
    enum ResidueKind {
        /** No residual resource is present. */
        RESIDUE_KIND_NONE = 0,

        /** Residual resource is an allocation that must eventually be released. */
        RESIDUE_KIND_ALLOCATION,
    } ResidueKind;

/**
 * @brief Describes an operation Error.
 *
 * An Error consists of:
 * - a domain identifying the subsystem that produced the Error;
 * - a domain-specific numeric error code.
 *
 * The interpretation of 'code' depends entirely on 'domain'.
 *
 * @attention ERROR_DOMAIN_NONE represents the absence of an Error and is valid
 *            only for successful Result states.
 */
struct Error {
    /** Subsystem from which the Error originates. */
    ErrorDomain domain;

    /** Domain-specific numeric Error code. */
    int32_t code;
};

/**
 * @brief Describes residual ownership left behind by a failed operation.
 *
 * A Residue represents a resource that could not be released while an
 * operation was rolling back or otherwise handling failure.
 *
 * A Result may contain at most one Residue.
 *
 * For RESIDUE_KIND_NONE:
 * - 'ptr' must be NULL;
 * - 'allocator' must be NULL.
 *
 * For RESIDUE_KIND_ALLOCATION:
 * - 'ptr' must not be NULL;
 * - 'allocator' must not be NULL;
 * - 'allocator' must be initialized;
 * - 'allocator' must support ALLOCATOR_FEATURE_FREE;
 * - ownership of the allocation is transferred to the caller.
 *
 * @attention The caller is responsible for eventually releasing any residual
 *            resource transferred through a Result.
 *
 * @attention An allocation Residue must always contain enough information to
 *            attempt final cleanup independently of the operation that
 *            originally produced it.
 */
struct Residue {
    /** Kind of residual resource represented by this object. */
    ResidueKind kind;

    /** Pointer to the residual resource, if one exists. */
    void* ptr;

    /**
     * @brief Allocator associated with an allocation Residue.
     *
     * For RESIDUE_KIND_ALLOCATION, this must be the initialized Allocator that
     * owns 'ptr' and must support ALLOCATOR_FEATURE_FREE.
     *
     * For RESIDUE_KIND_NONE, this must be NULL.
     */
    Allocator* allocator;
};

/**
 * @brief Generic operation Result.
 *
 * A Result represents either success or failure and optionally carries
 * ownership of one residual resource after failure.
 *
 * A successful Result satisfies:
 * - 'ok' is true;
 * - 'error.domain' is ERROR_DOMAIN_NONE;
 * - 'error.code' is zero;
 * - 'residue.kind' is RESIDUE_KIND_NONE;
 * - 'residue.ptr' is NULL;
 * - 'residue.allocator' is NULL.
 *
 * A failed Result satisfies:
 * - 'ok' is false;
 * - 'error.domain' is not ERROR_DOMAIN_NONE;
 * - the Result either contains no Residue or exactly one valid Residue.
 *
 * @attention A successful Result must never contain an Error or Residue.
 *
 * @attention A Result may contain at most one residual resource.
 *
 * @attention If a failed Result contains a Residue, ownership of that residual
 *            resource is transferred to the caller.
 */
struct Result {
    /** 'true' if the represented operation succeeded. */
    bool ok;

    /** Error information associated with the Result. */
    Error error;

    /** Optional residual resource ownership associated with failure. */
    Residue residue;
};

/**
 * @brief Creates a successful Result.
 *
 * Constructs the canonical successful Result state.
 *
 * The returned Result satisfies:
 * - 'ok' is true;
 * - 'error.domain' is ERROR_DOMAIN_NONE;
 * - 'error.code' is zero;
 * - 'residue.kind' is RESIDUE_KIND_NONE;
 * - 'residue.ptr' is NULL;
 * - 'residue.allocator' is NULL.
 *
 * @return A valid successful Result.
 *
 * @post The returned Result satisfies result_is_valid().
 *
 * @note A successful Result never contains an Error or Residue.
 */
Result result_success(void);

/**
 * @brief Creates a failed Result without residual ownership.
 *
 * Constructs a failed Result from 'error'.
 *
 * @param[in] error Error describing the cause of the failure.
 *
 * @return A valid failed Result containing 'error' and no Residue.
 *
 * @post The returned Result satisfies:
 *       - 'ok' is false;
 *       - 'error' equals the supplied Error;
 *       - 'residue.kind' is RESIDUE_KIND_NONE;
 *       - 'residue.ptr' is NULL;
 *       - 'residue.allocator' is NULL.
 *
 * @post The returned Result satisfies result_is_valid().
 *
 * @attention 'error.domain' must not be ERROR_DOMAIN_NONE.
 *
 * @attention Supplying an invalid Error represents a programming error and
 *            terminates program execution.
 */
Result result_failure(Error error);

/**
 * @brief Creates a failed Result containing residual ownership.
 *
 * Constructs a failed Result from 'error' and transfers ownership of exactly
 * one residual resource described by 'residue' to the caller.
 *
 * A Residue exists when an operation fails and a resource acquired during that
 * operation cannot be released before returning.
 *
 * @param[in] error Error describing the cause of the failure.
 * @param[in] residue Residual resource whose ownership is transferred to the
 *                    caller.
 *
 * @return A valid failed Result containing 'error' and 'residue'.
 *
 * @post The returned Result satisfies:
 *       - 'ok' is false;
 *       - 'error' equals the supplied Error;
 *       - 'residue' equals the supplied Residue.
 *
 * @post The returned Result satisfies result_is_valid().
 *
 * @attention 'error.domain' must not be ERROR_DOMAIN_NONE.
 *
 * @attention 'residue' must describe a valid residual resource.
 *
 * @attention For RESIDUE_KIND_ALLOCATION:
 *            - 'residue.ptr' must not be NULL;
 *            - 'residue.allocator' must not be NULL;
 *            - 'residue.allocator' must be initialized.
 *
 * @attention Supplying an invalid Error or Residue represents a programming
 *            error and terminates program execution.
 *
 * @attention Ownership of the residual resource is transferred to the caller.
 *            The caller is responsible for eventually releasing that resource
 *            using the allocator stored by the Residue.
 *
 * @note A Result can contain at most one residual resource.
 */
Result result_failure_with_residue(Error error, Residue residue);

/**
 * @brief Tests whether a Result represents a valid canonical state.
 *
 * A successful Result is valid only if:
 * - 'ok' is true;
 * - 'error.domain' is ERROR_DOMAIN_NONE;
 * - 'error.code' is zero;
 * - 'residue.kind' is RESIDUE_KIND_NONE;
 * - 'residue.ptr' is NULL;
 * - 'residue.allocator' is NULL.
 *
 * A failed Result is valid only if:
 * - 'ok' is false;
 * - 'error.domain' is not ERROR_DOMAIN_NONE;
 * - the Result either contains no Residue or exactly one valid Residue.
 *
 * A Result without residue is valid only if:
 * - 'residue.kind' is RESIDUE_KIND_NONE;
 * - 'residue.ptr' is NULL;
 * - 'residue.allocator' is NULL.
 *
 * A Result containing an allocation residue is valid only if:
 * - 'residue.kind' is RESIDUE_KIND_ALLOCATION;
 * - 'residue.ptr' is not NULL;
 * - 'residue.allocator' is not NULL;
 * - 'residue.allocator' is initialized.
 *
 * @param[in] result Result to validate.
 *
 * @return true if 'result' represents a valid Result state.
 * @return false if 'result' is NULL or represents an invalid state.
 *
 * @note This function does not modify the Result or any resource referenced by
 *       its Residue.
 *
 * @note This function is useful for validating Results that were constructed
 *       manually rather than through the Result construction functions.
 */
bool result_is_valid(const Result* result);

/**
 * @brief Attempts to release the resource owned by a Residue.
 *
 * This operation represents the final cleanup step for residual ownership.
 *
 * For RESIDUE_KIND_ALLOCATION, the stored allocation is released using the
 * Allocator referenced by 'residue->allocator'.
 *
 * @param[in, out] residue Residue whose owned resource will be released.
 *
 * @return true if the residual resource is released successfully.
 * @return false if 'residue' is NULL, invalid, or if releasing the owned
 *         resource fails.
 *
 * @post On success:
 *       - the residual resource is released;
 *       - 'residue->kind' is RESIDUE_KIND_NONE;
 *       - 'residue->ptr' is NULL;
 *       - 'residue->allocator' is NULL.
 *
 * @post On failure, 'residue' remains unchanged and ownership of the residual
 *       resource remains with the caller.
 *
 * @attention For RESIDUE_KIND_ALLOCATION:
 *            - 'residue->ptr' must not be NULL;
 *            - 'residue->allocator' must not be NULL;
 *            - 'residue->allocator' must be initialized;
 *            - 'residue->allocator' must support ALLOCATOR_FEATURE_FREE.
 *
 * @attention Failure does not transfer or discard ownership. The caller remains
 *            responsible for deciding how to handle the unreleased resource,
 *            including whether the failure should be logged, tolerated, or
 *            treated as fatal.
 *
 * @note This function does not panic or terminate program execution.
 */
bool residue_destroy(Residue* residue);

/**
 * @brief Formats a human-readable description of a Residue.
 *
 * Writes diagnostic information describing the Residue into 'buffer'.
 *
 * The formatted output may include information such as:
 * - the Residue kind;
 * - the residual resource pointer;
 * - the associated Allocator pointer;
 * - relevant allocator state or capabilities.
 *
 * This function does not release, modify, or transfer ownership of the
 * residual resource.
 *
 * @param[in] residue Residue to describe.
 * @param[out] buffer Buffer receiving the formatted description.
 * @param[in] buffer_size Size of 'buffer' in bytes.
 *
 * @return true if the Residue is valid and its description is written
 *         successfully.
 * @return false if 'residue' is NULL or invalid, if 'buffer' is NULL, if
 *         'buffer_size' is zero, or if the description cannot be represented
 *         within the provided buffer.
 *
 * @post On success, 'buffer' contains a null-terminated human-readable
 *       description of 'residue'.
 *
 * @post On failure, the contents of 'buffer' are unspecified.
 *
 * @attention This function does not modify 'residue' or any resource referenced
 *            by it.
 *
 * @attention Formatting failure does not affect ownership of the residual
 *            resource and does not prevent the caller from inspecting the
 *            Residue directly.
 *
 * @note This function is intended for diagnostics, logging, and panic messages.
 */
bool residue_format(const Residue* residue, char* buffer, size_t buffer_size);

/**
 * @brief Converts an ErrorDomain value into a human-readable string.
 *
 * @param[in] domain Error domain to convert.
 *
 * @return A human-readable static string describing 'domain'.
 * @return NULL if 'domain' does not contain a valid ErrorDomain value.
 *
 * @note The returned string has static storage duration and must not be modified
 *       or freed.
 */
const char* error_domain_to_string(ErrorDomain domain);

/**
 * @brief Converts a ResidueKind value into a human-readable string.
 *
 * @param[in] kind Residue kind to convert.
 *
 * @return A human-readable static string describing 'kind'.
 * @return NULL if 'kind' does not contain a valid ResidueKind value.
 *
 * @note The returned string has static storage duration and must not be modified
 *       or freed.
 */
const char* residue_kind_to_string(ResidueKind kind);

#ifdef __cplusplus
}
#endif

#endif /* ARC_RESULT_H_ */
