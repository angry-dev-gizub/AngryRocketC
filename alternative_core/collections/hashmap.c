/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "allocator/allocator.h"
#include "hashmap.h"
#include "_hashmap_def.h"

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static arc_result_t _arc_map_error(uint32_t error) {
    return (arc_result_t){
        .ok    = false,
        .error = error,
    };
}

static bool _arc_map_desc_is_valid(const arc_map_desc_t* desc) {
    if(!desc)
        return false;

    if(desc->element_size == 0 || desc->base_capacity == 0)
        return false;

    if(!desc->key_equals || !desc->key_hash)
        return false;

    if(!desc->object_allocator || !desc->storage_allocator)
        return false;

    if(!arc_allocator_is_valid(desc->object_allocator))
        return false;

    if(!arc_allocator_is_valid(desc->storage_allocator))
        return false;

    switch(desc->growth_factor) {
        case ARC_MAP_GROWTH_NONE:
        case ARC_MAP_GROWTH_1_125X:
        case ARC_MAP_GROWTH_1_5X:
        case ARC_MAP_GROWTH_2X:
            break;

        default:
            return false;
    }

    switch(desc->load_factor) {
        case ARC_MAP_LOAD_FACTOR_NONE:
        case ARC_MAP_LOAD_FACTOR_1X:
        case ARC_MAP_LOAD_FACTOR_1_5X:
        case ARC_MAP_LOAD_FACTOR_1_8X:
        case ARC_MAP_LOAD_FACTOR_2X:
            break;

        default:
            return false;
    }

    if(desc->base_capacity > SIZE_MAX / sizeof(_arc_map_entry_t*))
        return false;

    return true;
}

static _arc_map_entry_t* _arc_map_find_entry(const arc_map_t* map, const void* key, size_t key_size, uint64_t hash) {
    const size_t index = (size_t)(hash % map->capacity);

    for(_arc_map_entry_t* entry = map->buckets[index]; entry; entry = entry->next) {
        if(entry->hash != hash)
            continue;

        if(entry->key_size != key_size)
            continue;

        if(!map->key_equals(entry->key, key, key_size))
            continue;

        return entry;
    }

    return NULL;
}

static size_t _arc_map_load_threshold_denominator(arc_map_load_factor_t factor) {
    switch(factor) {
        case ARC_MAP_LOAD_FACTOR_1X:
            return 1;

        case ARC_MAP_LOAD_FACTOR_1_5X:
            return 2;

        case ARC_MAP_LOAD_FACTOR_1_8X:
            return 5;

        case ARC_MAP_LOAD_FACTOR_2X:
            return 1;

        case ARC_MAP_LOAD_FACTOR_NONE:
        default:
            return 1;
    }
}

static size_t _arc_map_load_threshold_numerator(arc_map_load_factor_t factor) {
    switch(factor) {
        case ARC_MAP_LOAD_FACTOR_1X:
            return 1;

        case ARC_MAP_LOAD_FACTOR_1_5X:
            return 3;

        case ARC_MAP_LOAD_FACTOR_1_8X:
            return 9;

        case ARC_MAP_LOAD_FACTOR_2X:
            return 2;

        case ARC_MAP_LOAD_FACTOR_NONE:
        default:
            return 0;
    }
}

static bool _arc_map_load_exceeded(size_t entries, size_t capacity, arc_map_load_factor_t factor) {
    const size_t numerator   = _arc_map_load_threshold_numerator(factor);
    const size_t denominator = _arc_map_load_threshold_denominator(factor);

    if(numerator == 0)
        return false;

    /*
     * Use long double to avoid integer overflow when calculating
     * capacity * numerator.
     */
    return (long double)entries > ((long double)capacity * (long double)numerator) / (long double)denominator;
}

static arc_result_t _arc_map_next_capacity(const arc_map_t* map, size_t* out) {
    if(!out)
        arc_abort("Arc hashmap: NULL capacity output");

    *out = 0;

    size_t capacity = map->capacity;

    switch(map->growth_factor) {
        case ARC_MAP_GROWTH_1_125X: {
            /* ceil(capacity * 9 / 8), with overflow protection. */
            if(capacity > (SIZE_MAX - 7) / 9)
                return _arc_map_error(ARC_MAP_ERROR_OUT_OF_SPACE);

            capacity = (capacity * 9 + 7) / 8;
            break;
        }

        case ARC_MAP_GROWTH_1_5X: {
            /* ceil(capacity * 3 / 2), with overflow protection. */
            if(capacity > (SIZE_MAX - 1) / 3)
                return _arc_map_error(ARC_MAP_ERROR_OUT_OF_SPACE);

            capacity = (capacity * 3 + 1) / 2;
            break;
        }

        case ARC_MAP_GROWTH_2X:
            if(capacity > SIZE_MAX / 2)
                return _arc_map_error(ARC_MAP_ERROR_OUT_OF_SPACE);

            capacity *= 2;
            break;

        case ARC_MAP_GROWTH_NONE:
        default:
            return _arc_map_error(ARC_MAP_ERROR_OUT_OF_SPACE);
    }

    if(capacity <= map->capacity) {
        if(map->capacity == SIZE_MAX)
            return _arc_map_error(ARC_MAP_ERROR_OUT_OF_SPACE);

        capacity = map->capacity + 1;
    }

    if(capacity > SIZE_MAX / sizeof(_arc_map_entry_t*))
        return _arc_map_error(ARC_MAP_ERROR_OUT_OF_SPACE);

    *out = capacity;
    return arc_result_success();
}

/*
 * Rehashing only changes entry links and bucket heads. Keys, values, and
 * entry allocations remain where they are, so their addresses stay stable.
 */
static arc_result_t _arc_map_rehash(arc_map_t* map, size_t new_capacity) {
    if(new_capacity == 0 || new_capacity > SIZE_MAX / sizeof(*map->buckets)) {
        return _arc_map_error(ARC_MAP_ERROR_OUT_OF_SPACE);
    }

    _arc_map_entry_t** new_buckets = NULL;

    arc_result_t r =
        arc_allocator_alloc(map->storage_allocator, new_capacity * sizeof(*new_buckets), (void**)&new_buckets);

    if(!r.ok)
        return r;

    memset(new_buckets, 0, new_capacity * sizeof(*new_buckets));

    /*
     * All potentially failing allocations happen before relinking entries.
     */
    _arc_map_entry_t** old_buckets = map->buckets;
    const size_t old_capacity      = map->capacity;

    for(size_t i = 0; i < old_capacity; ++i) {
        _arc_map_entry_t* entry = old_buckets[i];

        while(entry) {
            _arc_map_entry_t* next = entry->next;
            const size_t index     = (size_t)(entry->hash % new_capacity);

            entry->prev = NULL;
            entry->next = new_buckets[index];

            if(entry->next)
                entry->next->prev = entry;

            new_buckets[index] = entry;
            entry              = next;
        }
    }

    map->buckets  = new_buckets;
    map->capacity = new_capacity;

    /*
     * The map is valid after the swap. If freeing the old bucket array fails,
     * return the allocator error; the new array remains installed.
     */
    return arc_allocator_free(map->storage_allocator, old_buckets);
}

static arc_result_t _arc_map_prepare_insert(arc_map_t* map) {
    if(map->size == SIZE_MAX)
        return _arc_map_error(ARC_MAP_ERROR_OUT_OF_SPACE);

    if(map->growth_factor == ARC_MAP_GROWTH_NONE || map->load_factor == ARC_MAP_LOAD_FACTOR_NONE) {
        return arc_result_success();
    }

    const size_t prospective_size = map->size + 1;

    if(!_arc_map_load_exceeded(prospective_size, map->capacity, map->load_factor)) {
        return arc_result_success();
    }

    size_t new_capacity = 0;

    arc_result_t r = _arc_map_next_capacity(map, &new_capacity);

    if(!r.ok)
        return r;

    return _arc_map_rehash(map, new_capacity);
}

/*
 * Releases an entry's owned key and value. A failed release leaves the
 * corresponding pointer intact so the caller can retry.
 */
static arc_result_t _arc_map_release_entry_data(arc_map_t* map, _arc_map_entry_t* entry) {
    if(entry->key) {
        arc_result_t r = arc_allocator_free(map->storage_allocator, entry->key);

        if(!r.ok)
            return r;

        entry->key = NULL;
    }

    if(entry->data) {
        arc_result_t r = arc_allocator_free(map->storage_allocator, entry->data);

        if(!r.ok)
            return r;

        entry->data = NULL;
    }

    return arc_result_success();
}

/* --------------------------------------------------------------------------
 * Creation and destruction
 * -------------------------------------------------------------------------- */

arc_result_t arc_map_new(arc_map_t** out, const arc_map_desc_t* desc) {
    if(!out)
        arc_abort("Arc hashmap: NULL output pointer");

    *out = NULL;

    if(!_arc_map_desc_is_valid(desc))
        arc_abort("Arc hashmap: invalid descriptor");

    const size_t buckets_size = desc->base_capacity * sizeof(_arc_map_entry_t*);

    _arc_map_entry_t** buckets = NULL;

    ARC_TRY(arc_allocator_alloc(desc->storage_allocator, buckets_size, (void**)&buckets));

    memset(buckets, 0, buckets_size);

    arc_map_t* map = NULL;

    arc_result_t r = arc_allocator_alloc(desc->object_allocator, sizeof(*map), (void**)&map);

    if(!r.ok) {
        arc_result_t cleanup = arc_allocator_free(desc->storage_allocator, buckets);

        if(!cleanup.ok)
            return cleanup;

        return r;
    }

    *map = (arc_map_t){
        .buckets           = buckets,
        .size              = 0,
        .element_size      = desc->element_size,
        .capacity          = desc->base_capacity,
        .seed              = desc->seed,
        .load_factor       = desc->load_factor,
        .growth_factor     = desc->growth_factor,
        .key_equals        = desc->key_equals,
        .key_hash          = desc->key_hash,
        .object_allocator  = desc->object_allocator,
        .storage_allocator = desc->storage_allocator,
    };

    *out = map;
    return arc_result_success();
}

arc_result_t arc_map_clear(arc_map_t* map) {
    if(!map)
        arc_abort("Arc hashmap: NULL hashmap");

    if(!map->buckets || map->capacity == 0)
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    for(size_t i = 0; i < map->capacity; ++i) {
        while(map->buckets[i]) {
            _arc_map_entry_t* entry = map->buckets[i];

            arc_result_t r = _arc_map_release_entry_data(map, entry);

            if(!r.ok)
                return r;

            _arc_map_entry_t* next = entry->next;

            map->buckets[i] = next;

            if(next)
                next->prev = NULL;

            r = arc_allocator_free(map->storage_allocator, entry);

            if(!r.ok) {
                map->buckets[i] = entry;

                if(next)
                    next->prev = entry;

                return r;
            }

            --map->size;
        }
    }

    map->size = 0;
    return arc_result_success();
}

arc_result_t arc_map_destroy(arc_map_t* map) {
    if(!map)
        return arc_result_success();

    if(map->buckets) {
        arc_result_t r = arc_map_clear(map);

        if(!r.ok)
            return r;

        r = arc_allocator_free(map->storage_allocator, map->buckets);

        if(!r.ok)
            return r;

        map->buckets  = NULL;
        map->capacity = 0;
    }

    return arc_allocator_free(map->object_allocator, map);
}

/* --------------------------------------------------------------------------
 * Validation and lookup
 * -------------------------------------------------------------------------- */

bool arc_map_is_valid(const arc_map_t* map) {
    if(!map)
        return false;

    if(!map->buckets || map->capacity == 0)
        return false;

    if(map->element_size == 0)
        return false;

    if(!map->key_equals || !map->key_hash)
        return false;

    if(!map->object_allocator || !map->storage_allocator)
        return false;

    if(!arc_allocator_is_valid(map->object_allocator) || !arc_allocator_is_valid(map->storage_allocator)) {
        return false;
    }

    switch(map->growth_factor) {
        case ARC_MAP_GROWTH_NONE:
        case ARC_MAP_GROWTH_1_125X:
        case ARC_MAP_GROWTH_1_5X:
        case ARC_MAP_GROWTH_2X:
            break;

        default:
            return false;
    }

    switch(map->load_factor) {
        case ARC_MAP_LOAD_FACTOR_NONE:
        case ARC_MAP_LOAD_FACTOR_1X:
        case ARC_MAP_LOAD_FACTOR_1_5X:
        case ARC_MAP_LOAD_FACTOR_1_8X:
        case ARC_MAP_LOAD_FACTOR_2X:
            break;

        default:
            return false;
    }

    return true;
}

arc_result_t arc_map_get(const arc_map_t* map, const void** out, const void* key, size_t key_size) {
    if(!out)
        arc_abort("Arc hashmap: NULL output argument");

    *out = NULL;

    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    if(!key || key_size == 0)
        arc_abort("Arc hashmap: invalid key");

    const uint64_t hash = map->key_hash(key, key_size, map->seed);

    _arc_map_entry_t* entry = _arc_map_find_entry(map, key, key_size, hash);

    if(!entry)
        return _arc_map_error(ARC_MAP_ERROR_KEY_NOT_FOUND);

    *out = entry->data;
    return arc_result_success();
}

arc_result_t arc_map_get_mut(arc_map_t* map, void** out, const void* key, size_t key_size) {
    if(!out)
        arc_abort("Arc hashmap: NULL output argument");

    *out = NULL;

    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    if(!key || key_size == 0)
        arc_abort("Arc hashmap: invalid key");

    const uint64_t hash = map->key_hash(key, key_size, map->seed);

    _arc_map_entry_t* entry = _arc_map_find_entry(map, key, key_size, hash);

    if(!entry)
        return _arc_map_error(ARC_MAP_ERROR_KEY_NOT_FOUND);

    *out = entry->data;
    return arc_result_success();
}

/* --------------------------------------------------------------------------
 * Set and insert
 * -------------------------------------------------------------------------- */

arc_result_t arc_map_set(arc_map_t* map, const void* key, size_t key_size, const void* element) {
    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    if(!key || key_size == 0 || !element)
        arc_abort("Arc hashmap: invalid key or element");

    const uint64_t hash = map->key_hash(key, key_size, map->seed);

    _arc_map_entry_t* existing = _arc_map_find_entry(map, key, key_size, hash);

    if(existing) {
        /*
         * Allocate and copy the replacement before releasing the old value.
         */
        void* replacement = NULL;

        ARC_TRY(arc_allocator_alloc(map->storage_allocator, map->element_size, &replacement));

        memcpy(replacement, element, map->element_size);

        arc_result_t r = arc_allocator_free(map->storage_allocator, existing->data);

        if(!r.ok) {
            arc_allocator_free(map->storage_allocator, replacement);
            return r;
        }

        existing->data = replacement;
        return arc_result_success();
    }

    ARC_TRY(_arc_map_prepare_insert(map));

    /*
     * Recompute the index after potential rehashing.
     */
    const size_t index = (size_t)(hash % map->capacity);

    _arc_map_entry_t* entry = NULL;
    void* key_copy          = NULL;
    void* data_copy         = NULL;

    ARC_TRY(arc_allocator_alloc(map->storage_allocator, sizeof(*entry), (void**)&entry));

    arc_result_t r = arc_allocator_alloc(map->storage_allocator, key_size, &key_copy);

    if(!r.ok) {
        arc_allocator_free(map->storage_allocator, entry);
        return r;
    }

    r = arc_allocator_alloc(map->storage_allocator, map->element_size, &data_copy);

    if(!r.ok) {
        arc_allocator_free(map->storage_allocator, key_copy);
        arc_allocator_free(map->storage_allocator, entry);
        return r;
    }

    memcpy(key_copy, key, key_size);
    memcpy(data_copy, element, map->element_size);

    *entry = (_arc_map_entry_t){
        .next     = map->buckets[index],
        .prev     = NULL,
        .hash     = hash,
        .key      = key_copy,
        .key_size = key_size,
        .data     = data_copy,
    };

    if(entry->next)
        entry->next->prev = entry;

    map->buckets[index] = entry;
    ++map->size;

    return arc_result_success();
}

arc_result_t arc_map_insert(arc_map_t* map, const void* key, size_t key_size, const void* element) {
    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    if(!key || key_size == 0 || !element)
        arc_abort("Arc hashmap: invalid key or element");

    const uint64_t hash = map->key_hash(key, key_size, map->seed);

    /*
     * Insert does not replace an existing association. Because the public
     * error definitions contain no duplicate-key error, an existing key is
     * treated as an unchanged successful operation.
     */
    if(_arc_map_find_entry(map, key, key_size, hash))
        return arc_result_success();

    return arc_map_set(map, key, key_size, element);
}

/* --------------------------------------------------------------------------
 * Metadata accessors
 * -------------------------------------------------------------------------- */

arc_result_t arc_map_get_element_size(const arc_map_t* map, size_t* out) {
    if(!out)
        arc_abort("Arc hashmap: NULL output argument");

    *out = 0;

    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    *out = map->element_size;
    return arc_result_success();
}

arc_result_t arc_map_get_size(const arc_map_t* map, size_t* out) {
    if(!out)
        arc_abort("Arc hashmap: NULL output argument");

    *out = 0;

    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    *out = map->size;
    return arc_result_success();
}

arc_result_t arc_map_get_capacity(const arc_map_t* map, size_t* out) {
    if(!out)
        arc_abort("Arc hashmap: NULL output argument");

    *out = 0;

    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    *out = map->capacity;
    return arc_result_success();
}

arc_result_t arc_map_contains(const arc_map_t* map, const void* key, size_t key_size, bool* out) {
    if(!out)
        arc_abort("Arc hashmap: NULL output argument");

    *out = false;

    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    if(!key || key_size == 0)
        arc_abort("Arc hashmap: invalid key");

    const uint64_t hash = map->key_hash(key, key_size, map->seed);

    *out = _arc_map_find_entry(map, key, key_size, hash) != NULL;

    return arc_result_success();
}

arc_result_t arc_map_is_dynamic(const arc_map_t* map, bool* out) {
    if(!out)
        arc_abort("Arc hashmap: NULL output argument");

    *out = false;

    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    *out = map->growth_factor != ARC_MAP_GROWTH_NONE && map->load_factor != ARC_MAP_LOAD_FACTOR_NONE;

    return arc_result_success();
}

arc_result_t arc_map_is_empty(const arc_map_t* map, bool* out) {
    if(!out)
        arc_abort("Arc hashmap: NULL output argument");

    *out = false;

    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    *out = map->size == 0;
    return arc_result_success();
}

/* --------------------------------------------------------------------------
 * Removal
 * -------------------------------------------------------------------------- */

arc_result_t arc_map_remove(arc_map_t* map, const void* key, size_t key_size) {
    if(!arc_map_is_valid(map))
        return _arc_map_error(ARC_MAP_ERROR_INVALID_MAP);

    if(!key || key_size == 0)
        arc_abort("Arc hashmap: invalid key");

    const uint64_t hash = map->key_hash(key, key_size, map->seed);

    _arc_map_entry_t* entry = _arc_map_find_entry(map, key, key_size, hash);

    if(!entry)
        return _arc_map_error(ARC_MAP_ERROR_KEY_NOT_FOUND);

    /*
     * Release key and value first. If either release fails, the entry remains
     * linked and can be retried, although a successfully freed field is NULL.
     */
    arc_result_t r = _arc_map_release_entry_data(map, entry);

    if(!r.ok)
        return r;

    if(entry->prev)
        entry->prev->next = entry->next;
    else
        map->buckets[hash % map->capacity] = entry->next;

    if(entry->next)
        entry->next->prev = entry->prev;

    r = arc_allocator_free(map->storage_allocator, entry);

    if(!r.ok) {
        /*
         * Restore the chain if the allocator guarantees a failed free leaves
         * the entry allocation intact.
         */
        if(entry->prev)
            entry->prev->next = entry;
        else
            map->buckets[hash % map->capacity] = entry;

        if(entry->next)
            entry->next->prev = entry;

        return r;
    }

    --map->size;
    return arc_result_success();
}
