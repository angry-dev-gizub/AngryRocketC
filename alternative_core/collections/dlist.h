/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file dlist.h
 *
 * @author Nelson Somé
 *
 * @brief Generic doubly linked list storage.
 *
 * A doubly linked list stores a sequence of fixed-size elements in separately
 * allocated nodes. Each node is linked to its predecessor and successor,
 * allowing insertion and removal without shifting other element bytes.
 *
 * Elements are stored by value. Operations that insert or replace elements
 * copy exactly 'element_size' bytes from the supplied source memory. The List
 * does not manage resources indirectly owned by its elements.
 *
 * The List tracks its logical size but has no contiguous storage capacity or
 * automatic growth factor. Indexed access requires traversing the nodes and
 * is therefore generally linear in the number of traversed links.
 *
 * @attention Structural modifications may invalidate pointers to removed nodes
 *            and can change the index of other elements. Pointers to retained
 *            nodes are not ordinarily invalidated by relinking other nodes.
 *
 * @note The list object and the nodes may use different allocators. If the
 *       node allocator cannot individually release allocations, removing nodes
 *       cannot necessarily reclaim their storage immediately. Such storage
 *       must be managed according to the node allocator's lifetime policy.
 *
 * @note Index errors use ARC_DLIST_ERROR_OUT_OF_RANGE. Underlying allocator
 *       errors should preserve their original error information.
 *
 * @note The precise cleanup and ownership behavior for failed releases must
 *       be specified by the implementation and its allocator contracts.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_DLIST_H_
#define ARC_DLIST_H_

#include <stdbool.h>
#include <stddef.h>

#include "result/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Dlist specific errors. */
#define ARC_DOMAIN_DLIST ((uint8_t)3U)

/** The supplied list is NULL, invalid, or otherwise unusable. */
#define ARC_DLIST_ERROR_INVALID_LIST ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_DLIST, 1U)
/** An index refers to a position outside the list's logical element range. */
#define ARC_DLIST_ERROR_OUT_OF_RANGE ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_DLIST, 2U)

/** Forward declarations. */
typedef struct arc_allocator_t arc_allocator_t;
typedef struct arc_dlist_desc_t arc_dlist_desc_t;
typedef struct arc_dlist_t arc_dlist_t;

/**
 * @brief Configuration used to create a doubly linked List.
 *
 * Both allocator objects must remain valid for the List's lifetime. The List
 * borrows the allocators and does not own them. The descriptor itself need not
 * remain alive after creation.
 *
 * @attention 'element_size' must be nonzero. Both allocators must be valid;
 *            the object allocator must support allocation and individual free.
 *            The node allocator must support allocation, but individual free
 *            support is optional.
 */
struct arc_dlist_desc_t {
    /**
     * @brief Size in bytes of every element stored in the list.
     *
     * The value must be greater than zero and remains constant for the entire
     * lifetime of the list.
     */
    size_t element_size;

    /**
     * @brief Allocator used to create and release the list object itself.
     *
     * @attention The allocator must support allocation and individual release.
     *
     * @note The list stores this pointer but does not own the Allocator.
     * @note This allocator must outlive the list.
     */
    arc_allocator_t* object_allocator;

    /**
     * @brief Allocator used to allocate and release the internal node storage and node metadata.
     *
     * @attention The allocator must support allocation.
     * @attention The allocator may not support individual release.
     *
     * @note The list stores this pointer but does not own the Allocator.
     * @note This allocator must outlive the list.
     */
    arc_allocator_t* node_allocator;
};

/**
 * @brief Builds a List descriptor for 'type' with separate allocators.
 *
 * @param type Element type whose size is used.
 * @param obj_allocator_ptr Allocator for List metadata.
 * @param node_allocator_ptr Allocator for List nodes.
 *
 * @note This macro constructs a compound-literal descriptor.
 */
#define ARC_DLIST_DESC_FROM_TYPE_EX(type, obj_allocator_ptr, node_allocator_ptr)                                       \
    (arc_dlist_desc_t) {                                                                                               \
        .element_size = sizeof(type), .object_allocator = (obj_allocator_ptr), .node_allocator = (node_allocator_ptr), \
    }

/**
 * @brief Builds a List descriptor using one allocator for metadata and nodes.
 *
 * @param type Element type whose size is used.
 * @param allocator_ptr Allocator shared by List metadata and nodes.
 */
#define ARC_DLIST_DESC_FROM_TYPE(type, allocator_ptr) ARC_DLIST_DESC_FROM_TYPE_EX(type, allocator_ptr, allocator_ptr)

/**
 * @brief Creates an empty doubly linked List using a descriptor.
 *
 * @param[out] out Receives the created List on success.
 * @param[in] desc List configuration.
 *
 * @post On success, the List is empty and '*out' points to it.
 * @post On failure, '*out' is NULL when 'out' is non-NULL.
 * @return Success or the applicable creation/allocation failure.
 */
arc_result_t arc_dlist_new(arc_dlist_t** out, const arc_dlist_desc_t* desc);

/**
 * @brief Destroys a List and releases resources where supported.
 *
 * @param[in, out] list List to destroy; may be NULL.
 *
 * @note A NULL List is treated as a successful no-op.
 * @attention All element pointers become unusable after destruction.
 * @attention When the node allocator lacks individual free, node storage may
 *            remain allocated until reclaimed by that allocator.
 * @return Success or the applicable release failure.
 */
arc_result_t arc_dlist_destroy(arc_dlist_t* list);

/**
 * @brief Checks whether a List is initialized and internally valid.
 *
 * @param[in] list List to inspect.
 * @return true if valid; false otherwise.
 * @note Passing an arbitrary dangling pointer is not safe.
 */
bool arc_dlist_is_valid(const arc_dlist_t* list);

/**
 * @brief Obtains a read-only pointer to an element at an index.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives a borrowed pointer to the element.
 * @param[in] index Zero-based index.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE when index is outside [0, size).
 * @post On failure, '*out' is NULL when the output argument is valid.
 * @note Indexed access requires node traversal.
 */
arc_result_t arc_dlist_get_at(const arc_dlist_t* list, const void** out, size_t index);

/**
 * @brief Obtains a read-only pointer to the first element.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives a borrowed pointer to the first element.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if empty.
 * @post On failure, '*out' is NULL when the output argument is valid.
 */
arc_result_t arc_dlist_get_first(const arc_dlist_t* list, const void** out);

/**
 * @brief Obtains a mutable pointer to the first element.
 *
 * @param[in, out] list List to inspect.
 * @param[out] out Receives mutable borrowed access to the first element.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if empty.
 * @attention The pointer must not be used after its node is removed.
 */
arc_result_t arc_dlist_get_first_mut(arc_dlist_t* list, void** out);

/**
 * @brief Obtains a read-only pointer to the last element.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives a borrowed pointer to the last element.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if empty.
 * @post On failure, '*out' is NULL when the output argument is valid.
 */
arc_result_t arc_dlist_get_last(const arc_dlist_t* list, const void** out);

/**
 * @brief Obtains a mutable pointer to the last element.
 *
 * @param[in, out] list List to inspect.
 * @param[out] out Receives mutable borrowed access to the last element.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if empty.
 * @attention The pointer must not be used after its node is removed.
 */
arc_result_t arc_dlist_get_last_mut(arc_dlist_t* list, void** out);

/**
 * @brief Gets the fixed size of one List element in bytes.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives the element size, including when the List is empty.
 * @return Success or ARC_DLIST_ERROR_INVALID_LIST.
 * @post On failure, '*out' is zero when the output argument is valid.
 */
arc_result_t arc_dlist_get_element_size(const arc_dlist_t* list, size_t* out);

/**
 * @brief Gets the number of elements in the List.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives the logical element count.
 * @return Success or ARC_DLIST_ERROR_INVALID_LIST.
 * @post On failure, '*out' is zero when the output argument is valid.
 */
arc_result_t arc_dlist_get_size(const arc_dlist_t* list, size_t* out);

/**
 * @brief Determines whether the List contains no elements.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives true exactly when the logical size is zero.
 * @return Success or ARC_DLIST_ERROR_INVALID_LIST.
 * @post On failure, '*out' is false when the output argument is valid.
 */
arc_result_t arc_dlist_is_empty(const arc_dlist_t* list, bool* out);

/**
 * @brief Replaces an existing element at an index by bytewise copy.
 *
 * @param[in, out] list List to modify.
 * @param[in] index Zero-based index of an existing element.
 * @param[in] element Pointer to at least 'element_size' source bytes.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE for an invalid index.
 * @note Node links and element addresses do not change.
 * @attention Source and destination bytes must not overlap for memcpy-style replacement.
 */
arc_result_t arc_dlist_set_at(arc_dlist_t* list, size_t index, const void* element);

/**
 * @brief Appends one element to the end of the List.
 *
 * @param[in, out] list List to modify.
 * @param[in] element Pointer to the element bytes to copy.
 * @return Success or an underlying node allocation failure.
 * @post On success, logical size increases by one.
 * @note Existing nodes need not be moved.
 */
arc_result_t arc_dlist_append(arc_dlist_t* list, const void* element);

/**
 * @brief Inserts one element at the beginning of the List.
 *
 * @param[in, out] list List to modify.
 * @param[in] element Pointer to the element bytes to copy.
 * @return Success or an underlying node allocation failure.
 * @post On success, logical size increases by one.
 */
arc_result_t arc_dlist_prepend(arc_dlist_t* list, const void* element);

/**
 * @brief Inserts one element at a specified index.
 *
 * @param[in, out] list List to modify.
 * @param[in] element Pointer to the element bytes to copy.
 * @param[in] index Insertion index in [0, size].
 * @return Success, ARC_DLIST_ERROR_OUT_OF_RANGE, or an allocator failure.
 * @note Inserting at index == size is equivalent to appending.
 */
arc_result_t arc_dlist_insert(arc_dlist_t* list, const void* element, size_t index);

/**
 * @brief Appends a contiguous sequence of elements to the List.
 *
 * @param[in, out] list Destination List.
 * @param[in] count Number of elements to copy.
 * @param[in] elements Pointer to 'count' consecutive source elements.
 * @return Success or an underlying node allocation failure.
 * @note A count of zero is a no-op and may use elements == NULL.
 * @attention Partial-allocation rollback behavior depends on the implementation
 *            and whether the node allocator can release nodes individually.
 */
arc_result_t arc_dlist_append_elements(arc_dlist_t* list, size_t count, const void* elements);

/**
 * @brief Inserts a contiguous sequence of elements at an index.
 *
 * @param[in, out] list Destination List.
 * @param[in] index Insertion position in [0, size].
 * @param[in] count Number of elements to insert.
 * @param[in] elements Pointer to 'count' consecutive source elements.
 * @return Success, ARC_DLIST_ERROR_OUT_OF_RANGE, or an allocator failure.
 * @note A count of zero is a no-op; index must still be in [0, size].
 * @attention Partial-allocation rollback behavior requires an explicit
 *            implementation policy when individual node free is unsupported.
 */
arc_result_t arc_dlist_insert_elements(arc_dlist_t* list, size_t index, size_t count, const void* elements);

/**
 * @brief Removes the first logical element.
 *
 * @param[in, out] list List to modify.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if the List is empty.
 * @post On success, logical size decreases by one.
 * @attention A borrowed pointer to the removed element becomes invalid.
 */
arc_result_t arc_dlist_remove_first(arc_dlist_t* list);

/**
 * @brief Removes the last logical element.
 *
 * @param[in, out] list List to modify.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if the List is empty.
 * @post On success, logical size decreases by one.
 * @attention A borrowed pointer to the removed element becomes invalid.
 */
arc_result_t arc_dlist_remove_last(arc_dlist_t* list);

/**
 * @brief Removes the element at a specified index.
 *
 * @param[in, out] list List to modify.
 * @param[in] index Index of the element to remove, in [0, size).
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE for an invalid index.
 * @post On success, logical size decreases by one.
 * @attention A borrowed pointer to the removed element becomes invalid.
 */
arc_result_t arc_dlist_remove_at(arc_dlist_t* list, size_t index);

/**
 * @brief Removes a consecutive range of logical elements.
 *
 * @param[in, out] list List to modify.
 * @param[in] index Index of the first element to remove.
 * @param[in] count Number of elements to remove.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE when the range is invalid.
 * @note A zero count is a no-op and permits index == size.
 * @post On success, logical size decreases by count.
 * @attention Borrowed pointers to removed nodes become invalid.
 */
arc_result_t arc_dlist_remove_elements(arc_dlist_t* list, size_t index, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* ARC_DLIST_H_ */
