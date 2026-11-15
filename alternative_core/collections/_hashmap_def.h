/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file _hashmap_def.h
 *
 * @author Nelson Somé
 *
 * @brief Generic Hashmap storage.
 *
 * The Hashmap API describes a collection of key/value associations accessed
 * through keys rather than numeric indexes. Keys are hashed using a supplied
 * callback and compared for equality using another supplied callback.
 *
 * @note This file is part of Arc Core.
 *
 * @attention This header is for Arc Core internal use only and must not be
 *            included by public API users.
 */

#ifndef ARC_HASHMAP_DEF_H_
#define ARC_HASHMAP_DEF_H_

#include "hashmap.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal entry stored in a Hashmap collision chain.
 *
 * Each entry represents one key/value association and links to adjacent
 * entries in the same bucket. The stored hash is computed using the Hashmap's
 * configured hashing callback and seed.
 *
 * The key and data fields point to separately allocated copies owned by the
 * Hashmap. Both allocations use the Hashmap's storage allocator and are
 * released when the entry is removed or the Hashmap is cleared or destroyed.
 */
typedef struct _arc_map_entry_t {
    /** Pointer to the next entry in the bucket chain, or NULL. */
    struct _arc_map_entry_t* next;

    /** Pointer to the previous entry in the bucket chain, or NULL. */
    struct _arc_map_entry_t* prev;

    /** Hash value associated with this entry's key. */
    uint64_t hash;

    /** Hashmap-owned allocation containing a copy of the key bytes. */
    void* key;

    /** Size of the copied key in bytes. */
    size_t key_size;

    /** Hashmap-owned allocation containing a copy of the element bytes. */
    void* data;
} _arc_map_entry_t;

/**
 * @brief Internal Hashmap object using separate chaining for collisions.
 *
 * Buckets form an array of entry pointers. Each non-empty bucket refers to
 * the first entry in a doubly linked collision chain; empty buckets contain
 * NULL. Capacity counts buckets, while size counts stored associations.
 *
 * The Hashmap owns its bucket array, entries, key copies, and value copies.
 * These allocations use the configured storage allocator and are released
 * when their corresponding entries or the entire Hashmap are removed.
 *
 * The Hashmap retains pointers to its allocator objects and does not own
 * those allocator objects. Both allocators must remain valid throughout the
 * Hashmap's lifetime.
 */
struct arc_map_t {
    /** Array of 'capacity' bucket-head pointers, or NULL before initialization. */
    _arc_map_entry_t** buckets;

    /** Number of key/value entries currently stored. */
    size_t size;

    /** Configured size in bytes of each stored element. */
    size_t element_size;

    /** Number of buckets in the bucket-head array. */
    size_t capacity;

    /** Seed passed to the configured key-hashing callback. */
    uint64_t seed;

    /** Load threshold configuration used when deciding whether to expand. */
    arc_map_load_factor_t load_factor;

    /** Capacity growth strategy used during expansion. */
    arc_map_growth_factor_t growth_factor;

    /** Callback used to compare keys for equality. */
    arc_key_equals_fun_t key_equals;

    /** Callback used to compute hashes from keys. */
    arc_key_hash_fun_t key_hash;

    /** Allocator used for the Hashmap object itself; not owned by the Hashmap. */
    arc_allocator_t* object_allocator;

    /**
     * Allocator used for buckets, entries, and owned key/value copies.
     * The allocator object itself is not owned by the Hashmap.
     */
    arc_allocator_t* storage_allocator;
};

#ifdef __cplusplus
}
#endif

#endif /* ARC_HASHMAP_DEF_H_ */
