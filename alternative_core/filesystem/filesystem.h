/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file filesystem.h
 * @author Nelson Somé
 * @brief Generic filesystem interface.
 *
 * Defines a backend-independent filesystem interface for accessing files,
 * directories, streams, and filesystem metadata.
 *
 * A filesystem consists of an interface and a backend context. The interface
 * defines the available operations, while the context stores the state required
 * by a particular filesystem implementation.
 *
 * The abstraction may represent a disk filesystem, an in-memory filesystem,
 * an archive, or another virtual filesystem. Backend-specific behavior is
 * permitted where this interface does not define a stronger guarantee.
 *
 * Filesystem objects do not own their own storage. Context ownership is
 * determined by the initialization operation and the filesystem's ownership
 * state. Allocators referenced by a filesystem are borrowed and must remain
 * valid for as long as the filesystem may use them.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_FILESYSTEM_H_
#define ARC_FILESYSTEM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "result/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Filesystem-specific errors. */
#define ARC_DOMAIN_FILESYSTEM ((uint8_t)6U)

/** The supplied filesystem is NULL, invalid, or otherwise unusable. */
#define ARC_FILESYSTEM_ERROR_INVALID_FILESYSTEM ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_FILESYSTEM, 1U)
/** The supplied stream handle is NULL, invalid, or otherwise unusable. */
#define ARC_FILESYSTEM_ERROR_INVALID_STREAM ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_FILESYSTEM, 2U)
/** The supplied path is NULL, invalid, or otherwise unusable. */
#define ARC_FILESYSTEM_ERROR_INVALID_PATH ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_FILESYSTEM, 3U)
/** The supplied directory iterator is NULL, invalid, or otherwise unusable. */
#define ARC_FILESYSTEM_ERROR_INVALID_ITERATOR ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_FILESYSTEM, 4U)

/** Forward declarations. */
typedef struct arc_allocator_t arc_allocator_t;
typedef struct arc_path arc_path_t;
typedef struct arc_filesystem_interface arc_filesystem_interface_t;
typedef struct arc_filesystem_desc arc_filesystem_desc_t;
typedef struct arc_filesystem arc_filesystem_t;
typedef struct arc_stream_info arc_stream_info_t;
typedef struct arc_stream_open_options arc_stream_open_options_t;
typedef struct arc_dir_iterator arc_dir_iterator_t;
typedef struct arc_file_info arc_file_info_t;
typedef struct arc_stream_handle arc_stream_handle_t;

typedef arc_stream_handle_t arc_out_stream_handle_t;
typedef arc_stream_handle_t arc_in_stream_handle_t;

/**
 * @brief Specifies the access permissions requested for a stream.
 *
 * The selected access mode determines which stream operations are permitted.
 * A backend may reject a requested mode when it cannot provide the requested
 * permissions.
 */
typedef enum arc_stream_access_t {
    /** Open the stream for reading. */
    ARC_STREAM_ACCESS_READ = 0,

    /** Open the stream for writing. */
    ARC_STREAM_ACCESS_WRITE,

    /** Open the stream for both reading and writing. */
    ARC_STREAM_ACCESS_READ_WRITE
} arc_stream_access_t;

/**
 * @brief Specifies how a file is opened or created.
 *
 * These values describe the requested creation behavior independently of the
 * access permissions. The selected combination must be supported by the
 * filesystem backend.
 */
typedef enum arc_stream_creation_t {
    /** The file must already exist. */
    ARC_STREAM_OPEN_EXISTING = 0,

    /** The file must not exist and is created if possible. */
    ARC_STREAM_CREATE_NEW,

    /** Open the existing file, or create it if it does not exist. */
    ARC_STREAM_OPEN_OR_CREATE,

    /** Create the file if necessary, or truncate the existing file. */
    ARC_STREAM_CREATE_OR_TRUNCATE
} arc_stream_creation_t;

/**
 * @brief Identifies the type of a filesystem entry.
 *
 * ARC_FILE_TYPE_UNKNOWN indicates that the entry type is unavailable or
 * cannot be determined by the backend.
 */
typedef enum arc_file_type_t {
    /** The entry type is unknown. */
    ARC_FILE_TYPE_UNKNOWN = 0,

    /** The entry is a regular file. */
    ARC_FILE_TYPE_REGULAR,

    /** The entry is a directory. */
    ARC_FILE_TYPE_DIRECTORY,

    /** The entry is another filesystem-supported type. */
    ARC_FILE_TYPE_OTHER
} arc_file_type_t;

/**
 * @brief Specifies the reference point used for seeking within a stream.
 */
typedef enum arc_seek_whence_t {
    /** Interpret the offset relative to the beginning of the stream. */
    ARC_SEEK_SET = 0,

    /** Interpret the offset relative to the current stream position. */
    ARC_SEEK_CUR,

    /** Interpret the offset relative to the end of the stream. */
    ARC_SEEK_END
} arc_seek_whence_t;

/**
 * @brief Options used to open or create a file stream.
 *
 * @attention 'access' must contain a valid arc_stream_access_t value.
 *
 * @attention 'creation' must contain a valid arc_stream_creation_t value.
 *
 * @note 'append' requests append behavior for writes. The backend determines
 *       how this behavior is implemented according to its underlying
 *       filesystem capabilities.
 *
 * @note The options object is borrowed for the duration of the opening
 *       operation and does not need to remain alive afterward.
 */
struct arc_stream_open_options {
    /** Access permissions requested for the stream. */
    arc_stream_access_t access;

    /** File creation behavior requested for the stream. */
    arc_stream_creation_t creation;

    /** Whether writes should be performed at the end of the file. */
    bool append;
};

/**
 * @brief Describes metadata associated with a filesystem entry.
 *
 * The structure describes an entry identified by a path rather than an
 * individual open stream.
 *
 * Timestamps use a backend-defined epoch and unit. Implementations of the
 * same filesystem format must use a consistent representation.
 *
 * @note 'size' is the size in bytes for a regular file. For entry types where
 *       a meaningful size is unavailable, the backend may report -1.
 *
 * @note 'attributes' contains backend-defined attribute bits. Their meanings
 *       depend on the filesystem implementation.
 */
struct arc_file_info {
    /** Type of the filesystem entry. */
    arc_file_type_t type;

    /** Entry size in bytes, or -1 if unavailable or inapplicable. */
    int64_t size;

    /** Entry creation timestamp, or -1 if unavailable. */
    int64_t creation_time;

    /** Entry last-modified timestamp, or -1 if unavailable. */
    int64_t last_modified_time;

    /** Entry last-access timestamp, or -1 if unavailable. */
    int64_t last_access_time;

    /** Backend-defined filesystem attributes. */
    uint64_t attributes;
};

/**
 * @brief Describes the current state and capabilities of an open stream.
 *
 * This structure describes a stream handle, not the path from which it was
 * opened.
 *
 * @note 'size' is the stream size in bytes, or -1 if the size is unknown.
 *
 * @note 'position' is the current stream position in bytes, or -1 if the
 *       position cannot be determined.
 */
struct arc_stream_info {
    /** Stream size in bytes, or -1 if unavailable. */
    int64_t size;

    /** Current stream position in bytes, or -1 if unavailable. */
    int64_t position;

    /** Whether the stream permits reading. */
    bool is_readable;

    /** Whether the stream permits writing. */
    bool is_writable;

    /** Whether the stream supports seeking. */
    bool is_seekable;
};

/**
 * @brief Creates an independent filesystem context.
 *
 * @param[in] context Context of the source filesystem.
 * @param[out] out_context Pointer receiving the newly created context.
 * @param[in] allocator Allocator used for allocations required by the clone.
 *
 * @return A successful arc_result_t on success.
 * @return ARC_ERROR_UNSUPPORTED_OPERATION if cloning is unsupported.
 *
 * @post On success, '*out_context' points to the newly created context.
 *
 * @post On failure, '*out_context' is NULL.
 *
 * @attention 'out_context' must not be NULL.
 *
 * @attention The returned context must be suitable for use with the source
 *            filesystem's interface.
 *
 * @attention The clone must not accidentally share ownership of resources
 *            whose lifetime is exclusive to the source context.
 *
 * @note Cloning a context does not require duplicating every underlying
 *       resource. The backend defines which resources are shared and which
 *       state is independent, provided the resulting context remains valid.
 *
 * @note The allocator is borrowed and must remain valid for as long as the
 *       cloned context may require it.
 */
typedef arc_result_t (*arc_filesystem_clone_fun_t)(const void* context, void** out_context, const arc_allocator_t* allocator);

/**
 * @brief Destroys a filesystem context.
 *
 * @param[in, out] context Context to destroy.
 * @param[in] allocator Allocator associated with the context's owned resources.
 *
 * @return A successful arc_result_t if the context is destroyed.
 * @return ARC_ERROR_UNSUPPORTED_OPERATION if context destruction is unsupported.
 *
 * @post On success, the context and its owned resources are no longer usable.
 *
 * @post On failure, the context must remain valid for a subsequent destruction
 *       attempt. Any resource that could not be released remains owned and must
 *       not be silently discarded.
 *
 * @attention This callback is used only when the filesystem owns its context.
 *
 * @attention The callback must not release the filesystem object itself.
 *
 * @note The allocator is borrowed. Destroying the context does not destroy the
 *       allocator object or its context.
 */
typedef arc_result_t (*arc_filesystem_destroy_fun_t)(void* context, const arc_allocator_t* allocator);

/**
 * @brief Opens a file and initializes a stream handle.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[out] out_stream Storage receiving the opened stream handle.
 * @param[in] path Path identifying the file to open or create.
 * @param[in] options Requested access and creation behavior.
 *
 * @return A successful arc_result_t on success.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 * @return An appropriate filesystem or underlying I/O error on failure.
 *
 * @post On success, 'out_stream' contains a valid open stream handle.
 *
 * @post On failure, no usable stream handle is transferred to the caller.
 *       Any resources acquired during the failed operation must be released
 *       or reported through the returned Result's Residue.
 *
 * @attention 'out_stream', 'path', and 'options' must not be NULL.
 *
 * @attention The path and options are borrowed for the duration of the call.
 *
 * @note Multiple streams may refer to the same file. Exclusivity and sharing
 *       restrictions are determined by the backend.
 */
typedef arc_result_t (*arc_open_fun_t)(void* context, arc_stream_handle_t** out_stream, const arc_path_t* path, const arc_stream_open_options_t* options);

/**
 * @brief Closes an open stream.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in, out] stream Handle of the stream to close.
 *
 * @return A successful arc_result_t if the stream is closed.
 * @return ARC_FILESYSTEM_ERROR_INVALID_STREAM if the stream is invalid.
 *
 * @post On success, the stream is no longer open and its handle is invalidated.
 *
 * @post On failure, the backend must preserve sufficient state to report
 *       whether the stream remains open and to permit a valid recovery attempt.
 *
 * @attention A successfully closed stream must not be used for further stream
 *            operations.
 *
 * @note Closing a stream does not destroy the filesystem context.
 */
typedef arc_result_t (*arc_close_fun_t)(void* context, arc_stream_handle_t* stream);

/**
 * @brief Closes every stream currently managed by the filesystem.
 *
 * @param[in, out] context Filesystem backend context.
 *
 * @return A successful arc_result_t if all managed streams are closed.
 *
 * @post On success, no stream previously managed by the backend remains open.
 *
 * @post On failure, streams that could not be closed remain accounted for by
 *       the backend and must not be silently forgotten.
 *
 * @note This operation does not destroy the filesystem context.
 *
 * @attention Stream handles invalidated by this operation must not be used
 *            afterward, even if they were obtained by other callers.
 */
typedef arc_result_t (*arc_close_all_fun_t)(void* context);

/**
 * @brief Reads bytes from an open stream.
 *
 * @param[in] context Filesystem backend context.
 * @param[in] stream Handle of the stream to read from.
 * @param[out] out Buffer receiving the bytes read.
 * @param[in] bytes_to_read Maximum number of bytes to read.
 * @param[out] out_bytes_read Pointer receiving the number of bytes read.
 *
 * @return A successful arc_result_t if the read operation succeeds.
 * @return ARC_FILESYSTEM_ERROR_INVALID_STREAM if the stream is invalid.
 *
 * @post On success, '*out_bytes_read' is at most 'bytes_to_read'.
 *
 * @post If zero bytes are requested, the operation succeeds without reading
 *       data and '*out_bytes_read' is zero.
 *
 * @post On failure, '*out_bytes_read' is zero.
 *
 * @attention If 'bytes_to_read' is greater than zero, 'out' must not be NULL.
 *
 * @attention 'out_bytes_read' must not be NULL.
 *
 * @note Reading fewer bytes than requested is not necessarily an error.
 *       End-of-file may be represented by a successful read of zero bytes.
 *
 * @note On success, the stream position advances by the number of bytes read.
 */
typedef arc_result_t (*arc_read_fun_t)(void* context, const arc_stream_handle_t* stream, void* out, size_t bytes_to_read, size_t* out_bytes_read);

/**
 * @brief Writes bytes to an open stream.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in, out] stream Handle of the stream to write to.
 * @param[in] buffer Source buffer containing the bytes to write.
 * @param[in] bytes_to_write Number of bytes requested for writing.
 * @param[out] out_bytes_written Pointer receiving the number of bytes written.
 *
 * @return A successful arc_result_t if the write operation succeeds.
 * @return ARC_FILESYSTEM_ERROR_INVALID_STREAM if the stream is invalid.
 *
 * @post On success, '*out_bytes_written' is at most 'bytes_to_write'.
 *
 * @post If zero bytes are requested, the operation succeeds without writing
 *       data and '*out_bytes_written' is zero.
 *
 * @post On failure, '*out_bytes_written' is zero unless the backend's contract
 *       explicitly permits reporting partial progress on failure.
 *
 * @attention If 'bytes_to_write' is greater than zero, 'buffer' must not be NULL.
 *
 * @attention 'out_bytes_written' must not be NULL.
 *
 * @note A successful write may write fewer bytes than requested. The caller is
 *       responsible for retrying when complete output is required.
 *
 * @note On success, the stream position advances according to the number of
 *       bytes written and the backend's append behavior.
 */
typedef arc_result_t (*arc_write_fun_t)(void* context, arc_stream_handle_t* stream, const void* buffer, size_t bytes_to_write, size_t* out_bytes_written);

/**
 * @brief Changes the position of an open stream.
 *
 * @param[in] context Filesystem backend context.
 * @param[in, out] stream Handle of the stream to seek.
 * @param[in] offset Signed offset relative to 'whence'.
 * @param[in] whence Reference point for the offset.
 *
 * @return A successful arc_result_t if the position is changed.
 * @return ARC_FILESYSTEM_ERROR_INVALID_STREAM if the stream is invalid.
 *
 * @post On success, the stream position is changed to the requested location.
 *
 * @post On failure, the stream position remains unchanged.
 *
 * @attention 'whence' must contain a valid arc_seek_whence_t value.
 *
 * @attention Seeking is supported only when the stream permits it.
 *
 * @note Whether positions beyond the current end of a file are permitted is
 *       determined by the backend.
 */
typedef arc_result_t (*arc_seek_fun_t)(const void* context, arc_stream_handle_t* stream, int64_t offset, arc_seek_whence_t whence);

/**
 * @brief Obtains the current position of an open stream.
 *
 * @param[in] context Filesystem backend context.
 * @param[in] stream Handle of the stream to inspect.
 * @param[out] out_position Pointer receiving the current position in bytes.
 *
 * @return A successful arc_result_t on success.
 * @return ARC_FILESYSTEM_ERROR_INVALID_STREAM if the stream is invalid.
 *
 * @post On failure, '*out_position' is zero.
 *
 * @attention 'out_position' must not be NULL.
 */
typedef arc_result_t (*arc_tell_fun_t)(void* context, const arc_stream_handle_t* stream, int64_t* out_position);

/**
 * @brief Flushes pending output for an open stream.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in, out] stream Handle of the stream to flush.
 *
 * @return A successful arc_result_t if pending output is flushed.
 * @return ARC_FILESYSTEM_ERROR_INVALID_STREAM if the stream is invalid.
 *
 * @post On success, buffered output covered by the backend's flush contract
 *       has been passed to the underlying filesystem.
 *
 * @note A successful flush does not necessarily guarantee persistence across
 *       sudden power loss unless the backend explicitly provides that guarantee.
 */
typedef arc_result_t (*arc_flush_fun_t)(void* context, arc_stream_handle_t* stream);

/**
 * @brief Removes a filesystem entry.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in] path Path identifying the entry to remove.
 *
 * @return A successful arc_result_t if the entry is removed.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, the identified entry no longer exists at that path.
 *
 * @post On failure, the operation must not report success or silently discard
 *       an entry that remains present.
 *
 * @attention This operation does not imply recursive removal of a directory.
 *            Recursive removal is provided by arc_rremove_fun_t.
 */
typedef arc_result_t (*arc_remove_fun_t)(void* context, const arc_path_t* path);

/**
 * @brief Removes a filesystem entry and its descendants recursively.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in] path Path identifying the entry to remove.
 *
 * @return A successful arc_result_t if the entry and its descendants are removed.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, the identified entry and all entries beneath it no longer
 *       exist.
 *
 * @post On failure, entries already removed may remain removed. The operation
 *       is not guaranteed to be atomic and must not claim that the original
 *       tree has been restored.
 *
 * @attention The implementation must not recursively remove entries outside
 *            the subtree identified by the supplied path.
 */
typedef arc_result_t (*arc_rremove_fun_t)(void* context, const arc_path_t* path);

/**
 * @brief Determines whether a path identifies a regular file.
 *
 * @param[in] context Filesystem backend context.
 * @param[in] path Path to inspect.
 * @param[out] out_is_file Pointer receiving the result.
 *
 * @return A successful arc_result_t on success.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, '*out_is_file' is true exactly when the entry is a regular
 *       file according to the backend.
 *
 * @post On failure, '*out_is_file' is false.
 *
 * @attention 'out_is_file' must not be NULL.
 */
typedef arc_result_t (*arc_is_file_fun_t)(const void* context, const arc_path_t* path, bool* out_is_file);

/**
 * @brief Determines whether a path identifies a directory.
 *
 * @param[in] context Filesystem backend context.
 * @param[in] path Path to inspect.
 * @param[out] out_is_directory Pointer receiving the result.
 *
 * @return A successful arc_result_t on success.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, '*out_is_directory' is true exactly when the entry is a
 *       directory according to the backend.
 *
 * @post On failure, '*out_is_directory' is false.
 *
 * @attention 'out_is_directory' must not be NULL.
 */
typedef arc_result_t (*arc_is_directory_fun_t)(const void* context, const arc_path_t* path, bool* out_is_directory);

/**
 * @brief Creates a directory at the specified path.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in] path Path of the directory to create.
 *
 * @return A successful arc_result_t if the directory is created.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, the path identifies a directory.
 *
 * @note Whether missing parent directories are created automatically is
 *       determined by the backend's documented behavior.
 */
typedef arc_result_t (*arc_create_directory_fun_t)(void* context, const arc_path_t* path);

/**
 * @brief Opens a directory for iteration.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in] path Path of the directory to enumerate.
 * @param[out] out_it Storage receiving the initialized directory iterator.
 *
 * @return A successful arc_result_t if the directory is opened.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, 'out_it' contains an initialized iterator associated with
 *       the specified directory.
 *
 * @post On failure, the iterator is not left in a usable initialized state.
 *       Any resources acquired during the failed operation must be released
 *       or reported through the returned Result's Residue.
 *
 * @attention 'out_it' and 'path' must not be NULL.
 *
 * @note The iterator's advancement, entry retrieval, and destruction contracts
 *       are defined by the directory iterator API.
 *
 * @attention Destroying the filesystem invalidates its directory iterators.
 */
typedef arc_result_t (*arc_open_directory_fun_t)(void* context, const arc_path_t* path, arc_dir_iterator_t* out_it);

/**
 * @brief Obtains information about an open stream.
 *
 * @param[in] context Filesystem backend context.
 * @param[in] stream Stream handle to inspect.
 * @param[out] out_info Structure receiving stream information.
 *
 * @return A successful arc_result_t on success.
 * @return ARC_FILESYSTEM_ERROR_INVALID_STREAM if the stream is invalid.
 *
 * @post On success, 'out_info' contains the information available for the stream.
 *
 * @post On failure, 'out_info' is zero-initialized.
 *
 * @attention 'out_info' must not be NULL.
 */
typedef arc_result_t (*arc_get_stream_info_fun_t)(const void* context, const arc_stream_handle_t* stream, arc_stream_info_t* out_info);

/**
 * @brief Determines whether a filesystem entry exists.
 *
 * @param[in] context Filesystem backend context.
 * @param[in] path Path to inspect.
 * @param[out] out_exists Pointer receiving the result.
 *
 * @return A successful arc_result_t if existence can be determined.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, '*out_exists' is true if the entry exists and false if it
 *       does not exist.
 *
 * @post On failure, '*out_exists' is false.
 *
 * @attention 'out_exists' must not be NULL.
 *
 * @note A missing entry is not itself an operation failure. Access failures
 *       and other errors that prevent the backend from determining existence
 *       are reported through the Result.
 */
typedef arc_result_t (*arc_exists_fun_t)(const void* context, const arc_path_t* path, bool* out_exists);

/**
 * @brief Obtains metadata for a filesystem entry.
 *
 * @param[in] context Filesystem backend context.
 * @param[in] path Path identifying the entry.
 * @param[out] out_info Structure receiving the entry metadata.
 *
 * @return A successful arc_result_t on success.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, 'out_info' contains the metadata available for the entry.
 *
 * @post On failure, 'out_info' is zero-initialized.
 *
 * @attention 'out_info' must not be NULL.
 *
 * @note Metadata that is not available from the backend is represented using
 *       the documented unknown or unavailable values of arc_file_info_t.
 */
typedef arc_result_t (*arc_get_file_info_fun_t)(const void* context, const arc_path_t* path, arc_file_info_t* out_info);

/**
 * @brief Renames or moves a filesystem entry.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in] source Path identifying the existing entry.
 * @param[in] destination Destination path for the entry.
 *
 * @return A successful arc_result_t if the operation succeeds.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if either path is invalid.
 *
 * @post On success, the entry is identified by 'destination' rather than
 *       'source'.
 *
 * @post On failure, the operation's partial effects, if any, are governed by
 *       the backend's documented rename guarantees.
 *
 * @attention 'source' and 'destination' must not be NULL.
 *
 * @note Whether an existing destination may be replaced is determined by the
 *       backend and must be documented by that implementation.
 */
typedef arc_result_t (*arc_rename_fun_t)(void* context, const arc_path_t* source, const arc_path_t* destination);

/**
 * @brief Copies a filesystem entry to another path.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in] source Path identifying the entry to copy.
 * @param[in] destination Destination path for the copy.
 *
 * @return A successful arc_result_t if the copy succeeds.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if either path is invalid.
 *
 * @post On success, the destination contains the copied entry according to
 *       the backend's copy semantics. The source remains unchanged.
 *
 * @post On failure, a partially created destination may remain unless the
 *       backend explicitly guarantees cleanup or atomicity.
 *
 * @attention 'source' and 'destination' must not be NULL.
 *
 * @note Copying directory trees recursively is not implied by this callback
 *       unless the backend explicitly defines that behavior.
 */
typedef arc_result_t (*arc_copy_fun_t)(void* context, const arc_path_t* source, const arc_path_t* destination);

/**
 * @brief Obtains the filesystem's current directory.
 *
 * @param[in] context Filesystem backend context.
 * @param[out] out_path Path object receiving the current directory.
 *
 * @return A successful arc_result_t on success.
 *
 * @post On success, 'out_path' represents the current directory according to
 *       the backend.
 *
 * @post On failure, 'out_path' remains in its documented empty state.
 *
 * @attention 'out_path' must not be NULL and must be suitable for receiving
 *            a path value according to the path API.
 *
 * @note The returned path's storage and ownership follow the path API's
 *       contract.
 */
typedef arc_result_t (*arc_get_current_directory_fun_t)(const void* context, arc_path_t* out_path);

/**
 * @brief Changes the filesystem's current directory.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[in] path Path identifying the new current directory.
 *
 * @return A successful arc_result_t if the current directory is changed.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, subsequent operations whose path semantics depend on the
 *       current directory use the new directory.
 *
 * @post On failure, the current directory remains unchanged.
 *
 * @attention 'path' must identify an existing directory.
 *
 * @note Current-directory state belongs to the filesystem context. Whether
 *       cloned filesystems share or independently maintain this state is
 *       determined by the clone implementation.
 */
typedef arc_result_t (*arc_set_current_directory_fun_t)(void* context, const arc_path_t* path);

/**
 * @brief Creates a temporary file and opens it as a stream.
 *
 * @param[in, out] context Filesystem backend context.
 * @param[out] out_stream Storage receiving the opened stream handle.
 * @param[in] path Path specifying the location or naming input for the
 *                 temporary file, according to the backend's contract.
 * @param[in] options Requested stream access and creation options.
 *
 * @return A successful arc_result_t if the temporary file is created and opened.
 * @return ARC_FILESYSTEM_ERROR_INVALID_PATH if the path is invalid.
 *
 * @post On success, 'out_stream' contains a valid open stream handle.
 *
 * @post On failure, no usable stream handle is transferred to the caller.
 *       Resources acquired during the failed operation must be released or
 *       reported through the returned Result's Residue.
 *
 * @attention 'out_stream', 'path', and 'options' must not be NULL.
 *
 * @note The exact meaning of 'path', the uniqueness guarantee, and the
 *       cleanup policy for temporary files must be specified by the backend.
 *       A temporary-file implementation intended for secure use must avoid
 *       selecting a name and creating the file in separate, race-prone steps.
 */
typedef arc_result_t (*arc_create_temp_file_fun_t)(void* context, arc_stream_handle_t* out_stream, const arc_path_t* path, const arc_stream_open_options_t* options);

/**
 * @brief Filesystem backend interface.
 *
 * The interface provides the operations supported by a filesystem
 * implementation. The filesystem stores a copy of this structure.
 *
 * All callbacks other than 'clone' and 'destroy' are required for a complete
 * interface. Those two callbacks may be NULL when cloning or context
 * destruction is not supported by the backend.
 *
 * @attention Every required callback must be non-NULL before the interface is
 *            used. An incomplete interface is an invalid descriptor and must
 *            be rejected during filesystem initialization.
 *
 * @attention Callback implementations must satisfy their documented output,
 *            state, and ownership guarantees on both success and failure.
 *
 * @note The callbacks receive the backend context explicitly. They must not
 *       assume that the filesystem object itself is stored inside that context.
 */
struct arc_filesystem_interface {
    /**
     * @brief Creates an independent context for cloning.
     *
     * May be NULL if cloning is unsupported.
     */
    arc_filesystem_clone_fun_t clone;

    /**
     * @brief Destroys an owned context.
     *
     * May be NULL if context destruction is unsupported. If a filesystem owns
     * a context, its destruction operation must account for this callback's
     * availability before attempting to destroy the context.
     */
    arc_filesystem_destroy_fun_t destroy;

    /** Opens or creates a file stream. */
    arc_open_fun_t open;

    /** Closes one open stream. */
    arc_close_fun_t close;

    /** Closes every stream managed by the backend. */
    arc_close_all_fun_t close_all;

    /** Reads bytes from an open stream. */
    arc_read_fun_t read;

    /** Writes bytes to an open stream. */
    arc_write_fun_t write;

    /** Changes an open stream's position. */
    arc_seek_fun_t seek;

    /** Obtains an open stream's current position. */
    arc_tell_fun_t tell;

    /** Flushes pending output for an open stream. */
    arc_flush_fun_t flush;

    /** Removes a filesystem entry without implying recursive removal. */
    arc_remove_fun_t remove;

    /** Removes a filesystem entry and its descendants recursively. */
    arc_rremove_fun_t remove_recursive;

    /** Determines whether a path identifies a regular file. */
    arc_is_file_fun_t is_file;

    /** Determines whether a path identifies a directory. */
    arc_is_directory_fun_t is_directory;

    /** Creates a directory. */
    arc_create_directory_fun_t create_directory;

    /** Opens a directory for iteration. */
    arc_open_directory_fun_t open_directory;

    /** Obtains information about an open stream. */
    arc_get_stream_info_fun_t get_stream_info;

    /** Determines whether a filesystem entry exists. */
    arc_exists_fun_t exists;

    /** Obtains metadata for a filesystem entry. */
    arc_get_file_info_fun_t get_file_info;

    /** Renames or moves a filesystem entry. */
    arc_rename_fun_t rename;

    /** Copies a filesystem entry. */
    arc_copy_fun_t copy;

    /** Obtains the current directory. */
    arc_get_current_directory_fun_t get_current_directory;

    /** Changes the current directory. */
    arc_set_current_directory_fun_t set_current_directory;

    /** Creates and opens a temporary file. */
    arc_create_temp_file_fun_t create_temp_file;
};

/**
 * @brief Describes the initial state of a filesystem.
 *
 * The descriptor provides the allocator, backend interface, and initial
 * backend context used to initialize a filesystem object.
 *
 * @attention 'allocator' must not be NULL and must refer to a valid initialized
 *            allocator suitable for the filesystem implementation.
 *
 * @attention Every required callback in 'interface' must be non-NULL.
 *            'clone' and 'destroy' may be NULL.
 *
 * @attention The allocator object and its context must remain valid for the
 *            lifetime of any filesystem state that depends on them.
 *
 * @note The descriptor is borrowed during initialization and does not need to
 *       remain alive afterward.
 *
 * @note The context supplied here is not implicitly owned merely because a
 *       destroy callback exists. Ownership is established by the initialization
 *       contract and represented by the initialized filesystem's owns_context.
 */
struct arc_filesystem_desc {
    /** Borrowed allocator used for filesystem-owned allocations. */
    const arc_allocator_t* allocator;

    /** Backend operations copied into the filesystem object. */
    const arc_filesystem_interface_t interface;

    /** Backend-specific context used by the callbacks. */
    void* context;
};

/**
 * @brief Represents an initialized filesystem.
 *
 * The filesystem object stores its backend context, context ownership state,
 * allocator reference, and backend interface.
 *
 * The object storage is caller-owned and is not allocated or freed by the
 * filesystem context callbacks.
 *
 * @attention The allocator is borrowed and must remain valid for the lifetime
 *            of the filesystem and any cleanup operations that may use it.
 *
 * @attention A filesystem must not be used after successful destruction.
 *
 * @attention Destroying a filesystem invalidates streams and directory
 *            iterators associated with that filesystem. Those handles must
 *            not be used afterward.
 *
 * @note The owns_context field determines whether context destruction is the
 *       responsibility of the filesystem. It does not determine ownership of
 *       the filesystem object itself.
 */
struct arc_filesystem {
    /** Backend-specific context passed to interface callbacks. */
    void* context;

    /** Whether this filesystem owns its backend context. */
    bool owns_context;

    /** Borrowed allocator used by this filesystem. */
    const arc_allocator_t* allocator;

    /** Backend operations available to this filesystem. */
    arc_filesystem_interface_t interface;
};

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

#ifdef __cplusplus
}
#endif

#endif /* ARC_FILESYSTEM_H_ */
