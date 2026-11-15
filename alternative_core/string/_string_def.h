
/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file _string_def.h
 * @author Nelson Somé
 * @brief Mutable byte string with UTF-8-oriented operations.
 *
 * Provides owned, NUL-terminated strings with explicit byte sizes,
 * small-string optimization, allocator integration, byte-oriented
 * operations, UTF-8 code-point operations, comparisons, searching,
 * and non-owning string views.
 *
 * String storage permits malformed UTF-8 but forbids embedded NUL
 * bytes. Byte-oriented operations do not require valid UTF-8.
 * Code-point operations report encoding errors when decoding fails.
 *
 * @note This file is part of Arc Core.
 * 
 * @attention This header is for Arc Core internal use only and must not be
 *            included by public API users.
 */

#ifndef ARC_STRING_DEF_H_
#define ARC_STRING_DEF_H_

#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief String object representing a mutable, NUL-terminated byte sequence.
 *
 * The String object is allocated using 'object_allocator' and its internal
 * storage is allocated using 'storage_allocator' when heap storage is used.
 *
 * The String uses small-string optimization (SSO) to store short strings
 * directly within the String object. When SSO is active, the inline buffer
 * is used instead of a separate storage allocation.
 *
 * The String does not own the allocator objects and does not manage their
 * lifetime. The allocators must remain valid for the entire lifetime of the
 * String.
 *
 * The String stores its content as bytes and permits malformed UTF-8.
 * Embedded NUL bytes are forbidden. The active buffer is always NUL-terminated,
 * and the terminating NUL byte is excluded from 'size' and 'capacity'.
 *
 * @attention Successful structural mutations may invalidate all previously
 *            obtained pointers referring to the String's internal storage.
 *
 * @note Replacing bytes without changing the String's structure does not
 *       invalidate existing borrowed pointers, provided the storage location
 *       remains unchanged.
 */
struct arc_string_t {
    /**
     * Indicates whether the String uses its inline SSO buffer.
     *
     * If true, 'as.sso' is the active storage.
     * If false, 'as.raw_data' is the active storage.
     */
    bool uses_sso;

    /**
     * Union containing the String's active storage representation.
     *
     * When SSO is active, 'sso' stores the String content and its terminating
     * NUL byte directly within the String object.
     *
     * When SSO is inactive, 'raw_data' points to the heap-allocated buffer
     * containing the String content and its terminating NUL byte.
     */
    union {
        /** Inline storage used when small-string optimization is active. */
        char sso[ARC_STRING_SSO_CAPACITY + 1U];

        /** Heap storage used when small-string optimization is inactive. */
        char* raw_data;
    } as;

    /** Current logical size of the String in bytes, excluding the NUL terminator. */
    size_t size;

    /**
     * Current storage capacity of the String in bytes, excluding the NUL
     * terminator.
     *
     * The capacity must be at least the logical size. When SSO is active,
     * the capacity is ARC_STRING_SSO_CAPACITY.
     */
    size_t capacity;

    /** Allocator used to allocate and release the String object itself. */
    const arc_allocator_t* object_allocator;

    /**
     * Allocator used to allocate and release heap-backed String storage.
     *
     * This allocator is not used while SSO is active.
     */
    const arc_allocator_t* storage_allocator;
};

#ifdef __cplusplus
}
#endif

#endif /* ARC_STRING_DEF_H_ */
