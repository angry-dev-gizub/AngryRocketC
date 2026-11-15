/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file string.h
 * @author Nelson Somé
 * @brief Mutable, owned, NUL-terminated byte strings.
 *
 * Provides byte-oriented strings with explicit content sizes, small-string
 * optimization, allocator integration, mutation, comparison, searching,
 * hashing, and non-owning string views.
 *
 * Strings store arbitrary non-NUL bytes followed by a terminating NUL.
 * Embedded NUL bytes are not permitted. No character encoding is assumed
 * or validated. All indexes, sizes, capacities, and ranges are measured
 * in bytes.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_STRING_H_
#define ARC_STRING_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "result/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/** String-specific error domain. */
#define ARC_DOMAIN_STRING ((uint8_t)5U)

/** String-specific errors. */
#define ARC_STRING_ERROR_INVALID_STRING ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_STRING, 1U)
#define ARC_STRING_ERROR_OUT_OF_RANGE ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_STRING, 2U)
#define ARC_STRING_ERROR_SIZE_OVERFLOW ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_STRING, 3U)

/**
 * @brief Configurable inline content capacity.
 *
 * The implementation reserves one additional byte for the terminating NUL.
 *
 * @note Defaults to 64 content bytes.
 * @note All translation units using the library must agree on this value.
 */
#ifndef ARC_STRING_SSO_CAPACITY
#define ARC_STRING_SSO_CAPACITY 64U
#endif

/** Forward declarations. */
typedef struct arc_allocator_t arc_allocator_t;
typedef struct arc_string_desc_t arc_string_desc_t;
typedef struct arc_string_t arc_string_t;
typedef struct arc_stringview_t arc_stringview_t;

/**
 * @brief String initialization descriptor.
 *
 * Separates allocation of the string object from allocation of its
 * dynamically allocated content storage.
 *
 * @note Allocator pointers are borrowed, never owned by the string.
 */
struct arc_string_desc_t {
    const arc_allocator_t* object_allocator;
    const arc_allocator_t* storage_allocator;
};

/**
 * @brief Creates an empty string.
 *
 * @param[out] out Writable output pointer.
 * @param[in] desc Initialization descriptor.
 * @return Success on creation; failure otherwise.
 *
 * @post On success, *out is a valid empty string.
 * @post On failure, *out is NULL.
 */
arc_result_t arc_string_new(arc_string_t** out, const arc_string_desc_t* desc);

/**
 * @brief Creates a string by copying a NUL-terminated C string.
 *
 * @param[in] cstr Non-NULL, NUL-terminated input.
 * @param[out] out Writable output pointer.
 * @param[in] desc Initialization descriptor.
 * @return Success on creation; failure otherwise.
 *
 * @post On success, the string owns an independent copy of the input.
 * @post On failure, *out is NULL.
 */
arc_result_t arc_string_from_cstr(const char* cstr, arc_string_t** out, const arc_string_desc_t* desc);

/**
 * @brief Creates a string from a printf-style format string.
 *
 * Formats the arguments using the standard printf conversion rules and
 * constructs a new Arc string containing the resulting bytes.
 *
 * The resulting string is allocated using the allocators in `desc`.
 * The caller owns the resulting string and must destroy it with
 * `arc_string_destroy()`.
 *
 * The output cannot contain embedded NUL bytes. If formatting produces
 * an embedded NUL byte, the operation fails.
 *
 * The format string and its arguments must be valid for the specified
 * printf conversion specifiers. The `%n` conversion specifier is not
 * supported because formatting requires two passes.
 *
 * @param[out] out Receives the newly created string. Set to NULL on failure.
 * @param[in] desc String allocator configuration.
 * @param[in] format printf-style format string.
 *
 * @return Success on completion, or a failure result.
 *
 * @note A NULL output pointer, descriptor, or format string violates the
 *       programmer contract and causes an abort.
 */
arc_result_t arc_string_format(arc_string_t** out, const arc_string_desc_t* desc, const char* format, ...);

/**
 * @brief Replaces a string's contents with formatted text.
 *
 * Formats the supplied arguments according to the given format string and
 * replaces the contents of the destination string with the resulting bytes.
 *
 * The format string follows the formatting rules supported by
 * arc_string_format.
 *
 * @param str    The destination string.
 * @param format The format string.
 * @param ...    Arguments referenced by the format string.
 *
 * @return Success if formatting and replacement complete successfully;
 *         failure otherwise.
 *
 * @note The destination string must be valid.
 * @note The format string must be a valid, NUL-terminated string.
 * @note On failure, the destination string remains unchanged.
 */
arc_result_t arc_string_set_format(arc_string_t* str, const char* format, ...);

/**
 * @brief Creates a string by copying an explicit byte range.
 *
 * @param[in] data Non-NULL input pointer, even when size is zero.
 * @param[in] size Number of input bytes.
 * @param[out] out Writable output pointer.
 * @param[in] desc Initialization descriptor.
 * @return Success on creation; failure otherwise.
 *
 * @post On success, exactly size bytes are copied.
 * @post On failure, *out is NULL.
 * @return Failure if the input contains an embedded NUL byte.
 */
arc_result_t arc_string_from_bytes(const char* data, size_t size, arc_string_t** out, const arc_string_desc_t* desc);

/**
 * @brief Creates an independent copy of a string.
 *
 * @param[in] str Source string.
 * @param[out] out Writable output pointer.
 * @return Success on creation; failure otherwise.
 *
 * @post On success, the copy owns independent storage.
 * @post On failure, *out is NULL.
 */
arc_result_t arc_string_clone(const arc_string_t* str, arc_string_t** out);

/**
 * @brief Destroys a string and releases its owned resources.
 *
 * @param[in] str String to destroy.
 * @return Success on destruction; failure otherwise.
 *
 * @note The caller must clear any pointer that referred to the object.
 */
arc_result_t arc_string_destroy(arc_string_t* str);

/**
 * @brief Checks whether a string's structural invariants hold.
 *
 * @param[in] str String to inspect.
 * @return true if valid; false otherwise.
 *
 * @note This function does not validate or interpret character encodings.
 */
bool arc_string_is_valid(const arc_string_t* str);

/**
 * @brief Computes a hash of the string's content bytes.
 *
 * @param[in] str String to hash.
 * @param[out] out Writable hash output.
 * @return Success if computed; failure otherwise.
 *
 * @post Equal byte sequences produce equal hashes.
 * @note The algorithm and cross-version stability are implementation-defined.
 */
arc_result_t arc_string_get_hash(const arc_string_t* str, uint64_t* out);

/**
 * @brief Retrieves a borrowed pointer to the NUL-terminated content.
 *
 * @param[in] str Source string.
 * @param[out] out Writable pointer output.
 * @return Success if retrieved; failure otherwise.
 *
 * @note The pointer must not be freed by the caller.
 * @note Mutation, reallocation, or destruction may invalidate the pointer.
 */
arc_result_t arc_string_get_cstr(const arc_string_t* str, const char** out);

/**
 * @brief Retrieves the content pointer and optional byte size.
 *
 * @param[in] str Source string.
 * @param[out] out Writable pointer output.
 * @param[out] out_size Optional byte-size output.
 * @return Success if retrieved; failure otherwise.
 *
 * @post If out_size is non-NULL, it excludes the terminating NUL.
 */
arc_result_t arc_string_get_cstr_ex(const arc_string_t* str, const char** out, size_t* out_size);

/** @brief Retrieves the content size in bytes. */
arc_result_t arc_string_get_size(const arc_string_t* str, size_t* out);

/** @brief Retrieves the content capacity in bytes, excluding the terminator. */
arc_result_t arc_string_get_capacity(const arc_string_t* str, size_t* out);

/**
 * @brief Clears the string while retaining its allocated capacity.
 *
 * @post On success, size is zero and the first content byte is NUL.
 */
arc_result_t arc_string_clear(arc_string_t* str);

/**
 * @brief Ensures at least the requested content capacity.
 *
 * @param[in] capacity Required capacity in bytes, excluding the terminator.
 *
 * @post Existing content is preserved.
 * @post Capacity never decreases.
 * @post On failure, observable string state remains unchanged.
 */
arc_result_t arc_string_reserve(arc_string_t* str, size_t capacity);

/**
 * @brief Reduces storage usage where possible.
 *
 * @post Content remains unchanged.
 * @post On failure, observable string state remains unchanged.
 */
arc_result_t arc_string_shrink_to_fit(arc_string_t* str);

/** @brief Retrieves the byte at index. Requires index < size. */
arc_result_t arc_string_get(const arc_string_t* str, size_t index, char* out);

/** @brief Retrieves the first byte. Fails if the string is empty. */
arc_result_t arc_string_get_first(const arc_string_t* str, char* out);

/** @brief Retrieves the last byte. Fails if the string is empty. */
arc_result_t arc_string_get_last(const arc_string_t* str, char* out);

/**
 * @brief Replaces the byte at index.
 *
 * @param[in] index Byte index satisfying index < size.
 * @param[in] value Replacement byte, which must not be NUL.
 * @post On failure, the string remains unchanged.
 */
arc_result_t arc_string_set(arc_string_t* str, size_t index, char value);

/** @brief Replaces the first byte. Fails if the string is empty. */
arc_result_t arc_string_set_first(arc_string_t* str, char value);

/** @brief Replaces the last byte. Fails if the string is empty. */
arc_result_t arc_string_set_last(arc_string_t* str, char value);

/**
 * @brief Inserts one byte at an index.
 *
 * @param[in] index Insertion index satisfying index <= size.
 * @param[in] value Inserted byte, which must not be NUL.
 * @post Inserting at size appends the byte.
 * @post On failure, the string remains unchanged.
 */
arc_result_t arc_string_insert(arc_string_t* str, size_t index, char value);

/**
 * @brief Inserts a range of bytes.
 *
 * @param[in] index Insertion index satisfying index <= size.
 * @param[in] data Non-NULL input pointer, even when size is zero.
 * @param[in] size Number of bytes to insert.
 *
 * @post Embedded NUL bytes are rejected.
 * @post On failure, the string remains unchanged.
 * @note Input may alias the destination string's content.
 * @note Inserting an empty range is a successful no-op.
 */
arc_result_t arc_string_insert_range(arc_string_t* str, size_t index, const char* data, size_t size);

/** @brief Inserts a NUL-terminated C string at a byte index. */
arc_result_t arc_string_insert_cstr(arc_string_t* str, size_t index, const char* cstr);

/** @brief Appends one non-NUL byte. */
arc_result_t arc_string_append(arc_string_t* str, char value);

/** @brief Appends a byte range; embedded NUL bytes are rejected. */
arc_result_t arc_string_append_range(arc_string_t* str, const char* data, size_t size);

/** @brief Appends a NUL-terminated C string. */
arc_result_t arc_string_append_cstr(arc_string_t* str, const char* cstr);

/** @brief Prepends one non-NUL byte. */
arc_result_t arc_string_prepend(arc_string_t* str, char value);

/** @brief Prepends a byte range; embedded NUL bytes are rejected. */
arc_result_t arc_string_prepend_range(arc_string_t* str, const char* data, size_t size);

/** @brief Prepends a NUL-terminated C string. */
arc_result_t arc_string_prepend_cstr(arc_string_t* str, const char* cstr);

/** @brief Removes the byte at index. Requires index < size. */
arc_result_t arc_string_remove(arc_string_t* str, size_t index);

/**
 * @brief Removes a half-open byte range [start, end).
 *
 * @pre start <= end <= size.
 * @post An empty range is a successful no-op.
 * @post On failure, the string remains unchanged.
 */
arc_result_t arc_string_remove_range(arc_string_t* str, size_t start, size_t end);

/** @brief Removes the first byte. Fails if the string is empty. */
arc_result_t arc_string_remove_first(arc_string_t* str);

/** @brief Removes the last byte. Fails if the string is empty. */
arc_result_t arc_string_remove_last(arc_string_t* str);

/**
 * @brief Compares two strings for exact byte equality.
 *
 * @post *out is true exactly when their content bytes and sizes match.
 */
arc_result_t arc_string_equals(const arc_string_t* str1, const arc_string_t* str2, bool* out);

/**
 * @brief Compares two strings using ASCII-only case-insensitive equality.
 *
 * @note Only bytes representing ASCII letters are case-folded.
 *       All other byte values are compared unchanged.
 */
arc_result_t arc_string_equals_ignore_case(const arc_string_t* str1, const arc_string_t* str2, bool* out);

/**
 * @brief Searches for a non-NUL byte.
 *
 * @param[out] out True if the byte occurs in the string.
 * @param[out] out_first_index Optional first matching byte index.
 * @param[out] out_last_index Optional last matching byte index.
 *
 * @note Optional index outputs may be NULL.
 * @note If no match is found, optional index outputs remain unchanged.
 */
arc_result_t arc_string_has_char(const arc_string_t* str, char value, bool* out, size_t* out_first_index,
                                 size_t* out_last_index);

/**
 * @brief Searches for a byte sequence.
 *
 * @param[in] data Non-NULL search data, even when size is zero.
 * @param[in] size Search sequence size in bytes.
 * @param[out] out True if a match is found.
 * @param[out] out_first_index Optional first matching byte index.
 *
 * @note An empty search sequence matches at index zero.
 * @note Optional index output may be NULL.
 * @note Search is byte-oriented.
 */
arc_result_t arc_string_has_seq(const arc_string_t* str, const char* data, size_t size, bool* out,
                                size_t* out_first_index);

/**
 * @brief Creates a non-owning view of the entire string.
 *
 * @note The view does not own its content.
 * @note Mutation or destruction of the source may invalidate the view.
 */
arc_result_t arc_string_get_view(const arc_string_t* str, arc_stringview_t* out);

/**
 * @brief Creates a non-owning view from a byte offset to the end.
 *
 * @param[in] start Byte offset satisfying start <= size.
 */
arc_result_t arc_string_get_view_from(const arc_string_t* str, size_t start, arc_stringview_t* out);

/**
 * @brief Creates a non-owning view over the half-open range [start, end).
 *
 * @pre start <= end <= size.
 */
arc_result_t arc_string_get_view_range(const arc_string_t* str, size_t start, size_t end, arc_stringview_t* out);

#ifdef __cplusplus
}
#endif

#endif /* ARC_STRING_H_ */
