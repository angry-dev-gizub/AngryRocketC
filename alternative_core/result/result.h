/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file result.h
 *
 * @author Nelson Somé
 *
 * @brief Generic operation result representation.
 *
 * Provides a common result model for Arc Core operations using a packed
 * uint32_t error value with numeric layout 0x00SSDDCC.
 *
 * An arc_result_t represents either:
 * - a successful operation;
 * - a failed operation without residual ownership;
 * - a failed operation that transfers ownership of one residual resource to
 *   the caller.
 *
 * Residues describe resources left owned after an operation fails and cannot
 * complete its cleanup before returning.
 *
 * Result constructors panic and abort program execution on detected contract
 * violations. Ordinary operation failures are represented by failed Results.
 * Callers inspect 'ok'; no separate public validity check is required.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_RESULT_H_
#define ARC_RESULT_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "errorpack.h"
#include "errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convenience macro for propagating a Result failure.
 *
 * Evaluates 'expr' and returns its Result if it is a failure. Otherwise
 * execution continues normally.
 *
 * @param expr Expression that evaluates to an arc_result_t.
 *
 * @post On failure, the Result returned by 'expr' is returned from the
 *       enclosing function.
 */
#define ARC_TRY(expr)                                                                                                  \
    do {                                                                                                               \
        arc_result_t result = (expr);                                                                                  \
        if(!result.ok)                                                                                                 \
            return result;                                                                                             \
    } while(0)

/**
 * @brief Convenience macro for propagating a Result failure or panicking.
 *
 * Evaluates 'expr' and returns its Result if it is a failure. Otherwise
 * execution continues normally.
 *
 * If 'expr' fails, the macro formats the Result's error value and calls
 * arc_abort() with that description. This is intended for use in contexts
 * where failure is not expected and cannot be handled gracefully.
 *
 * @param expr Expression that evaluates to an arc_result_t.
 *
 * @post On failure, the Result returned by 'expr' is formatted and passed to
 *       arc_abort(), which aborts program execution.
 */
#define ARC_TRY_OR_ABORT(expr)                                                                                         \
    do {                                                                                                               \
        arc_result_t result = (expr);                                                                                  \
        char buff[1024];                                                                                               \
        if(!result.ok)                                                                                                 \
            if(!arc_error_format(result.error, buff, 1024))                                                            \
                arc_abort("Arc result: %08X", (unsigned)result.error);                                                             \
        arc_abort("Arc error: %s", buff);                                                                              \
    } while(0)

/** Forward declarations. */
typedef struct arc_allocator_t arc_allocator_t;
typedef struct arc_residue_t arc_residue_t;
typedef struct arc_result_t arc_result_t;

/**
 * @brief Identifies the kind of residual resource owned by a failed Result.
 *
 * ARC_RESIDUE_KIND_NONE represents the absence of residual ownership.
 */
typedef enum arc_residue_kind_t {
    /** No residual resource is present. */
    ARC_RESIDUE_KIND_NONE = 0,

    /** Allocation owned by the caller and awaiting release. */
    ARC_RESIDUE_KIND_ALLOCATION,
} arc_residue_kind_t;

/**
 * @brief Describes residual ownership left behind by a failed operation.
 *
 * An arc_residue_t represents a resource that could not be released while an
 * operation was rolling back or otherwise handling failure.
 *
 * For ARC_RESIDUE_KIND_NONE:
 * - 'ptr' must be NULL;
 * - 'allocator' must be NULL.
 *
 * For ARC_RESIDUE_KIND_ALLOCATION:
 * - 'ptr' must not be NULL;
 * - 'allocator' must not be NULL;
 * - 'allocator' must be initialized;
 * - 'allocator' must support ordinary allocation release;
 * - 'ptr' must be compatible with that allocator's ordinary free operation;
 * - ownership of the allocation is transferred to the caller;
 * - the allocator object and its context must remain alive and usable until
 *   cleanup completes;
 * - the allocation must not be independently freed or invalidated by an
 *   allocator reset while it is owned through this Residue.
 *
 * @attention The caller is responsible for eventually releasing any residual
 *            resource transferred through a Result.
 *
 * @attention An allocation Residue must always contain enough information to
 *            attempt final cleanup independently of the operation that
 *            originally produced it.
 *
 * @attention The allocator pointer is borrowed. Destroying the Residue releases
 *            its allocation, not the allocator object or its context.
 *
 * @note Copying a Residue or Result copies its fields, not the allocation.
 *       There must be only one logical owner responsible for cleanup. A copy
 *       used to inspect or validate the value does not acquire ownership.
 */
struct arc_residue_t {
    /** Kind of residual resource represented by this object. */
    arc_residue_kind_t kind;

    /** Pointer to the residual resource, if one exists. */
    void* ptr;

    /**
     * @brief Allocator associated with an allocation Residue.
     *
     * For ARC_RESIDUE_KIND_ALLOCATION, this must be the initialized Allocator
     * that owns 'ptr' and supports the matching ordinary free operation.
     *
     * For ARC_RESIDUE_KIND_NONE, this must be NULL.
     */
    const arc_allocator_t* allocator;
};

/**
 * @brief Generic operation Result.
 *
 * An arc_result_t represents either success or failure and optionally carries
 * ownership of one residual resource after failure.
 *
 * A successful Result satisfies:
 * - 'ok' is true;
 * - 'error' is ARC_ERROR_NONE (numeric value zero);
 * - 'residue.kind' is ARC_RESIDUE_KIND_NONE;
 * - 'residue.ptr' is NULL;
 * - 'residue.allocator' is NULL.
 *
 * A failed Result satisfies:
 * - 'ok' is false;
 * - 'error' is nonzero and its reserved upper 8 bits are zero;
 * - its severity is ARC_SEVERITY_LOG, ARC_SEVERITY_WARNING,
 *   ARC_SEVERITY_ERROR, or ARC_SEVERITY_FATAL;
 * - the Result either contains no Residue or exactly one valid Residue.
 *
 * @attention A successful Result must contain neither a nonzero error value
 *            nor residual ownership.
 *
 * @note 'ok' is the authoritative success indicator. Severity describes the
 *       associated error; it is not a replacement for testing 'ok'. In this
 *       model, successful Results do not carry warning or informational codes.
 *       A failed Result may carry a warning-classified code.
 *
 * @note Failure requires a recognized non-NONE severity. Domain and code
 *       identifiers need not be recognized by this implementation. Domain zero
 *       is the common domain and is valid on failure. Each operation documents
 *       which error values it can return.
 *
 * @attention Manually constructed or modified Results must satisfy these same
 *            invariants. Constructors cannot enforce invariants after a caller
 *            changes public fields or detect arbitrary memory corruption.
 *
 * @attention A Result may contain at most one residual resource.
 *
 * @attention If a failed Result contains a Residue, ownership of that residual
 *            resource is transferred to the caller.
 */
struct arc_result_t {
    /** 'true' if the represented operation succeeded. */
    bool ok;

    /** Packed 0x00SSDDCC error value; zero on success, nonzero on failure. */
    uint32_t error;

    /** Optional list of residual resource ownership associated with failures. */
    arc_residue_t residue;
};

/**
 * @brief Reports a formatted diagnostic and aborts program execution.
 *
 * Attempts to write the diagnostic to stderr, followed by a newline,
 * and then terminates the process by calling abort().
 *
 * @param[in] format Null-terminated printf-style format string.
 * @param[in] ... Arguments required by the format string.
 *
 * @attention The caller must satisfy the following requirements:
 *            - 'format' must not be NULL;
 *            - 'format' must point to a valid null-terminated string;
 *            - each argument must match its corresponding format conversion;
 *            - any objects accessed through the arguments must remain valid
 *              for the duration of diagnostic formatting.
 *
 * @attention This function does not return. Process termination must occur
 *            even if diagnostic formatting or output fails.
 *
 * @attention Automatic resource cleanup is not performed.
 *
 * @note This function is active in all build configurations, including
 *       builds where ARC_DEBUG is defined.
 *
 * @note Pass external text as a string argument using "%s", rather than
 *       using that text directly as the format string.
 */
void arc_abort(const char* format, ...);

/**
 * @brief Creates a successful Result.
 *
 * Constructs the canonical successful Result state.
 *
 * The returned Result satisfies:
 * - 'ok' is true;
 * - 'error' is ARC_ERROR_NONE (numeric value zero);
 * - 'residue.kind' is ARC_RESIDUE_KIND_NONE;
 * - 'residue.ptr' is NULL;
 * - 'residue.allocator' is NULL.
 *
 * @return A valid successful Result.
 *
 * @post The returned Result satisfies the invariants documented for
 *       arc_result_t.
 *
 * @note A successful Result contains no nonzero error value or residual
 *       ownership.
 */
arc_result_t arc_result_success(void);

/**
 * @brief Creates a failed Result without residual ownership.
 *
 * Constructs a failed Result from 'error'.
 *
 * @param[in] error Packed error value describing the cause of failure.
 *
 * @return A valid failed Result containing 'error' and no Residue.
 *
 * @post The returned Result satisfies:
 *       - 'ok' is false;
 *       - 'error' equals the supplied packed error value;
 *       - 'residue.kind' is ARC_RESIDUE_KIND_NONE;
 *       - 'residue.ptr' is NULL;
 *       - 'residue.allocator' is NULL.
 *
 * @post The returned Result satisfies the invariants documented for
 *       arc_result_t.
 *
 * @attention 'error' must be nonzero and its reserved upper 8 bits must be zero.
 *            Its severity must be ARC_SEVERITY_LOG, ARC_SEVERITY_WARNING,
 *            ARC_SEVERITY_ERROR, or ARC_SEVERITY_FATAL. Unrecognized domain
 *            and code identifiers are accepted. Domain zero is permitted.
 *
 * @attention An error value violating these requirements is a programming
 *            error. The constructor invokes panic and aborts program execution;
 *            it does not return an invalid Result. This applies in all builds.
 */
arc_result_t arc_result_failure(uint32_t error);

/**
 * @brief Creates a failed Result containing residual ownership.
 *
 * Constructs a failed Result from 'error' and transfers ownership of exactly
 * one residual resource described by 'residue' to the caller.
 *
 * A Residue exists when an operation fails and a resource acquired during that
 * operation cannot be released before returning.
 *
 * @param[in] error Packed error value describing the cause of failure.
 * @param[in] residue Residual resource whose ownership is transferred to the
 *                    caller.
 *
 * @return A valid failed Result containing 'error' and 'residue'.
 *
 * @post The returned Result satisfies:
 *       - 'ok' is false;
 *       - 'error' equals the supplied packed error value;
 *       - 'residue' equals the supplied Residue.
 *
 * @post The returned Result satisfies the invariants documented for
 *       arc_result_t.
 *
 * @attention 'error' must be nonzero and its reserved upper 8 bits must be zero.
 *            Its severity must be ARC_SEVERITY_LOG, ARC_SEVERITY_WARNING,
 *            ARC_SEVERITY_ERROR, or ARC_SEVERITY_FATAL. Unrecognized domain
 *            and code identifiers are accepted. Domain zero is permitted.
 *
 * @attention 'residue' must describe a valid allocation Residue.
 *            ARC_RESIDUE_KIND_NONE is not accepted by this constructor; use
 *            result_failure() when there is no residual ownership.
 *
 * @attention For ARC_RESIDUE_KIND_ALLOCATION:
 *            - 'residue.ptr' must not be NULL;
 *            - 'residue.allocator' must not be NULL;
 *            - 'residue.allocator' must be initialized;
 *            - the allocator must support the matching ordinary free operation;
 *            - the allocator and its context must outlive residual cleanup.
 *
 * @attention A detected violation of the error or Residue requirements invokes
 *            panic and aborts program execution. No invalid Result is returned.
 *            This applies in all builds.
 *
 * @attention Non-NULL allocator pointers must refer to live, readable objects.
 *            Allocation ownership, pointer lifetime, and compatibility with
 *            the allocator remain caller responsibilities. Constructor checks
 *            cannot reliably detect dangling pointers or prove ownership.
 *
 * @attention Ownership of the residual resource is transferred to the caller.
 *            The caller is responsible for eventually releasing that resource
 *            using the allocator stored by the Residue.
 *
 * @note This constructor packages existing ownership. It does not allocate,
 *       release, or duplicate the resource. After placing the returned Result
 *       in the next owner's hands, the previous owner must not release the
 *       resource through a retained copy. Passing Residue by value does not
 *       clear the original object.
 *
 * @note A Result can contain at most one residual resource.
 */
arc_result_t arc_result_failure_with_residue(uint32_t error, arc_residue_t residue);

/**
 * @brief Attempts to release the resource owned by a Residue.
 *
 * This operation represents the final cleanup step for residual ownership.
 *
 * For ARC_RESIDUE_KIND_ALLOCATION, the stored allocation is released using the
 * Allocator referenced by 'residue->allocator'.
 *
 * @param[in, out] residue Residue whose owned resource will be released.
 *
 * @return true if the residual resource is released successfully, or if
 *         'residue' already represents the canonical empty state.
 * @return false if 'residue' is NULL, invalid, or if releasing the owned
 *         resource fails.
 *
 * @post On success:
 *       - any previously owned residual resource is released;
 *       - 'residue->kind' is ARC_RESIDUE_KIND_NONE;
 *       - 'residue->ptr' is NULL;
 *       - 'residue->allocator' is NULL.
 *
 * @post On failure, 'residue' remains unchanged and ownership of the residual
 *       resource remains with the caller.
 *
 * @attention For ARC_RESIDUE_KIND_ALLOCATION:
 *            - 'residue->ptr' must not be NULL;
 *            - 'residue->allocator' must not be NULL;
 *            - 'residue->allocator' must be initialized;
 *            - 'residue->allocator' must support ordinary allocation release;
 *            - 'residue->ptr' must belong to that allocator and be compatible
 *              with its ordinary free operation;
 *            - the allocator and its context must still be alive and usable.
 *
 * @attention Non-NULL pointers must be valid for the objects they reference.
 *            Returning false for an invalid state does not promise safe handling
 *            of dangling pointers or corrupted allocator objects.
 *
 * @attention If the allocator can report a release failure, it must leave the
 *            allocation live and owned by the caller on that failure. Otherwise
 *            the unchanged-ownership guarantee cannot be satisfied.
 *
 * @attention Failure does not transfer or discard ownership. The caller remains
 *            responsible for deciding how to handle the unreleased resource,
 *            including whether the failure should be logged, tolerated, or
 *            treated as fatal.
 *
 * @note Successful cleanup of arc_result_t.residue does not change
 *       arc_result_t.ok or arc_result_t.error. The Result continues to represent
 *       the original failure.
 *
 * @note This wrapper does not deliberately terminate program execution.
 *       Custom allocator callbacks must obey their own documented contracts.
 */
bool arc_residue_destroy(arc_residue_t* residue);

/**
 * @brief Formats a human-readable description of a packed error value.
 *
 * @param[in] error Packed error value with numeric layout 0x00SSDDCC.
 * @param[out] buffer Writable buffer of at least 'buffer_size' bytes, or NULL.
 * @param[in] buffer_size Size of 'buffer' in bytes, including terminator space.
 *
 * @return true if the description is written completely and null-terminated.
 * @return false if 'buffer' is NULL, 'buffer_size' is zero, the reserved upper
 *         8 bits of 'error' are nonzero, formatting fails, or the buffer is too small.
 *
 * @post On failure, if 'buffer' is not NULL and 'buffer_size' is greater than
 *       zero, buffer[0] is set to the null terminator. Otherwise no output is
 *       written. No truncated description is returned as a string.
 *
 * @note ARC_ERROR_NONE is a valid input.
 *       Unknown field identifiers are represented numerically rather than
 *       rejected. Include the complete hexadecimal value so that unfamiliar
 *       errors remain distinguishable. Known names may accompany the value.
 *       Output wording is diagnostic text, not a stable serialization format.
 */
bool arc_error_format(uint32_t error, char* buffer, size_t buffer_size);

/**
 * @brief Describes the domain extracted from a complete packed error value.
 *
 * @param[in] error Complete packed error value, not an extracted domain.
 *
 * @return A human-readable static string describing the extracted domain.
 * @return NULL if the domain is not recognized by this implementation.
 *
 * @note Only the domain field is used for lookup. Other fields are not validated.
 *       Domain zero names the common domain, including for ARC_ERROR_NONE.
 *       The returned string must not be modified or freed.
 */
const char* arc_error_domain_to_string(uint32_t error);

/**
 * @brief Describes the domain-local code extracted from a packed error value.
 *
 * @param[in] error Complete packed error value, not an extracted code.
 *
 * @return A human-readable static string for the extracted domain/code pair.
 * @return NULL if that pair is not recognized by this implementation.
 *
 * @note Both domain and code participate in lookup. Equal code numbers in
 *       different domains may have different descriptions. The common-domain
 *       zero code describes no error. Severity and reserved bits are not used
 *       to identify the code; this function does not validate the whole value.
 *       The returned string must not be modified or freed.
 */
const char* arc_error_code_to_string(uint32_t error);

/**
 * @brief Describes the severity extracted from a complete packed error value.
 *
 * @param[in] error Complete packed error value, not an extracted severity.
 *
 * @return A human-readable static string describing the extracted severity,
 *         including ARC_SEVERITY_NONE.
 * @return NULL if the severity is not recognized by this implementation.
 *
 * @note Only the severity field is used for lookup. Other fields are not
 *       validated. This function does not determine operation success.
 *       The returned string must not be modified or freed.
 */
const char* arc_error_severity_to_string(uint32_t error);

/**
 * @brief Converts an arc_residue_kind_t value into a human-readable string.
 *
 * @param[in] kind Residue kind to convert.
 *
 * @return A human-readable static string describing 'kind'.
 * @return NULL if 'kind' does not contain a valid arc_residue_kind_t value.
 *
 * @note The returned string has static storage duration and must not be modified
 *       or freed.
 */
const char* arc_residue_kind_to_string(arc_residue_kind_t kind);

#ifdef __cplusplus
}
#endif

#endif /* ARC_RESULT_H_ */
