/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file allocator.h
 * @author Nelson Somé
 * @brief Generic allocator interface.
 *
 * Wraps heap, pool, arena, and user-defined allocation strategies.
 * Every callback and memory-operation wrapper returns arc_result_t. Operations
 * producing a pointer write it through a caller-supplied output parameter.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_ALLOCATOR_H_
#define ARC_ALLOCATOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "result/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Allocator specific errors. */
#define ARC_DOMAIN_ALLOCATOR ((uint8_t)1U)

/** @brief Packed allocator-domain error values. */
#define ARC_ALLOCATOR_ERROR_OUT_OF_MEMORY ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_ALLOCATOR, 1U)
#define ARC_ALLOCATOR_ERROR_INVALID_ALLOCATOR ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_ALLOCATOR, 2U)
#define ARC_ALLOCATOR_ERROR_SIZE_OVERFLOW ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_ALLOCATOR, 3U)

/** @brief Optional allocator capabilities. Basic allocation is mandatory. */
#define ARC_ALLOCATOR_FEATURE_ZEROALLOC (1U << 0U)
#define ARC_ALLOCATOR_FEATURE_REALLOC (1U << 1U)
#define ARC_ALLOCATOR_FEATURE_FREE (1U << 2U)
#define ARC_ALLOCATOR_FEATURE_CLEAR (1U << 3U)

/** Forward declarations. */
typedef struct arc_allocator_interface_t arc_allocator_interface_t;
typedef struct arc_allocator_desc_t arc_allocator_desc_t;
typedef struct arc_allocator_t arc_allocator_t;

/** @brief Optional allocator capabilities flags. */
typedef uint32_t arc_allocator_feature_flags_t;

/**
 * @brief Ordinary allocation callback; mandatory.
 * @param[in,out] ctx Allocator-specific context; NULL handling is strategy-specific.
 * @param[in] size Requested byte count.
 * @param[out] out Non-NULL writable pointer slot.
 * @return Result describing success or failure.
 *
 * @post size == 0 is a successful no-op: *out == NULL, no allocation made.
 * @post For size > 0, success provides at least size bytes and a non-NULL
 *       pointer aligned to at least _Alignof(max_align_t).
 * @post Failure sets *out to NULL; an acquired resource that survives a failed
 *       operation must be represented using Result.residue, if applicable.
 * @note Newly allocated bytes have unspecified contents. The allocation remains
 *       live until released according to the strategy's lifetime rules.
 * @attention An invalid out pointer is a programmer contract violation.
 */
typedef arc_result_t (*arc_alloc_fun_t)(void* ctx, size_t size, void** out);

/**
 * @brief Optional zero-initializing allocation callback.
 * @param[in,out] ctx Strategy context; NULL handling is strategy-specific.
 * @param[in] num Number of elements.
 * @param[in] size Size of each element.
 * @param[out] out Non-NULL writable pointer slot.
 * @return Result describing success or failure.
 *
 * @post num == 0 or size == 0 is a successful no-op, without allocating;
 *       *out == NULL.
 * @post If both are nonzero and num > SIZE_MAX / size, returns a failed
 *       Result with ARC_ALLOCATOR_ERROR_SIZE_OVERFLOW; multiplication must
 *       not wrap.
 * @post On nonempty success the allocation has at least num * size bytes,
 *       all bytes are zero, and alignment is at least _Alignof(max_align_t).
 * @post On failure *out == NULL; surviving allocations, if any, are conveyed
 *       through Result.residue.
 * @note The allocation's lifetime follows the strategy's ownership rules.
 */
typedef arc_result_t (*arc_zero_alloc_fun_t)(void* ctx, size_t num, size_t size, void** out);

/**
 * @brief Optional reallocation callback.
 * @param[in,out] ctx Strategy context; NULL handling is strategy-specific.
 * @param[in] ptr Non-NULL live allocation belonging to this allocator.
 * @param[in] new_size New byte count, greater than zero.
 * @param[out] out Non-NULL writable pointer slot.
 * @return Result describing success or failure.
 *
 * @attention ptr == NULL, new_size == 0, or out == NULL is a programmer
 *            contract violation. The pointer must be suitable for this
 *            callback and must not have been released or invalidated.
 * @post On success *out is non-NULL, references at least new_size bytes,
 *       and meets _Alignof(max_align_t). The first min(old_size, new_size)
 *       bytes are preserved; newly added bytes are unspecified.
 * @post The allocation may move on success: if moved, ptr becomes invalid
 *       and the caller must use *out.
 * @post On failure *out == NULL, and the original allocation remains live,
 *       unchanged, and caller-owned; the failed Result must not claim that
 *       original allocation as a residue.
 */
typedef arc_result_t (*arc_realloc_fun_t)(void* ctx, void* ptr, size_t new_size, void** out);

/**
 * @brief Optional ordinary release callback.
 * @param[in,out] ctx Strategy context; NULL handling is strategy-specific.
 * @param[in] ptr Allocation pointer, or NULL.
 * @return Result describing success or failure.
 *
 * @post ptr == NULL is a successful no-op, following the original API.
 * @attention Non-NULL ptr must identify a live allocation compatible with
 *            this allocator's ordinary free operation.
 * @post On success a non-NULL allocation is released.
 * @post On failure a surviving allocation must remain live and recoverable.
 *       If Result.residue represents that allocation, ownership passes to
 *       the Result, and the caller must not release it independently.
 *       If no residue represents it, the caller retains ownership.
 * @attention A failed release may transfer ownership through Result.residue.
 *            Such residues must satisfy the contracts in result.h, including
 *            their allocator-lifetime requirements.
 */
typedef arc_result_t (*arc_free_fun_t)(void* ctx, void* ptr);

/**
 * @brief Optional bulk release/reset callback, useful for arena strategies.
 * @param[in,out] ctx Strategy context; NULL handling is strategy-specific.
 * @return Result describing success or failure.
 *
 * @post On success, releases all memory managed by the strategy, as in the
 *       original allocator API. Previous allocation pointers are invalid.
 * @attention The implementation must define the disposition of its resources
 *            on failure. A live resource owned by Result.residue may not be
 *            independently freed or invalidated by a reset.
 * @attention No single-residue Result can by itself represent multiple
 *            independently owned surviving allocations.
 */
typedef arc_result_t (*arc_clear_fun_t)(void* ctx);

/** @brief Function table supplied by a strategy. */
struct arc_allocator_interface_t {
    arc_alloc_fun_t allocate;
    arc_zero_alloc_fun_t zero_allocate;
    arc_realloc_fun_t reallocate;
    arc_free_fun_t deallocate;
    arc_clear_fun_t clear;
};

/**
 * @brief Descriptor for allocator initialization.
 *
 * allocate must be non-NULL; other callbacks may be NULL. The strategy author
 * is responsible for callback validity, allocation provenance, and context
 * lifetime. Context is borrowed, never owned or destroyed by this wrapper.
 * Interface pointers are copied into the initialized allocator.
 */
struct arc_allocator_desc_t {
    void* context;
    arc_allocator_interface_t iface;
};

/**
 * @brief Allocator wrapper and its discovered capabilities.
 *
 * Members are inspectable, but must not be modified after initialization.
 * supported_features is derived from the presence of optional callbacks.
 * A copied allocator references the same context and allocations; copying
 * does not transfer or duplicate any allocation's ownership.
 */
struct arc_allocator_t {
    arc_allocator_feature_flags_t supported_features;
    
    void* context;
    arc_allocator_interface_t iface;
};

/**
 * @brief Retrieves the system allocator.
 *
 * Returns a pointer to the process-wide system allocator, which provides
 * general-purpose dynamic memory allocation.
 *
 * @return Non-NULL pointer to the initialized system allocator.
 *
 * @note The returned allocator is owned by Arc Core and must not be modified
 *       or destroyed by the caller.
 * @note The returned pointer has static lifetime and remains valid for the
 *       duration of the program.
 * @note The allocator's thread-safety guarantees are determined by the
 *       system allocator implementation.
 */
const arc_allocator_t* arc_get_system_allocator(void);

/**
 * @brief Initializes an allocator from a descriptor.
 * @param[out] out Non-NULL writable allocator object.
 * @param[in] desc Non-NULL readable descriptor.
 * @return Successful Result if initialized; otherwise a failed Result for
 *         an operational initialization error.
 *
 * @attention Invalid pointers and missing mandatory m_alloc are programmer
 *            errors and panic rather than returning an invalid-descriptor
 *            failure. The descriptor and callbacks must be valid.
 * @post On success is_valid is true; context and iface equal the descriptor,
 *       and supported_features reflects the optional callbacks.
 * @post On a returned initialization failure, out->is_valid is false, as in
 *       the previous API; a failed allocator cannot be used.
 * @attention Do not overwrite an allocator whose prior state/resources still
 *            need cleanup. The original interface did not specify a general
 *            allocator destruction or reinitialization mechanism.
 */
arc_result_t arc_allocator_init(arc_allocator_t* out, const arc_allocator_desc_t* desc);

/**
 * @brief Checks whether an allocator has been successfully initialized.
 *
 * Determines whether the allocator is in a valid initialized state.
 *
 * @param[in] allocator Non-NULL pointer to an allocator object.
 *
 * @return True if allocator is initialized and valid; otherwise false.
 *
 * @attention A NULL pointer violates the programming contract.
 * @attention The allocator object must remain readable for the duration
 *            of the call.
 *
 * @note This function does not validate the allocator's context, callback
 *       implementations, allocation provenance, or the lifetime of any
 *       resources managed by the allocator.
 */
bool arc_allocator_is_valid(const arc_allocator_t* allocator);

/**
 * @brief Checks whether an allocator supports all requested features.
 * @param[in] allocator Non-NULL initialized allocator.
 * @param[in] required_features Required feature bits.
 * @return True if (supported_features & required_features) == required_features;
 *         otherwise false. The empty feature mask therefore returns true.
 * @attention Invalid allocator pointers or state violate the contract.
 */
bool arc_allocator_has_features(const arc_allocator_t* allocator, arc_allocator_feature_flags_t required_features);

/**
 * @brief Allocates size bytes.
 * @param[in] allocator Non-NULL initialized allocator.
 * @param[in] size Requested byte count.
 * @param[out] out Non-NULL writable pointer slot.
 * @return Successful or failed Result.
 * @post size == 0 succeeds without allocation and sets *out == NULL.
 * @post On failure *out == NULL; nonempty success has the size and alignment
 *       guarantees of arc_alloc_fun_t.
 */
arc_result_t arc_allocator_alloc(const arc_allocator_t* allocator, size_t size, void** out);

/**
 * @brief Allocates num * size bytes initialized to zero.
 * @param[in] allocator Non-NULL initialized allocator.
 * @param[in] num Element count.
 * @param[in] size Bytes per element.
 * @param[out] out Non-NULL writable pointer slot.
 * @return Successful no-op for zero operands. Otherwise, failed Result with
 *         ARC_ERROR_UNSUPPORTED_OPERATION if the callback is absent,
 *         or the callback's Result when supported.
 * @post Zero arguments yield a successful no-op and *out == NULL even if the
 *       callback is absent. For nonzero operands, overflow returns
 *       ARC_ALLOCATOR_ERROR_SIZE_OVERFLOW and *out == NULL.
 * @post All ordinary failures set *out == NULL.
 */
arc_result_t arc_allocator_zero_alloc(const arc_allocator_t* allocator, size_t num, size_t size, void** out);

/**
 * @brief Resizes an existing allocation.
 * @param[in] allocator Non-NULL initialized allocator.
 * @param[in] ptr Non-NULL live, compatible allocation.
 * @param[in] new_size Nonzero requested size.
 * @param[out] out Non-NULL writable pointer slot.
 * @return Unsupported-operation failure if callback is absent, otherwise its
 *         successful or failed Result.
 * @attention Invalid ptr, new_size, or out violates the programming contract.
 * @post On failure *out == NULL and ptr stays live and unchanged.
 * @post On success the callback's content-preservation, alignment, and
 *       possible-move guarantees apply.
 */
arc_result_t arc_allocator_realloc(const arc_allocator_t* allocator, void* ptr, size_t new_size, void** out);

/**
 * @brief Attempts ordinary release of an allocation.
 * @param[in] allocator Non-NULL initialized allocator.
 * @param[in] ptr Compatible allocation or NULL.
 * @return Unsupported-operation failure if callback is absent, otherwise its
 *         successful or failed Result, possibly carrying a residue.
 * @post NULL is a successful no-op when ordinary free is supported.
 * @post On failed release, ownership is preserved by the caller unless a
 *       returned residue explicitly takes ownership.
 */
arc_result_t arc_allocator_free(const arc_allocator_t* allocator, void* ptr);

/**
 * @brief Releases all managed memory using the strategy's bulk cleanup.
 * @param[in] allocator Non-NULL initialized allocator.
 * @return Unsupported-operation failure if callback is absent, otherwise its
 *         successful or failed Result.
 * @attention A strategy that returns failure must describe any partial cleanup
 *            and surviving ownership without violating the Result contracts.
 */
arc_result_t arc_allocator_clear(const arc_allocator_t* allocator);

#ifdef __cplusplus
}
#endif

#endif /* ARC_ALLOCATOR_H_ */
