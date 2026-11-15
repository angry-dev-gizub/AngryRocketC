/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include <stdint.h>
#include <string.h>

#include "allocator/allocator.h"
#include "array.h"
#include "_array_def.h"

/**
 * @brief Checks if the provided Array descriptor is valid.
 *
 * @param desc Pointer to the Array descriptor to validate.
 *
 * @return true if the descriptor is valid, false otherwise.
 */
static bool arc_array_desc_is_valid(const arc_array_desc_t* desc) {
    if(!desc)
        return false;

    if(desc->base_capacity == 0 || desc->element_size == 0)
        return false;

    if(desc->base_capacity > SIZE_MAX / desc->element_size)
        return false;

    if(desc->growth_factor < ARC_ARRAY_GROWTH_NONE || desc->growth_factor > ARC_ARRAY_GROWTH_2X)
        return false;

    if(!desc->object_allocator || !desc->storage_allocator)
        return false;

    if(!arc_allocator_is_valid(desc->object_allocator) || !arc_allocator_is_valid(desc->storage_allocator))
        return false;

    if(!arc_allocator_has_features(desc->object_allocator, ARC_ALLOCATOR_FEATURE_FREE) ||
       !arc_allocator_has_features(desc->storage_allocator, ARC_ALLOCATOR_FEATURE_FREE))
        return false;

    return true;
}

/**
 * @brief Detects whether a pointer and size alias the internal storage of an Array.
 *
 * @param array Pointer to the Array to check.
 * @param ptr Pointer to the memory region to check for aliasing.
 * @param size Size of the memory region in bytes.
 *
 * @return true if the memory region overlaps with the Array's internal storage, false otherwise.
 */
static bool arc_array_detect_aliasing(const arc_array_t* array, const void* ptr, size_t size) {
    if(!array || !ptr || size == 0 || !arc_array_is_valid(array))
        return false;

    size_t storage_size = array->capacity * array->element_size;

    uintptr_t array_start = (uintptr_t)array->data;
    uintptr_t ptr_start   = (uintptr_t)ptr;

    if(array_start > UINTPTR_MAX - storage_size)
        return true;

    if(ptr_start > UINTPTR_MAX - size)
        return true;

    uintptr_t array_end = array_start + storage_size;
    uintptr_t ptr_end   = ptr_start + size;

    return ptr_start < array_end && ptr_end > array_start;
}

/**
 * @brief Resizes the Array's internal storage to 'new_capacity'.
 *
 * @param[in, out] array Array to resize.
 * @param[in] new_capacity New capacity in number of elements.
 *
 * @post On failure the Array object's fields remain unchanged. If rollback of
 *       a temporary replacement allocation also fails, its cleanup failure
 *       is returned with any residual ownership.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            internal-storage pointers are invalidated.
 */
static arc_result_t arc_array_resize(arc_array_t* array, size_t new_capacity) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(new_capacity == 0 || new_capacity < array->size)
        arc_abort("Arc array: invalid new capacity");

    if(new_capacity > SIZE_MAX / array->element_size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

    if(new_capacity == array->capacity)
        return arc_result_success();

    void* tmp = NULL;

    if(arc_allocator_has_features(array->storage_allocator, ARC_ALLOCATOR_FEATURE_REALLOC)) {
        arc_result_t result =
            arc_allocator_realloc(array->storage_allocator, array->data, array->element_size * new_capacity, &tmp);
        if(!result.ok)
            return result;
        array->data     = tmp;
        array->capacity = new_capacity;
        return arc_result_success();
    }

    arc_result_t result = arc_allocator_alloc(array->storage_allocator, array->element_size * new_capacity, &tmp);
    if(!result.ok)
        return result;

    memcpy(tmp, array->data, array->element_size * array->size);

    result = arc_allocator_free(array->storage_allocator, array->data);

    if(!result.ok) {
        arc_result_t r2 = arc_allocator_free(array->storage_allocator, tmp);
        if(!r2.ok)
            return r2;
        return result;
    }

    array->data     = tmp;
    array->capacity = new_capacity;
    return arc_result_success();
}

/**
 * @brief Computes the next capacity for a dynamic Array according to its growth factor.
 *
 * @param[in] current_capacity Current capacity of the Array.
 * @param[in] growth_factor Growth factor of the Array.
 *
 * @return The next capacity according to the growth factor. Returns
 *         'current_capacity' for ARC_ARRAY_GROWTH_NONE, or zero if growth would
 *         overflow size_t or the growth factor is unrecognized.
 */
static size_t arc_get_growth_capacity(size_t current_capacity, arc_array_growth_factor_t growth_factor) {
    size_t extra = 0;

    switch(growth_factor) {
        case ARC_ARRAY_GROWTH_NONE:
            return current_capacity;

        case ARC_ARRAY_GROWTH_2X:
            if(current_capacity > SIZE_MAX / 2)
                return 0;

            return current_capacity * 2;

        case ARC_ARRAY_GROWTH_1_5X:
            extra = current_capacity / 2 + (current_capacity % 2 != 0);

            if(current_capacity > SIZE_MAX - extra)
                return 0;

            return current_capacity + extra;

        case ARC_ARRAY_GROWTH_1_125X:
            extra = current_capacity / 8 + (current_capacity % 8 != 0);

            if(current_capacity > SIZE_MAX - extra)
                return 0;

            return current_capacity + extra;

        default:
            return 0;
    }
}

/**
 * @brief Creates a new empty Array using the supplied descriptor.
 *
 * The Array object is allocated using 'object_allocator' and its internal
 * storage is allocated using 'storage_allocator'.
 *
 * @param[out] out Pointer receiving the newly created Array.
 * @param[in] desc Read-only Array configuration descriptor.
 *
 * @return Successful Result if the Array was created; otherwise a failed
 *         Result containing the original allocation error, or a cleanup error
 *         if rollback release also fails.
 *
 * @attention 'out' must not be NULL.
 * @attention 'desc' must describe a valid Array configuration.
 *
 * @post On success:
 *       - '*out' refers to a valid Array;
 *       - the Array has size zero;
 *       - the Array has capacity equal to 'base_capacity';
 *       - its element size equals 'element_size';
 *       - its internal unused storage contains unspecified bytes.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note If Array-object allocation succeeds but storage allocation fails,
 *       creation attempts to release the Array object before returning.
 *       If rollback release fails, the cleanup failure is returned, not the
 *       original allocation failure. Its Result.residue owns the unreleased
 *       Array object; '*out' remains NULL. The caller must eventually release
 *       that residue. This contract requires the failed release to leave the
 *       object allocated and recoverable.
 */
arc_result_t arc_array_new(arc_array_t** out, const arc_array_desc_t* desc) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(!arc_array_desc_is_valid(desc))
        arc_abort("Arc array: invalid descriptor");

    size_t storage_size = desc->element_size * desc->base_capacity;

    void* object          = NULL;
    arc_result_t a_result = arc_allocator_alloc(desc->object_allocator, sizeof(arc_array_t), &object);

    if(!a_result.ok)
        return a_result;

    arc_array_t* array = object;

    void* data            = NULL;
    arc_result_t d_result = arc_allocator_alloc(desc->storage_allocator, storage_size, &data);

    if(!d_result.ok) {
        arc_result_t r = arc_allocator_free(desc->object_allocator, array);

        if(!r.ok) {
            if(r.residue.kind == ARC_RESIDUE_KIND_NONE) {
                arc_residue_t residue = {
                    .kind = ARC_RESIDUE_KIND_ALLOCATION, .ptr = array, .allocator = desc->object_allocator};

                return arc_result_failure_with_residue(r.error, residue);
            }

            return r;
        }

        return d_result;
    }

    *array = (arc_array_t){
        .data              = data,
        .size              = 0,
        .element_size      = desc->element_size,
        .capacity          = desc->base_capacity,
        .base_capacity     = desc->base_capacity,
        .growth_factor     = desc->growth_factor,
        .object_allocator  = desc->object_allocator,
        .storage_allocator = desc->storage_allocator,
    };

    *out = array;

    return arc_result_success();
}

/**
 * @brief Destroys an Array and releases all storage owned by it.
 *
 * The internal element storage is released before the Array object itself.
 *
 * @param[in, out] array Array to destroy. May be NULL.
 *
 * @return Successful Result if destruction succeeds; otherwise the release
 *         failure, preserving residual ownership when cleanup fails.
 *
 * @note Passing NULL results in a successful no-op.
 *
 * @attention All borrowed pointers referring to the
 *            Array become invalid after successful destruction.
 *
 * @attention If releasing internal storage fails, the Array object is not
 *            released. The cleanup failure is returned with its ownership
 *            information; the caller must inspect Result.residue if present.
 *
 * @attention If releasing the Array object fails after its storage has already
 *            been freed, return that cleanup failure with residual ownership
 *            of the Array object. Its former storage has already been released;
 *            the Array must not be used for normal operations.
 */
arc_result_t arc_array_destroy(arc_array_t* array) {
    if(!array)
        return arc_result_success();

    if(!arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    arc_allocator_t* obj_allocator = array->object_allocator;
    arc_result_t r;

    if(array->data) {
        r = arc_allocator_free(array->storage_allocator, array->data);
        if(!r.ok)
            return r;

        array->data     = NULL;
        array->size     = 0;
        array->capacity = 0;
    }

    r = arc_allocator_free(obj_allocator, array);
    if(!r.ok)
        return r;

    return arc_result_success();
}

/**
 * @brief Checks whether an Array is valid and initialized.
 *
 * @param[in] array Pointer to the Array to check.
 * @return true if the Array is valid and initialized, false otherwise.
 */
bool arc_array_is_valid(const arc_array_t* array) {
    if(!array)
        return false;

    if(array->base_capacity == 0 || array->element_size == 0)
        return false;

    if(array->base_capacity > SIZE_MAX / array->element_size)
        return false;

    if(array->capacity == 0)
        return false;

    if(array->capacity > SIZE_MAX / array->element_size)
        return false;

    if(array->size > array->capacity)
        return false;

    switch(array->growth_factor) {
        case ARC_ARRAY_GROWTH_NONE:
        case ARC_ARRAY_GROWTH_2X:
        case ARC_ARRAY_GROWTH_1_5X:
        case ARC_ARRAY_GROWTH_1_125X:
            break;

        default:
            return false;
    }

    if(!array->object_allocator || !array->storage_allocator)
        return false;

    if(!arc_allocator_is_valid(array->object_allocator) || !arc_allocator_is_valid(array->storage_allocator))
        return false;

    if(!arc_allocator_has_features(array->object_allocator, ARC_ALLOCATOR_FEATURE_FREE))
        return false;

    if(!arc_allocator_has_features(array->storage_allocator, ARC_ALLOCATOR_FEATURE_FREE))
        return false;

    if(!array->data)
        return false;

    return true;
}

/**
 * @brief Obtains a read-only pointer to the element at 'index'.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the address of the requested element.
 * @param[in] index Zero-based element index.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer refers directly to internal Array storage.
 * @note Ownership of the element is not transferred to the caller.
 *
 * @attention The pointer remains valid only while the corresponding storage
 *            remains valid and no invalidating Array operation is performed.
 */
arc_result_t arc_array_get_at(const arc_array_t* array, const void** out, size_t index) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(index >= array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    *out = (const char*)array->data + array->element_size * index;

    return arc_result_success();
}

/**
 * @brief Replaces the element stored at 'index'.
 *
 * Exactly 'element_size' bytes are copied from 'element' into the existing
 * array slot.
 *
 * @param[in, out] array Array to modify.
 * @param[in] index Zero-based index of the element to replace.
 * @param[in] element Pointer to the source element. Must not be NULL.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @attention 'element' must not refer to storage whose validity depends on the
 *            Array being modified.
 *
 * @post On failure the Array remains unchanged.
 *
 * @note Replacing element bytes does not change Array structure, size, capacity,
 *       or element addresses and therefore does not invalidate existing
 *       borrowed pointers.
 */
arc_result_t arc_array_set_at(arc_array_t* array, size_t index, const void* element) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(index >= array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    if(!element)
        arc_abort("Arc array: element is NULL");

    if(arc_array_detect_aliasing(array, element, array->element_size))
        return arc_result_failure(ARC_ARRAY_ERROR_ALIASING);

    void* dest = (char*)array->data + index * array->element_size;
    memcpy(dest, element, array->element_size);

    return arc_result_success();
}

/**
 * @brief Obtains a read-only pointer to the first element.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the first element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and follows the same invalidation
 *       rules as pointers returned by arc_array_get_at().
 */
arc_result_t arc_array_get_first(const arc_array_t* array, const void** out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    *out = (const char*)array->data;

    return arc_result_success();
}

/**
 * @brief Obtains a mutable pointer to the first logical element.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the first element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed.
 * @note Writing through the pointer modifies element bytes only and does not
 *       invalidate existing borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
arc_result_t arc_array_get_first_mut(arc_array_t* array, void** out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    *out = (char*)array->data;

    return arc_result_success();
}

/**
 * @brief Obtains a read-only pointer to the last element.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the last element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and follows the same invalidation
 *       rules as pointers returned by arc_array_get_at().
 */
arc_result_t arc_array_get_last(const arc_array_t* array, const void** out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    *out = (const char*)array->data + (array->size - 1) * array->element_size;

    return arc_result_success();
}

/**
 * @brief Obtains a mutable pointer to the last logical element.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the last element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed.
 * @note Writing through the pointer modifies element bytes only and does not
 *       invalidate existing borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
arc_result_t arc_array_get_last_mut(arc_array_t* array, void** out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    *out = (char*)array->data + (array->size - 1) * array->element_size;

    return arc_result_success();
}

/**
 * @brief Obtains a read-only pointer to the contiguous element storage.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the beginning of the logical element data.
 *
 * @return successful arc_result_t on success.
 *
 * @post If the Array is empty, '*out' is set to NULL even if storage capacity
 *       is currently allocated.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note Elements are stored contiguously with exactly 'element_size' bytes
 *       between consecutive elements.
 *
 * @note The returned pointer is borrowed and does not transfer ownership.
 */
arc_result_t arc_array_get_data(const arc_array_t* array, const void** out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_success();

    *out = (const void*)array->data;

    return arc_result_success();
}

/**
 * @brief Obtains a mutable pointer to the contiguous element storage.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the beginning of the
 *                 logical element data.
 *
 * @return successful arc_result_t on success.
 *
 * @post If the Array is empty, '*out' is set to NULL.
 * @post On failure '*out' is set to NULL.
 *
 * @note The caller may modify existing element bytes through the returned
 *       pointer but must not read or write beyond 'size * element_size' bytes.
 *
 * @note Modifying the returned memory does not change Array size or capacity.
 *
 * @attention The returned pointer is borrowed and becomes invalid when an Array
 *            operation invalidates internal storage pointers.
 */
arc_result_t arc_array_get_data_mut(arc_array_t* array, void** out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_success();

    *out = (void*)array->data;

    return arc_result_success();
}

/**
 * @brief Obtains the read-only contiguous element storage and logical byte size.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the beginning of the logical element data.
 * @param[out] out_size Pointer receiving the logical data size in bytes.
 *
 * @return successful arc_result_t on success.
 *
 * @post On success, '*out_size' equals 'size * element_size'.
 * @post If the Array is empty, '*out' is NULL and '*out_size' is zero.
 * @post On failure, '*out' is NULL and '*out_size' is zero.
 *
 * @note The returned pointer is borrowed and follows the same invalidation rules
 *       as arc_array_get_data().
 */
arc_result_t arc_array_get_data_ex(const arc_array_t* array, const void** out, size_t* out_size) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(out_size)
        *out_size = (size_t)0;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_success();

    *out = (const void*)array->data;
    if(out_size)
        *out_size = array->element_size * array->size;

    return arc_result_success();
}

/**
 * @brief Obtains mutable contiguous element storage and logical byte size.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the logical element data.
 * @param[out] out_size Pointer receiving the logical data size in bytes.
 *
 * @return successful arc_result_t on success.
 *
 * @post On success, '*out_size' equals 'size * element_size'.
 * @post If the Array is empty, '*out' is NULL and '*out_size' is zero.
 * @post On failure, '*out' is NULL and '*out_size' is zero.
 *
 * @note Writing within the returned logical byte range modifies element bytes
 *       only and does not invalidate existing 
 *       borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
arc_result_t arc_array_get_data_mut_ex(arc_array_t* array, void** out, size_t* out_size) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = NULL;

    if(out_size)
        *out_size = (size_t)0;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_success();

    *out = (void*)array->data;
    if(out_size)
        *out_size = array->element_size * array->size;

    return arc_result_success();
}

/**
 * @brief Obtains the fixed size in bytes of each Array element.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the element size.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to zero.
 */
arc_result_t arc_array_get_element_size(const arc_array_t* array, size_t* out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = (size_t)0;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    *out = array->element_size;

    return arc_result_success();
}

/**
 * @brief Obtains the number of currently stored elements.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the logical size.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to zero.
 */
arc_result_t arc_array_get_size(const arc_array_t* array, size_t* out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = (size_t)0;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_success();

    *out = array->size;

    return arc_result_success();
}

/**
 * @brief Obtains the current element capacity.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the capacity.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to zero.
 */
arc_result_t arc_array_get_capacity(const arc_array_t* array, size_t* out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = (size_t)0;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    *out = array->capacity;

    return arc_result_success();
}

/**
 * @brief Determines whether the Array supports automatic growth.
 *
 * An Array is dynamic exactly when its growth factor is not
 * ARC_ARRAY_GROWTH_NONE.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the result.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to false.
 */
arc_result_t arc_array_is_dynamic(const arc_array_t* array, bool* out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = false;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    *out = (array->growth_factor != ARC_ARRAY_GROWTH_NONE);

    return arc_result_success();
}

/**
 * @brief Determines whether the Array contains no elements.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving whether 'size == 0'.
 *
 * @return successful arc_result_t on success.
 *
 * @post On failure '*out' is set to false.
 */
arc_result_t arc_array_is_empty(const arc_array_t* array, bool* out) {
    if(!out)
        arc_abort("Arc array: output parameter is NULL");

    *out = false;

    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    *out = (array->size == 0);

    return arc_result_success();
}

/**
 * @brief Reduces a dynamic Array's capacity to the smallest permitted capacity.
 *
 * If the Array contains elements, the resulting capacity becomes exactly equal
 * to its logical size.
 *
 * If the Array is empty, the resulting capacity becomes the Array's original
 * base capacity.
 *
 * @param[in, out] array Array whose storage should be reduced.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ERROR_UNSUPPORTED_OPERATION (common domain) for fixed-capacity arrays.
 *
 * @note If the resulting capacity already equals the current capacity, the
 *       operation is a successful no-op.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            internal-storage pointers are
 *            invalidated.
 */
arc_result_t arc_array_shrink_to_fit(arc_array_t* array) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->growth_factor == ARC_ARRAY_GROWTH_NONE)
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);

    size_t new_capacity = (array->size > 0) ? array->size : array->base_capacity;

    if(new_capacity == array->capacity)
        return arc_result_success();

    arc_result_t r = arc_array_resize(array, new_capacity);
    if(!r.ok)
        return r;

    return arc_result_success();
}

/**
 * @brief Ensures that a dynamic Array has capacity for at least 'new_cap'
 *        elements.
 *
 * @param[in, out] array Array whose capacity should be reserved.
 * @param[in] new_cap Minimum requested capacity.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ERROR_UNSUPPORTED_OPERATION (common domain) for fixed-capacity arrays.
 *
 * @note If 'new_cap <= current capacity', the operation is a successful no-op.
 * @note The operation never reduces capacity.
 * @note The operation never changes logical size.
 * @note Newly reserved storage contains unspecified bytes.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            internal-storage pointers are
 *            invalidated.
 */
arc_result_t arc_array_reserve(arc_array_t* array, size_t new_cap) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->growth_factor == ARC_ARRAY_GROWTH_NONE)
        return arc_result_failure(ARC_ERROR_UNSUPPORTED_OPERATION);

    if(new_cap <= array->capacity)
        return arc_result_success();

    if(new_cap > SIZE_MAX / array->element_size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

    arc_result_t r = arc_array_resize(array, new_cap);
    if(!r.ok)
        return r;

    return arc_result_success();
}

/**
 * @brief Removes all logical elements without changing capacity.
 *
 * @param[in, out] array Array to clear.
 *
 * @return successful arc_result_t on success.
 *
 * @post On success:
 *       - size becomes zero;
 *       - capacity remains unchanged;
 *       - existing element bytes may remain physically present but are no
 *         longer logically accessible.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_clear(arc_array_t* array) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    array->size = 0;

    return arc_result_success();
}

/**
 * @brief Appends one element to the end of an Array.
 *
 * Exactly 'element_size' bytes are copied from 'element'.
 *
 * @param[in, out] array Array to modify.
 * @param[in] element Pointer to the source element. Must not be NULL.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_SPACE if a fixed-capacity Array is full.
 *
 * @note A dynamic Array grows automatically when necessary according to its
 *       configured growth factor.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_append(arc_array_t* array, const void* element) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(!element)
        arc_abort("Arc array: element is NULL");

    if(arc_array_detect_aliasing(array, element, array->element_size))
        return arc_result_failure(ARC_ARRAY_ERROR_ALIASING);

    if(array->size == array->capacity) {
        if(array->growth_factor == ARC_ARRAY_GROWTH_NONE)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        size_t new_capacity = arc_get_growth_capacity(array->capacity, array->growth_factor);

        if(new_capacity == 0)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        if(new_capacity > SIZE_MAX / array->element_size)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        arc_result_t r = arc_array_resize(array, new_capacity);
        if(!r.ok)
            return r;
    }

    void* dest = (char*)array->data + array->size * array->element_size;

    memcpy(dest, element, array->element_size);

    array->size += 1;

    return arc_result_success();
}

/**
 * @brief Inserts one element at the beginning of an Array.
 *
 * Existing elements are shifted one position toward higher indexes while
 * preserving their relative order.
 *
 * @param[in, out] array Array to modify.
 * @param[in] element Pointer to the source element. Must not be NULL.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_SPACE if insertion requires unavailable capacity
 *         in a fixed-capacity Array.
 *
 * @note This operation is logically equivalent to inserting at index zero.
 * @note A dynamic Array grows automatically when necessary according to its
 *       configured growth factor.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_prepend(arc_array_t* array, const void* element) {
    return arc_array_insert(array, element, 0);
}

/**
 * @brief Inserts one element at a specific logical index.
 *
 * Existing elements in the range '[index, size)' are shifted one position
 * toward higher indexes while preserving their relative order.
 *
 * @param[in, out] array Array to modify.
 * @param[in] element Pointer to the source element. Must not be NULL.
 * @param[in] index Insertion index.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_SPACE if insertion requires unavailable capacity
 *         in a fixed-capacity Array.
 *
 * @attention Valid insertion indexes are in the inclusive range '[0, size]'.
 *            Using 'index == size' is equivalent to appending the element.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_insert(arc_array_t* array, const void* element, size_t index) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(index > array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    if(!element)
        arc_abort("Arc array: element is NULL");

    if(arc_array_detect_aliasing(array, element, array->element_size))
        return arc_result_failure(ARC_ARRAY_ERROR_ALIASING);

    if(array->size == array->capacity) {
        if(array->growth_factor == ARC_ARRAY_GROWTH_NONE)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        size_t new_capacity = arc_get_growth_capacity(array->capacity, array->growth_factor);

        if(new_capacity == 0)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        if(new_capacity > SIZE_MAX / array->element_size)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        arc_result_t r = arc_array_resize(array, new_capacity);
        if(!r.ok)
            return r;
    }

    void* dest       = (char*)array->data + index * array->element_size;
    void* next_pos   = (char*)dest + array->element_size;
    size_t move_size = (array->size - index) * array->element_size;

    if(move_size > 0)
        memmove(next_pos, dest, move_size);

    memcpy(dest, element, array->element_size);

    array->size += 1;

    return arc_result_success();
}

/**
 * @brief Appends a contiguous sequence of elements.
 *
 * 'elements' must point to 'count' consecutive source elements, each exactly
 * 'element_size' bytes wide.
 *
 * @param[in, out] array Destination Array.
 * @param[in] count Number of elements to append.
 * @param[in] elements Pointer to the first source element.
 *
 * @return successful arc_result_t on success.
 *
 * @note 'count == 0' results in a successful no-op and permits 'elements == NULL'.
 *
 * @attention If 'count > 0', 'elements' must not be NULL.
 * @attention Self-aliasing is not supported. The source memory must not overlap
 *            the destination Array's internal storage.
 *
 * @post Source ordering is preserved.
 * @post On failure the Array is restored to its exact state before the call,
 *       provided every required rollback release succeeds. A failed rollback
 *       must report remaining ownership rather than silently leaking it.
 *
 * @attention On successful mutation all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_append_elements(arc_array_t* array, size_t count, const void* elements) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(count == 0)
        return arc_result_success();

    if(!elements)
        arc_abort("Arc array: elements is NULL");

    if(count > SIZE_MAX / array->element_size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

    if(count > SIZE_MAX - array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

    size_t byte_count = count * array->element_size;

    if(arc_array_detect_aliasing(array, elements, byte_count))
        return arc_result_failure(ARC_ARRAY_ERROR_ALIASING);

    size_t required_capacity = array->size + count;

    if(required_capacity > array->capacity) {
        if(array->growth_factor == ARC_ARRAY_GROWTH_NONE)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        size_t new_capacity = array->capacity;

        while(new_capacity < required_capacity) {
            new_capacity = arc_get_growth_capacity(new_capacity, array->growth_factor);

            if(new_capacity == 0)
                return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);
        }

        if(new_capacity > SIZE_MAX / array->element_size)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        arc_result_t r = arc_array_resize(array, new_capacity);
        if(!r.ok)
            return r;
    }

    void* dest = (char*)array->data + array->size * array->element_size;

    memcpy(dest, elements, byte_count);

    array->size += count;

    return arc_result_success();
}

/**
 * @brief Inserts a contiguous sequence of elements at 'index'.
 *
 * 'elements' must point to 'count' consecutive source elements, each exactly
 * 'element_size' bytes wide.
 *
 * Existing elements at and after 'index' are shifted toward higher indexes
 * while preserving order.
 *
 * @param[in, out] array Destination Array.
 * @param[in] index Destination insertion index.
 * @param[in] count Number of elements to insert.
 * @param[in] elements Pointer to the first source element.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index > size'.
 *
 * @attention Valid insertion indexes are in the inclusive range '[0, size]'.
 *
 * @note 'count == 0' results in a successful no-op and permits 'elements == NULL'.
 *
 * @attention If 'count > 0', 'elements' must not be NULL.
 * @attention Self-aliasing is not supported. The source memory must not overlap
 *            the destination Array's internal storage.
 *
 * @post Source ordering is preserved.
 * @post On failure the Array is restored to its exact state before the call,
 *       provided every required rollback release succeeds. A failed rollback
 *       must report remaining ownership rather than silently leaking it.
 *
 * @attention On successful mutation all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_insert_elements(arc_array_t* array, size_t index, size_t count, const void* elements) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(index > array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    if(count == 0)
        return arc_result_success();

    if(!elements)
        arc_abort("Arc array: elements is NULL");

    if(count > SIZE_MAX / array->element_size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

    if(count > SIZE_MAX - array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

    size_t byte_count = count * array->element_size;

    if(arc_array_detect_aliasing(array, elements, byte_count))
        return arc_result_failure(ARC_ARRAY_ERROR_ALIASING);

    size_t required_capacity = array->size + count;

    if(required_capacity > array->capacity) {
        if(array->growth_factor == ARC_ARRAY_GROWTH_NONE)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        size_t new_capacity = array->capacity;

        while(new_capacity < required_capacity) {
            new_capacity = arc_get_growth_capacity(new_capacity, array->growth_factor);

            if(new_capacity == 0)
                return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);
        }

        if(new_capacity > SIZE_MAX / array->element_size)
            return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_SPACE);

        arc_result_t r = arc_array_resize(array, new_capacity);
        if(!r.ok)
            return r;
    }

    void* dest       = (char*)array->data + index * array->element_size;
    void* next_pos   = (char*)dest + byte_count;
    size_t move_size = (array->size - index) * array->element_size;

    if(move_size > 0)
        memmove(next_pos, dest, move_size);

    memcpy(dest, elements, byte_count);

    array->size += count;

    return arc_result_success();
}

/**
 * @brief Removes the first logical element from an Array.
 *
 * All elements following the first element are shifted one position toward the
 * beginning of the Array while preserving their relative order.
 *
 * @param[in, out] array Array to modify.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @note This operation is logically equivalent to removing the element at
 *       index zero.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_remove_first(arc_array_t* array) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    void* target     = (char*)array->data;
    void* next_pos   = (char*)target + array->element_size;
    size_t move_size = (array->size - 1) * array->element_size;

    if(move_size > 0)
        memmove(target, next_pos, move_size);

    array->size -= 1;

    return arc_result_success();
}

/**
 * @brief Removes the last logical element.
 *
 * @param[in, out] array Array to modify.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_remove_last(arc_array_t* array) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(array->size == 0)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    array->size -= 1;

    return arc_result_success();
}

/**
 * @brief Removes the element at 'index' while preserving element order.
 *
 * Every element following 'index' is shifted one position toward the beginning
 * of the Array.
 *
 * @param[in, out] array Array to modify.
 * @param[in] index Zero-based index of the element to remove.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post The Array is not automatically shrunk.
 * @post Removing 'size - 1' has the same logical effect as arc_array_remove_last().
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained borrowed pointers
 *            into this Array are invalidated.
 */
arc_result_t arc_array_remove_at(arc_array_t* array, size_t index) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(index >= array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    void* target     = (char*)array->data + index * array->element_size;
    void* next_pos   = (char*)target + array->element_size;
    size_t move_size = (array->size - index - 1) * array->element_size;

    if(move_size > 0)
        memmove(target, next_pos, move_size);

    array->size -= 1;

    return arc_result_success();
}

/**
 * @brief Removes a contiguous sequence of logical elements.
 *
 * Removes 'count' elements beginning at 'index'. Elements following the
 * removed range are shifted toward the beginning of the Array while preserving
 * their relative order.
 *
 * @param[in, out] array Array to modify.
 * @param[in] index Zero-based index of the first element to remove.
 * @param[in] count Number of consecutive elements to remove.
 *
 * @return successful arc_result_t on success.
 * @return ARC_ARRAY_ERROR_OUT_OF_RANGE if 'index > size' or if the requested
 *         removal range extends beyond the logical size of the Array.
 *
 * @attention For 'count > 0', 'index' must refer to an existing logical
 *            element and the range '[index, index + count)' must be contained
 *            within '[0, size)'.
 *
 * @note 'count == 0' results in a successful no-op and permits
 *       'index == size'.
 *
 * @post On successful removal, size decreases by exactly 'count'.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention If 'count > 0', all previously obtained borrowed pointers into this Array are invalidated.
 */
arc_result_t arc_array_remove_elements(arc_array_t* array, size_t index, size_t count) {
    if(!array || !arc_array_is_valid(array))
        return arc_result_failure(ARC_ARRAY_ERROR_INVALID_ARRAY);

    if(index > array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    if(count == 0)
        return arc_result_success();

    if(index > array->size)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    if(count > array->size - index)
        return arc_result_failure(ARC_ARRAY_ERROR_OUT_OF_RANGE);

    if(index == array->size - 1 && count == 1) {
        array->size -= 1;
        return arc_result_success();
    }

    if(index == 0 && count == 1)
        return arc_array_remove_first(array);

    size_t byte_count = count * array->element_size;
    void* target      = (char*)array->data + index * array->element_size;
    void* next_pos    = (char*)target + byte_count;
    size_t move_size  = (array->size - index - count) * array->element_size;

    if(move_size > 0)
        memmove(target, next_pos, move_size);

    array->size -= count;

    return arc_result_success();
}
