/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file _dlist_def.h
 *
 * @author Nelson Somé
 *
 * @brief Internal definitions for generic doubly linked list storage.
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
 * @note This file is part of Arc Core.
 * 
 * @attention This header is for Arc Core internal use only and must not be
 *            included by public API users.
 */

#ifndef ARC_DLIST_DEF_H_
#define ARC_DLIST_DEF_H_

#include "dlist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef /* _arc_dlist_node_t */
    /**
 * @brief Internal node of a doubly linked list.
 *
 * Each node stores links to its neighboring nodes and a pointer to the
 * element bytes associated with that node.
 *
 * @note The node is an implementation detail and is not exposed by the public
 *       List API.
 */
 struct _arc_dlist_node_t {
    /** Pointer to the following node, or NULL when this is the last node. */
    struct _arc_dlist_node_t* next;

    /** Pointer to the preceding node, or NULL when this is the first node. */
    struct _arc_dlist_node_t* prev;

    /** Pointer to the stored element bytes, whose length is 'element_size'. */
    void* data;
} _arc_dlist_node_t;

/**
 * @brief Internal representation of a generic doubly linked list.
 *
 * The List keeps pointers to its first and last nodes and records the number
 * of elements and the byte size of each element.
 *
 * List elements are stored by value in node-associated memory. The List does
 * not own resources indirectly referenced by those values.
 *
 * @note The allocator objects are borrowed and must remain valid for the
 *       lifetime of the List.
 *
 * @attention Inserting or removing a node may invalidate pointers referring
 *            to that node's data, but does not move data in other nodes.
 */
struct arc_dlist_t {
    /** Number of logical elements currently stored in the List. */
    size_t size;

    /** Fixed size in bytes of each element stored in the List. */
    size_t element_size;

    /** Pointer to the first node; NULL when the List is empty. */
    _arc_dlist_node_t* head;

    /** Pointer to the last node; NULL when the List is empty. */
    _arc_dlist_node_t* tail;

    /** Allocator used for internal nodes and their element storage. */
    arc_allocator_t* node_allocator;

    /** Allocator used to allocate and release the List object itself. */
    arc_allocator_t* object_allocator;
};

#ifdef __cplusplus
}
#endif

#endif /* ARC_DLIST_DEF_H_ */
