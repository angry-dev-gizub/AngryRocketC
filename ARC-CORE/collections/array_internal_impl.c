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
#include "algorithm/common/sentinel.h"
#include "array_internal.h"

/**
 * @brief Checks whether an Array iterator context is valid.
 *
 * @param[in] context Pointer to the ArrayIteratorContext to check.
 * @return true if the context is valid, false otherwise.
 */
static bool array_iterator_context_is_valid(const ArrayIteratorContext* context) {
    if(!context)
        return false;

    if(!context->array || !array_is_valid(context->array))
        return false;

    if(context->start_index > context->end_index)
        return false;

    if(context->end_index > context->array->size)
        return false;

    size_t length = context->end_index - context->start_index;

    if(context->position > length)
        return false;

    return true;
}

// the formatting messed up here lol...
static bool array_iterator_contexts_are_compatible(const ArrayIteratorContext* a, const ArrayIteratorContext* b) {
    if(!array_iterator_context_is_valid(a) || !array_iterator_context_is_valid(b))
        return false;

    return a->array == b->array && a->start_index == b->start_index && a->end_index == b->end_index &&
           a->reverse == b->reverse;
}

/**
 * @brief Advances an Array iterator by one element in its traversal direction.
 *
 * For a forward iterator, advancing moves toward higher logical Array indexes.
 * For a reverse iterator, advancing moves toward lower logical Array indexes.
 *
 * The iterator context uses 'position' as a traversal-relative position inside
 * the half-open logical domain '[start_index, end_index)'.
 *
 * A dereferenceable iterator satisfies:
 *
 *     position < end_index - start_index
 *
 * The past-the-end iterator satisfies:
 *
 *     position == end_index - start_index
 *
 * @param[in, out] context Pointer to an ArrayIteratorContext.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'context' does not describe a
 *         valid Array iterator context.
 * @return ITERATOR_ERROR_OUT_OF_RANGE if the iterator is already at the
 *         past-the-end position and cannot be advanced further.
 *
 * @post On success, 'position' is increased by exactly one.
 * @post On failure, the context remains unchanged.
 *
 * @note The physical Array index is not stored directly in 'position'.
 *       'position' represents traversal progress within the iterator domain.
 */
static IteratorResult array_iterator_next(void* context) {
    ArrayIteratorContext* ctx = context;

    if(!array_iterator_context_is_valid(ctx))
        return ITERATOR_ERROR_INVALID_ITERATOR;

    size_t length = ctx->end_index - ctx->start_index;

    if(ctx->position == length)
        return ITERATOR_ERROR_OUT_OF_RANGE;

    ctx->position += 1;

    return ITERATOR_SUCCESS;
}

/**
 * @brief Moves an Array iterator one element backward relative to its traversal
 *        direction.
 *
 * For a forward iterator, moving backward moves toward lower logical Array
 * indexes.
 *
 * For a reverse iterator, moving backward moves toward higher logical Array
 * indexes.
 *
 * @param[in, out] context Pointer to an ArrayIteratorContext.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'context' does not describe a
 *         valid Array iterator context.
 * @return ITERATOR_ERROR_OUT_OF_RANGE if 'position == 0' and the iterator
 *         cannot move backward any further.
 *
 * @post On success, 'position' is decreased by exactly one.
 * @post On failure, the context remains unchanged.
 *
 * @note The operation is defined relative to traversal order rather than
 *       physical Array index order.
 */
static IteratorResult array_iterator_previous(void* context) {
    ArrayIteratorContext* ctx = context;

    if(!array_iterator_context_is_valid(ctx))
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(ctx->position == 0)
        return ITERATOR_ERROR_OUT_OF_RANGE;

    ctx->position -= 1;

    return ITERATOR_SUCCESS;
}

/**
 * @brief Moves an Array iterator by a signed traversal-relative offset.
 *
 * A positive offset moves in the same direction as iterator_next().
 * A negative offset moves in the same direction as iterator_previous().
 *
 * The resulting traversal position must remain inside the inclusive position
 * interval:
 *
 *     [0, end_index - start_index]
 *
 * The upper bound represents the past-the-end position and is valid as an
 * iterator position but must not be dereferenced.
 *
 * @param[in, out] context Pointer to an ArrayIteratorContext.
 * @param[in] offset Signed traversal-relative displacement.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'context' does not describe a
 *         valid Array iterator context.
 * @return ITERATOR_ERROR_OUT_OF_RANGE if applying 'offset' would move before
 *         position zero or beyond the past-the-end position.
 *
 * @post On success, 'position' is changed by exactly 'offset'.
 * @post On failure, the context remains unchanged.
 *
 * @attention Offset arithmetic must be checked without signed or unsigned
 *            integer overflow.
 */
static IteratorResult array_iterator_advance(void* context, ptrdiff_t offset) {
    ArrayIteratorContext* ctx = context;

    if(!array_iterator_context_is_valid(ctx))
        return ITERATOR_ERROR_INVALID_ITERATOR;

    size_t length = ctx->end_index - ctx->start_index;

    if(offset > 0) {
        size_t distance = (size_t)offset;

        if(distance > length - ctx->position)
            return ITERATOR_ERROR_OUT_OF_RANGE;

        ctx->position += distance;
    } else if(offset < 0) {
        size_t distance = (size_t)(-(offset + 1)) + 1;

        if(distance > ctx->position)
            return ITERATOR_ERROR_OUT_OF_RANGE;

        ctx->position -= distance;
    }

    return ITERATOR_SUCCESS;
}

/**
 * @brief Computes the traversal-relative distance between two compatible Array
 *        iterators.
 *
 * Both contexts must describe iterators belonging to the same traversal domain.
 * Compatible Array iterators must refer to:
 * - the same Array;
 * - the same 'start_index';
 * - the same 'end_index';
 * - the same traversal direction.
 *
 * The returned distance represents the signed number of iterator_next()
 * operations required to move from 'start' to 'end'.
 *
 * Conceptually:
 *
 *     distance = end.position - start.position
 *
 * @param[in] start Pointer to the starting ArrayIteratorContext.
 * @param[in] end Pointer to the ending ArrayIteratorContext.
 * @param[out] out Pointer receiving the signed traversal distance.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ARGUMENTS if 'out' is NULL.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if either context is invalid.
 * @return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS if the contexts belong to
 *         different traversal domains.
 * @return ITERATOR_ERROR_OUT_OF_RANGE if the distance cannot be represented
 *         by ptrdiff_t.
 *
 * @post On failure, '*out' is set to zero.
 *
 * @note Reverse traversal does not require special distance arithmetic because
 *       'position' already represents traversal-relative progress.
 */
static IteratorResult array_iterator_distance(const void* start, const void* end, ptrdiff_t* out) {
    if(!out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    *out = (ptrdiff_t)0;

    const ArrayIteratorContext* start_ctx = start;
    const ArrayIteratorContext* end_ctx   = end;

    if(!array_iterator_context_is_valid(start_ctx))
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!array_iterator_context_is_valid(end_ctx))
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!array_iterator_contexts_are_compatible(start_ctx, end_ctx))
        return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    if(end_ctx->position >= start_ctx->position) {
        size_t distance = end_ctx->position - start_ctx->position;

        if(distance > (size_t)PTRDIFF_MAX)
            return ITERATOR_ERROR_OUT_OF_RANGE;

        *out = (ptrdiff_t)distance;
    } else {
        size_t distance = start_ctx->position - end_ctx->position;

        if(distance > (size_t)PTRDIFF_MAX)
            return ITERATOR_ERROR_OUT_OF_RANGE;

        *out = -(ptrdiff_t)distance;
    }

    return ITERATOR_SUCCESS;
}

/**
 * @brief Obtains a read-only pointer to the element currently referenced by an
 *        Array iterator.
 *
 * The iterator must be positioned on an actual element. The past-the-end
 * position is valid for comparison and traversal but is not dereferenceable.
 *
 * For a forward iterator, the physical logical index is:
 *
 *     start_index + position
 *
 * For a reverse iterator, the physical logical index is:
 *
 *     end_index - 1 - position
 *
 * @param[in] context Pointer to an ArrayIteratorContext.
 * @param[out] out Pointer receiving the address of the referenced Array element.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ARGUMENTS if 'out' is NULL.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if the context is invalid.
 * @return ITERATOR_ERROR_OUT_OF_RANGE if the iterator is positioned at the
 *         past-the-end position.
 *
 * @post On failure, '*out' is set to NULL.
 *
 * @note The returned pointer is borrowed and refers directly to Array storage.
 * @note Ownership of the element is not transferred.
 *
 * @attention The Array must outlive the iterator.
 * @attention Structural Array mutation invalidates the returned pointer and the
 *            iterator itself.
 */
static IteratorResult array_iterator_get(const void* context, const void** out) {
    if(!out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    const ArrayIteratorContext* ctx = context;

    if(!array_iterator_context_is_valid(ctx))
        return ITERATOR_ERROR_INVALID_ITERATOR;

    size_t length = ctx->end_index - ctx->start_index;

    if(ctx->position == length)
        return ITERATOR_ERROR_OUT_OF_RANGE;

    size_t index;

    if(ctx->reverse) {
        index = ctx->end_index - 1 - ctx->position;
    } else {
        index = ctx->start_index + ctx->position;
    }

    *out = (const char*)ctx->array->data + index * ctx->array->element_size;

    return ITERATOR_SUCCESS;
}

/**
 * @brief Determines whether two Array iterators represent the same traversal
 *        position.
 *
 * Two Array iterators are comparable only when they belong to the same
 * traversal domain.
 *
 * Compatible contexts must refer to:
 * - the same Array;
 * - the same 'start_index';
 * - the same 'end_index';
 * - the same traversal direction.
 *
 * Compatible iterators compare equal exactly when their traversal-relative
 * 'position' values are equal.
 *
 * @param[in] a Pointer to the first ArrayIteratorContext.
 * @param[in] b Pointer to the second ArrayIteratorContext.
 * @param[out] out Pointer receiving the equality result.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ARGUMENTS if 'out' is NULL.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if either context is invalid.
 * @return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS if the contexts belong to
 *         different traversal domains.
 *
 * @post On failure, '*out' is set to false.
 */
static IteratorResult array_iterator_equals(const void* a, const void* b, bool* out) {
    if(!out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    *out = false;

    const ArrayIteratorContext* ctx_a = a;
    const ArrayIteratorContext* ctx_b = b;

    if(!array_iterator_context_is_valid(ctx_a) || !array_iterator_context_is_valid(ctx_b))
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!array_iterator_contexts_are_compatible(ctx_a, ctx_b))
        return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    *out = ctx_a->position == ctx_b->position;

    return ITERATOR_SUCCESS;
}

/**
 * @brief Creates an independent owned copy of an Array iterator context.
 *
 * A new ArrayIteratorContext is allocated using 'allocator' and initialized as
 * an exact copy of the supplied context.
 *
 * The cloned context refers to the same borrowed Array but has independent
 * traversal state. Advancing either context must not modify the other.
 *
 * @param[in] context Pointer to the ArrayIteratorContext to clone.
 * @param[out] out Pointer receiving the newly allocated context.
 * @param[in, out] allocator Allocator used to allocate the cloned context.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ARGUMENTS if 'out' is NULL.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'context' is invalid.
 * @return ITERATOR_ERROR_INVALID_ARGUMENTS if 'allocator' is NULL or unusable
 *         according to the Iterator clone contract.
 *
 * @post On success, '*out' points to an independently allocated
 *       ArrayIteratorContext.
 * @post On failure, '*out' is set to NULL.
 *
 * @attention The clone owns only the context object. The referenced Array is
 *            borrowed and must outlive the cloned Iterator.
 */
static IteratorResult array_iterator_clone(const void* context, void** out, Allocator* allocator) {
    if(!out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!allocator || !allocator->initialized)
        return ITERATOR_ERROR_INVALID_ALLOCATOR;

    if(!array_iterator_context_is_valid((const ArrayIteratorContext*)context))
        return ITERATOR_ERROR_INVALID_ITERATOR;

    AllocResult result = allocator_alloc(allocator, sizeof(ArrayIteratorContext));
    if(!result.ok)
        return ITERATOR_ERROR_ALLOCATOR_ERROR;

    *out = result.as.ptr;

    memcpy(*out, context, sizeof(ArrayIteratorContext));

    return ITERATOR_SUCCESS;
}

/**
 * @brief Releases an owned Array iterator context.
 *
 * The context is released using the allocator supplied by the generic Iterator
 * ownership system.
 *
 * @param[in, out] context Pointer to an owned ArrayIteratorContext.
 * @param[in, out] allocator Allocator used to release the context.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'context' is invalid.
 * @return ITERATOR_ERROR_INVALID_ARGUMENTS if 'allocator' is NULL or unusable
 *         according to the Iterator destruction contract.
 *
 * @post On success, the context memory has been released and must no longer be
 *       accessed.
 *
 * @attention If releasing the allocation fails, ownership remains with the
 *            caller so destruction may be retried according to the generic
 *            Iterator contract.
 */
static IteratorResult array_iterator_destroy(void* context, Allocator* allocator) {
    if(!context)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!allocator || !allocator->initialized)
        return ITERATOR_ERROR_INVALID_ALLOCATOR;

    if(!allocator_has_features(allocator, ALLOCATOR_FEATURE_FREE))
        return ITERATOR_ERROR_UNSUPPORTED_OPERATION;

    ReleaseResult result = allocator_free(allocator, context);

    if(result != RELEASE_SUCCESS)
        return ITERATOR_ERROR_ALLOCATOR_ERROR;

    return ITERATOR_SUCCESS;
}

static bool array_sentinel_context_is_valid(const ArraySentinelContext* context) {
    if(!context)
        return false;

    if(!context->array || !array_is_valid(context->array))
        return false;

    if(context->start_index > context->end_index)
        return false;

    if(context->end_index > context->array->size)
        return false;

    size_t length = context->end_index - context->start_index;

    if(context->position > length)
        return false;

    return true;
}

// bruh i will NOT modifie that formating file again....
static bool array_iterator_sentinel_contexts_are_compatible(const ArrayIteratorContext* iterator,
                                                            const ArraySentinelContext* sentinel) {
    if(!array_iterator_context_is_valid(iterator) || !array_sentinel_context_is_valid(sentinel))
        return false;

    return iterator->array == sentinel->array && iterator->start_index == sentinel->start_index &&
           iterator->end_index == sentinel->end_index && iterator->reverse == sentinel->reverse;
}

/**
 * @brief Determines whether an Array iterator has reached an Array sentinel.
 *
 * The Sentinel and Iterator must belong to the same traversal domain.
 *
 * Compatible contexts must refer to:
 * - the same Array;
 * - the same 'start_index';
 * - the same 'end_index';
 * - the same traversal direction.
 *
 * The iterator satisfies the Sentinel exactly when its traversal-relative
 * position equals the Sentinel position.
 *
 * For Array ranges, the Sentinel normally represents:
 *
 *     position == end_index - start_index
 *
 * @param[in] sentinel_ctx Pointer to an ArraySentinelContext.
 * @param[in] iterator_ctx Pointer to an ArrayIteratorContext.
 * @param[out] out Pointer receiving whether the iterator satisfies the Sentinel.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'out' is NULL.
 * @return SENTINEL_ERROR_INVALID_SENTINEL if 'sentinel_ctx' does not describe a
 *         valid Array sentinel.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'iterator_ctx' does not describe
 *         a valid Array iterator context.
 * @return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN if the Iterator and Sentinel refer
 *         to different traversal domains.
 *
 * @post On failure, '*out' is set to false.
 */
static SentinelResult array_sentinel_equals(const void* sentinel_ctx, const void* iterator_ctx, bool* out) {
    if(!out)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = false;

    ArrayIteratorContext* it_ctx = iterator_ctx;
    ArraySentinelContext* se_ctx = sentinel_ctx;

    if(!array_sentinel_context_is_valid(se_ctx))
        return SENTINEL_ERROR_INVALID_SENTINEL;

    if(!array_iterator_context_is_valid(it_ctx))
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    if(!array_iterator_sentinel_contexts_are_compatible(it_ctx, se_ctx))
        return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN;

    *out = (it_ctx->position == se_ctx->position);

    return SENTINEL_SUCCESS;
}

/**
 * @brief Creates an independent owned copy of an Array sentinel context.
 *
 * A new ArraySentinelContext is allocated using 'allocator' and initialized as
 * an exact copy of the supplied context.
 *
 * The cloned Sentinel refers to the same borrowed Array but owns an independent
 * context allocation.
 *
 * @param[in] context Pointer to the ArraySentinelContext to clone.
 * @param[out] out Pointer receiving the newly allocated context.
 * @param[in, out] allocator Allocator used to allocate the cloned context.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'out' is NULL.
 * @return SENTINEL_ERROR_INVALID_SENTINEL if 'context' is invalid.
 * @return SENTINEL_ERROR_INVALID_ALLOCATOR if 'allocator' is NULL, invalid, or
 *         does not provide the required allocation operation.
 * @return SENTINEL_ERROR_ALLOCATOR_ERROR if allocation fails.
 *
 * @post On success, '*out' points to an independently allocated
 *       ArraySentinelContext.
 * @post On failure, '*out' is set to NULL.
 *
 * @attention The clone owns only the context object. The referenced Array is
 *            borrowed and must outlive the cloned Sentinel.
 */
static SentinelResult array_sentinel_clone(const void* context, void** out, Allocator* allocator) {
    if(!out)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = NULL;

    if(!allocator || !allocator->initialized)
        return SENTINEL_ERROR_INVALID_ALLOCATOR;

    if(!array_sentinel_context_is_valid((const ArraySentinelContext*)context))
        return SENTINEL_ERROR_INVALID_SENTINEL;

    AllocResult result = allocator_alloc(allocator, sizeof(ArraySentinelContext));
    if(!result.ok)
        return SENTINEL_ERROR_ALLOCATOR_ERROR;

    *out = result.as.ptr;

    memcpy(*out, context, sizeof(ArraySentinelContext));

    return SENTINEL_SUCCESS;
}

/**
 * @brief Releases an owned Array sentinel context.
 *
 * The context is released using the allocator supplied by the generic Sentinel
 * ownership system.
 *
 * @param[in, out] context Pointer to an owned ArraySentinelContext.
 * @param[in, out] allocator Allocator used to release the context.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_SENTINEL if 'context' is invalid.
 * @return SENTINEL_ERROR_INVALID_ALLOCATOR if 'allocator' is NULL, invalid, or
 *         does not provide the required release operation.
 * @return SENTINEL_ERROR_ALLOCATOR_ERROR if releasing the context fails.
 *
 * @post On success, the context memory has been released and must no longer be
 *       accessed.
 *
 * @attention If releasing the allocation fails, ownership remains with the
 *            caller so destruction may be retried according to the generic
 *            Sentinel contract.
 */
static SentinelResult array_sentinel_destroy(void* context, Allocator* allocator) {
    if(!context)
        return SENTINEL_ERROR_INVALID_SENTINEL;

    if(!allocator || !allocator->initialized)
        return SENTINEL_ERROR_INVALID_ALLOCATOR;

    if(!allocator_has_features(allocator, ALLOCATOR_FEATURE_FREE))
        return SENTINEL_ERROR_UNSUPPORTED_OPERATION;

    ReleaseResult result = allocator_free(allocator, context);

    if(result != RELEASE_SUCCESS)
        return SENTINEL_ERROR_ALLOCATOR_ERROR;

    return SENTINEL_SUCCESS;
}

/**
 * @brief Initializes an Array-backed Iterator from an Array iterator context.
 *
 * The resulting Iterator owns an independent copy of the supplied context.
 * The supplied context itself is borrowed and is not modified or adopted.
 *
 * The Iterator represents a random-access traversal over the logical Array
 * interval '[start_index, end_index)'.
 *
 * The context must satisfy:
 * - 'array' is not NULL and refers to a valid Array;
 * - 'start_index <= end_index';
 * - 'end_index <= array->size';
 * - 'position <= end_index - start_index'.
 *
 * The resulting Iterator must use the Array iterator callback interface:
 * - next;
 * - previous;
 * - advance;
 * - distance;
 * - get;
 * - equals;
 * - clone;
 * - destroy.
 *
 * @param[out] out Iterator to initialize.
 * @param[in] context Array iterator state to copy.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_INVALID_ARGUMENTS if 'out' or 'context' is NULL or the
 *         supplied traversal state is malformed.
 * @return ARRAY_ERROR_INVALID_ARRAY if 'context->array' is invalid.
 * @return ARRAY_ERROR_ALLOCATOR_ERROR if the independent context allocation
 *         cannot be created.
 * @return ARRAY_ERROR_ITERATOR_ERROR if generic Iterator initialization fails.
 *
 * @post On success:
 *       - 'out' is a valid initialized random-access Iterator;
 *       - 'out' owns an independent ArrayIteratorContext;
 *       - the Iterator borrows 'context->array';
 *       - modifying the original 'context' does not affect the Iterator.
 *
 * @post On failure, 'out' must not contain a usable initialized Iterator.
 *
 * @attention 'out' must not contain a live initialized Iterator.
 * @attention The Array and its object allocator must outlive the resulting
 *            Iterator.
 * @attention Structural mutation of the Array invalidates the Iterator.
 */
ArrayResult array_iterator_init(Iterator* out, const ArrayIteratorContext* context) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(!context)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(!context->array || !array_is_valid(context->array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(!array_iterator_context_is_valid(context))
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    IteratorInterface iface = {
        .get      = array_iterator_get,
        .next     = array_iterator_next,
        .previous = array_iterator_previous,
        .equals   = array_iterator_equals,
        .advance  = array_iterator_advance,
        .distance = array_iterator_distance,
        .clone    = array_iterator_clone,
        .destroy  = array_iterator_destroy,
    };

    IteratorDesc desc = {
        .context   = (void*)context,
        .type_size = context->array->element_size,
        .iface     = iface,
    };

    Iterator fake = {0};

    if(!iterator_init(&fake, &desc))
        return ARRAY_ERROR_ITERATOR_ERROR;

    if(iterator_clone(&fake, out, context->array->object_allocator) != ITERATOR_SUCCESS)
        return ARRAY_ERROR_ITERATOR_ERROR;

    return ARRAY_SUCCESS;
}

/**
 * @brief Initializes an Array-backed Sentinel from an Array sentinel context.
 *
 * The resulting Sentinel owns an independent copy of the supplied context.
 * The supplied context itself is borrowed and is not modified or adopted.
 *
 * The Sentinel represents a fixed traversal boundary for the logical Array
 * interval '[start_index, end_index)'.
 *
 * The context must satisfy:
 * - 'array' is not NULL and refers to a valid Array;
 * - 'start_index <= end_index';
 * - 'end_index <= array->size';
 * - 'position <= end_index - start_index'.
 *
 * For normal Array Range construction, the Sentinel position should represent
 * the traversal end:
 *
 *     position == end_index - start_index
 *
 * The resulting Sentinel must use the Array sentinel callback interface:
 * - equals;
 * - clone;
 * - destroy.
 *
 * @param[out] out Sentinel to initialize.
 * @param[in] context Array sentinel state to copy.
 *
 * @return ARRAY_SUCCESS on success.
 * @return ARRAY_ERROR_INVALID_ARGUMENTS if 'out' or 'context' is NULL or the
 *         supplied traversal state is malformed.
 * @return ARRAY_ERROR_INVALID_ARRAY if 'context->array' is invalid.
 * @return ARRAY_ERROR_ALLOCATOR_ERROR if the independent context allocation
 *         cannot be created.
 * @return ARRAY_ERROR_RANGE_ERROR if generic Sentinel initialization fails.
 *
 * @post On success:
 *       - 'out' is a valid initialized Sentinel;
 *       - 'out' owns an independent ArraySentinelContext;
 *       - the Sentinel borrows 'context->array';
 *       - modifying the original 'context' does not affect the Sentinel.
 *
 * @post On failure, 'out' must not contain a usable initialized Sentinel.
 *
 * @attention 'out' must not contain a live initialized Sentinel.
 * @attention The Array and its object allocator must outlive the resulting
 *            Sentinel.
 * @attention Structural mutation of the Array invalidates the Sentinel.
 */
ArrayResult array_sentinel_init(Sentinel* out, const ArraySentinelContext* context) {
    if(!out)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(!context)
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    if(!context->array || !array_is_valid(context->array))
        return ARRAY_ERROR_INVALID_ARRAY;

    if(!array_sentinel_context_is_valid(context))
        return ARRAY_ERROR_INVALID_ARGUMENTS;

    SentinelInterface iface = {
        .equals  = array_sentinel_equals,
        .clone   = array_sentinel_clone,
        .destroy = array_sentinel_destroy,
    };

    SentinelDesc desc = {
        .context   = (void*)context,
        .type_size = context->array->element_size,
        .iface     = iface,
    };

    Sentinel fake = {0};

    if(sentinel_init(&fake, &desc) != SENTINEL_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    if(sentinel_clone(&fake, out, context->array->object_allocator) != SENTINEL_SUCCESS)
        return ARRAY_ERROR_RANGE_ERROR;

    return ARRAY_SUCCESS;
}
