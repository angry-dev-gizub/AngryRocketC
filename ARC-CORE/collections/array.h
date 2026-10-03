/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file array.h
 *
 * @author Nelson Somé
 *
 * @brief Generic contiguous array storage.
 *
 * The Array API provides contiguous storage for a sequence of fixed-size
 * elements.
 *
 * An array has:
 * - a logical size representing the number of currently stored elements;
 * - a capacity representing the number of elements that can be stored without
 *   changing the storage capacity;
 * - a fixed element size shared by every element stored in the array;
 * - a growth strategy defining whether and how its capacity may grow.
 *
 * Array elements are stored by value. Operations that insert or replace
 * elements copy exactly 'element_size' bytes from the provided source object
 * into the array's internal storage.
 *
 * The Array does not manage resources owned indirectly by stored elements.
 * Copying an element performs a bytewise copy only.
 *
 * Unless explicitly documented otherwise, a failed mutating operation leaves
 * the array unchanged.
 *
 * @attention Successful structural mutations invalidate all previously obtained
 *            iterators and pointers referring to the array's internal storage.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_ARRAY_H_
#define ARC_ARRAY_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations */
typedef struct Allocator Allocator;
typedef struct Iterator Iterator;
typedef struct ArrayDesc ArrayDesc;
typedef struct Array Array;

typedef /** ArrayResult */
    /**
 * @brief ArrayResult is the set of values that describe whether an array
 *        operation succeeded or why it failed.
 */
    enum ArrayResult {
        /** The operation completed successfully. */
        ARRAY_SUCCESS = 0,

        /** The supplied Array is NULL, invalid, or otherwise unusable. */
        ARRAY_ERROR_INVALID_ARRAY,

        /** A required allocator is NULL, invalid, or does not provide required operations. */
        ARRAY_ERROR_INVALID_ALLOCATOR,

        /** One or more call arguments are invalid. */
        ARRAY_ERROR_INVALID_ARGUMENTS,

        /** The requested operation is not supported by this array configuration. */
        ARRAY_ERROR_OPERATION_NOT_SUPPORTED,

        /** The operation requires more capacity than can be represented or provided. */
        ARRAY_ERROR_OUT_OF_SPACE,

        /** An index refers to a position outside the array's logical element range. */
        ARRAY_ERROR_OUT_OF_RANGE,

        /** An underlying allocator operation failed. */
        ARRAY_ERROR_ALLOCATOR_ERROR,

        /** An underlying iterator operation failed. */
        ARRAY_ERROR_ITERATOR_ERROR,
    } ArrayResult;

typedef /** ArrayGrowthFactor */
    /**
 * @brief ArrayGrowthFactor provides the supported automatic capacity growth
 *        strategies.
 *
 * ARRAY_GROWTH_NONE represents a fixed-capacity array. Any other value
 * represents a dynamically growing array.
 */
    enum ArrayGrowthFactor {
        /**
         * No automatic capacity growth.
         *
         * Arrays using this strategy have fixed capacity.
         */
        ARRAY_GROWTH_NONE = 0,

        /**
         * factor = 2
         *
         * pros: Fewer growth operations and copies.
         * cons: Higher potential unused capacity.
         */
        ARRAY_GROWTH_2X,

        /**
         * factor = 1.5
         *
         * pros: Lower potential unused capacity than 2x growth.
         * cons: More frequent growth operations and copies.
         */
        ARRAY_GROWTH_1_5X,

        /**
         * factor = 1.125
         *
         * pros: Minimizes excess capacity.
         * cons: Significantly more frequent growth operations and copies.
         */
        ARRAY_GROWTH_1_125X,
    } ArrayGrowthFactor;

/**
 * @brief Array configuration object used to create an array.
 *
 * @attention A valid descriptor must satisfy the following:
 *            - 'base_capacity' must be greater than zero;
 *            - 'element_size' must be greater than zero;
 *            - 'base_capacity * element_size' must be representable by size_t;
 *            - 'growth_factor' must contain a valid ArrayGrowthFactor value;
 *            - 'object_allocator' must not be NULL and must provide allocation
 *              and individual release operations;
 *            - 'storage_allocator' must not be NULL and must provide allocation
 *              and individual release operations;
 *            - the allocators may refer to the same Allocator;
 *            - both allocator objects must remain valid for the entire lifetime
 *              of the Array.
 *
 * @note The descriptor itself is only used during array creation and does not
 *       need to outlive the created Array.
 */
struct ArrayDesc {
    /**
     * @brief Initial storage capacity measured in elements.
     *
     * The value must be greater than zero.
     *
     * A successfully created Array initially has:
     * - size = 0;
     * - capacity = base_capacity.
     *
     * The base capacity is also the minimum capacity restored by
     * array_shrink_to_fit() when the array is empty.
     */
    size_t base_capacity;

    /**
     * @brief Size in bytes of every element stored in the array.
     *
     * The value must be greater than zero and remains constant for the entire
     * lifetime of the Array.
     */
    size_t element_size;

    /**
     * @brief Capacity growth strategy.
     *
     * ARRAY_GROWTH_NONE creates a fixed-capacity array.
     * Any other valid growth factor creates a dynamic array.
     */
    ArrayGrowthFactor growth_factor;

    /**
     * @brief Allocator used to create and release the Array object itself.
     *
     * @attention The allocator must support allocation and individual release.
     *
     * @note The Array stores this pointer but does not own the Allocator.
     * @note This allocator must outlive the Array.
     */
    Allocator* object_allocator;

    /**
     * @brief Allocator used to allocate and release the internal element storage.
     *
     * Reallocation support is optional. When available, implementations may use
     * it to resize storage directly. Otherwise resizing may be implemented using
     * allocation, copying, and release.
     *
     * @attention The allocator must support allocation and individual release.
     *
     * @note The Array stores this pointer but does not own the Allocator.
     * @note This allocator must outlive the Array.
     */
    Allocator* storage_allocator;
};

#define STATIC_ARRAY_DESC_FROM_TYPE_EX(type, capacity, obj_allocator_ptr, arr_allocator_ptr)                           \
    (ArrayDesc) {                                                                                                      \
        .base_capacity = capacity, .element_size = sizeof(type), .object_allocator = obj_allocator_ptr,                \
        .storage_allocator = arr_allocator_ptr, .growth_factor = ARRAY_GROWTH_NONE,                                    \
    }

#define DYNAMIC_ARRAY_DESC_FROM_TYPE_EX(type, capacity, obj_allocator_ptr, arr_allocator_ptr, gfactor)                 \
    (ArrayDesc) {                                                                                                      \
        .base_capacity = capacity, .element_size = sizeof(type), .object_allocator = obj_allocator_ptr,                \
        .storage_allocator = arr_allocator_ptr, .growth_factor = gfactor,                                              \
    }

#define STATIC_ARRAY_DESC_FROM_TYPE(type, capacity, allocator_ptr)                                                     \
    STATIC_ARRAY_DESC_FROM_TYPE_EX(type, capacity, allocator_ptr, allocator_ptr)

#define DYNAMIC_ARRAY_DESC_FROM_TYPE(type, capacity, allocator_ptr)                                                    \
    DYNAMIC_ARRAY_DESC_FROM_TYPE_EX(type, capacity, allocator_ptr, allocator_ptr, ARRAY_GROWTH_2X)

/**
 * @brief Creates a new empty Array using the supplied descriptor.
 *
 * The Array object is allocated using 'object_allocator' and its internal
 * storage is allocated using 'storage_allocator'.
 *
 * @param[out] out Pointer receiving the newly created Array.
 * @param[in] desc Read-only Array configuration descriptor.
 *
 * @return ARRAY_SUCCESS if the Array was created successfully, or an error
 *         describing why creation failed.
 *
 * @attention 'out' must not be NULL.
 * @attention 'desc' must describe a valid Array configuration.
 *
 * @post On success:
 *       - '*out' refers to a valid Array;
 *       - the Array has size zero;
 *       - the Array has capacity equal to 'base_capacity';
 *       - its element size equals 'element_size';
 *       - its internal unused storage contains unspecified bytes.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note If Array-object allocation succeeds but storage allocation fails, the
 *       already allocated Array object is released before the function returns.
 */
ArrayResult array_new(Array** out, const ArrayDesc* desc);

/**
 * @brief Destroys an Array and releases all storage owned by it.
 *
 * The internal element storage is released before the Array object itself.
 *
 * @param[in, out] array Array to destroy. May be NULL.
 *
 * @return ARRAY_SUCCESS if destruction succeeds.
 *
 * @note Passing NULL results in a successful no-op.
 *
 * @attention All pointers and iterators referring to the Array become invalid
 *            after successful destruction.
 *
 * @attention If releasing the internal storage fails, the Array object itself
 *            is not released.
 */
ArrayResult array_destroy(Array* array);

/**
 * @brief Obtains a read-only pointer to the element at 'index'.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the address of the requested element.
 * @param[in] index Zero-based element index.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer refers directly to internal Array storage.
 * @note Ownership of the element is not transferred to the caller.
 *
 * @attention The pointer remains valid only while the corresponding storage
 *            remains valid and no invalidating Array operation is performed.
 */
ArrayResult array_get_at(const Array* array, const void** out, size_t index);

/**
 * @brief Replaces the element stored at 'index'.
 *
 * Exactly 'element_size' bytes are copied from 'element' into the existing
 * array slot.
 *
 * @param[in, out] array Array to modify.
 * @param[in] index Zero-based index of the element to replace.
 * @param[in] element Pointer to the source element. Must not be NULL.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @attention 'element' must not refer to storage whose validity depends on the
 *            Array being modified.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_set_at(Array* array, size_t index, const void* element);

/**
 * @brief Obtains a read-only pointer to the first element.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the first element.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and follows the same invalidation
 *       rules as pointers returned by array_get_at().
 */
ArrayResult array_get_first(const Array* array, const void** out);

/**
 * @brief Obtains a read-only pointer to the last element.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the last element.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and follows the same invalidation
 *       rules as pointers returned by array_get_at().
 */
ArrayResult array_get_last(const Array* array, const void** out);

/**
 * @brief Obtains a read-only pointer to the contiguous element storage.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the beginning of the logical element data.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post If the Array is empty, '*out' is set to NULL even if storage capacity
 *       is currently allocated.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note Elements are stored contiguously with exactly 'element_size' bytes
 *       between consecutive elements.
 *
 * @note The returned pointer is borrowed and does not transfer ownership.
 */
ArrayResult array_get_data(const Array* array, const void** out);

/**
 * @brief Obtains a mutable pointer to the contiguous element storage.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the beginning of the
 *                 logical element data.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post If the Array is empty, '*out' is set to NULL.
 * @post On failure '*out' is set to NULL.
 *
 * @note The caller may modify existing element bytes through the returned
 *       pointer but must not read or write beyond 'size * element_size' bytes.
 *
 * @note Modifying the returned memory does not change Array size or capacity.
 *
 * @attention The returned pointer is borrowed and becomes invalid when an Array
 *            operation invalidates internal storage pointers.
 */
ArrayResult array_get_data_mut(Array* array, void** out);

/**
 * @brief Obtains the number of currently stored elements.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the logical size.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to zero.
 */
ArrayResult array_get_size(const Array* array, size_t* out);

/**
 * @brief Obtains the current element capacity.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the capacity.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to zero.
 */
ArrayResult array_get_capacity(const Array* array, size_t* out);

/**
 * @brief Determines whether the Array supports automatic growth.
 *
 * An Array is dynamic exactly when its growth factor is not
 * ARRAY_GROWTH_NONE.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the result.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to false.
 */
ArrayResult array_is_dynamic(const Array* array, bool* out);

/**
 * @brief Determines whether the Array contains no elements.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving whether 'size == 0'.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to false.
 */
ArrayResult array_is_empty(const Array* array, bool* out);

/**
 * @brief Creates an iterator positioned at the first logical element.
 *
 * The returned Array iterator provides random-access traversal.
 *
 * @param[in] array Array whose elements will be traversed.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure 'out' is reset to an invalid iterator state.
 *
 * @note If the Array is empty, the resulting begin iterator compares equal to
 *       the corresponding end iterator.
 *
 * @note The returned iterator borrows the Array and does not own it.
 * @note Independent Array iterators have independent traversal state.
 *
 * @attention The returned iterator provides read-only element access through
 *            iterator_get().
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 * @attention The Array must outlive the returned iterator.
 * @attention Any invalidating Array mutation invalidates the iterator.
 */
ArrayResult array_get_begin_iterator(const Array* array, Iterator* out);

/**
 * @brief Creates a reverse iterator positioned at the last logical element.
 *
 * Reverse traversal treats the Array's elements as a sequence ordered from the
 * highest logical index to the lowest logical index.
 *
 * @param[in] array Array whose elements will be traversed.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @note For a reverse iterator, iterator_next() moves toward lower Array indexes
 *       and iterator_previous() moves toward higher Array indexes.
 *
 * @note If the Array is empty, the resulting reverse begin iterator compares
 *       equal to the corresponding reverse end iterator.
 *
 * @attention The returned iterator provides read-only element access through
 *            iterator_get().
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 * @attention The Array must outlive the returned iterator.
 * @attention Any invalidating Array mutation invalidates the iterator.
 */
ArrayResult array_get_begin_reverse_iterator(const Array* array, Iterator* out);

/**
 * @brief Creates a past-the-end iterator for forward traversal.
 *
 * @param[in] array Array whose traversal domain is represented.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @note The resulting iterator must not be dereferenced.
 * @note It is compatible with forward Array iterators produced for the same Array.
 *
 * @attention The returned iterator provides read-only element access through
 *            iterator_get().
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 * @attention The Array must outlive the returned iterator.
 * @attention Any invalidating Array mutation invalidates the iterator.
 */
ArrayResult array_get_end_iterator(const Array* array, Iterator* out);

/**
 * @brief Creates the past-the-end iterator for reverse traversal.
 *
 * The reverse end represents the position reached after traversing past the
 * first logical Array element in reverse order.
 *
 * @param[in] array Array whose reverse traversal domain is represented.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @note The resulting iterator must not be dereferenced.
 * @note It is compatible with reverse Array iterators produced for the same Array.
 *
 * @attention The returned iterator provides read-only element access through
 *            iterator_get().
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 * @attention The Array must outlive the returned iterator.
 * @attention Any invalidating Array mutation invalidates the iterator.
 */
ArrayResult array_get_end_reverse_iterator(const Array* array, Iterator* out);

/**
 * @brief Reduces a dynamic Array's capacity to the smallest permitted capacity.
 *
 * If the Array contains elements, the resulting capacity becomes exactly equal
 * to its logical size.
 *
 * If the Array is empty, the resulting capacity becomes the Array's original
 * base capacity.
 *
 * @param[in, out] array Array whose storage should be reduced.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OPERATION_NOT_SUPPORTED for fixed-capacity arrays.
 *
 * @note If the resulting capacity already equals the current capacity, the
 *       operation is a successful no-op.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            iterators and internal-storage pointers are invalidated.
 */
ArrayResult array_shrink_to_fit(Array* array);

/**
 * @brief Ensures that a dynamic Array has capacity for at least 'new_cap'
 *        elements.
 *
 * @param[in, out] array Array whose capacity should be reserved.
 * @param[in] new_cap Minimum requested capacity.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OPERATION_NOT_SUPPORTED for fixed-capacity arrays.
 *
 * @note If 'new_cap <= current capacity', the operation is a successful no-op.
 * @note The operation never reduces capacity.
 * @note The operation never changes logical size.
 * @note Newly reserved storage contains unspecified bytes.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            iterators and internal-storage pointers are invalidated.
 */
ArrayResult array_reserve(Array* array, size_t new_cap);

/**
 * @brief Removes all logical elements without changing capacity.
 *
 * @param[in, out] array Array to clear.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On success:
 *       - size becomes zero;
 *       - capacity remains unchanged;
 *       - existing element bytes may remain physically present but are no
 *         longer logically accessible.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_clear(Array* array);

/**
 * @brief Appends one element to the end of an Array.
 *
 * Exactly 'element_size' bytes are copied from 'element'.
 *
 * @param[in, out] array Array to modify.
 * @param[in] element Pointer to the source element. Must not be NULL.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_SPACE if a fixed-capacity Array is full.
 *
 * @note A dynamic Array grows automatically when necessary according to its
 *       configured growth factor.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_append(Array* array, const void* element);

/**
 * @brief Inserts one element at a specific logical index.
 *
 * Existing elements in the range '[index, size)' are shifted one position
 * toward higher indexes while preserving their relative order.
 *
 * @param[in, out] array Array to modify.
 * @param[in] element Pointer to the source element. Must not be NULL.
 * @param[in] index Insertion index.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_SPACE if insertion requires unavailable capacity
 *         in a fixed-capacity Array.
 *
 * @attention Valid insertion indexes are in the inclusive range '[0, size]'.
 *            Using 'index == size' is equivalent to appending the element.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_insert(Array* array, const void* element, size_t index);

/**
 * @brief Appends all elements in the iterator range '[start, end)'.
 *
 * The iterators are borrowed and are not modified by this operation.
 *
 * The element type size reported by both iterators must equal the destination
 * Array's 'element_size'. A size mismatch results in ARRAY_ERROR_ITERATOR_ERROR.
 *
 * Every dereferenced source element must refer to data compatible with the
 * destination Array's element representation.
 *
 * @param[in, out] array Destination Array.
 * @param[in] start Iterator representing the first source element.
 * @param[in] end Iterator representing the past-the-end source position.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_ITERATOR_ERROR if an iterator operation fails.
 * @return ARRAY_ERROR_OUT_OF_SPACE if required capacity cannot be provided.
 *
 * @attention 'start' and 'end' must be compatible iterators belonging to the
 *            same iteration domain.
 *
 * @attention The source range must not refer to the destination Array itself.
 *            Self-range insertion is not supported.
 *
 * @note Forward iterators are sufficient.
 * @note The implementation may clone 'start' internally to traverse the range;
 *       the caller-provided iterators remain unchanged.
 * @note If 'start' and 'end' represent the same position, the operation is a
 *       successful no-op.
 * @note Random-access iterators may be used to determine the range size before
 *       mutation and reserve storage efficiently.
 *
 * @post On failure the destination Array is restored to its exact state before
 *       the call.
 *
 * @attention On successful mutation all previously obtained iterators and
 *            borrowed pointers into the destination Array are invalidated.
 */
ArrayResult array_append_range(Array* array, const Iterator* start, const Iterator* end);

/**
 * @brief Inserts all elements in the iterator range '[start, end)' at 'index'.
 *
 * Source element order is preserved.
 *
 * The iterators are borrowed and are not modified by this operation.
 *
 * The element type size reported by both iterators must equal the destination
 * Array's 'element_size'. A size mismatch results in ARRAY_ERROR_ITERATOR_ERROR.
 *
 * @param[in, out] array Destination Array.
 * @param[in] start Iterator representing the first source element.
 * @param[in] end Iterator representing the past-the-end source position.
 * @param[in] index Destination insertion index.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_ITERATOR_ERROR if an iterator operation fails.
 * @return ARRAY_ERROR_OUT_OF_SPACE if required capacity cannot be provided.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index > size'.
 *
 * @attention Valid insertion indexes are in the inclusive range '[0, size]'.
 * @attention 'start' and 'end' must be compatible and belong to the same domain.
 * @attention The source range must not refer to the destination Array itself.
 *
 * @note Forward iterators are sufficient.
 * @note The implementation may clone 'start' internally to traverse the range;
 *       the caller-provided iterators remain unchanged.
 * @note If the source range is empty, the operation is a successful no-op.
 *
 * @post On failure the destination Array is restored to its exact state before
 *       the call.
 *
 * @attention On successful mutation all previously obtained iterators and
 *            borrowed pointers into the destination Array are invalidated.
 */
ArrayResult array_insert_range(Array* array, const Iterator* start, const Iterator* end, size_t index);

/**
 * @brief Appends a contiguous sequence of elements.
 *
 * 'elements' must point to 'count' consecutive source elements, each exactly
 * 'element_size' bytes wide.
 *
 * @param[in, out] array Destination Array.
 * @param[in] count Number of elements to append.
 * @param[in] elements Pointer to the first source element.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @note 'count == 0' results in a successful no-op and permits 'elements == NULL'.
 *
 * @attention If 'count > 0', 'elements' must not be NULL.
 * @attention Self-aliasing is not supported. The source memory must not overlap
 *            the destination Array's internal storage.
 *
 * @post Source ordering is preserved.
 * @post On failure the Array is restored to its exact state before the call.
 *
 * @attention On successful mutation all previously obtained iterators and
 *            borrowed pointers into this Array are invalidated.
 */
ArrayResult array_append_elements(Array* array, size_t count, const void* elements);

/**
 * @brief Inserts a contiguous sequence of elements at 'index'.
 *
 * 'elements' must point to 'count' consecutive source elements, each exactly
 * 'element_size' bytes wide.
 *
 * Existing elements at and after 'index' are shifted toward higher indexes
 * while preserving order.
 *
 * @param[in, out] array Destination Array.
 * @param[in] index Destination insertion index.
 * @param[in] count Number of elements to insert.
 * @param[in] elements Pointer to the first source element.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index > size'.
 *
 * @attention Valid insertion indexes are in the inclusive range '[0, size]'.
 *
 * @note 'count == 0' results in a successful no-op and permits 'elements == NULL'.
 *
 * @attention If 'count > 0', 'elements' must not be NULL.
 * @attention Self-aliasing is not supported. The source memory must not overlap
 *            the destination Array's internal storage.
 *
 * @post Source ordering is preserved.
 * @post On failure the Array is restored to its exact state before the call.
 *
 * @attention On successful mutation all previously obtained iterators and
 *            borrowed pointers into this Array are invalidated.
 */
ArrayResult array_insert_elements(Array* array, size_t index, size_t count, const void* elements);

/**
 * @brief Removes the last logical element.
 *
 * @param[in, out] array Array to modify.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_remove_last(Array* array);

/**
 * @brief Removes the element at 'index' while preserving element order.
 *
 * Every element following 'index' is shifted one position toward the beginning
 * of the Array.
 *
 * @param[in, out] array Array to modify.
 * @param[in] index Zero-based index of the element to remove.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post The Array is not automatically shrunk.
 * @post Removing 'size - 1' has the same logical effect as array_remove_last().
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_remove_at(Array* array, size_t index);

/**
 * @brief Converts an ArrayResult value to a human-readable static string.
 *
 * @param[in] result Result code to convert.
 *
 * @return A human-readable description of 'result', or NULL if 'result' is not
 *         a valid ArrayResult value.
 *
 * @note The returned string has static lifetime and must not be modified or freed.
 */
const char* array_result_to_string(ArrayResult result);

#ifdef __cplusplus
}
#endif

#endif /* ARC_ARRAY_H_ */
