/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file iterator.h
 *
 * @author Nelson Somé
 *
 * @brief Generic iterator interface.
 *
 * The iterator interface provides a generic wrapper around different
 * iteration strategies, such as forward, bidirectional, or random-access iterators.
 * It also allows API users to define custom iteration strategies.
 *
 * Iterator implementations must satisfy the contracts documented by this API.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_ITERATOR_H_
#define ARC_ITERATOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations */
typedef struct Allocator Allocator;
typedef struct IteratorDesc IteratorDesc;
typedef struct IteratorInterface IteratorInterface;
typedef struct Iterator Iterator;

typedef /** IteratorResult */
    /**
 * @brief IteratorResult is the set of values that describe whether an iterator
 *        operation succeeded or why it failed.
 */
    enum IteratorResult {
        ITERATOR_SUCCESS = 0,
        ITERATOR_ERROR_INVALID_ITERATOR,
        ITERATOR_ERROR_INVALID_ARGUMENTS,
        ITERATOR_ERROR_UNSUPPORTED_OPERATION,
        ITERATOR_ERROR_OUT_OF_RANGE,
        ITERATOR_ERROR_INCOMPATIBLE_ITERATORS,
    } IteratorResult;

typedef /** IteratorCategory */
    /**
 * @brief IteratorCategory describes the strongest traversal guarantees provided
 *        by an iterator.
 */
    enum IteratorCategory {
        ITERATOR_CATEGORY_INVALID = 0,
        ITERATOR_CATEGORY_FORWARD,
        ITERATOR_CATEGORY_BIDIRECTIONAL,
        ITERATOR_CATEGORY_RANDOM_ACCESS,
    } IteratorCategory;

/**
 * @brief Iterator advancement callback. The function is expected to advance the
 *        iterator by exactly one position.
 *
 * @param[in, out] context Iterator-specific context.
 * @return The result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - every iterator implementation must provide a non-NULL implementation
 *              of this callback;
 *            - the behavior for context = NULL is implementation-specific;
 *            - if another valid position follows the current position, the iterator
 *              advances exactly one position;
 *            - advancing the last dereferenceable position must move the iterator
 *              to the past-the-end position;
 *            - advancing a past-the-end iterator must result in an out-of-range error;
 *            - on failure, the iterator position must remain unchanged.
 */
typedef IteratorResult (*IteratorNextFn)(void* context);

/**
 * @brief Iterator reverse advancement callback. The function is expected to move
 *        the iterator backward by exactly one position.
 *
 * @param[in, out] context Iterator-specific context.
 * @return The result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - forward-only iterator implementations may omit this callback;
 *            - bidirectional and random-access iterator implementations must provide
 *              a non-NULL implementation of this callback;
 *            - the behavior for context = NULL is implementation-specific;
 *            - if another valid position precedes the current position, the iterator
 *              moves backward exactly one position;
 *            - moving backward from the past-the-end position must move the iterator
 *              to the last dereferenceable position, provided one exists;
 *            - moving backward from the first dereferenceable position must result
 *              in an out-of-range error;
 *            - moving backward from the past-the-end position of an empty range must
 *              result in an out-of-range error;
 *            - on failure, the iterator position must remain unchanged.
 */
typedef IteratorResult (*IteratorPreviousFn)(void* context);

/**
 * @brief Random-access iterator advancement callback. The function is expected
 *        to move the iterator by 'offset' positions.
 *
 * @param[in, out] context Iterator-specific context.
 * @param[in] offset Signed number of positions by which to move the iterator.
 * @return The result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - forward and bidirectional iterator implementations may omit this callback;
 *            - random-access iterator implementations must provide a non-NULL
 *              implementation of this callback;
 *            - the behavior for context = NULL is implementation-specific;
 *            - offset = 0 must result in a successful no-op;
 *            - positive offsets move the iterator forward;
 *            - negative offsets move the iterator backward;
 *            - movement outside the iterator's valid traversal domain must result
 *              in an out-of-range error;
 *            - on failure, the iterator position must remain unchanged;
 *            - the operation must execute in constant time.
 */
typedef IteratorResult (*IteratorAdvanceFn)(void* context, ptrdiff_t offset);

/**
 * @brief Random-access iterator distance callback. The function is expected to
 *        compute the signed distance between two iterator positions.
 *
 * @param[in] start Context representing the starting iterator position.
 * @param[in] end Context representing the ending iterator position.
 * @param[out] out Pointer receiving the signed distance between the positions.
 * @return The result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - forward and bidirectional iterator implementations may omit this callback;
 *            - random-access iterator implementations must provide a non-NULL
 *              implementation of this callback;
 *            - out must not be NULL;
 *            - the behavior for start = NULL or end = NULL is implementation-specific;
 *            - start and end must belong to compatible iteration domains;
 *            - incompatible iterator domains must result in an incompatible iterator error;
 *            - if start and end represent the same position, '*out' must be 0;
 *            - if end follows start, '*out' must be positive;
 *            - if end precedes start, '*out' must be negative;
 *            - the operation must execute in constant time;
 *            - on failure, the value pointed to by out is unspecified.
 */
typedef IteratorResult (*IteratorDistanceFn)(const void* start, const void* end, ptrdiff_t* out);

/**
 * @brief Iterator dereference callback. The function is expected to obtain a
 *        pointer to the element at the iterator's current position.
 *
 * @param[in] context Iterator-specific context.
 * @param[out] out Read-only pointer receiving the address of the current element.
 * @return The result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - every iterator implementation must provide a non-NULL implementation
 *              of this callback;
 *            - out must not be NULL;
 *            - the behavior for context = NULL is implementation-specific;
 *            - dereferencing a past-the-end iterator must result in an out-of-range error;
 *            - on success, '*out' must refer to the element at the iterator's current position;
 *            - ownership of the referenced element is not transferred to the caller;
 *            - the lifetime and invalidation rules of the returned pointer are determined
 *              by the underlying iterator implementation;
 *            - on failure, the value pointed to by out is unspecified.
 */
typedef IteratorResult (*IteratorGetFn)(const void* context, const void** out);

/**
 * @brief Iterator comparison callback. The function is expected to determine whether
 *        two iterator contexts represent the same position.
 *
 * @param[in] a Context of the first iterator.
 * @param[in] b Context of the second iterator.
 * @param[out] out Pointer receiving the comparison result.
 * @return The result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - every iterator implementation must provide a non-NULL implementation
 *              of this callback;
 *            - out must not be NULL;
 *            - the behavior for a = NULL or b = NULL is implementation-specific;
 *            - a and b must belong to compatible iteration domains;
 *            - incompatible iterator domains must result in an incompatible iterator error;
 *            - on success, '*out' must be true if both contexts represent the same
 *              iterator position and false otherwise;
 *            - on failure, the value pointed to by out is unspecified.
 */
typedef IteratorResult (*IteratorEqualsFn)(const void* a, const void* b, bool* out);

/**
 * @brief Iterator context cloning callback.
 *
 * Creates an independent copy of an iterator-specific context using the
 * supplied allocator.
 *
 * @param[in] context Read-only iterator context to clone.
 * @param[out] out Pointer receiving the newly allocated independent context.
 * @param[in] allocator Allocator used to allocate the cloned context.
 *
 * @return The result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - 'out' must not be NULL;
 *            - 'allocator' must not be NULL and must provide the operations
 *              required by the implementation;
 *            - on success, '*out' must refer to an independent traversal state;
 *            - modifying the cloned context must not modify the traversal state
 *              represented by 'context';
 *            - the cloned context must represent the same iterator position as
 *              the source context immediately after cloning;
 *            - the cloned context must remain compatible with the source
 *              iterator's traversal domain;
 *            - on failure, '*out' must be set to NULL;
 *            - any resources allocated before a failure must be released before
 *              returning.
 *
 * @note The behavior for context = NULL is implementation-specific.
 *
 * @note A successfully cloned context must later be released using the
 *       corresponding IteratorDestroyFn and the same allocator.
 */
typedef IteratorResult (*IteratorCloneFn)(const void* context, void** out, Allocator* allocator);

/**
 * @brief Iterator context destruction callback.
 *
 * Releases an iterator-specific context previously created by the corresponding
 * IteratorCloneFn.
 *
 * @param[in, out] context Iterator context to destroy.
 * @param[in] allocator Allocator originally used to create the context.
 *
 * @return The result of the operation.
 *
 * @attention Any implementation of this callback must guarantee the following:
 *            - 'allocator' must be the allocator originally used to create the
 *              cloned context;
 *            - on success, all resources owned by the context are released;
 *            - after successful destruction, 'context' must no longer be used;
 *            - on failure, ownership of the context remains with the caller and
 *              the context must remain valid for another destruction attempt.
 *
 * @note The behavior for context = NULL is implementation-specific.
 */
typedef IteratorResult (*IteratorDestroyFn)(void* context, Allocator* allocator);

/**
 * @brief Generic iterator interface.
 */
struct IteratorInterface {
    /** Forward advancement callback. */
    IteratorNextFn next;

    /** Reverse advancement callback. */
    IteratorPreviousFn previous;

    /** Random-access advancement callback. */
    IteratorAdvanceFn advance;

    /** Random-access distance callback. */
    IteratorDistanceFn distance;

    /** Iterator dereference callback. */
    IteratorGetFn get;

    /** Iterator comparison callback. */
    IteratorEqualsFn equals;

    /** Iterator context cloning callback. May be NULL if cloning is unsupported. */
    IteratorCloneFn clone;

    /** Iterator context destruction callback. May be NULL if cloning is unsupported. */
    IteratorDestroyFn destroy;
};

/**
 * @brief Iterator configuration object used to initialize an iterator.
 *
 * @note The descriptor must be valid before initializing the iterator.
 *       For the descriptor to be valid, it must satisfy the following requirements:
 *         - 'type_size' must not be zero;
 *         - 'next' must not be NULL;
 *         - 'get' must not be NULL;
 *         - 'equals' must not be NULL;
 *         - if 'previous' is NULL, 'advance' and 'distance' must also be NULL;
 *         - if 'previous' is not NULL and both 'advance' and 'distance' are NULL,
 *           the iterator is bidirectional;
 *         - if 'previous', 'advance', and 'distance' are all non-NULL, the iterator
 *           is random access;
 *         - 'advance' and 'distance' must either both be NULL or both be non-NULL;
 *         - it is the responsibility of the iterator's designer to ensure the
 *           validity of the custom iteration strategy.
 *         - 'clone' and 'destroy' must either both be NULL or both be non-NULL;
 */
struct IteratorDesc {
    /** Iterator-specific state, may be NULL. */
    void* context;

    /**
     * Size in bytes of the element type traversed by this iterator.
     *
     * The value is fixed at initialization and must not be zero.
     */
    size_t type_size;

    /**
     * - Forward advancement callback must not be NULL.
     * - Reverse advancement callback may be NULL.
     * - Random-access advancement callback may be NULL.
     * - Random-access distance callback may be NULL.
     * - Iterator dereference callback must not be NULL.
     * - Iterator comparison callback must not be NULL.
     *
     * @note The interface is used at iterator initialization for
     *       category discovery.
     */
    IteratorInterface iface;
};

/**
 * @brief Generic iterator.
 *
 * @note Members are implementation details and must not be modified after initialization.
 *
 * @attention Copying an Iterator directly performs a shallow copy.
 *            If the Iterator owns its context, shallow copying duplicates
 *            ownership metadata and may result in multiple Iterator objects
 *            attempting to release the same context.
 *
 *            Use iterator_clone() when an independent iterator is required.
 */
struct Iterator {
    /** 'true' if the iterator was initialized successfully. */
    bool initialized;

    /** 'true' if the iterator owns its context and is responsible for releasing it. */
    bool owns_context;

    /**
     * Size in bytes of the element type traversed by this iterator.
     *
     * The value is fixed at initialization and must not be zero.
     */
    size_t type_size;

    /** Strongest traversal category supported by the iterator. */
    IteratorCategory category;

    /** Iterator-specific state. */
    void* context;

    /** Iterator interface. */
    IteratorInterface iface;

    /**
     * Allocator used to create an owned cloned context.
     *
     * NULL when the Iterator does not own its context.
     */
    Allocator* cloning_allocator;
};

/**
 * @brief Initializes an iterator.
 *
 * @param[in, out] out Pointer to the iterator to initialize, must not be NULL.
 * @param[in] desc Read-only configuration of the iterator, must not be NULL.
 *
 * @return true if successfully initialized, false otherwise.
 *
 * @note On failure, the iterator's 'initialized' flag is set to false and its
 *       category is set to ITERATOR_CATEGORY_INVALID.
 *       Trying to use the iterator after failure results in an invalid iterator error.
 *
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 */
bool iterator_init(Iterator* out, const IteratorDesc* desc);

/**
 * @brief Checks whether an iterator provides at least the requested traversal category.
 *
 * @param[in] iterator Pointer to an initialized iterator. Must not be NULL.
 * @param[in] required_category Minimum traversal category required.
 *
 * @return true if the iterator provides at least the requested category, false otherwise.
 */
bool iterator_is_at_least(const Iterator* iterator, IteratorCategory required_category);

/**
 * @brief Obtains the size of the element type traversed by an iterator.
 *
 * @param[in] iterator Pointer to an initialized iterator.
 *
 * @return The size in bytes of the iterator's element type.
 * @return 0 if 'iterator' is NULL or not initialized.
 *
 * @note The returned value is the type size supplied when the iterator was
 *       initialized and remains constant for the iterator's lifetime.
 */
size_t iterator_get_type_size(const Iterator* iterator);

/**
 * @brief Advances an iterator by exactly one position.
 *
 * @param[in, out] iterator Pointer to an initialized iterator. Must not be NULL.
 * @return The result of the operation.
 */
IteratorResult iterator_next(Iterator* iterator);

/**
 * @brief Moves an iterator backward by exactly one position.
 *
 * @param[in, out] iterator Pointer to an initialized iterator. Must not be NULL.
 * @return The result of the operation.
 *
 * @note If the iterator is not bidirectional or random access, the operation
 *       fails with ITERATOR_ERROR_UNSUPPORTED_OPERATION.
 */
IteratorResult iterator_previous(Iterator* iterator);

/**
 * @brief Moves an iterator by 'offset' positions.
 *
 * @param[in, out] iterator Pointer to an initialized iterator. Must not be NULL.
 * @param[in] offset Signed number of positions by which to move the iterator.
 * @return The result of the operation.
 *
 * @note If the iterator is not random access, the operation fails with
 *       ITERATOR_ERROR_UNSUPPORTED_OPERATION.
 */
IteratorResult iterator_advance(Iterator* iterator, ptrdiff_t offset);

/**
 * @brief Computes the signed distance between two iterator positions.
 *
 * @param[in] start Pointer to the starting iterator. Must not be NULL.
 * @param[in] end Pointer to the ending iterator. Must not be NULL.
 * @param[out] out Pointer receiving the signed distance. Must not be NULL.
 * @return The result of the operation.
 *
 * @note Both iterators must be random access.
 *       If either iterator is not random access, the operation fails with
 *       ITERATOR_ERROR_UNSUPPORTED_OPERATION.
 *
 * @note The iterators must belong to compatible iteration domains.
 */
IteratorResult iterator_distance(const Iterator* start, const Iterator* end, ptrdiff_t* out);

/**
 * @brief Obtains a pointer to the element at the iterator's current position.
 *
 * @param[in] iterator Pointer to an initialized iterator. Must not be NULL.
 * @param[out] out Read-only pointer receiving the address of the current element. Must not be NULL.
 * @return The result of the operation.
 *
 * @note The iterator retains no ownership over the returned pointer beyond the
 *       guarantees provided by the underlying iterator implementation.
 */
IteratorResult iterator_get(const Iterator* iterator, const void** out);

/**
 * @brief Checks whether two iterators represent the same position.
 *
 * @param[in] a Pointer to the first initialized iterator. Must not be NULL.
 * @param[in] b Pointer to the second initialized iterator. Must not be NULL.
 * @param[out] out Pointer receiving the comparison result. Must not be NULL.
 * @return The result of the operation.
 *
 * @note The iterators must belong to compatible iteration domains.
 */
IteratorResult iterator_equals(const Iterator* a, const Iterator* b, bool* out);

/**
 * @brief Creates an independent clone of an iterator.
 *
 * The cloned iterator represents the same traversal position and iteration
 * domain as the source iterator while owning an independent iterator-specific
 * context.
 *
 * Moving either iterator after cloning does not affect the traversal position
 * of the other.
 *
 * @param[in] iterator Initialized iterator to clone. Must not be NULL.
 * @param[out] out Iterator receiving the independent clone. Must not be NULL.
 * @param[in] allocator Allocator used to allocate the cloned context. Must not
 *                      be NULL and must remain valid until the clone is destroyed.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'iterator' is NULL or invalid.
 * @return ITERATOR_ERROR_INVALID_ARGUMENTS if 'out' or 'allocator' is NULL.
 * @return ITERATOR_ERROR_UNSUPPORTED_OPERATION if cloning is not supported by
 *         the iterator implementation.
 * @return Any error returned by the implementation-specific cloning callback.
 *
 * @post On success:
 *       - 'out' is initialized;
 *       - 'out' represents the same traversal position and domain as 'iterator';
 *       - 'out' owns an independent context;
 *       - 'out' stores 'allocator' for later context destruction.
 *
 * @post On failure, 'out' is reset to an invalid iterator state.
 *
 * @attention A successfully cloned iterator must eventually be passed to
 *            iterator_destroy().
 * 
 * @attention 'out' must not contain a live initialized Iterator.
           An existing Iterator must be destroyed before being reused as output.
 */
IteratorResult iterator_clone(const Iterator* iterator, Iterator* out, Allocator* allocator);

/**
 * @brief Destroys an iterator.
 *
 * If the iterator owns its context, the context is released using the
 * implementation-specific destruction callback and the allocator originally
 * supplied to iterator_clone().
 *
 * If the iterator does not own its context, only the Iterator object itself is
 * invalidated; the borrowed context is left untouched.
 *
 * @param[in, out] iterator Iterator to destroy. Must not be NULL and must be
 *                         initialized.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'iterator' is NULL or invalid.
 * @return ITERATOR_ERROR_UNSUPPORTED_OPERATION if an owned iterator does not
 *         provide the required destruction callback.
 *
 * @post On success, the Iterator is reset to an invalid state and must not be
 *       used until initialized again.
 *
 * @post On failure, an owned Iterator retains ownership of its context and
 *       remains unchanged.
 *
 * @note Destroying an initialized Iterator that does not own its context is a
 *       successful operation and does not affect the borrowed context.
 */
IteratorResult iterator_destroy(Iterator* iterator);

/**
 * @brief Converts a numeric iterator result code into a human-readable string.
 *
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 *
 * @note The returned string is static and must not be freed.
 */
const char* iterator_result_to_string(IteratorResult result);

/**
 * @brief Converts an IteratorCategory value to a human-readable string.
 *
 * @param[in] category Iterator category to convert.
 *
 * @return A human-readable description of 'category', or NULL if 'category'
 *         is not a valid IteratorCategory value.
 *
 * @note The returned string has static lifetime and must not be modified or freed.
 */
const char* iterator_category_to_string(IteratorCategory category);

#ifdef __cplusplus
}
#endif

#endif /* ARC_ITERATOR_H_ */