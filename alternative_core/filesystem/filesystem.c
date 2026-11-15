/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

#include "filesystem.h"

/**
 * @brief Initializes a filesystem instance.
 *
 * Initializes caller-owned filesystem storage from a descriptor.
 *
 * @param[out] filesystem Filesystem instance to initialize.
 * @param[in] desc Filesystem initialization descriptor.
 *
 * @return A successful result on initialization.
 * @return A failure result if an argument or descriptor is invalid, or if
 *         initialization fails.
 *
 * @post On success, the filesystem is valid and ready for use.
 * @post The filesystem's context ownership follows the initialization
 *       contract defined by the descriptor.
 */
arc_result_t arc_filesystem_init(arc_filesystem_t* filesystem, const arc_filesystem_desc_t* desc);

/**
 * @brief Checks whether a filesystem instance is valid.
 *
 * @param[in] filesystem Filesystem instance to check.
 *
 * @return true if the filesystem is valid; false otherwise.
 */
bool arc_filesystem_is_valid(const arc_filesystem_t* filesystem);

/**
 * @brief Destroys a filesystem instance's owned context.
 *
 * The filesystem object itself is caller-owned storage and is not freed.
 * A failed context destruction must leave the instance valid for a retry.
 *
 * @param[in,out] filesystem Filesystem instance to destroy.
 *
 * @return A successful result if destruction succeeds.
 * @return A failure result if the filesystem is invalid or context
 *         destruction fails.
 *
 * @attention Streams and directory iterators associated with the filesystem
 *            become invalid when destruction succeeds.
 */
arc_result_t arc_filesystem_destroy(arc_filesystem_t* filesystem);

/**
 * @brief Clones a filesystem instance.
 *
 * Creates a new filesystem context using the source filesystem's clone
 * callback and allocator. The backend defines which underlying resources
 * are shared by the clone.
 *
 * @param[out] out_filesystem Receives the cloned filesystem.
 * @param[in] filesystem Filesystem instance to clone.
 *
 * @return A successful result if cloning succeeds.
 * @return A failure result if an argument or filesystem is invalid, cloning
 *         fails, or cloning is unsupported.
 */
arc_result_t arc_filesystem_clone(arc_filesystem_t* out_filesystem, const arc_filesystem_t* filesystem);

/**
 * @brief Returns the system filesystem.
 *
 * Returns a pointer to the process-wide system filesystem instance.
 * The returned instance is managed by the library and must not be
 * initialized or destroyed by the caller.
 *
 * @return A pointer to the system filesystem.
 */
arc_filesystem_t* arc_system_file_system(void);

/**
 * @brief Opens a stream.
 *
 * @param[in] filesystem Filesystem to operate on.
 * @param[out] out_stream Receives the opened stream handle.
 * @param[in] path Path to open.
 * @param[in] options Stream access and creation options.
 *
 * @return A successful result if the stream is opened.
 * @return A failure result if an argument is invalid or opening fails.
 */
arc_result_t arc_filesystem_open(arc_filesystem_t* filesystem, arc_stream_handle_t** out_stream, const arc_path_t* path, const arc_stream_open_options_t* options);

/**
 * @brief Closes a stream.
 */
arc_result_t arc_filesystem_close(arc_filesystem_t* filesystem, arc_stream_handle_t* stream);

/**
 * @brief Closes all streams tracked by a filesystem.
 */
arc_result_t arc_filesystem_close_all(arc_filesystem_t* filesystem);

/**
 * @brief Reads data from a stream.
 */
arc_result_t arc_filesystem_read(const arc_filesystem_t* filesystem, const arc_stream_handle_t* stream, void* out, size_t bytes_to_read, size_t* out_bytes_read);

/**
 * @brief Writes data to a stream.
 */
arc_result_t arc_filesystem_write(arc_filesystem_t* filesystem, arc_stream_handle_t* stream, const void* buffer, size_t bytes_to_write, size_t* out_bytes_written);

/**
 * @brief Changes a stream's position.
 */
arc_result_t arc_filesystem_seek(arc_filesystem_t* filesystem, arc_stream_handle_t* stream, int64_t offset, arc_seek_whence_t whence);

/**
 * @brief Retrieves a stream's current position.
 */
arc_result_t arc_filesystem_tell(const arc_filesystem_t* filesystem, const arc_stream_handle_t* stream, int64_t* out_position);

/**
 * @brief Flushes a stream's buffered data.
 */
arc_result_t arc_filesystem_flush(arc_filesystem_t* filesystem, arc_stream_handle_t* stream);

/**
 * @brief Removes a file or empty directory.
 *
 * This operation is not recursive.
 */
arc_result_t arc_filesystem_remove(arc_filesystem_t* filesystem, const arc_path_t* path);

/**
 * @brief Recursively removes a directory and its contents.
 */
arc_result_t arc_filesystem_remove_recursive(arc_filesystem_t* filesystem, const arc_path_t* path);

/**
 * @brief Checks whether a path identifies a regular file.
 */
arc_result_t arc_filesystem_is_file(const arc_filesystem_t* filesystem, const arc_path_t* path, bool* out_is_file);

/**
 * @brief Checks whether a path identifies a directory.
 */
arc_result_t arc_filesystem_is_directory(const arc_filesystem_t* filesystem, const arc_path_t* path, bool* out_is_directory);

/**
 * @brief Creates a directory.
 */
arc_result_t arc_filesystem_create_directory(arc_filesystem_t* filesystem, const arc_path_t* path);

/**
 * @brief Opens a directory for iteration.
 */
arc_result_t arc_filesystem_open_directory(arc_filesystem_t* filesystem, const arc_path_t* path, arc_dir_iterator_t* out_it);

/**
 * @brief Retrieves information about an open stream.
 */
arc_result_t arc_filesystem_get_stream_info(const arc_filesystem_t* filesystem, const arc_stream_handle_t* stream, arc_stream_info_t* out_info);

/**
 * @brief Checks whether a path exists.
 *
 * A nonexistent path is reported as a successful result with false in
 * @p out_exists. Actual filesystem errors are reported as failures.
 */
arc_result_t arc_filesystem_exists(const arc_filesystem_t* filesystem, const arc_path_t* path, bool* out_exists);

/**
 * @brief Retrieves information about a file or directory.
 */
arc_result_t arc_filesystem_get_file_info(const arc_filesystem_t* filesystem, const arc_path_t* path, arc_file_info_t* out_info);

/**
 * @brief Renames or moves a filesystem entry.
 */
arc_result_t arc_filesystem_rename(arc_filesystem_t* filesystem, const arc_path_t* source, const arc_path_t* destination);

/**
 * @brief Copies a filesystem entry.
 */
arc_result_t arc_filesystem_copy(arc_filesystem_t* filesystem, const arc_path_t* source, const arc_path_t* destination);

/**
 * @brief Retrieves the filesystem's current working directory.
 */
arc_result_t arc_filesystem_get_current_directory(const arc_filesystem_t* filesystem, arc_path_t* out_path);

/**
 * @brief Changes the filesystem's current working directory.
 */
arc_result_t arc_filesystem_set_current_directory(arc_filesystem_t* filesystem, const arc_path_t* path);

/**
 * @brief Creates and opens a temporary file.
 */
arc_result_t arc_filesystem_create_temp_file(arc_filesystem_t* filesystem, arc_stream_handle_t* out_stream, const arc_path_t* path, const arc_stream_open_options_t* options);
