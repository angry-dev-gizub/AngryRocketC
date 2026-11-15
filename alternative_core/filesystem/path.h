/**
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file path.h
 * @author Nelson Somé
 * @brief Path manipulation API.
 *
 * Provides an opaque path type and operations for parsing, composing,
 * inspecting, and converting filesystem paths.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_PATH_H_
#define ARC_PATH_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "result/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations */
typedef struct arc_allocator_t arc_allocator_t;
typedef struct arc_string_t arc_string_t;
typedef struct arc_stringview_t arc_stringview_t;
typedef struct arc_path arc_path_t;

/**
 * @brief Creates a new path.
 *
 * @param[out] out_path Receives the newly created path.
 * @param[in] allocator Allocator used for path storage.
 *
 * @return A successful result if the path is created.
 * @return A failure result if an argument is invalid or allocation fails.
 *
 * @post On success, @p out_path contains a valid path.
 * @post On failure, @p out_path is NULL.
 */
arc_result_t arc_path_new(arc_path_t** out_path, const arc_allocator_t* allocator);

/**
 * @brief Creates a copy of an existing path.
 *
 * The cloned path uses the same allocator as the source path.
 *
 * @param[out] out_path Receives the cloned path.
 * @param[in] src_path Path to clone.
 *
 * @return A successful result if cloning succeeds.
 * @return A failure result if an argument is invalid or allocation fails.
 *
 * @post On success, @p out_path contains an independent path object.
 * @post On failure, @p out_path is NULL.
 */
arc_result_t arc_path_clone(arc_path_t** out_path, const arc_path_t* src_path);

/**
 * @brief Destroys a path.
 *
 * Releases the path and its internally allocated storage.
 *
 * @param[in,out] path Path to destroy.
 *
 * @return A successful result if destruction succeeds.
 * @return A failure result if the path is invalid.
 */
arc_result_t arc_path_destroy(arc_path_t* path);

/**
 * @brief Checks whether a path is valid.
 *
 * @param[in] path Path to check.
 *
 * @return true if the path is valid; false otherwise.
 */
bool arc_path_is_valid(const arc_path_t* path);


arc_result_t arc_path_is_absolute(const arc_path_t* path);

/**
 * @brief Retrieves a string view of a path.
 *
 * The returned string view references storage owned by the path.
 * The view becomes invalid when the path is modified or destroyed.
 *
 * @param[in] path Path to inspect.
 * @param[out] out_str Receives the string view.
 *
 * @return A successful result if the string view is retrieved.
 * @return A failure result if an argument is invalid.
 */
arc_result_t arc_path_get_str(const arc_path_t* path, arc_stringview_t* out_str);

/**
 * @brief Converts a path into a null-terminated C string.
 *
 * Allocates a new string containing the path representation.
 * The caller owns the returned allocation.
 *
 * @param[in] path Path to convert.
 * @param[out] out_cstr Receives the allocated C string.
 * @param[out] out_size Receives the string size, excluding the null terminator.
 * @param[in] allocator Allocator used for the output string.
 *
 * @return A successful result if conversion succeeds.
 * @return A failure result if an argument is invalid or allocation fails.
 *
 * @post On success, @p out_cstr contains a null-terminated string.
 * @post On failure, @p out_cstr is NULL.
 */
arc_result_t arc_path_get_cstr(const arc_path_t* path, char** out_cstr, size_t* out_size, const arc_allocator_t* allocator);

/**
 * @brief Parses a null-terminated C string into a path.
 *
 * @param[in,out] path Path to modify.
 * @param[in] path_cstr Null-terminated path string.
 *
 * @return A successful result if parsing succeeds.
 * @return A failure result if an argument is invalid or parsing fails.
 */
arc_result_t arc_path_parse(arc_path_t* path, const char* path_cstr);

/**
 * @brief Parses a character sequence into a path.
 *
 * Unlike arc_path_parse(), this function does not require the input
 * to be null-terminated.
 *
 * @param[in,out] path Path to modify.
 * @param[in] cstr Character sequence to parse.
 * @param[in] size Number of bytes in the input sequence.
 *
 * @return A successful result if parsing succeeds.
 * @return A failure result if an argument is invalid or parsing fails.
 */
arc_result_t arc_path_parse_bytes(arc_path_t* path, const char* cstr, size_t size);

/**
 * @brief Parses a string into a path.
 *
 * @param[in,out] path Path to modify.
 * @param[in] path_str String to parse.
 *
 * @return A successful result if parsing succeeds.
 * @return A failure result if an argument is invalid or parsing fails.
 */
arc_result_t arc_path_parse_str(arc_path_t* path, const arc_string_t* path_str);

/**
 * @brief Joins two paths.
 *
 * Stores the result of joining @p p1 and @p p2 in @p dest.
 *
 * @param[in,out] dest Path receiving the result.
 * @param[in] p1 First path.
 * @param[in] p2 Second path.
 *
 * @return A successful result if the paths are joined.
 * @return A failure result if an argument is invalid or joining fails.
 *
 * @note The behavior when @p p2 is an absolute path must be defined.
 * @note Aliasing between @p dest and the input paths must be defined.
 */
arc_result_t arc_path_join(arc_path_t* dest, const arc_path_t* p1, const arc_path_t* p2);

/**
 * @brief Retrieves the filename component of a path.
 *
 * @param[in] path Path to inspect.
 * @param[out] out_str Receives a string view of the filename.
 *
 * @return A successful result if the filename is retrieved.
 * @return A failure result if an argument is invalid.
 *
 * @note The returned view references storage owned by @p path.
 */
arc_result_t arc_path_get_filename(const arc_path_t* path, arc_stringview_t* out_str);

/**
 * @brief Retrieves the filename without its extension.
 *
 * @param[in] path Path to inspect.
 * @param[out] out_str Receives a string view of the filename.
 *
 * @return A successful result if the filename is retrieved.
 * @return A failure result if an argument is invalid.
 *
 * @note The returned view references storage owned by @p path.
 */
arc_result_t arc_path_get_filename_no_extension(const arc_path_t* path, arc_stringview_t* out_str);

/**
 * @brief Retrieves the extension of a path.
 *
 * @param[in] path Path to inspect.
 * @param[out] out_str Receives a string view of the extension.
 *
 * @return A successful result if the extension is retrieved.
 * @return A failure result if an argument is invalid.
 *
 * @note The returned view references storage owned by @p path.
 */
arc_result_t arc_path_get_extension(const arc_path_t* path, arc_stringview_t* out_str);

/**
 * @brief Retrieves the parent directory of a path.
 *
 * Stores the parent path in an existing path object.
 *
 * @param[in] path Path to inspect.
 * @param[out] out_path Receives the parent path.
 *
 * @return A successful result if the parent path is retrieved.
 * @return A failure result if an argument is invalid or no parent exists.
 *
 * @note The output path must be initialized before calling this function.
 */
arc_result_t arc_path_get_parent(const arc_path_t* path, arc_path_t* out_path);

#ifdef __cplusplus
}
#endif

#endif /* ARC_PATH_H_ */
