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

    if(!desc->iface.next || !desc->iface.get || !desc->iface.equals)
        return false;

    const bool has_previous = desc->iface.previous != NULL;
    const bool has_advance  = desc->iface.advance != NULL;
    const bool has_distance = desc->iface.distance != NULL;

    if(has_advance != has_distance)
        return false;

    if(!has_previous && (has_advance || has_distance))
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
        .category    = category,
        .context     = desc->context,
        .iface       = desc->iface,
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

    if(required_category <= ITERATOR_CATEGORY_INVALID ||
       required_category > ITERATOR_CATEGORY_RANDOM_ACCESS)
        return false;

    return iterator->category >= required_category;
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
IteratorResult iterator_get(const Iterator* iterator, void** out) {
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

    if(a->iface.equals != b->iface.equals)
        return ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    return a->iface.equals(a->context, b->context, out);
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