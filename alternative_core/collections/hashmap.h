
/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file hashmap.h
 *
 * @author Nelson Somé
 *
 * @brief Generic Hashmap storage.
 *
 * The Hashmap API describes a collection of key/value associations accessed
 * through keys rather than numeric indexes. Keys are hashed using a supplied
 * callback and compared for equality using another supplied callback.
 *
 * The descriptor configures the element size, initial storage capacity,
 * hash seed, capacity growth strategy, load threshold, and allocators.
 *
 * The Hashmap copies the supplied key bytes and element bytes into storage
 * allocated through its storage allocator. It owns these copies and releases
 * them when entries are removed or the Hashmap is cleared or destroyed. The
 * caller only needs to keep input pointers valid for the duration of the
 * operation. Pointers returned by lookup functions are borrowed pointers into
 * Hashmap-owned value storage.
 *
 * The growth factor specifies how capacity may change, while the load factor
 * specifies the configured occupancy threshold. The representation of buckets,
 * collision handling, and exact threshold behavior are determined by the
 * implementation and are not defined by this header.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_HASHMAP_H_
#define ARC_HASHMAP_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "result/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Hashmap specific erroras */
#define ARC_DOMAIN_HASHMAP ((uint8_t)4U)

/** The supplied Hashmap is NULL, invalid, or otherwise unusable. */
#define ARC_MAP_ERROR_INVALID_MAP ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_HASHMAP, 1U)
/** The operation requires more capacity than can be represented or provided. */
#define ARC_MAP_ERROR_OUT_OF_SPACE ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_HASHMAP, 2U)
/** A requested key does not exist in the Hashmap. */
#define ARC_MAP_ERROR_KEY_NOT_FOUND ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_HASHMAP, 3U)

/** Forward declarations. */
typedef struct arc_allocator_t arc_allocator_t;
typedef struct arc_map_desc_t arc_map_desc_t;
typedef struct arc_map_t arc_map_t;

/**
  * @brief Supported Hashmap storage-capacity growth strategies.
  *
  * ARC_MAP_GROWTH_NONE disables automatic capacity growth; the other values
  * select a multiplier used when the Hashmap expands its storage.
  *
  * @note The growth factor controls the size of an expansion, not the load
  *       threshold that triggers it.
  */
typedef enum arc_map_growth_factor_t {
    /**
         * No automatic capacity growth.
         *
         * Maps using this strategy have fixed storage capacity.
         */
    ARC_MAP_GROWTH_NONE = 0,

    /**
         * Capacity growth factor of 1.125.
         *
         * Minimizes excess capacity at the cost of more frequent expansions.
         */
    ARC_MAP_GROWTH_1_125X,

    /**
         * Capacity growth factor of 1.5.
         *
         * Balances unused capacity against expansion frequency.
         */
    ARC_MAP_GROWTH_1_5X,

    /**
         * Capacity growth factor of 2.
         *
         * Reduces expansion frequency at the cost of more unused capacity.
         */
    ARC_MAP_GROWTH_2X,
} arc_map_growth_factor_t;

/**
 * @brief Configured load thresholds for Hashmap expansion.
 *
 * The named multipliers express the intended maximum load before expansion.
 * ARC_MAP_LOAD_FACTOR_NONE selects no load-based expansion threshold and is
 * intended for static maps.
 *
 * @note The interpretation of load depends on the Hashmap's collision and
 *       bucket-management strategy. This enum does not prescribe either.
 */
typedef enum arc_map_load_factor_t {
    /** No load-based expansion threshold; intended for static maps. */
    ARC_MAP_LOAD_FACTOR_NONE = 0,

    /** Load threshold of 1.0. */
    ARC_MAP_LOAD_FACTOR_1X,

    /** Load threshold of 1.5. */
    ARC_MAP_LOAD_FACTOR_1_5X,

    /** Load threshold of 1.8. */
    ARC_MAP_LOAD_FACTOR_1_8X,

    /** Load threshold of 2.0. */
    ARC_MAP_LOAD_FACTOR_2X,
} arc_map_load_factor_t;

/**
 * @brief Compares two Hashmap keys for equality.
 *
 * @param[in] k1 Pointer to the first key.
 * @param[in] k2 Pointer to the second key.
 * @param[in] k_size Size of the keys in bytes.
 *
 * @return true if the keys are equal, false otherwise.
 *
 * @attention Equal keys must produce equal hash values when passed to the
 *            configured hash callback with the same key size and seed.
 */
typedef bool (*arc_key_equals_fun_t)(const void* k1, const void* k2, size_t k_size);

/**
 * @brief Computes a 64-bit hash for a supplied key.
 *
 * @param[in] key Pointer to the key data.
 * @param[in] key_size Size of the key in bytes.
 * @param[in] seed Hash seed configured for the Hashmap.
 *
 * @return 64-bit hash value for the key.
 *
 * @note The callback should produce consistent results for the same key,
 *       key size, and seed while the key is stored in the Hashmap.
 */
typedef uint64_t (*arc_key_hash_fun_t)(const void* key, size_t key_size, uint64_t seed);

/**
 * @brief Hashmap configuration descriptor used to create a new Hashmap.
 *
 * The descriptor defines the initial storage capacity, element size,
 * hashing and equality functions, capacity growth strategy, load factor,
 * and allocators.
 *
 * @attention A valid descriptor must satisfy the following:
 *            - 'element_size' must be greater than zero;
 *            - 'base_capacity' must be greater than zero;
 *            - 'growth_factor' must contain a valid arc_map_growth_factor_t value;
 *            - 'load_factor' must contain a valid arc_map_load_factor_t value;
 *            - 'key_equals' must not be NULL;
 *            - 'key_hash' must not be NULL;
 *            - 'object_allocator' must not be NULL and must be valid;
 *            - 'storage_allocator' must not be NULL and must be valid;
 *            - 'object_allocator' must support allocation and individual release;
 *            - 'storage_allocator' must support the memory operations required
 *              by the Hashmap implementation;
 *            - both allocators may refer to the same Allocator;
 *            - both allocator objects must remain valid for the entire
 *              lifetime of the Hashmap.
 *
 * @note The descriptor itself is only used during Hashmap creation and
 *       does not need to outlive the created Hashmap.
 *
 * @note The hash seed may contain any uint64_t value, including zero.
 *
 * @note The hashing and equality functions must remain valid throughout
 *       the lifetime of the Hashmap and must produce consistent results
 *       for equivalent keys.
 *
 * @attention Invalid descriptors and other detected programmer contract
 *            violations call arc_abort() rather than returning ordinary
 *            failure Results, according to Arc Core's API conventions.
 */
struct arc_map_desc_t {
    /**
     * @brief Size in bytes of each stored element.
     *
     * @note Each supplied element is copied into Hashmap-owned storage.
     */
    size_t element_size;

    /**
     * @brief Initial storage capacity of the Hashmap.
     *
     * The capacity unit and any minimum or rounding requirements must be
     * defined by the Hashmap implementation.
     */
    size_t base_capacity;

    /**
     * @brief Seed passed to the key-hashing callback.
     *
     * A fixed seed allows a hash callback to return reproducible hashes
     * for identical key bytes and key sizes.
     */
    uint64_t seed;

    /** @brief Configured threshold used to decide when to expand storage. */
    arc_map_load_factor_t load_factor;

    /** @brief Capacity growth strategy used when expansion is required. */
    arc_map_growth_factor_t growth_factor;

    /**
     * @brief Callback used to determine whether two keys are equal.
     *
     * @attention Keys considered equal must hash identically under key_hash.
     */
    arc_key_equals_fun_t key_equals;

    /** @brief Callback used to compute a hash for a key. */
    arc_key_hash_fun_t key_hash;

    /**
     * @brief Allocator used for the Hashmap object itself.
     *
     * @note The Hashmap does not own this allocator object.
     */
    arc_allocator_t* object_allocator;

    /**
     * @brief Allocator used for the Hashmap's internal storage.
     *
     * This allocator is used for the bucket array, entries, and the owned
     * copies of keys and values.
     *
     * @note The Hashmap does not own this allocator object.
     */
    arc_allocator_t* storage_allocator;
};

/**
 * @brief Creates a fixed-capacity Hashmap descriptor from an element type.
 *
 * The descriptor uses separate allocators for the Hashmap object and its
 * internal storage. Keys and values inserted into the Hashmap are copied into
 * storage allocated by the internal storage allocator.
 *
 * @param type Type of the elements stored in the Hashmap.
 * @param capacity Initial Hashmap capacity.
 * @param _seed Seed used by the hashing function.
 * @param equalsfn Function used to compare keys.
 * @param hashfn Function used to hash keys.
 * @param obj_allocator_ptr Allocator used for the Hashmap object.
 * @param entry_allocator_ptr Allocator used for internal storage.
 *
 * @note The growth and load factors are both set to NONE.
 */
#define ARC_STATIC_MAP_DESC_FROM_TYPE_EX(type, capacity, _seed, equalsfn, hashfn, obj_allocator_ptr,                   \
                                         entry_allocator_ptr)                                                          \
    (arc_map_desc_t) {                                                                                                 \
        .element_size = sizeof(type), .base_capacity = (capacity), .seed = (_seed),                                    \
        .growth_factor = ARC_MAP_GROWTH_NONE, .load_factor = ARC_MAP_LOAD_FACTOR_NONE, .key_equals = (equalsfn),       \
        .key_hash = (hashfn), .object_allocator = (obj_allocator_ptr), .storage_allocator = (entry_allocator_ptr),     \
    }

/**
 * @brief Creates a dynamic Hashmap descriptor from an element type.
 *
 * Allows explicit configuration of growth and load factors, as well as
 * separate allocators for object and storage allocations. Keys and values
 * inserted into the Hashmap are copied into storage allocated by the internal
 * storage allocator.
 *
 * @param type Type of the elements stored in the Hashmap.
 * @param capacity Initial Hashmap capacity.
 * @param _seed Seed used by the hashing function.
 * @param equalsfn Function used to compare keys.
 * @param hashfn Function used to hash keys.
 * @param loadft Maximum load factor before automatic expansion.
 * @param growft Capacity growth factor.
 * @param obj_allocator_ptr Allocator used for the Hashmap object.
 * @param entry_allocator_ptr Allocator used for internal storage.
 */
#define ARC_DYNAMIC_MAP_DESC_FROM_TYPE_EX(type, capacity, _seed, equalsfn, hashfn, loadft, growft, obj_allocator_ptr,  \
                                          entry_allocator_ptr)                                                         \
    (arc_map_desc_t) {                                                                                                 \
        .element_size = sizeof(type), .base_capacity = (capacity), .seed = (_seed), .growth_factor = (growft),         \
        .load_factor = (loadft), .key_equals = (equalsfn), .key_hash = (hashfn),                                       \
        .object_allocator = (obj_allocator_ptr), .storage_allocator = (entry_allocator_ptr),                           \
    }

/**
 * @brief Creates a dynamic Hashmap descriptor using one allocator.
 *
 * Uses the same allocator for the Hashmap object and internal storage. Keys
 * and values inserted into the Hashmap are copied into owned storage.
 *
 * Uses the following default configuration:
 * - Load factor: ARC_MAP_LOAD_FACTOR_1_5X.
 * - Growth factor: ARC_MAP_GROWTH_2X.
 *
 * @param type Type of the elements stored in the Hashmap.
 * @param capacity Initial Hashmap capacity.
 * @param _seed Seed used by the hashing function.
 * @param equalsfn Function used to compare keys.
 * @param hashfn Function used to hash keys.
 * @param allocator_ptr Allocator used for both object and storage allocations.
 */
#define ARC_DYNAMIC_MAP_DESC_FROM_TYPE(type, capacity, _seed, equalsfn, hashfn, allocator_ptr)                         \
    ARC_DYNAMIC_MAP_DESC_FROM_TYPE_EX(type, capacity, _seed, equalsfn, hashfn, ARC_MAP_LOAD_FACTOR_1_5X,               \
                                      ARC_MAP_GROWTH_2X, allocator_ptr, allocator_ptr)

/**
 * @brief Creates a fixed-capacity Hashmap descriptor using one allocator.
 *
 * Both object and internal storage allocations use the supplied Allocator.
 * Keys and values inserted into the Hashmap are copied into owned storage.
 *
 * @param type Type of the elements stored in the Hashmap.
 * @param capacity Initial Hashmap capacity.
 * @param _seed Seed used by the hashing function.
 * @param equalsfn Function used to compare keys.
 * @param hashfn Function used to hash keys.
 * @param allocator_ptr Allocator used for both object and storage allocations.
 */
#define ARC_STATIC_MAP_DESC_FROM_TYPE(type, capacity, _seed, equalsfn, hashfn, allocator_ptr)                          \
    ARC_STATIC_MAP_DESC_FROM_TYPE_EX(type, capacity, _seed, equalsfn, hashfn, allocator_ptr, allocator_ptr)

/**
 * @brief Creates an empty Hashmap using the supplied descriptor.
 *
 * @param[out] out Receives the newly created Hashmap.
 * @param[in] desc Hashmap configuration descriptor.
 *
 * @return Success or the underlying allocation failure.
 * @post On success, '*out' points to an empty Hashmap.
 * @post On failure, '*out' is NULL when 'out' is valid.
 * @attention 'out' and 'desc' must be valid; invalid arguments panic.
 * @note The Hashmap stores allocator pointers but does not own the allocators.
 */
arc_result_t arc_map_new(arc_map_t** out, const arc_map_desc_t* desc);

/**
 * @brief Destroys a Hashmap and releases its owned keys, values, entries,
 *        buckets, and object storage.
 *
 * @param[in, out] map Hashmap to destroy; may be NULL.
 *
 * @return Success or an underlying release failure.
 *
 * @note Passing NULL is a successful no-op.
 *
 * @attention All previously borrowed value pointers become invalid after
 *            successful destruction.
 */
arc_result_t arc_map_destroy(arc_map_t* map);

/**
 * @brief Checks whether a Hashmap is initialized and internally valid.
 *
 * @param[in] map Hashmap to inspect.
 *
 * @return true if the Hashmap is valid; false otherwise.
 *
 * @note An arbitrary dangling pointer cannot be validated safely.
 */
bool arc_map_is_valid(const arc_map_t* map);

/**
 * @brief Finds the value associated with a key.
 *
 * @param[in] map Hashmap to inspect.
 * @param[out] out Receives a read-only borrowed pointer to the Hashmap-owned
 *                 value copy.
 * @param[in] key Pointer to the key bytes to look up.
 * @param[in] key_size Number of bytes in the key.
 *
 * @return Success, ARC_MAP_ERROR_KEY_NOT_FOUND, or ARC_MAP_ERROR_INVALID_MAP.
 *
 * @post On failure, '*out' is NULL when the output argument is valid.
 *
 * @note The returned pointer refers to Hashmap-owned storage. No contiguous
 *       storage or iteration order is implied by this pointer.
 *
 * @attention A later removal, replacement, rehash, clear, or destruction may
 *            invalidate the pointer.
 */
arc_result_t arc_map_get(const arc_map_t* map, const void** out, const void* key, size_t key_size);

/**
 * @brief Finds a value and obtains mutable access to it.
 *
 * @param[in, out] map Hashmap to inspect.
 * @param[out] out Receives a mutable borrowed pointer to the Hashmap-owned
 *                 value copy.
 * @param[in] key Pointer to the key bytes to look up.
 * @param[in] key_size Number of bytes in the key.
 *
 * @return Success, ARC_MAP_ERROR_KEY_NOT_FOUND, or ARC_MAP_ERROR_INVALID_MAP.
 *
 * @post On failure, '*out' is NULL when the output argument is valid.
 *
 * @note Modifying value bytes does not change the associated key.
 *
 * @attention The pointer must not be used after its entry is removed or
 *            invalidated.
 */
arc_result_t arc_map_get_mut(arc_map_t* map, void** out, const void* key, size_t key_size);

/**
 * @brief Sets the value associated with a key.
 *
 * The supplied key and element bytes are copied into Hashmap-owned storage
 * when a new association is created or an existing value is replaced.
 *
 * @param[in, out] map Hashmap to modify.
 * @param[in] key Pointer to the key bytes to copy into Hashmap-owned storage.
 * @param[in] key_size Number of key bytes to copy.
 * @param[in] element Pointer to at least 'element_size' value bytes to copy.
 *
 * @return Success or an applicable lookup, capacity, or allocator failure.
 *
 * @note Whether a missing key is inserted or produces KEY_NOT_FOUND must be
 *       specified by the implementation; this declaration does not decide it.
 */
arc_result_t arc_map_set(arc_map_t* map, const void* key, size_t key_size, const void* element);

/**
 * @brief Gets the fixed size in bytes of each stored value.
 *
 * @param[in] map Hashmap to inspect.
 * @param[out] out Receives the value size in bytes, including for an empty map.
 *
 * @return Success or ARC_MAP_ERROR_INVALID_MAP.
 *
 * @post On failure, '*out' is zero when the output argument is valid.
 */
arc_result_t arc_map_get_element_size(const arc_map_t* map, size_t* out);

/**
 * @brief Gets the number of key/value associations in the Hashmap.
 *
 * @param[in] map Hashmap to inspect.
 * @param[out] out Receives the number of stored entries.
 *
 * @return Success or ARC_MAP_ERROR_INVALID_MAP.
 *
 * @post On failure, '*out' is zero when the output argument is valid.
 */
arc_result_t arc_map_get_size(const arc_map_t* map, size_t* out);

/**
 * @brief Gets the current storage capacity of the Hashmap.
 *
 * @param[in] map Hashmap to inspect.
 * @param[out] out Receives the current capacity.
 *
 * @return Success or ARC_MAP_ERROR_INVALID_MAP.
 *
 * @post On failure, '*out' is zero when the output argument is valid.
 *
 * @note The unit of capacity follows the implementation's bucket/storage model;
 *       capacity is not necessarily the maximum number of entries.
 */
arc_result_t arc_map_get_capacity(const arc_map_t* map, size_t* out);

/**
 * @brief Checks whether a key exists in the Hashmap.
 *
 * @param[in] map Hashmap to inspect.
 * @param[in] key Pointer to the key bytes to look up.
 * @param[in] key_size Number of bytes in the key.
 * @param[out] out Receives true if a matching key exists; false otherwise.
 *
 * @return Success or ARC_MAP_ERROR_INVALID_MAP.
 *
 * @post On failure, '*out' is false when the output argument is valid.
 *
 * @note A missing key is reported through '*out', not KEY_NOT_FOUND.
 */
arc_result_t arc_map_contains(const arc_map_t* map, const void* key, size_t key_size, bool* out);

/**
 * @brief Determines whether the Hashmap supports automatic capacity growth.
 *
 * @param[in] map Hashmap to inspect.
 * @param[out] out Receives true when the growth factor is not ARC_MAP_GROWTH_NONE.
 *
 * @return Success or ARC_MAP_ERROR_INVALID_MAP.
 *
 * @post On failure, '*out' is false when the output argument is valid.
 */
arc_result_t arc_map_is_dynamic(const arc_map_t* map, bool* out);

/**
 * @brief Checks whether the Hashmap contains no entries.
 *
 * @param[in] map Hashmap to inspect.
 * @param[out] out Receives true exactly when the entry count is zero.
 *
 * @return Success or ARC_MAP_ERROR_INVALID_MAP.
 *
 * @post On failure, '*out' is false when the output argument is valid.
 */
arc_result_t arc_map_is_empty(const arc_map_t* map, bool* out);

/**
 * @brief Removes all key/value associations from the Hashmap and releases
 *        their owned keys, values, and entries.
 *
 * @param[in, out] map Hashmap to clear.
 *
 * @return Success or an underlying storage-release failure.
 *
 * @post On success, the Hashmap contains no entries.
 *
 * @attention All previously borrowed value pointers become invalid on success.
 *
 * @note Whether internal bucket capacity is retained depends on the
 *       implementation.
 */
arc_result_t arc_map_clear(arc_map_t* map);

/**
 * @brief Inserts a key/value association into the Hashmap.
 *
 * The supplied key and element bytes are copied into Hashmap-owned storage.
 * The caller retains ownership of the input memory and may modify or release
 * it after this operation returns.
 *
 * @param[in, out] map Hashmap to modify.
 * @param[in] key Pointer to the key bytes to copy into Hashmap-owned storage.
 * @param[in] key_size Number of key bytes to copy.
 * @param[in] element Pointer to at least 'element_size' value bytes to copy.
 *
 * @return Success or an applicable capacity, allocation, or insertion failure.
 *
 * @note The behavior when an equivalent key already exists must be defined
 *       by the implementation; this declaration does not specify replacement.
 *
 * @attention Automatic expansion may invalidate previously borrowed value
 *            pointers.
 */
arc_result_t arc_map_insert(arc_map_t* map, const void* key, size_t key_size, const void* element);

/**
 * @brief Removes the key/value association identified by a key and releases
 *        its Hashmap-owned key and value copies.
 *
 * @param[in, out] map Hashmap to modify.
 * @param[in] key Pointer to the key bytes used to identify the entry.
 * @param[in] key_size Number of bytes in the key.
 *
 * @return Success, ARC_MAP_ERROR_KEY_NOT_FOUND, or an applicable release failure.
 *
 * @post On success, the requested key no longer exists in the Hashmap.
 *
 * @attention Borrowed pointers to the removed value become invalid on success.
 */
arc_result_t arc_map_remove(arc_map_t* map, const void* key, size_t key_size);

#ifdef __cplusplus
}
#endif

#endif /* ARC_HASHMAP_H_ */
