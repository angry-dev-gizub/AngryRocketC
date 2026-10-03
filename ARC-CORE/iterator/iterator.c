/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include "iterator.h"

/**
 * @brief Checks the validity of an iterator descriptor.
 * 
 * @param[in] desc Read-only configuration of the iterator, must not be NULL.
 * @return true if the configuration is valid, false otherwise.
 */
static bool iterator_desc_is_valid(const IteratorDesc* desc) {
    if(!desc)
        return false;

    if(desc->type_size == 0)
        return false;

    if(!desc->iface.next || !desc->iface.get || !desc->iface.equals)
        return false;

    const bool has_previous = desc->iface.previous != NULL;
    const bool has_advance  = desc->iface.advance != NULL;
    const bool has_distance = desc->iface.distance != NULL;
    const bool has_clone    = desc->iface.clone != NULL;
    const bool has_destroy  = desc->iface.destroy != NULL;

    if(has_advance != has_distance)
        return false;

    if(!has_previous && (has_advance || has_distance))
        return false;

    if(has_clone != has_destroy)
        return false;

    return true;
}

static bool category_from_desc(IteratorCategory* out, const IteratorDesc* desc) {
    if(!out || !desc)
        return false;

    const bool has_previous = desc->iface.previous != NULL;
    const bool has_advance  = desc->iface.advance != NULL;
    const bool has_distance = desc->iface.distance != NULL;

    if(!has_previous && !has_advance && !has_distance)
        *out = ITERATOR_CATEGORY_FORWARD;
    else if(has_previous && !has_advance && !has_distance)
        *out = ITERATOR_CATEGORY_BIDIRECTIONAL;
    else if(has_previous && has_advance && has_distance)
        *out = ITERATOR_CATEGORY_RANDOM_ACCESS;
    else {
        *out = ITERATOR_CATEGORY_INVALID;
        return false;
    }

    return true;
}

/**
 * @brief Initializes an iterator.
 *
 * @param[in, out] out Pointer to the iterator to initialize, must not be NULL.
 * @param[in] desc Read-only configuration of the iterator, must not be NULL.
 *
 * @return true if successfully initialized, false otherwise.
 *
 * @note On failure, the iterator's 'initialized' flag is set to false and its
 *       category is set to ITERATOR_CATEGORY_INVALID.
 *       Trying to use the iterator after failure results in an invalid iterator error.
 */
bool iterator_init(Iterator* out, const IteratorDesc* desc) {
    if(!out)
        return false;

    *out = (Iterator){0};

    if(!iterator_desc_is_valid(desc))
        return false;

    IteratorCategory category = ITERATOR_CATEGORY_INVALID;

    if(!category_from_desc(&category, desc))
        return false;

    *out = (Iterator){
        .initialized = true,
        .owns_context = false,
        .type_size   = desc->type_size,
        .category    = category,
        .context     = desc->context,
        .iface       = desc->iface,
        .cloning_allocator = NULL,
    };

    return true;
}

/**
 * @brief Checks whether an iterator provides at least the requested traversal category.
 *
 * @param[in] iterator Pointer to an initialized iterator. Must not be NULL.
 * @param[in] required_category Minimum traversal category required.
 *
 * @return true if the iterator provides at least the requested category, false otherwise.
 */
bool iterator_is_at_least(const Iterator* iterator, IteratorCategory required_category) {
    if(!iterator)
        return false;

    if(!iterator->initialized)
        return false;

    if(required_category <= ITERATOR_CATEGORY_INVALID || required_category > ITERATOR_CATEGORY_RANDOM_ACCESS)
        return false;

    return iterator->category >= required_category;
}

/**
 * @brief Obtains the size of the element type traversed by an iterator.
 *
 * @param[in] iterator Pointer to an initialized iterator.
 *
 * @return The size in bytes of the iterator's element type.
 * @return 0 if 'iterator' is NULL or not initialized.
 *
 * @note The returned value is the type size supplied when the iterator was
 *       initialized and remains constant for the iterator's lifetime.
 */
size_t iterator_get_type_size(const Iterator* iterator) {
    if(!iterator || !iterator->initialized)
        return 0;

    return iterator->type_size;
}

/**
 * @brief Advances an iterator by exactly one position.
 *
 * @param[in, out] iterator Pointer to an initialized iterator. Must not be NULL.
 * @return The result of the operation.
 */
IteratorResult iterator_next(Iterator* iterator) {
    if(!iterator || !iterator->initialized)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    return iterator->iface.next(iterator->context);
}

/**
 * @brief Moves an iterator backward by exactly one position.
 *
 * @param[in, out] iterator Pointer to an initialized iterator. Must not be NULL.
 * @return The result of the operation.
 *
 * @note If the iterator is not bidirectional or random access, the operation
 *       fails with ITERATOR_ERROR_UNSUPPORTED_OPERATION.
 */
IteratorResult iterator_previous(Iterator* iterator) {
    if(!iterator || !iterator->initialized)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!iterator_is_at_least(iterator, ITERATOR_CATEGORY_BIDIRECTIONAL))
        return ITERATOR_ERROR_UNSUPPORTED_OPERATION;

    return iterator->iface.previous(iterator->context);
}

/**
 * @brief Moves an iterator by 'offset' positions.
 *
 * @param[in, out] iterator Pointer to an initialized iterator. Must not be NULL.
 * @param[in] offset Signed number of positions by which to move the iterator.
 * @return The result of the operation.
 *
 * @note If the iterator is not random access, the operation fails with
 *       ITERATOR_ERROR_UNSUPPORTED_OPERATION.
 */
IteratorResult iterator_advance(Iterator* iterator, ptrdiff_t offset) {
    if(!iterator || !iterator->initialized)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!iterator_is_at_least(iterator, ITERATOR_CATEGORY_RANDOM_ACCESS))
        return ITERATOR_ERROR_UNSUPPORTED_OPERATION;

    return iterator->iface.advance(iterator->context, offset);
}

/**
 * @brief Computes the signed distance between two iterator positions.
 *
 * @param[in] start Pointer to the starting iterator. Must not be NULL.
 * @param[in] end Pointer to the ending iterator. Must not be NULL.
 * @param[out] out Pointer receiving the signed distance. Must not be NULL.
 * @return The result of the operation.
 *
 * @note Both iterators must be random access.
 *       If either iterator is not random access, the operation fails with
 *       ITERATOR_ERROR_UNSUPPORTED_OPERATION.
 *
 * @note The iterators must belong to compatible iteration domains.
 */
IteratorResult iterator_distance(const Iterator* start, const Iterator* end, ptrdiff_t* out) {
    if(!start || !end || !out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    if(!start->initialized || !end->initialized)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(start->type_size != end->type_size)
        return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    if(!iterator_is_at_least(start, ITERATOR_CATEGORY_RANDOM_ACCESS) ||
       !iterator_is_at_least(end, ITERATOR_CATEGORY_RANDOM_ACCESS))
        return ITERATOR_ERROR_UNSUPPORTED_OPERATION;

    if(start->iface.distance != end->iface.distance)
        return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    return start->iface.distance(start->context, end->context, out);
}

/**
 * @brief Obtains a pointer to the element at the iterator's current position.
 *
 * @param[in] iterator Pointer to an initialized iterator. Must not be NULL.
 * @param[out] out Pointer receiving the address of the current element. Must not be NULL.
 * @return The result of the operation.
 *
 * @note The iterator retains no ownership over the returned pointer beyond the
 *       guarantees provided by the underlying iterator implementation.
 */
IteratorResult iterator_get(const Iterator* iterator, const void** out) {
    if(!iterator)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    if(!iterator->initialized)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    return iterator->iface.get(iterator->context, out);
}

/**
 * @brief Checks whether two iterators represent the same position.
 *
 * @param[in] a Pointer to the first initialized iterator. Must not be NULL.
 * @param[in] b Pointer to the second initialized iterator. Must not be NULL.
 * @param[out] out Pointer receiving the comparison result. Must not be NULL.
 * @return The result of the operation.
 *
 * @note The iterators must belong to compatible iteration domains.
 */
IteratorResult iterator_equals(const Iterator* a, const Iterator* b, bool* out) {
    if(!a || !b || !out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    if(!a->initialized || !b->initialized)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(a->type_size != b->type_size)
        return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    if(a->iface.equals != b->iface.equals)
        return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    return a->iface.equals(a->context, b->context, out);
}

/**
 * @brief Creates an independent clone of an iterator.
 *
 * The cloned iterator represents the same traversal position and iteration
 * domain as the source iterator while owning an independent iterator-specific
 * context.
 *
 * Moving either iterator after cloning does not affect the traversal position
 * of the other.
 *
 * @param[in] iterator Initialized iterator to clone. Must not be NULL.
 * @param[out] out Iterator receiving the independent clone. Must not be NULL.
 * @param[in] allocator Allocator used to allocate the cloned context. Must not
 *                      be NULL and must remain valid until the clone is destroyed.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'iterator' is NULL or invalid.
 * @return ITERATOR_ERROR_INVALID_ARGUMENTS if 'out' or 'allocator' is NULL.
 * @return ITERATOR_ERROR_UNSUPPORTED_OPERATION if cloning is not supported by
 *         the iterator implementation.
 * @return Any error returned by the implementation-specific cloning callback.
 *
 * @post On success:
 *       - 'out' is initialized;
 *       - 'out' represents the same traversal position and domain as 'iterator';
 *       - 'out' owns an independent context;
 *       - 'out' stores 'allocator' for later context destruction.
 *
 * @post On failure, 'out' is reset to an invalid iterator state.
 *
 * @attention A successfully cloned iterator must eventually be passed to
 *            iterator_destroy().
 * 
 * @attention 'out' must not contain a live initialized Iterator.
           An existing Iterator must be destroyed before being reused as output.
 */
IteratorResult iterator_clone(const Iterator* iterator, Iterator* out, Allocator* allocator) {
    if(!out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    if(out == iterator)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    *out = (Iterator){0};

    if(!iterator || !iterator->initialized)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!allocator)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    if(!iterator->iface.clone)
        return ITERATOR_ERROR_UNSUPPORTED_OPERATION;

    void* cloned_context  = NULL;
    IteratorResult result = iterator->iface.clone(iterator->context, &cloned_context, allocator);

    if(result != ITERATOR_SUCCESS)
        return result;

    *out = (Iterator){
        .initialized       = true,
        .owns_context      = true,
        .type_size        = iterator->type_size,
        .category          = iterator->category,
        .context           = cloned_context,
        .iface             = iterator->iface,
        .cloning_allocator = allocator,
    };

    return ITERATOR_SUCCESS;
}

/**
 * @brief Destroys an iterator.
 *
 * If the iterator owns its context, the context is released using the
 * implementation-specific destruction callback and the allocator originally
 * supplied to iterator_clone().
 *
 * If the iterator does not own its context, only the Iterator object itself is
 * invalidated; the borrowed context is left untouched.
 *
 * @param[in, out] iterator Iterator to destroy. Must not be NULL and must be
 *                         initialized.
 *
 * @return ITERATOR_SUCCESS on success.
 * @return ITERATOR_ERROR_INVALID_ITERATOR if 'iterator' is NULL or invalid.
 * @return ITERATOR_ERROR_UNSUPPORTED_OPERATION if an owned iterator does not
 *         provide the required destruction callback.
 *
 * @post On success, the Iterator is reset to an invalid state and must not be
 *       used until initialized again.
 *
 * @post On failure, an owned Iterator retains ownership of its context and
 *       remains unchanged.
 *
 * @note Destroying an initialized Iterator that does not own its context is a
 *       successful operation and does not affect the borrowed context.
 */
IteratorResult iterator_destroy(Iterator* iterator) {
    if(!iterator || !iterator->initialized)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!iterator->owns_context) {
        *iterator = (Iterator){0};
        return ITERATOR_SUCCESS;
    }

    if(!iterator->cloning_allocator)
        return ITERATOR_ERROR_INVALID_ITERATOR;

    if(!iterator->iface.destroy)
        return ITERATOR_ERROR_UNSUPPORTED_OPERATION;

    IteratorResult result = iterator->iface.destroy(iterator->context, iterator->cloning_allocator);

    if(result != ITERATOR_SUCCESS)
        return result;

    *iterator = (Iterator){0};

    return ITERATOR_SUCCESS;
}

/**
 * @brief Converts a numeric iterator result code into a human-readable string.
 *
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 *
 * @note The returned string is static and must not be freed.
 */
const char* iterator_result_to_string(IteratorResult result) {
    switch(result) {
        case ITERATOR_SUCCESS:
            return "no error";
        case ITERATOR_ERROR_INVALID_ITERATOR:
            return "null or invalid iterator";
        case ITERATOR_ERROR_INVALID_ARGUMENTS:
            return "invalid call argument";
        case ITERATOR_ERROR_UNSUPPORTED_OPERATION:
            return "unsupported operation";
        case ITERATOR_ERROR_OUT_OF_RANGE:
            return "out of range access";
        case ITERATOR_ERROR_INCOMPATIBLE_ITERATORS:
            return "incompatible iterators";
        default:
            return NULL;
    }
}

/**
 * @brief Converts an IteratorCategory value to a human-readable string.
 *
 * @param[in] category Iterator category to convert.
 *
 * @return A human-readable description of 'category', or NULL if 'category'
 *         is not a valid IteratorCategory value.
 *
 * @note The returned string has static lifetime and must not be modified or freed.
 */
const char* iterator_category_to_string(IteratorCategory category) {
    switch(category) {
        case ITERATOR_CATEGORY_INVALID:
            return "invalid";
        case ITERATOR_CATEGORY_FORWARD:
            return "forward";
        case ITERATOR_CATEGORY_BIDIRECTIONAL:
            return "bidirectional";
        case ITERATOR_CATEGORY_RANDOM_ACCESS:
            return "random access";
        default:
            return NULL;
    }
}