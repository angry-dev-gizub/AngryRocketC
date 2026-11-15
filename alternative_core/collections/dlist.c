/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include <stdint.h>
#include <string.h>

#include "allocator/allocator.h"
#include "dlist.h"
#include "_dlist_def.h"

/**
 * @brief Checks if the provided list descriptor is valid.
 *
 * @param desc Pointer to the list descriptor to validate.
 *
 * @return true if the descriptor is valid, false otherwise.
 */
static bool arc_dlist_desc_is_valid(const arc_dlist_desc_t* desc) {
    if(!desc)
        return false;

    if(desc->element_size == 0)
        return false;

    if(!desc->object_allocator || !desc->node_allocator)
        return false;

    if(!arc_allocator_is_valid(desc->object_allocator) || !arc_allocator_is_valid(desc->node_allocator))
        return false;

    if(!arc_allocator_has_features(desc->object_allocator, ARC_ALLOCATOR_FEATURE_FREE))
        return false;

    return true;
}

/**
 * @brief Allocates and initializes a new doubly linked list node.
 *
 * @param[out] out Pointer to receive the newly created node.
 * @param[in] allocator Allocator to use for node and data allocation.
 * @param[in] element_size Size of the element in bytes.
 * @param[in] element Pointer to the element data to copy into the node.
 *
 * @return result indicating success or failure of the allocation.
 */
static arc_result_t arc_dlist_node_new(_arc_dlist_node_t** out, const arc_allocator_t* allocator, size_t element_size,
                                       const void* element) {
    if(!out)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    *out = NULL;

    if(!allocator || element_size == 0)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    _arc_dlist_node_t* node = NULL;
    arc_result_t result     = arc_allocator_alloc(allocator, sizeof(_arc_dlist_node_t), (void**)&node);
    if(!result.ok)
        return result;

    node->next = NULL;
    node->prev = NULL;
    node->data = NULL;

    result = arc_allocator_alloc(allocator, element_size, &node->data);
    if(!result.ok) {
        if(arc_allocator_has_features(allocator, ARC_ALLOCATOR_FEATURE_FREE)) {
            arc_result_t cleanup = arc_allocator_free(allocator, node);
            if(!cleanup.ok)
                return cleanup;
        }
        return result;
    }

    if(element)
        memcpy(node->data, element, element_size);

    *out = node;

    return arc_result_success();
}

/**
 * @brief Destroys a doubly linked list node and releases its resources.
 *
 * @param node Pointer to the node to destroy.
 * @param allocator Allocator to use for releasing the node and its data.
 *
 * @return result indicating success or failure of the destruction.
 */
static arc_result_t arc_dlist_node_destroy(_arc_dlist_node_t* node, const arc_allocator_t* allocator) {
    if(!node || !allocator || !arc_allocator_is_valid(allocator))
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    if(!arc_allocator_has_features(allocator, ARC_ALLOCATOR_FEATURE_FREE))
        return arc_result_success();

    if(node->data) {
        arc_result_t result = arc_allocator_free(allocator, node->data);
        if(!result.ok)
            return result;
        node->data = NULL;
    }

    return arc_allocator_free(allocator, node);
}

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
arc_result_t arc_dlist_new(arc_dlist_t** out, const arc_dlist_desc_t* desc) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = NULL;

    if(!arc_dlist_desc_is_valid(desc))
        arc_abort("Arc list: invalid descriptor");

    arc_dlist_t* list   = NULL;
    arc_result_t result = arc_allocator_alloc(desc->object_allocator, sizeof(arc_dlist_t), (void**)&list);
    if(!result.ok)
        return result;

    *list = (arc_dlist_t){
        .size             = 0,
        .head             = NULL,
        .tail             = NULL,
        .element_size     = desc->element_size,
        .node_allocator   = desc->node_allocator,
        .object_allocator = desc->object_allocator,
    };

    *out = list;
    return arc_result_success();
}

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
arc_result_t arc_dlist_destroy(arc_dlist_t* list) {
    if(!list)
        return arc_result_success();

    if(!arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    arc_allocator_t* obj_allocator = list->object_allocator;

    while(list->head) {
        _arc_dlist_node_t* cur = list->head;
        _arc_dlist_node_t* nxt = cur->next;

        arc_result_t r = arc_dlist_node_destroy(cur, list->node_allocator);
        if(!r.ok)
            return r;

        list->head = nxt;

        if(nxt)
            nxt->prev = NULL;
        else
            list->tail = NULL;

        list->size--;
    }

    return arc_allocator_free(obj_allocator, list);
}

/**
 * @brief Checks whether a List is initialized and internally valid.
 *
 * @param[in] list List to inspect.
 * @return true if valid; false otherwise.
 * @note Passing an arbitrary dangling pointer is not safe.
 */
bool arc_dlist_is_valid(const arc_dlist_t* list) {
    if(!list)
        return false;

    if(list->element_size == 0)
        return false;

    if(!list->object_allocator || !list->node_allocator)
        return false;

    if(!arc_allocator_is_valid(list->object_allocator )|| !arc_allocator_is_valid(list->node_allocator))
        return false;

    if(!arc_allocator_has_features(list->object_allocator, ARC_ALLOCATOR_FEATURE_FREE))
        return false;

    if(list->size == 0) {
        if(list->head || list->tail)
            return false;
    } else if(!list->head || !list->tail)
        return false;

    if(list->head && list->head->prev)
        return false;

    if(list->tail && list->tail->next)
        return false;

    return true;
}

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
arc_result_t arc_dlist_get_at(const arc_dlist_t* list, const void** out, size_t index) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = NULL;

    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(index >= list->size)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    _arc_dlist_node_t* cur;

    if(index < list->size / 2) {
        cur = list->head;

        for(size_t i = 0; i < index; i++)
            cur = cur->next;
    } else {
        cur = list->tail;

        for(size_t i = list->size - 1; i > index; i--)
            cur = cur->prev;
    }

    *out = cur->data;

    return arc_result_success();
}

/**
 * @brief Obtains a read-only pointer to the first element.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives a borrowed pointer to the first element.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if empty.
 * @post On failure, '*out' is NULL when the output argument is valid.
 */
arc_result_t arc_dlist_get_first(const arc_dlist_t* list, const void** out) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = NULL;

    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(list->size == 0)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    *out = list->head->data;

    return arc_result_success();
}

/**
 * @brief Obtains a mutable pointer to the first element.
 *
 * @param[in, out] list List to inspect.
 * @param[out] out Receives mutable borrowed access to the first element.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if empty.
 * @attention The pointer must not be used after its node is removed.
 */
arc_result_t arc_dlist_get_first_mut(arc_dlist_t* list, void** out) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = NULL;

    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(list->size == 0)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    *out = list->head->data;

    return arc_result_success();
}

/**
 * @brief Obtains a read-only pointer to the last element.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives a borrowed pointer to the last element.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if empty.
 * @post On failure, '*out' is NULL when the output argument is valid.
 */
arc_result_t arc_dlist_get_last(const arc_dlist_t* list, const void** out) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = NULL;

    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(list->size == 0)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    *out = list->tail->data;

    return arc_result_success();
}

/**
 * @brief Obtains a mutable pointer to the last element.
 *
 * @param[in, out] list List to inspect.
 * @param[out] out Receives mutable borrowed access to the last element.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if empty.
 * @attention The pointer must not be used after its node is removed.
 */
arc_result_t arc_dlist_get_last_mut(arc_dlist_t* list, void** out) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = NULL;

    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(list->size == 0)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    *out = list->tail->data;

    return arc_result_success();
}

/**
 * @brief Gets the fixed size of one List element in bytes.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives the element size, including when the List is empty.
 * @return Success or ARC_DLIST_ERROR_INVALID_LIST.
 * @post On failure, '*out' is zero when the output argument is valid.
 */
arc_result_t arc_dlist_get_element_size(const arc_dlist_t* list, size_t* out) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = (size_t)0;

    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    *out = list->element_size;

    return arc_result_success();
}

/**
 * @brief Gets the number of elements in the List.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives the logical element count.
 * @return Success or ARC_DLIST_ERROR_INVALID_LIST.
 * @post On failure, '*out' is zero when the output argument is valid.
 */
arc_result_t arc_dlist_get_size(const arc_dlist_t* list, size_t* out) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = (size_t)0;

    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    *out = list->size;

    return arc_result_success();
}

/**
 * @brief Determines whether the List contains no elements.
 *
 * @param[in] list List to inspect.
 * @param[out] out Receives true exactly when the logical size is zero.
 * @return Success or ARC_DLIST_ERROR_INVALID_LIST.
 * @post On failure, '*out' is false when the output argument is valid.
 */
arc_result_t arc_dlist_is_empty(const arc_dlist_t* list, bool* out) {
    if(!out)
        arc_abort("Arc list: output parameter is NULL");

    *out = false;

    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    *out = (list->size == 0);

    return arc_result_success();
}

/**
 * @brief Replaces an existing element at an index by bytewise copy.
 *
 * @param[in, out] list List to modify.
 * @param[in] index Zero-based index of an existing element.
 * @param[in] element Pointer to at least 'element_size' source bytes.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE for an invalid index.
 * @note Node links and element addresses do not change.
 * @note Source and destination may overlap; replacement uses memmove().
 */
arc_result_t arc_dlist_set_at(arc_dlist_t* list, size_t index, const void* element) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(!element)
        arc_abort("Arc list: element is NULL");

    if(index >= list->size)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    _arc_dlist_node_t* cur;

    if(index < list->size / 2) {
        cur = list->head;

        for(size_t i = 0; i < index; i++)
            cur = cur->next;
    } else {
        cur = list->tail;

        for(size_t i = list->size - 1; i > index; i--)
            cur = cur->prev;
    }

    memmove(cur->data, element, list->element_size);

    return arc_result_success();
}

/**
 * @brief Appends one element to the end of the List.
 *
 * @param[in, out] list List to modify.
 * @param[in] element Pointer to the element bytes to copy.
 * @return Success or an underlying node allocation failure.
 * @post On success, logical size increases by one.
 * @note Existing nodes need not be moved.
 */
arc_result_t arc_dlist_append(arc_dlist_t* list, const void* element) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(!element)
        arc_abort("Arc list: element is NULL");

    _arc_dlist_node_t* n = NULL;
    arc_result_t result  = arc_dlist_node_new(&n, list->node_allocator, list->element_size, element);
    if(!result.ok)
        return result;

    n->prev = list->tail;
    n->next = NULL;

    if(list->tail)
        list->tail->next = n;
    else
        list->head = n;

    list->tail = n;
    list->size++;

    return arc_result_success();
}

/**
 * @brief Inserts one element at the beginning of the List.
 *
 * @param[in, out] list List to modify.
 * @param[in] element Pointer to the element bytes to copy.
 * @return Success or an underlying node allocation failure.
 * @post On success, logical size increases by one.
 */
arc_result_t arc_dlist_prepend(arc_dlist_t* list, const void* element) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(!element)
        arc_abort("Arc list: element is NULL");

    _arc_dlist_node_t* n = NULL;
    arc_result_t result  = arc_dlist_node_new(&n, list->node_allocator, list->element_size, element);
    if(!result.ok)
        return result;

    n->prev = NULL;
    n->next = list->head;

    if(list->head)
        list->head->prev = n;
    else
        list->tail = n;

    list->head = n;
    list->size++;

    return arc_result_success();
}

/**
 * @brief Inserts one element at a specified index.
 *
 * @param[in, out] list List to modify.
 * @param[in] element Pointer to the element bytes to copy.
 * @param[in] index Insertion index in [0, size].
 * @return Success, ARC_DLIST_ERROR_OUT_OF_RANGE, or an allocator failure.
 * @note Inserting at index == size is equivalent to appending.
 */
arc_result_t arc_dlist_insert(arc_dlist_t* list, const void* element, size_t index) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(!element)
        arc_abort("Arc list: element is NULL");

    if(index > list->size)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    if(index == 0)
        return arc_dlist_prepend(list, element);

    if(index == list->size)
        return arc_dlist_append(list, element);

    _arc_dlist_node_t* cur;

    if(index < list->size / 2) {
        cur = list->head;

        for(size_t i = 0; i < index; i++)
            cur = cur->next;
    } else {
        cur = list->tail;

        for(size_t i = list->size - 1; i > index; i--)
            cur = cur->prev;
    }

    _arc_dlist_node_t* n = NULL;
    arc_result_t result  = arc_dlist_node_new(&n, list->node_allocator, list->element_size, element);
    if(!result.ok)
        return result;

    _arc_dlist_node_t* prev = cur->prev;

    n->prev = prev;
    n->next = cur;

    prev->next = n;
    cur->prev  = n;

    list->size++;

    return arc_result_success();
}

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
arc_result_t arc_dlist_append_elements(arc_dlist_t* list, size_t count, const void* elements) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    return arc_dlist_insert_elements(list, list->size, count, elements);
}

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
arc_result_t arc_dlist_insert_elements(arc_dlist_t* list, size_t index, size_t count, const void* elements) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(index > list->size)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    if(count == 0)
        return arc_result_success();

    if(!elements)
        arc_abort("Arc list: elements is NULL");

    if(count > SIZE_MAX / list->element_size || count > SIZE_MAX - list->size)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    _arc_dlist_node_t* first = NULL;
    _arc_dlist_node_t* last  = NULL;

    for(size_t i = 0; i < count; i++) {
        _arc_dlist_node_t* node = NULL;
        const void* source      = (const unsigned char*)elements + i * list->element_size;
        arc_result_t result     = arc_dlist_node_new(&node, list->node_allocator, list->element_size, source);

        if(!result.ok) {
            while(first) {
                _arc_dlist_node_t* next = first->next;
                arc_result_t cleanup    = arc_dlist_node_destroy(first, list->node_allocator);
                if(!cleanup.ok)
                    return cleanup;
                first = next;
            }
            return result;
        }

        node->prev = last;
        if(last)
            last->next = node;
        else
            first = node;
        last = node;
    }

    _arc_dlist_node_t* after = NULL;
    if(index < list->size) {
        if(index < list->size / 2) {
            after = list->head;
            for(size_t i = 0; i < index; i++)
                after = after->next;
        } else {
            after = list->tail;
            for(size_t i = list->size - 1; i > index; i--)
                after = after->prev;
        }
    }

    _arc_dlist_node_t* before = after ? after->prev : list->tail;
    first->prev               = before;
    last->next                = after;

    if(before)
        before->next = first;
    else
        list->head = first;

    if(after)
        after->prev = last;
    else
        list->tail = last;

    list->size += count;
    return arc_result_success();
}

/**
 * @brief Removes the first logical element.
 *
 * @param[in, out] list List to modify.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if the List is empty.
 * @post On success, logical size decreases by one.
 * @attention A borrowed pointer to the removed element becomes invalid.
 */
arc_result_t arc_dlist_remove_first(arc_dlist_t* list) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(list->size == 0)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    _arc_dlist_node_t* head = list->head;
    _arc_dlist_node_t* next = head->next;

    arc_result_t r = arc_dlist_node_destroy(head, list->node_allocator);
    if(!r.ok)
        return r;

    list->head = next;
    if(next)
        next->prev = NULL;
    else
        list->tail = NULL;

    list->size--;
    return arc_result_success();
}

/**
 * @brief Removes the last logical element.
 *
 * @param[in, out] list List to modify.
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE if the List is empty.
 * @post On success, logical size decreases by one.
 * @attention A borrowed pointer to the removed element becomes invalid.
 */
arc_result_t arc_dlist_remove_last(arc_dlist_t* list) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(list->size == 0)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    _arc_dlist_node_t* tail = list->tail;
    _arc_dlist_node_t* prev = tail->prev;

    arc_result_t r = arc_dlist_node_destroy(tail, list->node_allocator);
    if(!r.ok)
        return r;

    list->tail = prev;
    if(prev)
        prev->next = NULL;
    else
        list->head = NULL;

    list->size--;
    return arc_result_success();
}

/**
 * @brief Removes the element at a specified index.
 *
 * @param[in, out] list List to modify.
 * @param[in] index Index of the element to remove, in [0, size).
 * @return Success or ARC_DLIST_ERROR_OUT_OF_RANGE for an invalid index.
 * @post On success, logical size decreases by one.
 * @attention A borrowed pointer to the removed element becomes invalid.
 */
arc_result_t arc_dlist_remove_at(arc_dlist_t* list, size_t index) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(index >= list->size)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    if(index == 0)
        return arc_dlist_remove_first(list);

    if(index == list->size - 1)
        return arc_dlist_remove_last(list);

    _arc_dlist_node_t* cur;
    if(index < list->size / 2) {
        cur = list->head;
        for(size_t i = 0; i < index; i++)
            cur = cur->next;
    } else {
        cur = list->tail;
        for(size_t i = list->size - 1; i > index; i--)
            cur = cur->prev;
    }

    _arc_dlist_node_t* prev = cur->prev;
    _arc_dlist_node_t* next = cur->next;
    arc_result_t r          = arc_dlist_node_destroy(cur, list->node_allocator);
    if(!r.ok)
        return r;

    prev->next = next;
    next->prev = prev;
    list->size--;
    return arc_result_success();
}

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
arc_result_t arc_dlist_remove_elements(arc_dlist_t* list, size_t index, size_t count) {
    if(!list || !arc_dlist_is_valid(list))
        return arc_result_failure(ARC_DLIST_ERROR_INVALID_LIST);

    if(index > list->size || count > list->size - index)
        return arc_result_failure(ARC_DLIST_ERROR_OUT_OF_RANGE);

    for(size_t i = 0; i < count; i++) {
        arc_result_t r = arc_dlist_remove_at(list, index);
        if(!r.ok)
            return r;
    }

    return arc_result_success();
}
