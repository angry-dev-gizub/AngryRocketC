
/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "allocator/allocator.h"
#include "string.h"
#include "_string_def.h"
#include "stringview.h"

/*
 * Hash algorithm:
 * FNV-1a, 64-bit.
 *
 * Hashes the string's content bytes, excluding the terminating NUL.
 */
#define ARC_STRING_FNV_OFFSET_BASIS UINT64_C(14695981039346656037)
#define ARC_STRING_FNV_PRIME UINT64_C(1099511628211)

static char* arc_string_data(arc_string_t* str) {
    return str->uses_sso ? str->as.sso : str->as.raw_data;
}

static const char* arc_string_data_const(const arc_string_t* str) {
    return str->uses_sso ? str->as.sso : str->as.raw_data;
}

static uint64_t arc_string_hash_bytes(const char* data, size_t size) {
    uint64_t hash = ARC_STRING_FNV_OFFSET_BASIS;

    for(size_t i = 0U; i < size; ++i) {
        hash ^= (unsigned char)data[i];
        hash *= ARC_STRING_FNV_PRIME;
    }

    return hash;
}

static bool arc_string_desc_is_valid(const arc_string_desc_t* desc) {
    if(desc == NULL)
        return false;

    if(desc->object_allocator == NULL || desc->storage_allocator == NULL)
        return false;

    if(!arc_allocator_is_valid(desc->object_allocator))
        return false;

    if(!arc_allocator_is_valid(desc->storage_allocator))
        return false;

    return true;
}

/*
 * Construction
 */

arc_result_t arc_string_new(arc_string_t** out, const arc_string_desc_t* desc) {
    if(out == NULL)
        arc_abort("Arc string: output parameter is NULL");

    *out = NULL;

    if(!arc_string_desc_is_valid(desc))
        arc_abort("Arc string: invalid descriptor");

    arc_string_t* str = NULL;

    ARC_TRY(arc_allocator_alloc(desc->object_allocator, sizeof(*str), (void**)&str));

    *str = (arc_string_t){
        .uses_sso          = true,
        .size              = 0U,
        .capacity          = ARC_STRING_SSO_CAPACITY,
        .object_allocator  = desc->object_allocator,
        .storage_allocator = desc->storage_allocator,
        .as.sso            = {'\0'},
    };

    *out = str;

    return arc_result_success();
}

arc_result_t arc_string_from_cstr(const char* cstr, arc_string_t** out, const arc_string_desc_t* desc) {
    if(cstr == NULL)
        arc_abort("Arc string: input C string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output parameter is NULL");

    *out = NULL;

    if(!arc_string_desc_is_valid(desc))
        arc_abort("Arc string: invalid descriptor");

    return arc_string_from_bytes(cstr, strlen(cstr), out, desc);
}

arc_result_t arc_string_from_bytes(const char* data, size_t size, arc_string_t** out, const arc_string_desc_t* desc) {
    if(out == NULL)
        arc_abort("Arc string: output parameter is NULL");

    *out = NULL;

    if(!arc_string_desc_is_valid(desc))
        arc_abort("Arc string: invalid descriptor");

    /*
     * NULL is permitted for an empty byte range.
     */
    if(data == NULL && size != 0U)
        arc_abort("Arc string: input data is NULL");

    if(size != 0U && memchr(data, '\0', size) != NULL)
        arc_abort("Arc string: embedded NUL byte");

    arc_string_t* str = NULL;

    ARC_TRY(arc_string_new(&str, desc));

    if(size > ARC_STRING_SSO_CAPACITY) {
        /*
         * The allocation needs size + 1 bytes for the terminator.
         */
        if(size == SIZE_MAX) {
            arc_result_t cleanup = arc_allocator_free(str->object_allocator, str);

            if(!cleanup.ok)
                return cleanup;

            return arc_result_failure(ARC_STRING_ERROR_SIZE_OVERFLOW);
        }

        char* buffer = NULL;

        arc_result_t result = arc_allocator_alloc(str->storage_allocator, size + 1U, (void**)&buffer);

        if(!result.ok) {
            arc_result_t cleanup = arc_allocator_free(str->object_allocator, str);

            if(!cleanup.ok)
                return cleanup;

            return result;
        }

        memcpy(buffer, data, size);
        buffer[size] = '\0';

        str->uses_sso    = false;
        str->as.raw_data = buffer;
        str->capacity    = size;
        str->size        = size;
    } else {
        if(size != 0U)
            memcpy(str->as.sso, data, size);

        str->as.sso[size] = '\0';
        str->size         = size;
    }

    *out = str;

    return arc_result_success();
}

arc_result_t arc_string_format(arc_string_t** out, const arc_string_desc_t* desc, const char* format, ...) {
    if(out == NULL)
        arc_abort("Arc string: output parameter is NULL");

    *out = NULL;

    if(!arc_string_desc_is_valid(desc))
        arc_abort("Arc string: invalid descriptor");

    if(format == NULL)
        arc_abort("Arc string: format string is NULL");

    /*
     * First pass: determine the required output length.
     *
     * va_copy is required because a va_list cannot safely be reused
     * after passing it to a formatting function.
     */
    va_list args;
    va_start(args, format);

    va_list measure_args;
    va_copy(measure_args, args);

    const int required = vsnprintf(NULL, 0U, format, measure_args);

    va_end(measure_args);

    if(required < 0) {
        va_end(args);
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);
    }

    const size_t size = (size_t)required;

    /*
     * Allocate a temporary buffer for the formatted output and its
     * terminating NUL. Check the addition before performing it.
     */
    if(size == SIZE_MAX) {
        va_end(args);
        return arc_result_failure(ARC_STRING_ERROR_SIZE_OVERFLOW);
    }

    char* buffer;
    arc_result_t result = arc_allocator_alloc(desc->storage_allocator, size + 1, (void**)&buffer);

    if(!result.ok) {
        va_end(args);
        return result;
    }

    /*
     * Second pass: perform the actual formatting.
     */
    va_list format_args;
    va_copy(format_args, args);

    const int written = vsnprintf(buffer, size + 1U, format, format_args);

    va_end(format_args);
    va_end(args);

    /*
     * Reject formatting failures or an unexpected output length.
     */
    if(written < 0 || (size_t)written != size) {
        ARC_TRY(arc_allocator_free(desc->storage_allocator, buffer));
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);
    }

    /*
     * Arc strings forbid embedded NUL bytes. vsnprintf's return value
     * counts bytes that would have been written, even if an embedded
     * NUL was produced by a conversion such as %c.
     */
    if(memchr(buffer, '\0', size) != NULL) {
        ARC_TRY(arc_allocator_free(desc->storage_allocator, buffer));
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);
    }

    /*
     * Construct the Arc string using the requested allocator
     * configuration. arc_string_from_bytes copies the formatted bytes,
     * so the temporary buffer can be released afterward.
     */
    arc_result_t str_result = arc_string_from_bytes(buffer, size, out, desc);

    ARC_TRY(arc_allocator_free(desc->storage_allocator, buffer));

    return str_result;
}

/* Replaces the contents only after formatting succeeds. */
arc_result_t arc_string_set_format(arc_string_t* str, const char* format, ...) {
    if(str == NULL || format == NULL) {
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);
    }

    va_list args;
    va_start(args, format);

    va_list measure_args;
    va_copy(measure_args, args);

    const int required = vsnprintf(NULL, 0, format, measure_args);
    va_end(measure_args);

    if(required < 0) {
        va_end(args);
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);
    }

    const size_t length = (size_t)required;

    if(length == SIZE_MAX) {
        va_end(args);
        return arc_result_failure(ARC_ALLOCATOR_ERROR_SIZE_OVERFLOW);
    }

    char* buffer = malloc(length + 1U);
    if(buffer == NULL) {
        va_end(args);
        return arc_result_failure(ARC_ALLOCATOR_ERROR_OUT_OF_MEMORY);
    }

    const int written = vsnprintf(buffer, length + 1U, format, args);
    va_end(args);

    if(written < 0 || (size_t)written != length) {
        free(buffer);
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);
    }

    /*
     * Arc strings forbid embedded NUL bytes. A %c conversion can produce
     * one even though vsnprintf reports a larger output length.
     */
    if(strlen(buffer) != length) {
        free(buffer);
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);
    }

    /*
     * Reserve before clearing so allocation failure cannot destroy the
     * existing contents. Once capacity is sufficient, replacement should
     * not require another allocation.
     */
    arc_result_t result = arc_string_reserve(str, length);
    if(!result.ok) {
        free(buffer);
        return result;
    }

    result = arc_string_clear(str);
    if(result.ok && length > 0U) {
        result = arc_string_append_range(str, buffer, length);
    }

    free(buffer);
    return result;
}

arc_result_t arc_string_clone(const arc_string_t* str, arc_string_t** out) {
    if(str == NULL)
        arc_abort("Arc string: source string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output parameter is NULL");

    *out = NULL;

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: source string is invalid");

    const arc_string_desc_t desc = {
        .object_allocator  = str->object_allocator,
        .storage_allocator = str->storage_allocator,
    };

    return arc_string_from_bytes(arc_string_data_const(str), str->size, out, &desc);
}

/*
 * Destruction
 */

arc_result_t arc_string_destroy(arc_string_t* str) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    const arc_allocator_t* object_allocator  = str->object_allocator;
    const arc_allocator_t* storage_allocator = str->storage_allocator;

    if(!arc_allocator_has_features(object_allocator, ARC_ALLOCATOR_FEATURE_FREE))
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);

    if(!str->uses_sso && !arc_allocator_has_features(storage_allocator, ARC_ALLOCATOR_FEATURE_FREE)) {
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);
    }

    if(!str->uses_sso) {
        ARC_TRY(arc_allocator_free(storage_allocator, str->as.raw_data));

        str->uses_sso  = true;
        str->as.sso[0] = '\0';
        str->size      = 0U;
        str->capacity  = ARC_STRING_SSO_CAPACITY;
    }

    return arc_allocator_free(object_allocator, str);
}

/*
 * Validation
 */

bool arc_string_is_valid(const arc_string_t* str) {
    if(str == NULL)
        return false;

    if(str->object_allocator == NULL || str->storage_allocator == NULL)
        return false;

    if(str->size > str->capacity)
        return false;

    const char* data = NULL;

    if(str->uses_sso) {
        if(str->capacity != ARC_STRING_SSO_CAPACITY)
            return false;

        data = str->as.sso;
    } else {
        if(str->as.raw_data == NULL)
            return false;

        if(str->capacity <= ARC_STRING_SSO_CAPACITY)
            return false;

        data = str->as.raw_data;
    }

    if(data[str->size] != '\0')
        return false;

    if(memchr(data, '\0', str->size) != NULL)
        return false;

    return true;
}

/*
 * Queries
 */

arc_result_t arc_string_get_hash(const arc_string_t* str, uint64_t* out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    *out = arc_string_hash_bytes(arc_string_data_const(str), str->size);

    return arc_result_success();
}

arc_result_t arc_string_get_cstr(const arc_string_t* str, const char** out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    *out = arc_string_data_const(str);

    return arc_result_success();
}

arc_result_t arc_string_get_cstr_ex(const arc_string_t* str, const char** out, size_t* out_size) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    *out = arc_string_data_const(str);

    if(out_size != NULL)
        *out_size = str->size;

    return arc_result_success();
}

arc_result_t arc_string_get_size(const arc_string_t* str, size_t* out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    *out = str->size;

    return arc_result_success();
}

arc_result_t arc_string_get_capacity(const arc_string_t* str, size_t* out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    *out = str->capacity;

    return arc_result_success();
}

/*
 * Capacity management
 */

static arc_result_t arc_string_ensure_capacity(arc_string_t* str, size_t capacity) {
    if(capacity <= str->capacity)
        return arc_result_success();

    /*
     * We need capacity + 1 bytes.
     */
    if(capacity == SIZE_MAX)
        return arc_result_failure(ARC_STRING_ERROR_SIZE_OVERFLOW);

    if(!arc_allocator_has_features(str->storage_allocator, ARC_ALLOCATOR_FEATURE_FREE)) {
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);
    }

    char* new_data               = NULL;
    const size_t allocation_size = capacity + 1U;

    /*
     * Reallocation is only applicable to an existing heap allocation.
     * The allocator contract must guarantee that failed realloc leaves
     * the original allocation unchanged.
     */
    if(!str->uses_sso && arc_allocator_has_features(str->storage_allocator, ARC_ALLOCATOR_FEATURE_REALLOC)) {
        arc_result_t result =
            arc_allocator_realloc(str->storage_allocator, str->as.raw_data, allocation_size, (void**)&new_data);

        if(!result.ok)
            return result;

        str->as.raw_data = new_data;
        str->capacity    = capacity;

        return arc_result_success();
    }

    arc_result_t result = arc_allocator_alloc(str->storage_allocator, allocation_size, (void**)&new_data);

    if(!result.ok)
        return result;

    memcpy(new_data, arc_string_data_const(str), str->size + 1U);

    if(!str->uses_sso) {
        result = arc_allocator_free(str->storage_allocator, str->as.raw_data);

        if(!result.ok) {
            /*
             * The old allocation must remain valid on failed free.
             * If freeing the replacement also fails, return that failure
             * rather than silently claiming cleanup succeeded.
             */
            arc_result_t cleanup = arc_allocator_free(str->storage_allocator, new_data);

            if(!cleanup.ok)
                return cleanup;

            return result;
        }
    }

    str->uses_sso    = false;
    str->as.raw_data = new_data;
    str->capacity    = capacity;

    return arc_result_success();
}

arc_result_t arc_string_clear(arc_string_t* str) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    char* data = arc_string_data(str);

    data[0]   = '\0';
    str->size = 0U;

    return arc_result_success();
}

arc_result_t arc_string_reserve(arc_string_t* str, size_t capacity) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    return arc_string_ensure_capacity(str, capacity);
}

arc_result_t arc_string_shrink_to_fit(arc_string_t* str) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(str->uses_sso)
        return arc_result_success();

    if(str->size <= ARC_STRING_SSO_CAPACITY) {
        char temporary[ARC_STRING_SSO_CAPACITY + 1U];

        memcpy(temporary, str->as.raw_data, str->size + 1U);

        if(!arc_allocator_has_features(str->storage_allocator, ARC_ALLOCATOR_FEATURE_FREE)) {
            return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);
        }

        arc_result_t result = arc_allocator_free(str->storage_allocator, str->as.raw_data);

        if(!result.ok)
            return result;

        str->uses_sso = true;
        memcpy(str->as.sso, temporary, str->size + 1U);
        str->capacity = ARC_STRING_SSO_CAPACITY;

        return arc_result_success();
    }

    if(str->capacity == str->size)
        return arc_result_success();

    if(str->size == SIZE_MAX)
        return arc_result_failure(ARC_STRING_ERROR_SIZE_OVERFLOW);

    char* new_data               = NULL;
    const size_t allocation_size = str->size + 1U;

    if(arc_allocator_has_features(str->storage_allocator, ARC_ALLOCATOR_FEATURE_REALLOC)) {
        arc_result_t result =
            arc_allocator_realloc(str->storage_allocator, str->as.raw_data, allocation_size, (void**)&new_data);

        if(!result.ok)
            return result;

        str->as.raw_data = new_data;
        str->capacity    = str->size;

        return arc_result_success();
    }

    arc_result_t result = arc_allocator_alloc(str->storage_allocator, allocation_size, (void**)&new_data);

    if(!result.ok)
        return result;

    memcpy(new_data, str->as.raw_data, allocation_size);

    result = arc_allocator_free(str->storage_allocator, str->as.raw_data);

    if(!result.ok) {
        arc_result_t cleanup = arc_allocator_free(str->storage_allocator, new_data);

        if(!cleanup.ok)
            return cleanup;

        return result;
    }

    str->as.raw_data = new_data;
    str->capacity    = str->size;

    return arc_result_success();
}

/*
 * Byte access
 */

arc_result_t arc_string_get(const arc_string_t* str, size_t index, char* out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(index >= str->size)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    *out = arc_string_data_const(str)[index];

    return arc_result_success();
}

arc_result_t arc_string_get_first(const arc_string_t* str, char* out) {
    return arc_string_get(str, 0U, out);
}

arc_result_t arc_string_get_last(const arc_string_t* str, char* out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(str->size == 0U)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    *out = arc_string_data_const(str)[str->size - 1U];

    return arc_result_success();
}

arc_result_t arc_string_set(arc_string_t* str, size_t index, char value) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(index >= str->size)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    if(value == '\0')
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    arc_string_data(str)[index] = value;

    return arc_result_success();
}

arc_result_t arc_string_set_first(arc_string_t* str, char value) {
    return arc_string_set(str, 0U, value);
}

arc_result_t arc_string_set_last(arc_string_t* str, char value) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(str->size == 0U)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    return arc_string_set(str, str->size - 1U, value);
}

/*
 * Insertion
 */

arc_result_t arc_string_insert_range(arc_string_t* str, size_t index, const char* data, size_t size) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(index > str->size)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    if(size == 0U)
        return arc_result_success();

    if(data == NULL)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    if(memchr(data, '\0', size) != NULL)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    if(size > SIZE_MAX - str->size)
        return arc_result_failure(ARC_STRING_ERROR_SIZE_OVERFLOW);

    const size_t old_size = str->size;
    const size_t new_size = old_size + size;

    /*
     * Detect a source range that starts inside the string's current
     * content. Save its offset before capacity changes.
     *
     * uintptr_t is used for address-range inspection. A source that
     * begins within the content must also fit completely within it.
     */
    const char* old_data   = arc_string_data_const(str);
    const uintptr_t base   = (uintptr_t)old_data;
    const uintptr_t source = (uintptr_t)data;

    bool aliases         = false;
    size_t source_offset = 0U;

    if(source >= base && source - base <= old_size) {
        source_offset = (size_t)(source - base);

        if(size > old_size - source_offset)
            return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

        aliases = true;
    }

    arc_result_t result = arc_string_ensure_capacity(str, new_size);

    if(!result.ok)
        return result;

    char* buffer = arc_string_data(str);

    /*
     * Move the suffix, including its terminating NUL.
     */
    memmove(buffer + index + size, buffer + index, old_size - index + 1U);

    if(!aliases) {
        /*
         * External source: the caller must ensure that data remains
         * valid across this operation.
         */
        memmove(buffer + index, data, size);
    } else {
        /*
         * Original source byte at offset p:
         * - remains at p when p < index;
         * - moves to p + size when p >= index.
         *
         * Split a source range crossing the insertion point into
         * those two regions.
         */
        const size_t source_end = source_offset + size;

        if(source_end <= index) {
            /* Entire source stayed before the insertion point. */
            memmove(buffer + index, buffer + source_offset, size);
        } else if(source_offset >= index) {
            /* Entire source moved right with the suffix. */
            memmove(buffer + index, buffer + source_offset + size, size);
        } else {
            /* Source crosses the insertion point. */
            const size_t prefix_size = index - source_offset;
            const size_t suffix_size = size - prefix_size;

            /*
             * Copy the shifted portion first, then the unchanged
             * prefix. The two source locations are now known.
             */
            memmove(buffer + index + prefix_size, buffer + index + size, suffix_size);

            memmove(buffer + index, buffer + source_offset, prefix_size);
        }
    }

    str->size        = new_size;
    buffer[new_size] = '\0';

    return arc_result_success();
}

arc_result_t arc_string_insert(arc_string_t* str, size_t index, char value) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(value == '\0')
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    return arc_string_insert_range(str, index, &value, 1U);
}

arc_result_t arc_string_insert_cstr(arc_string_t* str, size_t index, const char* cstr) {
    if(cstr == NULL)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    return arc_string_insert_range(str, index, cstr, strlen(cstr));
}

arc_result_t arc_string_append_range(arc_string_t* str, const char* data, size_t size) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    return arc_string_insert_range(str, str->size, data, size);
}

arc_result_t arc_string_append(arc_string_t* str, char value) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    return arc_string_insert(str, str->size, value);
}

arc_result_t arc_string_append_cstr(arc_string_t* str, const char* cstr) {
    if(cstr == NULL)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    return arc_string_append_range(str, cstr, strlen(cstr));
}

arc_result_t arc_string_prepend_range(arc_string_t* str, const char* data, size_t size) {
    return arc_string_insert_range(str, 0U, data, size);
}

arc_result_t arc_string_prepend(arc_string_t* str, char value) {
    return arc_string_insert(str, 0U, value);
}

arc_result_t arc_string_prepend_cstr(arc_string_t* str, const char* cstr) {
    if(cstr == NULL)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    return arc_string_prepend_range(str, cstr, strlen(cstr));
}

/*
 * Removal
 */

arc_result_t arc_string_remove_range(arc_string_t* str, size_t start, size_t end) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(start > end || end > str->size)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    if(start == end)
        return arc_result_success();

    char* data                = arc_string_data(str);
    const size_t removed_size = end - start;

    memmove(data + start, data + end, str->size - end + 1U);

    str->size -= removed_size;

    return arc_result_success();
}

arc_result_t arc_string_remove(arc_string_t* str, size_t index) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(index >= str->size)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    return arc_string_remove_range(str, index, index + 1U);
}

arc_result_t arc_string_remove_first(arc_string_t* str) {
    return arc_string_remove(str, 0U);
}

arc_result_t arc_string_remove_last(arc_string_t* str) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(str->size == 0U)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    return arc_string_remove_range(str, str->size - 1U, str->size);
}

/*
 * Comparison
 */

arc_result_t arc_string_equals(const arc_string_t* str1, const arc_string_t* str2, bool* out) {
    if(str1 == NULL || str2 == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str1) || !arc_string_is_valid(str2))
        arc_abort("Arc string: string is invalid");

    if(str1->size != str2->size) {
        *out = false;
    } else {
        *out = memcmp(arc_string_data_const(str1), arc_string_data_const(str2), str1->size) == 0;
    }

    return arc_result_success();
}

static unsigned char arc_ascii_to_lower(unsigned char c) {
    if(c >= 'A' && c <= 'Z')
        return (unsigned char)(c + ('a' - 'A'));

    return c;
}

arc_result_t arc_string_equals_ignore_case(const arc_string_t* str1, const arc_string_t* str2, bool* out) {
    if(str1 == NULL || str2 == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str1) || !arc_string_is_valid(str2))
        arc_abort("Arc string: string is invalid");

    if(str1->size != str2->size) {
        *out = false;
        return arc_result_success();
    }

    const unsigned char* a = (const unsigned char*)arc_string_data_const(str1);

    const unsigned char* b = (const unsigned char*)arc_string_data_const(str2);

    *out = true;

    for(size_t i = 0U; i < str1->size; ++i) {
        if(arc_ascii_to_lower(a[i]) != arc_ascii_to_lower(b[i])) {
            *out = false;
            break;
        }
    }

    return arc_result_success();
}

/*
 * Searching
 */

arc_result_t arc_string_has_char(const arc_string_t* str, char value, bool* out, size_t* out_first_index,
                                 size_t* out_last_index) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(value == '\0')
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    const char* data = arc_string_data_const(str);

    *out = false;

    for(size_t i = 0U; i < str->size; ++i) {
        if(data[i] != value)
            continue;

        if(!*out) {
            *out = true;

            if(out_first_index != NULL)
                *out_first_index = i;
        }

        if(out_last_index != NULL)
            *out_last_index = i;
    }

    return arc_result_success();
}

arc_result_t arc_string_has_seq(const arc_string_t* str, const char* data, size_t size, bool* out,
                                size_t* out_first_index) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(data == NULL && size != 0U)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    *out = false;

    /*
     * By contract, the empty sequence occurs at index zero.
     */
    if(size == 0U) {
        *out = true;

        if(out_first_index != NULL)
            *out_first_index = 0U;

        return arc_result_success();
    }

    if(memchr(data, '\0', size) != NULL)
        return arc_result_failure(ARC_ERROR_INVALID_ARGUMENT);

    const char* str_data = arc_string_data_const(str);

    if(size > str->size)
        return arc_result_success();

    /*
     * Subtract first to avoid overflow in i + size.
     */
    const size_t last_start = str->size - size;

    for(size_t i = 0U; i <= last_start; ++i) {
        if(memcmp(str_data + i, data, size) != 0)
            continue;

        *out = true;

        if(out_first_index != NULL)
            *out_first_index = i;

        return arc_result_success();
    }

    return arc_result_success();
}

/*
 * String views
 *
 * Returned views borrow the string's storage. Mutating, reallocating,
 * shrinking, or destroying the string may invalidate a previously
 * returned view.
 */

arc_result_t arc_string_get_view_range(const arc_string_t* str, size_t start, size_t end, arc_stringview_t* out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(start > end || end > str->size)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    *out = (arc_stringview_t){
        .data   = arc_string_data_const(str) + start,
        .length = end - start,
    };

    return arc_result_success();
}

arc_result_t arc_string_get_view(const arc_string_t* str, arc_stringview_t* out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    *out = (arc_stringview_t){
        .data   = arc_string_data_const(str),
        .length = str->size,
    };

    return arc_result_success();
}

arc_result_t arc_string_get_view_from(const arc_string_t* str, size_t start, arc_stringview_t* out) {
    if(str == NULL)
        arc_abort("Arc string: string is NULL");

    if(out == NULL)
        arc_abort("Arc string: output pointer is NULL");

    if(!arc_string_is_valid(str))
        arc_abort("Arc string: string is invalid");

    if(start > str->size)
        return arc_result_failure(ARC_STRING_ERROR_OUT_OF_RANGE);

    *out = (arc_stringview_t){
        .data   = arc_string_data_const(str) + start,
        .length = str->size - start,
    };

    return arc_result_success();
}
