/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file allocator.h
 *
 * @author Nelson Somé
 *
 * @brief Allocator interface.
 *
 * The allocator interface provides a generic wrapper around different
 * memory allocation strategies, such as arena, pool, or heap allocators.
 * It also allows API users to define custom allocation strategies.
 * The API behaves predictably as long as its documented contracts are respected.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_ALLOCATOR_H_
#define ARC_ALLOCATOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations */
typedef struct AllocResult AllocResult;
typedef struct AllocatorDesc AllocatorDesc;
typedef struct AllocatorInterface AllocatorInterface;
typedef struct Allocator Allocator;

typedef /** AllocError */
/**
 * @brief AllocError is the set of error values that describe why
 *        an allocation operation failed.
 */
enum AllocError {
    ALLOC_ERROR_UNKNOWN = 0,
    ALLOC_ERROR_OUT_OF_MEMORY,
    ALLOC_ERROR_INVALID_ALLOCATOR,
    ALLOC_ERROR_INVALID_ALIGNMENT,
    ALLOC_ERROR_INVALID_ARGUMENTS,
    ALLOC_ERROR_UNSUPPORTED_OPERATION,
} AllocError;

typedef /** ReleaseResult */
/**
 * @brief ReleaseResult is the set of values that describe whether a release operation
 *        succeeded or why it failed.
 */
enum ReleaseResult {
    RELEASE_SUCCESS = 0,
    RELEASE_ERROR_INVALID_ALLOCATOR,
    RELEASE_ERROR_UNSUPPORTED_OPERATION,
} ReleaseResult;

/**
 * @brief AllocatorFeatures is a set of values representing the different operations
 *        supported by an allocator.
 */
enum {
    /** @note ALLOCATOR_FEATURE_ALLOC doesn't exist because a valid allocator must support it. */

    ALLOCATOR_FEATURE_ZERO_ALLOC    = (1U << 0U),
    ALLOCATOR_FEATURE_ALIGNED_ALLOC = (1U << 1U),
    ALLOCATOR_FEATURE_REALLOC       = (1U << 2U),
    ALLOCATOR_FEATURE_FREE          = (1U << 3U),
    ALLOCATOR_FEATURE_ALIGNED_FREE  = (1U << 4U),
    ALLOCATOR_FEATURE_CLEAR         = (1U << 5U),
};

/**
 * @brief AllocatorFeatureFlags is a bitmask used to store and query specific
 *        features supported by an allocator.
 */
typedef uint32_t AllocatorFeatureFlags;

/**
 * 
 * AllocResult is a tagged union representing two possible states after an
 * allocation operation.
 * The two states are:
 *  - ``` ok = true  ```: the operation succeeded. For operations that produce
 *    an allocation, 'ptr' refers to a valid memory block satisfying the
 *    operation's contract. For successful no-op operations, such as a
 *    zero-size allocation, 'ptr' may be NULL.
 *  - ``` ok = false ```: the operation failed. In that case, the cause of the
 *    failure can be inspected using the 'error' field.
 * The 'ok' field determines whether the operation succeeded or failed. Callers
 * must check 'ok' before accessing the corresponding union member.
 * 
 */
struct AllocResult {
    /** Used to determine the state of the result. */
    bool ok;

    /** tagged union */
    union
    {
        /**
         * @attention 'ptr' may be NULL even after an operation succeeds.
         *        For example if doing ```alloc(0);``` the custom behavior could
         *        return NULL, which means ptr is NULL even after 'alloc' perfectly
         *        did its job. Therefore it is important not to check the
         *        result's validity with ```if(!ptr)```, but with ```if(ok)```.
         */
        void* ptr;

        /** Error value. */
        AllocError error;
    } as;
};

/**
 * @brief Simple allocation function callback. The function is expected to allocate a memory
 *        block of 'size' bytes and return a pointer to it.
 * 
 * @param[in, out] context Allocator-specific context.
 * @param[in] size The size in bytes to allocate.
 * @return The allocation result of the operation.
 * 
 * @attention Any implementation of this callback must guarantee the following:
 *            - every allocator implementation must provide a non-NULL implementation
 *              of this callback;
 *            - size = 0 must be a successful no-op;
 *            - the behavior for context = NULL is implementation-specific;
 *            - for ```size > 0```, implementations must guarantee that the allocated block
 *              contains at least 'size' bytes;
 *            - on success, implementations must guarantee a minimum alignment of
 *              _Alignof(max_align_t);
 *            - on success, implementations must guarantee that ```ok = true```;
 *            - on failure, implementations must guarantee that ```ok = false```;
 *            - the allocation's contents are unspecified;
 *            - the allocation remains valid until it is released according to the allocator's lifetime strategy.
 */
typedef AllocResult (*MemAllocFn)(void* context, size_t size);

/**
 * @brief Zero-initialized allocation function callback. The function is expected to allocate a
 *        zero-initialized block of memory of 'num * size' bytes.
 * 
 * @param[in, out] context Allocator-specific context.
 * @param[in] num The number of elements to allocate.
 * @param[in] size The size in bytes of a single element.
 * @return The allocation result of the operation.
 * 
 * @attention Any implementation of this callback must guarantee the following:
 *            - custom allocator implementations may omit this callback;
 *            - num  = 0 must result in a successful no-op;
 *            - size = 0 must result in a successful no-op;
 *            - the behavior for context = NULL is implementation-specific;
 *            - for ```size > 0 and num > 0```, if ``` num > SIZE_MAX / size ``` the operation must
 *              result in an invalid argument error;
 *            - for ```size > 0 and num > 0```, implementations must guarantee that the allocated block
 *              contains at least 'num * size' bytes;
 *            - for ```size > 0 and num > 0```, implementations must guarantee a minimum alignment of
 *              _Alignof(max_align_t);
 *            - on success, implementations must guarantee that ```ok = true```;
 *            - on failure, implementations must guarantee that ```ok = false```;
 *            - the allocation's contents are zero-initialized;
 *            - the allocation remains valid until it is released according to the allocator's lifetime strategy.
 */
typedef AllocResult (*MemZeroAllocFn)(void* context, size_t num, size_t size);

/**
 * @brief Reallocation function callback. The function is expected to resize an
 *        already allocated memory block.
 *
 * @param[in, out] context Allocator-specific context.
 * @param[in] ptr Pointer to the allocated block to resize.
 * @param[in] new_size The new size in bytes of the block.
 * @return The allocation result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - custom allocator implementations may omit this callback;
 *            - ptr must reference a live allocation belonging to the allocator;
 *            - ptr = NULL must result in an invalid argument error;
 *            - new_size = 0 must result in an invalid argument error;
 *            - the behavior for context = NULL is implementation-specific;
 *            - on success, the returned allocation must contain at least
 *              'new_size' bytes;
 *            - on success, implementations must guarantee a minimum alignment of
 *              _Alignof(max_align_t);
 *            - on success, implementations must guarantee that ```ok = true```;
 *            - on success, the contents of the original allocation must be preserved
 *              up to the minimum of the original allocation's size and 'new_size';
 *            - when the allocation grows, the contents of the newly added region
 *              are unspecified;
 *            - on success, the original allocation may be moved. In that case,
 *              the original pointer becomes invalid and the returned pointer must
 *              be used instead;
 *            - on failure, implementations must guarantee that ```ok = false```;
 *            - on failure, the original allocation must remain valid and its
 *              contents must remain unchanged;
 *            - the resulting allocation remains valid until it is released
 *              according to the allocator's lifetime strategy.
 */
typedef AllocResult (*MemReallocFn)(void* context, void* ptr, size_t new_size);

/**
 * @brief Aligned allocation function callback. The function is expected to allocate an
 *        aligned memory block of 'size' bytes.
 * 
 * @param[in, out] context Allocator-specific context.
 * @param[in] alignment Requested alignment.
 * @param[in] size The size in bytes to allocate.
 * @return The allocation result of the operation.
 * 
 * @attention Any implementation of this callback must guarantee the following:
 *            - custom allocator implementations may omit this callback;
 *            - size = 0 must result in a successful no-op, except if alignment is invalid;
 *            - the behavior for context = NULL is implementation-specific;
 *            - for ```size > 0```, implementations must guarantee that the allocated block
 *              contains at least 'size' bytes;
 *            - for ```alignment > 0```, implementations must guarantee a minimum alignment of
 *              'alignment';
 *            - if 'alignment' is not a non-zero power of two, the operation results in an invalid
 *              alignment error;
 *            - on success, implementations must guarantee that ```ok = true```;
 *            - on failure, implementations must guarantee that ```ok = false```;
 *            - the allocation's contents are unspecified;
 *            - the allocation remains valid until it is released according to the allocator's lifetime strategy.
 */
typedef AllocResult (*MemAlignAllocFn)(void* context, size_t alignment, size_t size);

/**
 * @brief Free function callback. The function is expected to release an allocated
 *        block of memory.
 * 
 * @param[in, out] context Allocator-specific context.
 * @param[in] ptr Pointer to the allocated memory block to release.
 * 
 * @attention Any implementation of this callback must guarantee the following:
 *            - custom allocator implementations may omit this callback;
 *            - the behavior for context = NULL is implementation-specific;
 *            - ```ptr = NULL``` must result in a no-op.
 */
typedef void (*MemFreeFn)(void* context, void* ptr);

/**
 * @brief Aligned free function callback. The function is expected to release a
 *        block of memory that has been allocated with a specific alignment.
 * 
 * @param[in, out] context Allocator-specific context.
 * @param[in] ptr Pointer to the allocated memory block to release.
 * 
 * @attention Any implementation of this callback must guarantee the following:
 *            - custom allocator implementations may omit this callback;
 *            - the behavior for context = NULL is implementation-specific;
 *            - ```ptr = NULL``` must result in a no-op.
 */
typedef void (*MemAlignFreeFn)(void* context, void* ptr);

/**
 * @brief Memory cleanup function callback. The function is expected to release all
 *        memory managed by an allocator.
 * @note This callback is especially useful for arena-like allocators.
 * 
 * @param[in, out] context Allocator-specific context.
 * 
 * @attention - custom allocator implementations may omit this callback;
 *            - the behavior for context = NULL is implementation-specific.
 */
typedef void (*MemClearFn)(void* context);

/**
 * @brief Generic allocator interface.
 */
struct AllocatorInterface {
    /** Allocation function callback. */
    MemAllocFn mem_alloc;

    /** Aligned allocation function callback. */
    MemAlignAllocFn mem_aligned_alloc;
    
    /** Zero-initialized allocation function callback. */
    MemZeroAllocFn mem_zero_alloc;
    
    /** Reallocation function callback. */
    MemReallocFn mem_realloc;
    
    /** Aligned free function callback. */
    MemAlignFreeFn mem_aligned_free;
    
    /** Free function callback. */
    MemFreeFn mem_free;
    
    /** Memory cleanup function callback. */
    MemClearFn mem_clear;
};

/**
 * @brief Allocator configuration object used to initialize an allocator.
 * 
 * @note The descriptor must be valid before initializing the allocator.
 *       For the descriptor to be valid, it must satisfy the following requirements:
 *         - 'mem_alloc' must not be NULL;
 *         - it is the responsibility of the allocator's designer to ensure
 *           the validity of the custom allocation strategy.
 */
struct AllocatorDesc {
    /** Allocator-strategy-specific state, may be NULL. */
    void* context;
    /**
     * - Allocation function callback must not be NULL.
     * - Aligned allocation function callback may be NULL.
     * - Zero-initialized allocation function callback may be NULL.
     * - Reallocation function callback may be NULL.
     * - Aligned free function callback may be NULL.
     * - Free function callback may be NULL.
     * - Memory cleanup function callback may be NULL.
     * 
     * @note The interface is used at allocator initialization for
     *       features discovery.
     */
    AllocatorInterface iface;
};

/**
 * @brief Generic allocator.
 * 
 * @note Members are implementation details and must not be modified after initialization.
 */
struct Allocator {
    /** 'true' if the allocator was initialized successfully. */
    bool initialized;

    /** The set of operations that the allocator can execute. */
    AllocatorFeatureFlags supported_features;

    /** Allocator-strategy-specific state. */
    void* context;

    /** Allocator interface */
    AllocatorInterface iface;
};

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
bool allocator_init(Allocator* out, const AllocatorDesc* desc);

/**
 * @brief Checks whether an allocator supports all requested features.
 *
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] required_features Bitmask of features that must be supported.
 *
 * @return true if all requested features are supported, false otherwise.
 */
bool allocator_has_features(const Allocator* allocator, AllocatorFeatureFlags required_features);

/**
 * @brief Allocates a memory block of 'size' bytes.
 * 
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @param[in] size The size in bytes to allocate.
 * @return The allocation result of the operation.
 */
AllocResult allocator_alloc(const Allocator* allocator, size_t size);

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
AllocResult allocator_zero_alloc(const Allocator* allocator, size_t num, size_t size);

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
AllocResult allocator_realloc(const Allocator* allocator, void* ptr, size_t new_size);

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
AllocResult allocator_aligned_alloc(const Allocator* allocator, size_t alignment, size_t size);

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
ReleaseResult allocator_free(const Allocator* allocator, void* ptr);

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
ReleaseResult allocator_aligned_free(const Allocator* allocator, void* ptr);

/**
 * @brief Releases all memory managed by an allocator.
 * 
 * @param[in] allocator Pointer to an initialized allocator. Must not be NULL.
 * @return The release result of the operation.
 *
 * @note If the allocator does not provide a clear callback, the operation returns
 *       RELEASE_ERROR_UNSUPPORTED_OPERATION.
 */
ReleaseResult allocator_clear(const Allocator* allocator);

/**
 * @brief Converts a numeric allocation error code into a human-readable string.
 * 
 * @param[in] error The error code to convert.
 * @return A human-readable description of the error code, or NULL for invalid inputs.
 * 
 * @note The returned string is static and must not be freed.
 */
const char* alloc_error_to_string(AllocError error);

/**
 * @brief Converts a numeric release result code into a human-readable string.
 * 
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 * 
 * @note The returned string is static and must not be freed.
 */
const char* release_result_to_string(ReleaseResult result);

#ifdef __cplusplus
}
#endif

#endif /* ARC_ALLOCATOR_H_ */
