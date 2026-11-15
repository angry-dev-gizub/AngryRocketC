/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file stringview.h
 * @author Nelson Somé
 * @brief Non-owning view of a byte sequence.
 *
 * Provides non-owning views over byte sequences, with constructors,
 * hashing, and byte-wise comparison.
 *
 * A string view does not own its underlying storage and does not
 * allocate or free memory. The referenced storage must remain valid
 * for the lifetime of the view.
 *
 * String views may represent arbitrary byte sequences, including
 * malformed UTF-8 and embedded NUL bytes. The length is specified
 * explicitly and does not include any implicit terminator.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_STRINGVIEW_H_
#define ARC_STRINGVIEW_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Non-owning view of a byte sequence.
 *
 * @note The data pointer is borrowed and must not be freed through
 *       the string view.
 */
typedef struct arc_stringview_t {
    const char* data;
    size_t length;
} arc_stringview_t;

/**
 * @brief Constructs a view over a byte sequence.
 *
 * @param data Pointer to the byte sequence.
 * @param length Number of bytes in the sequence.
 * @return A view over the specified byte sequence.
 *
 * @note The view does not copy or own the data.
 */
arc_stringview_t arc_stringview_make(const char* data, size_t length);

/**
 * @brief Constructs a view over a NUL-terminated C string.
 *
 * @param cstr Pointer to a NUL-terminated C string.
 * @return A view excluding the terminating NUL byte.
 *
 * @note The view does not copy or own the data.
 */
arc_stringview_t arc_stringview(const char* cstr);

/**
 * @brief Computes a hash of a string view.
 *
 * @param sv String view to hash.
 * @return Hash value.
 */
uint64_t arc_stringview_hash(const arc_stringview_t* sv);

/**
 * @brief Computes a seeded hash of a string view.
 *
 * @param sv String view to hash.
 * @param seed Hash seed.
 * @return Hash value.
 */
uint64_t arc_stringview_hash_ex(const arc_stringview_t* sv, uint64_t seed);

/**
 * @brief Compares two string views for byte-wise equality.
 *
 * @param sv1 First string view.
 * @param sv2 Second string view.
 * @return true if both views contain identical byte sequences;
 *         false otherwise.
 */
bool arc_stringview_equals(const arc_stringview_t* sv1, const arc_stringview_t* sv2);

#ifdef __cplusplus
}
#endif

#endif /* ARC_STRINGVIEW_H_ */
