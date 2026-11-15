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
 * the array unchanged. This guarantee requires rollback releases to succeed;
 * failures during cleanup must preserve and report surviving ownership using
 * arc_result_t.residue where applicable.
 *
 * @attention Successful structural mutations invalidate all previously obtained
 *            borrowed pointers referring to the Array's internal storage.
 *
 * @note Replacing element bytes without changing Array structure, including
 *       arc_array_set_at() and writes through mutable element/data pointers, does
 *       not invalidate existing borrowed pointers.
 *
 * @note Invalid call arguments, invalid descriptors, and other detected
 *       programmer errors panic. Out-of-range indexes and ordinary capacity
 *       failures return failed Results. Underlying allocator failures retain
 *       their original error domain, code, and residual ownership.
 *
 * @note Unsupported operations use the common ARC_ERROR_UNSUPPORTED_OPERATION
 *       code rather than an Array-specific error.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_ARRAY_H_
#define ARC_ARRAY_H_

#include <stdbool.h>
#include <stddef.h>

#include "result/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Allocator specific errors. */
#define ARC_DOMAIN_ARRAY ((uint8_t)2U)

/** The supplied Array is NULL, invalid, or otherwise unusable. */
#define ARC_ARRAY_ERROR_INVALID_ARRAY ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_ARRAY, 1U)
/** The operation requires more capacity than can be represented or provided. */
#define ARC_ARRAY_ERROR_OUT_OF_SPACE ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_ARRAY, 2U)
/** An index refers to a position outside the array's logical element range. */
#define ARC_ARRAY_ERROR_OUT_OF_RANGE ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_ARRAY, 3U)
/** Aliasing error */
#define ARC_ARRAY_ERROR_ALIASING ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_ARRAY, 4U)

/** Forward declarations */
typedef struct arc_allocator_t arc_allocator_t;
typedef struct arc_array_desc_t arc_array_desc_t;
typedef struct arc_array_t arc_array_t;

typedef /** arc_array_growth_factor_t */
    /**
 * @brief arc_array_growth_factor_t provides the supported automatic capacity growth
 *        strategies.
 *
 * ARC_ARRAY_GROWTH_NONE represents a fixed-capacity array. Any other value
 * represents a dynamically growing array.
 */
    enum arc_array_growth_factor_t {
        /**
         * No automatic capacity growth.
         *
         * Arrays using this strategy have fixed capacity.
         */
        ARC_ARRAY_GROWTH_NONE = 0,

        /**
         * factor = 1.125
         *
         * pros: Minimizes excess capacity.
         * cons: Significantly more frequent growth operations and copies.
         */
        ARC_ARRAY_GROWTH_1_125X,

        /**
         * factor = 1.5
         *
         * pros: Lower potential unused capacity than 2x growth.
         * cons: More frequent growth operations and copies.
         */
        ARC_ARRAY_GROWTH_1_5X,

        /**
         * factor = 2
         *
         * pros: Fewer growth operations and copies.
         * cons: Higher potential unused capacity.
         */
        ARC_ARRAY_GROWTH_2X,
    } arc_array_growth_factor_t;

/**
 * @brief Array configuration object used to create an array.
 *
 * @attention Invalid descriptors and other detected programmer contract violations
 *            call arc_abort() rather than returning ordinary failure Results.
 *            Out-of-memory and allocator operational failures return arc_result_t.
 *
 * @attention A valid descriptor must satisfy the following:
 *            - 'base_capacity' must be greater than zero;
 *            - 'element_size' must be greater than zero;
 *            - 'base_capacity * element_size' must be representable by size_t;
 *            - 'growth_factor' must contain a valid arc_array_growth_factor_t value;
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
struct arc_array_desc_t {
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
     * arc_array_shrink_to_fit() when the array is empty.
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
     * ARC_ARRAY_GROWTH_NONE creates a fixed-capacity array.
     * Any other valid growth factor creates a dynamic array.
     */
    arc_array_growth_factor_t growth_factor;

    /**
     * @brief Allocator used to create and release the Array object itself.
     *
     * @attention The allocator must support allocation and individual release.
     *
     * @note The Array stores this pointer but does not own the Allocator.
     * @note This allocator must outlive the Array.
     */
    arc_allocator_t* object_allocator;

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
    arc_allocator_t* storage_allocator;
};

#define ARC_STATIC_ARRAY_DESC_FROM_TYPE_EX(type, capacity, obj_allocator_ptr, arr_allocator_ptr)                       \
    (arc_array_desc_t) {                                                                                               \
        .base_capacity = capacity, .element_size = sizeof(type), .object_allocator = obj_allocator_ptr,                \
        .storage_allocator = arr_allocator_ptr, .growth_factor = ARC_ARRAY_GROWTH_NONE,                                \
    }

#define ARC_DYNAMIC_ARRAY_DESC_FROM_TYPE_EX(type, capacity, obj_allocator_ptr, arr_allocator_ptr, gfactor)             \
    (arc_array_desc_t) {                                                                                               \
        .base_capacity = capacity, .element_size = sizeof(type), .object_allocator = obj_allocator_ptr,                \
        .storage_allocator = arr_allocator_ptr, .growth_factor = gfactor,                                              \
    }

/** 
 * Note that the object metadata and the data are allocated by the same allocator.
 */
#define ARC_STATIC_ARRAY_DESC_FROM_TYPE(type, capacity, allocator_ptr)                                                 \
    ARC_STATIC_ARRAY_DESC_FROM_TYPE_EX(type, capacity, allocator_ptr, allocator_ptr)

/** 
 * Note that the object metadata and the data are allocated by the same allocator.
 */
#define ARC_DYNAMIC_ARRAY_DESC_FROM_TYPE(type, capacity, allocator_ptr)                                                \
    ARC_DYNAMIC_ARRAY_DESC_FROM_TYPE_EX(type, capacity, allocator_ptr, allocator_ptr, ARC_ARRAY_GROWTH_2X)

/**
 * @brief Creates a new empty Array using the supplied descriptor.
 *
 * The Array object is allocated using 'object_allocator' and its internal
 * storage is allocated using 'storage_allocator'.
 *
 * @param[out] out Pointer receiving the newly created Array.
 * @param[in] desc Read-only Array configuration descriptor.
 *
 * @return Successful Result if the Array was created; otherwise a failed
 *         Result containing the original allocation error, or a cleanup error
 *         if rollback release also fails.
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
 * @note If Array-object allocation succeeds but storage allocation fails,
 *       creation attempts to release the Array object before returning.
 *       If rollback release fails, the cleanup failure is returned, not the
 *       original allocation failure. Its Result.residue owns the unreleased
 *       Array object; '*out' remains NULL. The caller must eventually release
 *       that residue. This contract requires the failed release to leave the
 *       object allocated and recoverable.
 */
arc_result_t arc_array_new(arc_array_t** out, const arc_array_desc_t* desc);

/**
 * @brief Destroys an Array and releases all storage owned by it.
 *
 * The internal element storage is released before the Array object itself.
 *
 * @param[in, out] array Array to destroy. May be NULL.
 *
 * @return Successful Result if destruction succeeds; otherwise the release
 *         failure, preserving residual ownership when cleanup fails.
 *
 * @note Passing NULL results in a successful no-op.
 *
 * @attention All borrowed pointers referring to the
 *            Array become invalid after successful destruction.
 *
 * @attention If releasing internal storage fails, the Array object is not
 *            released. The cleanup failure is returned with its ownership
 *            information; the caller must inspect Result.residue if present.
 *
 * @attention If releasing the Array object fails after its storage has already
 *            been freed, return that cleanup failure with residual ownership
 *            of the Array object. Its former storage has already been released;
 *            the Array must not be used for normal operations.
 */
arc_result_t arc_array_destroy(arc_array_t* array);

/**
 * @brief Checks whether an Array is valid and initialized.
 *
 * @param[in] array Pointer to the Array to check.
 * @return true if the Array is valid and initialized, false otherwise.
 */
bool arc_array_is_valid(const arc_array_t* array);

/**
 * @brief Obtains a read-only pointer to the element at 'index'.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the address of the requested element.
 * @param[in] index Zero-based element index.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer refers directly to internal Array storage.
 * @note Ownership of the element is not transferred to the caller.
 *
 * @attention The pointer remains valid only while the corresponding storage
 *            remains valid and no invalidating Array operation is performed.
 */
arc_result_t arc_array_get_at(const arc_array_t* array, const void** out, size_t index);

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
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @attention 'element' must not refer to storage whose validity depends on the
 *            Array being modified.
 *
 * @post On failure the Array remains unchanged.
 *
 * @note Replacing element bytes does not change Array structure, size, capacity,
 *       or element addresses and therefore does not invalidate existing
 *       borrowed pointers.
 */
arc_result_t arc_array_set_at(arc_array_t* array, size_t index, const void* element);

/**
 * @brief Obtains a read-only pointer to the first element.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the first element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and follows the same invalidation
 *       rules as pointers returned by arc_array_get_at().
 */
arc_result_t arc_array_get_first(const arc_array_t* array, const void** out);

/**
 * @brief Obtains a mutable pointer to the first logical element.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the first element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed.
 * @note Writing through the pointer modifies element bytes only and does not
 *       invalidate existing borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
arc_result_t arc_array_get_first_mut(arc_array_t* array, void** out);

/**
 * @brief Obtains a read-only pointer to the last element.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the last element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and follows the same invalidation
 *       rules as pointers returned by arc_array_get_at().
 */
arc_result_t arc_array_get_last(const arc_array_t* array, const void** out);

/**
 * @brief Obtains a mutable pointer to the last logical element.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the last element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed.
 * @note Writing through the pointer modifies element bytes only and does not
 *       invalidate existing borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
arc_result_t arc_array_get_last_mut(arc_array_t* array, void** out);

/**
 * @brief Obtains a read-only pointer to the contiguous element storage.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the beginning of the logical element data.
 *
 * @return successful arc_result_t on success.
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
arc_result_t arc_array_get_data(const arc_array_t* array, const void** out);

/**
 * @brief Obtains a mutable pointer to the contiguous element storage.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the beginning of the
 *                 logical element data.
 *
 * @return successful arc_result_t on success.
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
arc_result_t arc_array_get_data_mut(arc_array_t* array, void** out);

/**
 * @brief Obtains the read-only contiguous element storage and logical byte size.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the beginning of the logical element data.
 * @param[out] out_size Pointer receiving the logical data size in bytes.
 *
 * @return successful arc_result_t on success.
 *
 * @post On success, '*out_size' equals 'size * element_size'.
 * @post If the Array is empty, '*out' is NULL and '*out_size' is zero.
 * @post On failure, '*out' is NULL and '*out_size' is zero.
 *
 * @note The returned pointer is borrowed and follows the same invalidation rules
 *       as arc_array_get_data().
 */
arc_result_t arc_array_get_data_ex(const arc_array_t* array, const void** out, size_t* out_size);

/**
 * @brief Obtains mutable contiguous element storage and logical byte size.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the logical element data.
 * @param[out] out_size Pointer receiving the logical data size in bytes.
 *
 * @return successful arc_result_t on success.
 *
 * @post On success, '*out_size' equals 'size * element_size'.
 * @post If the Array is empty, '*out' is NULL and '*out_size' is zero.
 * @post On failure, '*out' is NULL and '*out_size' is zero.
 *
 * @note Writing within the returned logical byte range modifies element bytes
 *       only and does not invalidate existing 
 *       borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
arc_result_t arc_array_get_data_mut_ex(arc_array_t* array, void** out, size_t* out_size);

/**
 * @brief Obtains the fixed size in bytes of each Array element.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the element size.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to zero.
 */
arc_result_t arc_array_get_element_size(const arc_array_t* array, size_t* out);

/**
 * @brief Obtains the number of currently stored elements.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the logical size.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to zero.
 */
arc_result_t arc_array_get_size(const arc_array_t* array, size_t* out);

/**
 * @brief Obtains the current element capacity.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the capacity.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to zero.
 */
arc_result_t arc_array_get_capacity(const arc_array_t* array, size_t* out);

/**
 * @brief Determines whether the Array supports automatic growth.
 *
 * An Array is dynamic exactly when its growth factor is not
 * ARC_ARRAY_GROWTH_NONE.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the result.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to false.
 */
arc_result_t arc_array_is_dynamic(const arc_array_t* array, bool* out);

/**
 * @brief Determines whether the Array contains no elements.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving whether 'size == 0'.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to false.
 */
arc_result_t arc_array_is_empty(const arc_array_t* array, bool* out);

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
 * @return successful arc_result_t on success.
 * @return ARC_ERROR_UNSUPPORTED_OPERATION (common domain) for fixed-capacity arrays.
 *
 * @note If the resulting capacity already equals the current capacity, the
 *       operation is a successful no-op.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            internal-storage pointers are
 *            invalidated.
 */
arc_result_t arc_array_shrink_to_fit(arc_array_t* array);

/**
 * @brief Ensures that a dynamic Array has capacity for at least 'new_cap'
 *        elements.
 *
 * @param[in, out] array Array whose capacity should be reserved.
 * @param[in] new_cap Minimum requested capacity.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ERROR_UNSUPPORTED_OPERATION (common domain) for fixed-capacity arrays.
 *
 * @note If 'new_cap <= current capacity', the operation is a successful no-op.
 * @note The operation never reduces capacity.
 * @note The operation never changes logical size.
 * @note Newly reserved storage contains unspecified bytes.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            internal-storage pointers are
 *            invalidated.
 */
arc_result_t arc_array_reserve(arc_array_t* array, size_t new_cap);

/**
 * @brief Removes all logical elements without changing capacity.
 *
 * @param[in, out] array Array to clear.
 *
 * @return successful arc_result_t on success.
 *
 * @post On success:
 *       - size becomes zero;
 *       - capacity remains unchanged;
 *       - existing element bytes may remain physically present but are no
 *         longer logically accessible.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_clear(arc_array_t* array);

/**
 * @brief Appends one element to the end of an Array.
 *
 * Exactly 'element_size' bytes are copied from 'element'.
 *
 * @param[in, out] array Array to modify.
 * @param[in] element Pointer to the source element. Must not be NULL.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_SPACE if a fixed-capacity Array is full.
 *
 * @note A dynamic Array grows automatically when necessary according to its
 *       configured growth factor.
 *
 * @return ARC_ARRAY_ERROR_ALIASING if source overlaps internal storage.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_append(arc_array_t* array, const void* element);

/**
 * @brief Inserts one element at the beginning of an Array.
 *
 * Existing elements are shifted one position toward higher indexes while
 * preserving their relative order.
 *
 * @param[in, out] array Array to modify.
 * @param[in] element Pointer to the source element. Must not be NULL.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_SPACE if insertion requires unavailable capacity
 *         in a fixed-capacity Array.
 *
 * @note This operation is logically equivalent to inserting at index zero.
 * @note A dynamic Array grows automatically when necessary according to its
 *       configured growth factor.
 *
 * @return ARC_ARRAY_ERROR_ALIASING if source overlaps internal storage.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_prepend(arc_array_t* array, const void* element);

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
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_SPACE if insertion requires unavailable capacity
 *         in a fixed-capacity Array.
 *
 * @attention Valid insertion indexes are in the inclusive range '[0, size]'.
 *            Using 'index == size' is equivalent to appending the element.
 *
 * @return ARC_ARRAY_ERROR_ALIASING if source overlaps internal storage.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_insert(arc_array_t* array, const void* element, size_t index);

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
 * @return successful arc_result_t on success.
 *
 * @note 'count == 0' results in a successful no-op and permits 'elements == NULL'.
 *
 * @attention If 'count > 0', 'elements' must not be NULL.
 * @return ARC_ARRAY_ERROR_ALIASING if source overlaps internal storage.
 *
 * @attention Self-aliasing is not supported. The source memory must not overlap
 *            the destination Array's internal storage.
 *
 * @post Source ordering is preserved.
 * @post On failure the Array is restored to its exact state before the call,
 *       provided every required rollback release succeeds. A failed rollback
 *       must report remaining ownership rather than silently leaking it.
 *
 * @attention On successful mutation all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_append_elements(arc_array_t* array, size_t count, const void* elements);

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
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index > size'.
 *
 * @attention Valid insertion indexes are in the inclusive range '[0, size]'.
 *
 * @note 'count == 0' results in a successful no-op and permits 'elements == NULL'.
 *
 * @attention If 'count > 0', 'elements' must not be NULL.
 * @return ARC_ARRAY_ERROR_ALIASING if source overlaps internal storage.
 *
 * @attention Self-aliasing is not supported. The source memory must not overlap
 *            the destination Array's internal storage.
 *
 * @post Source ordering is preserved.
 * @post On failure the Array is restored to its exact state before the call,
 *       provided every required rollback release succeeds. A failed rollback
 *       must report remaining ownership rather than silently leaking it.
 *
 * @attention On successful mutation all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_insert_elements(arc_array_t* array, size_t index, size_t count, const void* elements);

/**
 * @brief Removes the first logical element from an Array.
 *
 * All elements following the first element are shifted one position toward the
 * beginning of the Array while preserving their relative order.
 *
 * @param[in, out] array Array to modify.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @note This operation is logically equivalent to removing the element at
 *       index zero.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_remove_first(arc_array_t* array);

/**
 * @brief Removes the last logical element.
 *
 * @param[in, out] array Array to modify.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_remove_last(arc_array_t* array);

/**
 * @brief Removes the element at 'index' while preserving element order.
 *
 * Every element following 'index' is shifted one position toward the beginning
 * of the Array.
 *
 * @param[in, out] array Array to modify.
 * @param[in] index Zero-based index of the element to remove.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post The Array is not automatically shrunk.
 * @post Removing 'size - 1' has the same logical effect as arc_array_remove_last().
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_remove_at(arc_array_t* array, size_t index);

/**
 * @brief Removes a contiguous sequence of logical elements.
 *
 * Removes 'count' elements beginning at 'index'. Elements following the
 * removed range are shifted toward the beginning of the Array while preserving
 * their relative order.
 *
 * @param[in, out] array Array to modify.
 * @param[in] index Zero-based index of the first element to remove.
 * @param[in] count Number of consecutive elements to remove.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index > size' or if the requested
 *         removal range extends beyond the logical size of the Array.
 *
 * @attention For 'count > 0', 'index' must refer to an existing logical
 *            element and the range '[index, index + count)' must be contained
 *            within '[0, size)'.
 *
 * @note 'count == 0' results in a successful no-op and permits
 *       'index == size'.
 *
 * @post On successful removal, size decreases by exactly 'count'.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention If 'count > 0', all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_remove_elements(arc_array_t* array, size_t index, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* ARC_ARRAY_H_ */
