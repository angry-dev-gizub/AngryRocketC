/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include <stdint.h>
#include <string.h>

#include "allocator/allocator.h"
#include "iterator/iterator.h"
#include "algorithm/common/range.h"
#include "array_internal.h"
#include "array.h"

//======================================
// PRIVATE HELPERS
//======================================

/**
 * @brief Checks if the provided Array descriptor is valid.
 *
 * @param desc Pointer to the Array descriptor to validate.
 *
 * @return true if the descriptor is valid, false otherwise.
 */
static bool array_desc_is_valid(const ArrayDesc* desc) {
    if(!desc)
        return false;

    if(desc->base_capacity == 0 || desc->element_size == 0)
        return false;

    if(desc->base_capacity > SIZE_MAX / desc->element_size)
        return false;

    if(desc->growth_factor < ARRAY_GROWTH_NONE || desc->growth_factor > ARRAY_GROWTH_2X)
        return false;

    if(!desc->object_allocator || !desc->storage_allocator)
        return false;

    if(!desc->object_allocator->initialized || !desc->storage_allocator->initialized)
        return false;

    if(!allocator_has_features(desc->object_allocator, ALLOCATOR_FEATURE_FREE) ||
       !allocator_has_features(desc->storage_allocator, ALLOCATOR_FEATURE_FREE))
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
static bool detect_aliasing(const Array* array, const void* ptr, size_t size) {
    if(!array || !ptr || size == 0)
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
 * @brief Checks whether an Array is valid and initialized.
 *
 * @param[in] array Pointer to the Array to check.
 * @return true if the Array is valid and initialized, false otherwise.
 */
bool array_is_valid(const Array* array) {
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
        case ARRAY_GROWTH_NONE:
        case ARRAY_GROWTH_2X:
        case ARRAY_GROWTH_1_5X:
        case ARRAY_GROWTH_1_125X:
            break;

        default:
            return false;
    }

    if(!array->object_allocator || !array->storage_allocator)
        return false;

    if(!array->object_allocator->initialized || !array->storage_allocator->initialized)
        return false;

    if(!allocator_has_features(array->object_allocator, ALLOCATOR_FEATURE_FREE))
        return false;

    if(!allocator_has_features(array->storage_allocator, ALLOCATOR_FEATURE_FREE))
        return false;

    if(!array->data)
        return false;

    return true;
}

/**
 * @brief Resizes the Array's internal storage to 'new_capacity'.
 *
 * @param[in, out] array Array to resize.
 * @param[in] new_capacity New capacity in number of elements.
 *
 * @return true if resizing succeeds, false otherwise.
 *
 * @post On failure the Array object's fields remain unchanged. If rollback of
 *       a temporary replacement allocation also fails, that cleanup residue is a
 *       known limitation of the current bool-based helper.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            iterators, ranges, sentinels, and internal-storage pointers are
 *            invalidated.
 */
static bool array_resize(Array* array, size_t new_capacity) {
    if(!array || !array_is_valid(array))
        return false;

    if(new_capacity == 0)
        return false;

    if(new_capacity < array->size)
        return false;

    if(new_capacity > SIZE_MAX / array->element_size)
        return false;

    if(allocator_has_features(array->storage_allocator, ALLOCATOR_FEATURE_REALLOC)) {
        AllocResult result =
            allocator_realloc(array->storage_allocator, array->data, array->element_size * new_capacity);
        if(!result.ok)
            return false;
        array->data     = result.as.ptr;
        array->capacity = new_capacity;
        return true;
    }

    AllocResult result = allocator_alloc(array->storage_allocator, array->element_size * new_capacity);
    if(!result.ok)
        return false;

    memcpy(result.as.ptr, array->data, array->element_size * array->size);

    if(allocator_free(array->storage_allocator, array->data) != RELEASE_SUCCESS) {
        allocator_free(array->storage_allocator, result.as.ptr);
        return false; // @todo maybe add a residue parameter to free the allocation.
    }

    array->data     = result.as.ptr;
    array->capacity = new_capacity;
    return true;
}

/**
 * @brief Computes the next capacity for a dynamic Array according to its growth factor.
 *
 * @param[in] current_capacity Current capacity of the Array.
 * @param[in] growth_factor Growth factor of the Array.
 *
 * @return The next capacity according to the growth factor. Returns
 *         'current_capacity' for ARRAY_GROWTH_NONE, or zero if growth would
 *         overflow size_t or the growth factor is unrecognized.
 */
static size_t get_growth_capacity(size_t current_capacity, ArrayGrowthFactor growth_factor) {
    size_t extra = 0;

    switch(growth_factor) {
        case ARRAY_GROWTH_NONE:
            return current_capacity;

        case ARRAY_GROWTH_2X:
            if(current_capacity > SIZE_MAX / 2)
                return 0;

            return current_capacity * 2;

        case ARRAY_GROWTH_1_5X:
            extra = current_capacity / 2 + (current_capacity % 2 != 0);

            if(current_capacity > SIZE_MAX - extra)
                return 0;

            return current_capacity + extra;

        case ARRAY_GROWTH_1_125X:
            extra = current_capacity / 8 + (current_capacity % 8 != 0);

            if(current_capacity > SIZE_MAX - extra)
                return 0;

            return current_capacity + extra;

        default:
            return 0;
    }
}

//======================================
// LIFECYCLE
//======================================

/**
 * @brief Creates a new empty Array using the supplied descriptor.
 *
 * The Array object is allocated using 'object_allocator' and its internal
 * storage is allocated using 'storage_allocator'.
 *
 * @param[out] out Pointer receiving the newly created Array.
 * @param[in] desc Read-only Array configuration descriptor.
 *
 * @return ARRAY_SUCCESS if the Array was created successfully, or an error
 *         describing why creation failed.
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
 * @note If Array-object allocation succeeds but storage allocation fails, the
 *       implementation attempts to release the already allocated Array object
 *       before returning. A cleanup-release failure is currently reported as an
 *       allocator error and remains a known residue-reporting TODO.
 */
ArrayResult array_new(Array** out, const ArrayDesc* desc) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!array_desc_is_valid(desc))
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    size_t storage_size = desc->element_size * desc->base_capacity;

    AllocResult a_result = allocator_alloc(desc->object_allocator, sizeof(Array));

    if(!a_result.ok)
        return ARRAY_ERROR_ALLOCATOR_ERROR;

    Array* array = a_result.as.ptr;

    AllocResult d_result = allocator_alloc(desc->storage_allocator, storage_size);

    if(!d_result.ok) {
        if(allocator_free(desc->object_allocator, array) != RELEASE_SUCCESS)
            return ARRAY_ERROR_ALLOCATOR_ERROR; // @todo the error system is not explicit enough.
        return ARRAY_ERROR_ALLOCATOR_ERROR;
    }

    *array = (Array){
        .data              = d_result.as.ptr,
        .size              = 0,
        .element_size      = desc->element_size,
        .capacity          = desc->base_capacity,
        .base_capacity     = desc->base_capacity,
        .growth_factor     = desc->growth_factor,
        .object_allocator  = desc->object_allocator,
        .storage_allocator = desc->storage_allocator,
    };

    *out = array;

    return ARRAY_SUCCESS;
}

/**
 * @brief Destroys an Array and releases all storage owned by it.
 *
 * The internal element storage is released before the Array object itself.
 *
 * @param[in, out] array Array to destroy. May be NULL.
 *
 * @return ARRAY_SUCCESS if destruction succeeds.
 *
 * @note Passing NULL results in a successful no-op.
 *
 * @attention All pointers, iterators, sentinels, and ranges referring to the
 *            Array become invalid after successful destruction.
 *
 * @attention If releasing the internal storage fails, the Array object itself
 *            is not released.
 */
ArrayResult array_destroy(Array* array) {
    if(!array)
        return ARRAY_SUCCESS;

    Allocator* obj_allocator = array->object_allocator;

    if(array->data) {
        if(allocator_free(array->storage_allocator, array->data) != RELEASE_SUCCESS)
            return ARRAY_ERROR_ALLOCATOR_ERROR;

        array->data     = NULL;
        array->size     = 0;
        array->capacity = 0;
    }

    if(allocator_free(obj_allocator, array) != RELEASE_SUCCESS)
        return ARRAY_ERROR_ALLOCATOR_ERROR;

    return ARRAY_SUCCESS;
}

//======================================
// ACCESSORS
//======================================

/**
 * @brief Obtains a read-only pointer to the element at 'index'.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the address of the requested element.
 * @param[in] index Zero-based element index.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer refers directly to internal Array storage.
 * @note Ownership of the element is not transferred to the caller.
 *
 * @attention The pointer remains valid only while the corresponding storage
 *            remains valid and no invalidating Array operation is performed.
 */
ArrayResult array_get_at(const Array* array, const void** out, size_t index) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(index >= array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    *out = (const char*)array->data + array->element_size * index;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains a read-only pointer to the first element.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the first element.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and follows the same invalidation
 *       rules as pointers returned by array_get_at().
 */
ArrayResult array_get_first(const Array* array, const void** out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_ERROR_OUT_OF_RANGE;

    *out = (const char*)array->data;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains a mutable pointer to the first logical element.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the first element.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed.
 * @note Writing through the pointer modifies element bytes only and does not
 *       invalidate existing iterators, ranges, sentinels, or borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
ArrayResult array_get_first_mut(Array* array, void** out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_ERROR_OUT_OF_RANGE;

    *out = (char*)array->data;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains a read-only pointer to the last element.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the last element.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and follows the same invalidation
 *       rules as pointers returned by array_get_at().
 */
ArrayResult array_get_last(const Array* array, const void** out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_ERROR_OUT_OF_RANGE;

    *out = (const char*)array->data + (array->size - 1) * array->element_size;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains a mutable pointer to the last logical element.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the last element.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On failure '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed.
 * @note Writing through the pointer modifies element bytes only and does not
 *       invalidate existing iterators, ranges, sentinels, or borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
ArrayResult array_get_last_mut(Array* array, void** out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_ERROR_OUT_OF_RANGE;

    *out = (char*)array->data + (array->size - 1) * array->element_size;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains a read-only pointer to the contiguous element storage.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the beginning of the logical element data.
 *
 * @return ARRAY_SUCCESS on success.
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
ArrayResult array_get_data(const Array* array, const void** out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_SUCCESS;

    *out = (const void*)array->data;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains a mutable pointer to the contiguous element storage.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the beginning of the
 *                 logical element data.
 *
 * @return ARRAY_SUCCESS on success.
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
ArrayResult array_get_data_mut(Array* array, void** out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_SUCCESS;

    *out = array->data;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains the read-only contiguous element storage and logical byte size.
 *
 * @param[in] array Array to access.
 * @param[out] out Pointer receiving the beginning of the logical element data.
 * @param[out] out_size Optional pointer receiving the logical data size in bytes.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On success, if 'out_size' is not NULL, '*out_size' equals
 *       'size * element_size'.
 * @post If the Array is empty, '*out' is NULL and, if 'out_size' is not NULL,
 *       '*out_size' is zero.
 * @post On failure, '*out' is NULL and, if 'out_size' is not NULL, '*out_size'
 *       is zero.
 *
 * @note The returned pointer is borrowed and follows the same invalidation rules
 *       as array_get_data().
 */
ArrayResult array_get_data_ex(const Array* array, const void** out, size_t* out_size) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(out_size)
        *out_size = (size_t)0;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_SUCCESS;

    *out = (const void*)array->data;
    if(out_size)
        *out_size = array->element_size * array->size;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains mutable contiguous element storage and logical byte size.
 *
 * @param[in, out] array Array to access.
 * @param[out] out Pointer receiving mutable access to the logical element data.
 * @param[out] out_size Optional pointer receiving the logical data size in bytes.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On success, if 'out_size' is not NULL, '*out_size' equals
 *       'size * element_size'.
 * @post If the Array is empty, '*out' is NULL and, if 'out_size' is not NULL,
 *       '*out_size' is zero.
 * @post On failure, '*out' is NULL and, if 'out_size' is not NULL, '*out_size'
 *       is zero.
 *
 * @note Writing within the returned logical byte range modifies element bytes
 *       only and does not invalidate existing iterators, ranges, sentinels, or
 *       borrowed pointers.
 *
 * @attention Any later structural mutation may invalidate the returned pointer.
 */
ArrayResult array_get_data_mut_ex(Array* array, void** out, size_t* out_size) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(out_size)
        *out_size = (size_t)0;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_SUCCESS;

    *out = array->data;
    if(out_size)
        *out_size = array->element_size * array->size;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains the fixed size in bytes of each Array element.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the element size.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to zero.
 */
ArrayResult array_get_element_size(const Array* array, size_t* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (size_t)0;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    *out = array->element_size;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains the number of currently stored elements.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the logical size.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to zero.
 */
ArrayResult array_get_size(const Array* array, size_t* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (size_t)0;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    *out = array->size;

    return ARRAY_SUCCESS;
}

/**
 * @brief Obtains the current element capacity.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the capacity.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to zero.
 */
ArrayResult array_get_capacity(const Array* array, size_t* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (size_t)0;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    *out = array->capacity;

    return ARRAY_SUCCESS;
}

/**
 * @brief Determines whether the Array supports automatic growth.
 *
 * An Array is dynamic exactly when its growth factor is not
 * ARRAY_GROWTH_NONE.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving the result.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to false.
 */
ArrayResult array_is_dynamic(const Array* array, bool* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = false;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    *out = (array->growth_factor != ARRAY_GROWTH_NONE);

    return ARRAY_SUCCESS;
}

/**
 * @brief Determines whether the Array contains no elements.
 *
 * @param[in] array Array to inspect.
 * @param[out] out Pointer receiving whether 'size == 0'.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure '*out' is set to false.
 */
ArrayResult array_is_empty(const Array* array, bool* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = false;

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    *out = (array->size == 0);

    return ARRAY_SUCCESS;
}

//======================================
// ELEMENT MODIFICATION
//======================================

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
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @attention 'element' must not refer to storage whose validity depends on the
 *            Array being modified.
 *
 * @post On failure the Array remains unchanged.
 *
 * @note Replacing element bytes does not change Array structure, size, capacity,
 *       or element addresses and therefore does not invalidate existing
 *       iterators, ranges, sentinels, or borrowed pointers.
 */
ArrayResult array_set_at(Array* array, size_t index, const void* element) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(index >= array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    if(!element)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(detect_aliasing(array, element, array->element_size))
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    void* dest = (char*)array->data + index * array->element_size;
    memcpy(dest, element, array->element_size);

    return ARRAY_SUCCESS;
}

//======================================
// CAPACITY MANAGEMENT
//======================================

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
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OPERATION_NOT_SUPPORTED for fixed-capacity arrays.
 *
 * @note If the resulting capacity already equals the current capacity, the
 *       operation is a successful no-op.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            iterators, ranges, sentinels, and internal-storage pointers are
 *            invalidated.
 */
ArrayResult array_shrink_to_fit(Array* array) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->growth_factor == ARRAY_GROWTH_NONE)
        return ARRAY_ERROR_OPERATION_NOT_SUPPORTED;

    size_t new_capacity = (array->size > 0) ? array->size : array->base_capacity;

    if(new_capacity == array->capacity)
        return ARRAY_SUCCESS;

    if(!array_resize(array, new_capacity))
        return ARRAY_ERROR_ALLOCATOR_ERROR;

    return ARRAY_SUCCESS;
}

/**
 * @brief Ensures that a dynamic Array has capacity for at least 'new_cap'
 *        elements.
 *
 * @param[in, out] array Array whose capacity should be reserved.
 * @param[in] new_cap Minimum requested capacity.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OPERATION_NOT_SUPPORTED for fixed-capacity arrays.
 *
 * @note For dynamic Arrays, if 'new_cap <= current capacity', the operation is
 *       a successful no-op.
 * @note The operation never reduces capacity.
 * @note The operation never changes logical size.
 * @note Newly reserved storage contains unspecified bytes.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention If storage capacity changes successfully, all previously obtained
 *            iterators, ranges, sentinels, and internal-storage pointers are
 *            invalidated.
 */
ArrayResult array_reserve(Array* array, size_t new_cap) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->growth_factor == ARRAY_GROWTH_NONE)
        return ARRAY_ERROR_OPERATION_NOT_SUPPORTED;

    if(new_cap <= array->capacity)
        return ARRAY_SUCCESS;

    if(new_cap > SIZE_MAX / array->element_size)
        return ARRAY_ERROR_OUT_OF_SPACE;

    if(!array_resize(array, new_cap))
        return ARRAY_ERROR_ALLOCATOR_ERROR;

    return ARRAY_SUCCESS;
}

/**
 * @brief Removes all logical elements without changing capacity.
 *
 * @param[in, out] array Array to clear.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On success:
 *       - size becomes zero;
 *       - capacity remains unchanged;
 *       - existing element bytes may remain physically present but are no
 *         longer logically accessible.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators, ranges, sentinels, and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_clear(Array* array) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    array->size = 0;
    return ARRAY_SUCCESS;
}

//======================================
// INSERTION
//======================================

/**
 * @brief Appends one element to the end of an Array.
 *
 * Exactly 'element_size' bytes are copied from 'element'.
 *
 * @param[in, out] array Array to modify.
 * @param[in] element Pointer to the source element. Must not be NULL.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_SPACE if a fixed-capacity Array is full.
 *
 * @note A dynamic Array grows automatically when necessary according to its
 *       configured growth factor.
 *
 * @attention Self-aliasing is not supported. 'element' must not point into the
 *            destination Array's own storage.
 *
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators, ranges, sentinels, and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_append(Array* array, const void* element) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(!element)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(detect_aliasing(array, element, array->element_size))
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(array->size == array->capacity) {
        if(array->growth_factor == ARRAY_GROWTH_NONE)
            return ARRAY_ERROR_OUT_OF_SPACE;

        size_t new_capacity = get_growth_capacity(array->capacity, array->growth_factor);

        if(new_capacity == 0)
            return ARRAY_ERROR_OUT_OF_SPACE;

        if(new_capacity > SIZE_MAX / array->element_size)
            return ARRAY_ERROR_OUT_OF_SPACE;

        if(!array_resize(array, new_capacity))
            return ARRAY_ERROR_ALLOCATOR_ERROR;
    }

    void* dest = (char*)array->data + array->size * array->element_size;

    memcpy(dest, element, array->element_size);

    array->size += 1;

    return ARRAY_SUCCESS;
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
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_SPACE if insertion requires unavailable capacity
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
 * @attention On success all previously obtained iterators, ranges, sentinels,
 *            and borrowed pointers into this Array are invalidated.
 */
ArrayResult array_prepend(Array* array, const void* element) {
    return array_insert(array, element, 0);
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
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_SPACE if insertion requires unavailable capacity
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
 * @attention On success all previously obtained iterators, ranges, sentinels, and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_insert(Array* array, const void* element, size_t index) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(index > array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    if(!element)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(detect_aliasing(array, element, array->element_size))
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(array->size == array->capacity) {
        if(array->growth_factor == ARRAY_GROWTH_NONE)
            return ARRAY_ERROR_OUT_OF_SPACE;

        size_t new_capacity = get_growth_capacity(array->capacity, array->growth_factor);

        if(new_capacity == 0)
            return ARRAY_ERROR_OUT_OF_SPACE;

        if(new_capacity > SIZE_MAX / array->element_size)
            return ARRAY_ERROR_OUT_OF_SPACE;

        if(!array_resize(array, new_capacity))
            return ARRAY_ERROR_ALLOCATOR_ERROR;
    }

    void* dest       = (char*)array->data + index * array->element_size;
    void* next_pos   = (char*)dest + array->element_size;
    size_t move_size = (array->size - index) * array->element_size;

    if(move_size > 0)
        memmove(next_pos, dest, move_size);

    memcpy(dest, element, array->element_size);

    array->size += 1;

    return ARRAY_SUCCESS;
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
 * @return ARRAY_SUCCESS on success.
 *
 * @note 'count == 0' results in a successful no-op and permits 'elements == NULL'.
 *
 * @attention If 'count > 0', 'elements' must not be NULL.
 * @attention Self-aliasing is not supported. The source memory must not overlap
 *            the destination Array's internal storage.
 *
 * @post Source ordering is preserved.
 * @post On failure the Array is restored to its exact state before the call.
 *
 * @attention On successful mutation all previously obtained iterators, ranges,
 *            sentinels, and borrowed pointers into this Array are invalidated.
 */
ArrayResult array_append_elements(Array* array, size_t count, const void* elements) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(count == 0)
        return ARRAY_SUCCESS;

    if(!elements)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(count > SIZE_MAX / array->element_size)
        return ARRAY_ERROR_OUT_OF_SPACE;

    size_t byte_count = count * array->element_size;

    if(detect_aliasing(array, elements, byte_count))
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(count > SIZE_MAX - array->size)
        return ARRAY_ERROR_OUT_OF_SPACE;

    size_t required_capacity = array->size + count;

    if(required_capacity > array->capacity) {
        if(array->growth_factor == ARRAY_GROWTH_NONE)
            return ARRAY_ERROR_OUT_OF_SPACE;

        size_t new_capacity = array->capacity;

        while(new_capacity < required_capacity) {
            new_capacity = get_growth_capacity(new_capacity, array->growth_factor);

            if(new_capacity == 0)
                return ARRAY_ERROR_OUT_OF_SPACE;
        }

        if(new_capacity > SIZE_MAX / array->element_size)
            return ARRAY_ERROR_OUT_OF_SPACE;

        if(!array_resize(array, new_capacity))
            return ARRAY_ERROR_ALLOCATOR_ERROR;
    }

    void* dest = (char*)array->data + array->size * array->element_size;

    memcpy(dest, elements, byte_count);

    array->size += count;

    return ARRAY_SUCCESS;
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
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index > size'.
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
 * @post On failure the Array is restored to its exact state before the call.
 *
 * @attention On successful mutation all previously obtained iterators, ranges,
 *            sentinels, and borrowed pointers into this Array are invalidated.
 */
ArrayResult array_insert_elements(Array* array, size_t index, size_t count, const void* elements) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(index > array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    if(count == 0)
        return ARRAY_SUCCESS;

    if(!elements)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(count > SIZE_MAX / array->element_size)
        return ARRAY_ERROR_OUT_OF_SPACE;

    size_t byte_count = count * array->element_size;

    if(detect_aliasing(array, elements, byte_count))
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(count > SIZE_MAX - array->size)
        return ARRAY_ERROR_OUT_OF_SPACE;

    size_t required_capacity = array->size + count;

    if(required_capacity > array->capacity) {
        if(array->growth_factor == ARRAY_GROWTH_NONE)
            return ARRAY_ERROR_OUT_OF_SPACE;

        size_t new_capacity = array->capacity;

        while(new_capacity < required_capacity) {
            new_capacity = get_growth_capacity(new_capacity, array->growth_factor);

            if(new_capacity == 0)
                return ARRAY_ERROR_OUT_OF_SPACE;
        }

        if(new_capacity > SIZE_MAX / array->element_size)
            return ARRAY_ERROR_OUT_OF_SPACE;

        if(!array_resize(array, new_capacity))
            return ARRAY_ERROR_ALLOCATOR_ERROR;
    }

    void* dest       = (char*)array->data + index * array->element_size;
    void* next_pos   = (char*)dest + byte_count;
    size_t move_size = (array->size - index) * array->element_size;

    if(move_size > 0)
        memmove(next_pos, dest, move_size);

    memcpy(dest, elements, byte_count);

    array->size += count;

    return ARRAY_SUCCESS;
}

//======================================
// REMOVAL
//======================================

/**
 * @brief Removes the first logical element from an Array.
 *
 * All elements following the first element are shifted one position toward the
 * beginning of the Array while preserving their relative order.
 *
 * @param[in, out] array Array to modify.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @note This operation is logically equivalent to removing the element at
 *       index zero.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators, ranges, sentinels,
 *            and borrowed pointers into this Array are invalidated.
 */
ArrayResult array_remove_first(Array* array) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_ERROR_OUT_OF_RANGE;

    void* target     = (char*)array->data;
    void* next_pos   = (char*)target + array->element_size;
    size_t move_size = (array->size - 1) * array->element_size;

    if(move_size > 0)
        memmove(target, next_pos, move_size);

    array->size -= 1;

    return ARRAY_SUCCESS;
}

/**
 * @brief Removes the last logical element.
 *
 * @param[in, out] array Array to modify.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if the Array is empty.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post Removed bytes may remain physically present in unused storage.
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators, ranges, sentinels, and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_remove_last(Array* array) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(array->size == 0)
        return ARRAY_ERROR_OUT_OF_RANGE;

    array->size -= 1;

    return ARRAY_SUCCESS;
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
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index >= size'.
 *
 * @post On success size decreases by one.
 * @post Capacity remains unchanged.
 * @post The Array is not automatically shrunk.
 * @post Removing 'size - 1' has the same logical effect as array_remove_last().
 * @post On failure the Array remains unchanged.
 *
 * @attention On success all previously obtained iterators, ranges, sentinels, and borrowed pointers
 *            into this Array are invalidated.
 */
ArrayResult array_remove_at(Array* array, size_t index) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(index >= array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    if(index == array->size - 1) {
        array->size -= 1;
        return ARRAY_SUCCESS;
    }

    void* target     = (char*)array->data + index * array->element_size;
    void* next_pos   = (char*)target + array->element_size;
    size_t move_size = (array->size - index - 1) * array->element_size;

    if(move_size > 0)
        memmove(target, next_pos, move_size);

    array->size -= 1;

    return ARRAY_SUCCESS;
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
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index > size' or if the requested
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
 * @attention If 'count > 0', all previously obtained iterators, ranges,
 *            sentinels, and borrowed pointers into this Array are invalidated.
 */
ArrayResult array_remove_elements(Array* array, size_t index, size_t count) {
    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(count == 0)
        return ARRAY_SUCCESS;

    if(index > array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    if(count > array->size - index)
        return ARRAY_ERROR_OUT_OF_RANGE;

    if(count > SIZE_MAX / array->element_size)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(index == array->size - 1 && count == 1) {
        array->size -= 1;
        return ARRAY_SUCCESS;
    }

    if(index == 0 && count == 1)
        return array_remove_first(array);

    size_t byte_count = count * array->element_size;
    void* target      = (char*)array->data + index * array->element_size;
    void* next_pos    = (char*)target + byte_count;
    size_t move_size  = (array->size - index - count) * array->element_size;

    if(move_size > 0)
        memmove(target, next_pos, move_size);

    array->size -= count;

    return ARRAY_SUCCESS;
}

//======================================
// RESULT UTILITIES
//======================================

/**
 * @brief Converts an ArrayResult value to a human-readable static string.
 *
 * @param[in] result Result code to convert.
 *
 * @return A human-readable description of 'result', or NULL if 'result' is not
 *         a valid ArrayResult value.
 *
 * @note The returned string has static lifetime and must not be modified or freed.
 */
const char* array_result_to_string(ArrayResult result) {
    switch(result) {
        case ARRAY_SUCCESS:
            return "array success";

        case ARRAY_ERROR_INVALID_ARGUMENTS:
            return "invalid arguments";

        case ARRAY_ERROR_INVALID_ARRAY:
            return "invalid array";

        case ARRAY_ERROR_OUT_OF_RANGE:
            return "out of range";

        case ARRAY_ERROR_OUT_OF_SPACE:
            return "out of space";

        case ARRAY_ERROR_ALLOCATOR_ERROR:
            return "allocator error";

        case ARRAY_ERROR_OPERATION_NOT_SUPPORTED:
            return "operation not supported";

        case ARRAY_ERROR_INVALID_ALLOCATOR:
            return "invalid allocator";

        case ARRAY_ERROR_ITERATOR_ERROR:
            return "iterator error";

        case ARRAY_ERROR_RANGE_ERROR:
            return "range error";

        default:
            return NULL;
    }
}

//======================================
// ITERATOR / RANGE API
//======================================

/**
 * @brief Creates an iterator positioned at the first logical element.
 *
 * The returned Array iterator provides random-access traversal.
 *
 * @param[in] array Array whose elements will be traversed.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @post On failure 'out' is reset to an invalid iterator state.
 *
 * @note If the Array is empty, the resulting begin iterator compares equal to
 *       the corresponding end iterator.
 *
 * @note The returned Iterator owns an independent traversal context allocated
 *       using the Array's object allocator.
 * @note The Iterator borrows the Array itself and does not own it.
 * @note Independent Array iterators have independent traversal state.
 *
 * @attention A successful result must eventually be passed to iterator_destroy().
 * @attention The returned iterator provides read-only element access through
 *            iterator_get().
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 * @attention The Array must outlive the returned iterator.
 * @attention Any invalidating Array mutation invalidates the iterator.
 */
ArrayResult array_get_begin_iterator(const Array* array, Iterator* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Iterator){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    ArrayIteratorContext context = {
        .array       = array,
        .start_index = 0,
        .end_index   = array->size,
        .position    = 0,
        .reverse     = false,
    };

    return array_iterator_init(out, &context);
}

/**
 * @brief Creates a reverse iterator positioned at the last logical element.
 *
 * Reverse traversal treats the Array's elements as a sequence ordered from the
 * highest logical index to the lowest logical index.
 *
 * @param[in] array Array whose elements will be traversed.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @note For a reverse iterator, iterator_next() moves toward lower Array indexes
 *       and iterator_previous() moves toward higher Array indexes.
 *
 * @note If the Array is empty, the resulting reverse begin iterator compares
 *       equal to the corresponding reverse end iterator.
 *
 * @note The returned Iterator owns an independent traversal context allocated
 *       using the Array's object allocator and borrows the Array itself.
 *
 * @attention A successful result must eventually be passed to iterator_destroy().
 * @attention The returned iterator provides read-only element access through
 *            iterator_get().
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 * @attention The Array must outlive the returned iterator.
 * @attention Any invalidating Array mutation invalidates the iterator.
 */
ArrayResult array_get_begin_reverse_iterator(const Array* array, Iterator* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Iterator){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    ArrayIteratorContext context = {
        .array       = array,
        .start_index = 0,
        .end_index   = array->size,
        .position    = 0,
        .reverse     = true,
    };

    return array_iterator_init(out, &context);
}

/**
 * @brief Creates a forward Array Iterator at a specific logical position.
 *
 * @param[in] array Array whose traversal domain is represented.
 * @param[in] index Logical position in the inclusive range '[0, size]'.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index > size'.
 *
 * @post If 'index < size', the Iterator is dereferenceable at that element.
 * @post If 'index == size', the Iterator represents the forward past-the-end
 *       position and must not be dereferenced.
 * @post On failure, 'out' is reset to an invalid Iterator state.
 *
 * @note The returned Iterator owns an independent traversal context allocated
 *       using the Array's object allocator and borrows the Array itself.
 *
 * @attention A successful result must eventually be passed to iterator_destroy().
 * @attention The Array and its object allocator must outlive the Iterator.
 * @attention Structural Array mutation invalidates the Iterator.
 */
ArrayResult array_get_iterator_at(const Array* array, size_t index, Iterator* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Iterator){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(index > array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    ArrayIteratorContext context = {
        .array       = array,
        .start_index = 0,
        .end_index   = array->size,
        .position    = index,
        .reverse     = false,
    };

    return array_iterator_init(out, &context);
}

/**
 * @brief Creates a past-the-end iterator for forward traversal.
 *
 * @param[in] array Array whose traversal domain is represented.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @note The resulting iterator must not be dereferenced.
 * @note It is compatible with forward Array iterators produced for the same Array.
 * @note The returned Iterator owns an independent traversal context allocated
 *       using the Array's object allocator and borrows the Array itself.
 *
 * @attention A successful result must eventually be passed to iterator_destroy().
 * @attention The returned iterator provides read-only element access through
 *            iterator_get().
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 * @attention The Array must outlive the returned iterator.
 * @attention Any invalidating Array mutation invalidates the iterator.
 */
ArrayResult array_get_end_iterator(const Array* array, Iterator* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Iterator){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    ArrayIteratorContext context = {
        .array       = array,
        .start_index = 0,
        .end_index   = array->size,
        .position    = array->size,
        .reverse     = false,
    };

    return array_iterator_init(out, &context);
}

/**
 * @brief Creates the past-the-end iterator for reverse traversal.
 *
 * The reverse end represents the position reached after traversing past the
 * first logical Array element in reverse order.
 *
 * @param[in] array Array whose reverse traversal domain is represented.
 * @param[out] out Iterator to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 *
 * @note The resulting iterator must not be dereferenced.
 * @note It is compatible with reverse Array iterators produced for the same Array.
 * @note The returned Iterator owns an independent traversal context allocated
 *       using the Array's object allocator and borrows the Array itself.
 *
 * @attention A successful result must eventually be passed to iterator_destroy().
 * @attention The returned iterator provides read-only element access through
 *            iterator_get().
 * @attention 'out' must not contain a live initialized Iterator.
 *            An existing Iterator must be destroyed before being reused as output.
 * @attention The Array must outlive the returned iterator.
 * @attention Any invalidating Array mutation invalidates the iterator.
 */
ArrayResult array_get_end_reverse_iterator(const Array* array, Iterator* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Iterator){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    ArrayIteratorContext context = {
        .array       = array,
        .start_index = 0,
        .end_index   = array->size,
        .position    = array->size,
        .reverse     = true,
    };

    return array_iterator_init(out, &context);
}

/**
 * @brief Creates a read-only forward Range spanning the entire logical Array.
 *
 * The Range traverses logical indexes '[0, size)' in increasing index order.
 *
 * @param[in] array Array whose elements will be traversed.
 * @param[out] out Range to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_RANGE_ERROR if endpoint construction or Range ownership
 *         initialization fails.
 *
 * @post On success, 'out' owns an independent beginning Iterator and Sentinel.
 * @post If the Array is empty, the beginning Iterator immediately satisfies the
 *       Sentinel and the Range contains zero elements.
 * @post On failure:
 *       - 'out' is not a valid usable Range;
 *       - any endpoint resources that can be released successfully are released;
 *       - if rollback destruction fails, 'out' may retain ownership of an
 *         unreleased endpoint and must be passed to range_destroy().
 *
 * @note The Range provides read-only element access.
 * @note Endpoint contexts are allocated using the Array's object allocator.
 *
 * @attention 'out' must not contain any live owned endpoint state.
 *            An existing or partially constructed Range must be successfully
 *            destroyed before being reused as output.
 * @attention A successful Range must eventually be passed to range_destroy().
 * @attention The Array and its object allocator must outlive the Range.
 * @attention Structural Array mutation invalidates the Range.
 */
ArrayResult array_get_range(const Array* array, Range* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Range){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    Iterator begin     = {0};
    ArrayResult result = array_get_begin_iterator(array, &begin);
    if(result != ARRAY_SUCCESS)
        return result;

    ArraySentinelContext context = {
        .array       = array,
        .start_index = 0,
        .end_index   = array->size,
        .position    = array->size,
        .reverse     = false,
    };

    Sentinel end = {0};
    result       = array_sentinel_init(&end, &context);

    if(result != ARRAY_SUCCESS) {
        if(iterator_destroy(&begin) != ITERATOR_SUCCESS)
            return ARRAY_ERROR_ITERATOR_ERROR;
        return result;
    }

    RangeResult range_result               = range_init(out, &begin, &end, array->object_allocator);
    IteratorResult iterator_destroy_result = iterator_destroy(&begin);
    SentinelResult sentinel_destroy_result = sentinel_destroy(&end);

    if(iterator_destroy_result != ITERATOR_SUCCESS)
        return ARRAY_ERROR_ITERATOR_ERROR;

    if(sentinel_destroy_result != SENTINEL_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    if(range_result != RANGE_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    return ARRAY_SUCCESS;
}

/**
 * @brief Creates a read-only forward Range over '[start_index, end_index)'.
 *
 * @param[in] array Array whose elements will be traversed.
 * @param[in] start_index Inclusive first logical index.
 * @param[in] end_index Exclusive logical end index.
 * @param[out] out Range to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'start_index > end_index' or
 *         'end_index > size'.
 * @return ARRAY_ERROR_RANGE_ERROR if endpoint construction or Range ownership
 *         initialization fails.
 *
 * @post The number of elements in the Range is 'end_index - start_index'.
 * @post If 'start_index == end_index', the Range is empty.
 * @post On failure:
 *       - 'out' is not a valid usable Range;
 *       - any endpoint resources that can be released successfully are released;
 *       - if rollback destruction fails, 'out' may retain ownership of an
 *         unreleased endpoint and must be passed to range_destroy().
 *
 * @note The Range provides read-only traversal in increasing logical index order.
 * @note Endpoint contexts are allocated using the Array's object allocator.
 *
 * @attention 'out' must not contain any live owned endpoint state.
 *            An existing or partially constructed Range must be successfully
 *            destroyed before being reused as output.
 * @attention A successful Range must eventually be passed to range_destroy().
 * @attention The Array and its object allocator must outlive the Range.
 * @attention Structural Array mutation invalidates the Range.
 */
ArrayResult array_get_range_ex(const Array* array, size_t start_index, size_t end_index, Range* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Range){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(start_index > end_index)
        return ARRAY_ERROR_OUT_OF_RANGE;

    if(end_index > array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    ArrayIteratorContext iterator_context = {
        .array       = array,
        .start_index = start_index,
        .end_index   = end_index,
        .position    = 0,
        .reverse     = false,
    };

    Iterator begin = {0};

    ArrayResult result = array_iterator_init(&begin, &iterator_context);

    if(result != ARRAY_SUCCESS)
        return result;

    ArraySentinelContext sentinel_context = {
        .array       = array,
        .start_index = start_index,
        .end_index   = end_index,
        .position    = end_index - start_index,
        .reverse     = false,
    };

    Sentinel end = {0};

    result = array_sentinel_init(&end, &sentinel_context);

    if(result != ARRAY_SUCCESS) {
        if(iterator_destroy(&begin) != ITERATOR_SUCCESS)
            return ARRAY_ERROR_ITERATOR_ERROR;

        return result;
    }

    RangeResult range_result = range_init(out, &begin, &end, array->object_allocator);

    IteratorResult iterator_destroy_result = iterator_destroy(&begin);

    SentinelResult sentinel_destroy_result = sentinel_destroy(&end);

    if(iterator_destroy_result != ITERATOR_SUCCESS)
        return ARRAY_ERROR_ITERATOR_ERROR;

    if(sentinel_destroy_result != SENTINEL_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    if(range_result != RANGE_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    return ARRAY_SUCCESS;
}
/**
 * @brief Creates a read-only reverse Range spanning the entire logical Array.
 *
 * The Range describes the same logical interval '[0, size)' as array_get_range()
 * but traverses it in decreasing logical index order.
 *
 * @param[in] array Array whose elements will be traversed.
 * @param[out] out Range to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_RANGE_ERROR if endpoint construction or Range ownership
 *         initialization fails.
 *
 * @post For a non-empty Array, traversal begins at logical index 'size - 1' and
 *       continues toward index zero.
 * @post If the Array is empty, the Range contains zero elements.
 * @post On failure:
 *       - 'out' is not a valid usable Range;
 *       - any endpoint resources that can be released successfully are released;
 *       - if rollback destruction fails, 'out' may retain ownership of an
 *         unreleased endpoint and must be passed to range_destroy().
 *
 * @note The Range provides read-only element access.
 * @note Endpoint contexts are allocated using the Array's object allocator.
 *
 * @attention 'out' must not contain any live owned endpoint state.
 *            An existing or partially constructed Range must be successfully
 *            destroyed before being reused as output.
 * @attention A successful Range must eventually be passed to range_destroy().
 * @attention The Array and its object allocator must outlive the Range.
 * @attention Structural Array mutation invalidates the Range.
 */
ArrayResult array_get_reverse_range(const Array* array, Range* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Range){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    Iterator begin     = {0};
    ArrayResult result = array_get_begin_reverse_iterator(array, &begin);
    if(result != ARRAY_SUCCESS)
        return result;

    ArraySentinelContext context = {
        .array       = array,
        .start_index = 0,
        .end_index   = array->size,
        .position    = array->size,
        .reverse     = true,
    };

    Sentinel end = {0};
    result       = array_sentinel_init(&end, &context);

    if(result != ARRAY_SUCCESS) {
        if(iterator_destroy(&begin) != ITERATOR_SUCCESS)
            return ARRAY_ERROR_ITERATOR_ERROR;
        return result;
    }

    RangeResult range_result               = range_init(out, &begin, &end, array->object_allocator);
    IteratorResult iterator_destroy_result = iterator_destroy(&begin);
    SentinelResult sentinel_destroy_result = sentinel_destroy(&end);

    if(iterator_destroy_result != ITERATOR_SUCCESS)
        return ARRAY_ERROR_ITERATOR_ERROR;

    if(sentinel_destroy_result != SENTINEL_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    if(range_result != RANGE_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    return ARRAY_SUCCESS;
}

/**
 * @brief Creates a read-only reverse Range over '[start_index, end_index)'.
 *
 * The logical interval uses the same half-open indexing convention as
 * array_get_range_ex(), but traversal order is reversed.
 *
 * For a non-empty interval, the first traversed element is 'end_index - 1' and
 * the last traversed element is 'start_index'.
 *
 * @param[in] array Array whose elements will be traversed.
 * @param[in] start_index Inclusive lower logical index of the interval.
 * @param[in] end_index Exclusive upper logical index of the interval.
 * @param[out] out Range to initialize.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'start_index > end_index' or
 *         'end_index > size'.
 * @return ARRAY_ERROR_RANGE_ERROR if endpoint construction or Range ownership
 *         initialization fails.
 *
 * @post The number of elements in the Range is 'end_index - start_index'.
 * @post If 'start_index == end_index', the Range is empty.
 * @post On failure:
 *       - 'out' is not a valid usable Range;
 *       - any endpoint resources that can be released successfully are released;
 *       - if rollback destruction fails, 'out' may retain ownership of an
 *         unreleased endpoint and must be passed to range_destroy().
 *
 * @note Endpoint contexts are allocated using the Array's object allocator.
 *
 * @attention 'out' must not contain any live owned endpoint state.
 *            An existing or partially constructed Range must be successfully
 *            destroyed before being reused as output.
 * @attention A successful Range must eventually be passed to range_destroy().
 * @attention The Array and its object allocator must outlive the Range.
 * @attention Structural Array mutation invalidates the Range.
 */
ArrayResult array_get_reverse_range_ex(const Array* array, size_t start_index, size_t end_index, Range* out) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    *out = (Range){0};

    if(!array || !array_is_valid(array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(start_index > end_index)
        return ARRAY_ERROR_OUT_OF_RANGE;

    if(end_index > array->size)
        return ARRAY_ERROR_OUT_OF_RANGE;

    ArrayIteratorContext iterator_context = {
        .array       = array,
        .start_index = start_index,
        .end_index   = end_index,
        .position    = 0,
        .reverse     = true,
    };

    Iterator begin = {0};

    ArrayResult result = array_iterator_init(&begin, &iterator_context);

    if(result != ARRAY_SUCCESS)
        return result;

    ArraySentinelContext sentinel_context = {
        .array       = array,
        .start_index = start_index,
        .end_index   = end_index,
        .position    = end_index - start_index,
        .reverse     = true,
    };

    Sentinel end = {0};

    result = array_sentinel_init(&end, &sentinel_context);

    if(result != ARRAY_SUCCESS) {
        if(iterator_destroy(&begin) != ITERATOR_SUCCESS)
            return ARRAY_ERROR_ITERATOR_ERROR;

        return result;
    }

    RangeResult range_result = range_init(out, &begin, &end, array->object_allocator);

    IteratorResult iterator_destroy_result = iterator_destroy(&begin);

    SentinelResult sentinel_destroy_result = sentinel_destroy(&end);

    if(iterator_destroy_result != ITERATOR_SUCCESS)
        return ARRAY_ERROR_ITERATOR_ERROR;

    if(sentinel_destroy_result != SENTINEL_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    if(range_result != RANGE_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    return ARRAY_SUCCESS;
}

/** @todo implement these.. lol I couldn't */

/**
 * @brief Appends all elements traversed by a Range.
 *
 * The source Range is borrowed and is not modified by this operation.
 *
 * The element type size reported by the Range must equal the destination
 * Array's 'element_size'. A mismatch results in ARRAY_ERROR_RANGE_ERROR.
 *
 * @param[in, out] array Destination Array.
 * @param[in] range Source Range. Must not be NULL and must be valid.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_RANGE_ERROR if the Range is invalid, incompatible, or an
 *         underlying Range/Iterator/Sentinel operation fails.
 * @return ARRAY_ERROR_OUT_OF_SPACE if required capacity cannot be provided.
 *
 * @attention The source Range must not refer to the destination Array itself.
 *            Self-range insertion is not supported.
 *
 * @note Forward traversal capability is sufficient.
 * @note The implementation clones 'range->begin' internally and leaves the
 *       caller-provided Range unchanged.
 * @note An empty Range results in a successful no-op.
 * @note Random-access Iterators may be used to estimate the Range size and
 *       reserve storage efficiently, but are not required.
 *
 * @post On failure the destination Array is restored to its exact state before
 *       the call.
 *
 * @attention On successful mutation all previously obtained iterators, ranges,
 *            sentinels, and borrowed pointers into the destination Array are
 *            invalidated.
 */
ArrayResult array_append_range(Array* array, const Range* range);

/**
 * @brief Inserts all elements traversed by a Range at 'index'.
 *
 * Source element order is preserved according to the Range's traversal order.
 * The source Range is borrowed and is not modified.
 *
 * @param[in, out] array Destination Array.
 * @param[in] range Source Range. Must not be NULL and must be valid.
 * @param[in] index Destination insertion index.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_RANGE_ERROR if the Range is invalid, incompatible, or an
 *         underlying Range/Iterator/Sentinel operation fails.
 * @return ARRAY_ERROR_OUT_OF_SPACE if required capacity cannot be provided.
 * @return ARRAY_ERROR_OUT_OF_RANGE if 'index > size'.
 *
 * @attention Valid insertion indexes are in the inclusive range '[0, size]'.
 * @attention The source Range must not refer to the destination Array itself.
 *            Self-range insertion is not supported.
 *
 * @note Forward traversal capability is sufficient.
 * @note The implementation clones 'range->begin' internally and leaves the
 *       caller-provided Range unchanged.
 * @note An empty source Range results in a successful no-op.
 *
 * @post On failure the destination Array is restored to its exact state before
 *       the call.
 *
 * @attention On successful mutation all previously obtained iterators, ranges,
 *            sentinels, and borrowed pointers into the destination Array are
 *            invalidated.
 */
ArrayResult array_insert_range(Array* array, const Range* range, size_t index);
