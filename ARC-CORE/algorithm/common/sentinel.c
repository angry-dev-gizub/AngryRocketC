#include "iterator/iterator.h"
#include "sentinel.h"

/**
 * @brief Checks the validity of a sentinel descriptor.
 *
 * @param[in] desc Read-only configuration of the sentinel, must not be NULL.
 * @return true if the configuration is valid, false otherwise.
 */
static bool sentinel_desc_is_valid(const SentinelDesc* desc) {
    if(!desc)
        return false;

    if(desc->type_size == 0)
        return false;

    if(!desc->iface.equals)
        return false;

    return true;
}

/**
 * @brief Initializes a Sentinel from a descriptor.
 *
 * @param[out] out Pointer to the Sentinel to initialize. Must not be NULL.
 * @param[in] desc Read-only Sentinel configuration descriptor. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'out' or 'desc' is NULL, or if
 *         the descriptor is invalid.
 *
 * @post On success, 'out' is initialized and borrows 'desc->context'.
 *
 * @post On failure, 'out' is reset to an invalid Sentinel state.
 *
 * @attention 'out' must not contain a live initialized Sentinel.
 */
SentinelResult sentinel_init(Sentinel* out, const SentinelDesc* desc) {
    if(!out || !desc)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = (Sentinel){0};

    if(!sentinel_desc_is_valid(desc))
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = (Sentinel){
        .initialized = true,
        .type_size   = desc->type_size,
        .context     = desc->context,
        .iface       = desc->iface,
    };

    return SENTINEL_SUCCESS;
}

/**
 * @brief Tests whether a Sentinel is valid.
 *
 * @param[in] sentinel Pointer to the Sentinel to test.
 *
 * @return true if the Sentinel is valid, false otherwise.
 */
bool sentinel_is_valid(const Sentinel* sentinel) {
    if(!sentinel)
        return false;

    return sentinel->initialized;
}

/**
 * @brief Tests whether an iterator has reached a Sentinel boundary.
 *
 * @param[in] sentinel Pointer to an initialized Sentinel. Must not be NULL.
 * @param[in] iterator Pointer to an initialized Iterator. Must not be NULL.
 * @param[out] out Pointer receiving the comparison result. Must not be NULL.
 *
 * @return SENTINEL_SUCCESS on success.
 * @return SENTINEL_ERROR_INVALID_SENTINEL if 'sentinel' is NULL or invalid.
 * @return SENTINEL_ERROR_INVALID_ARGUMENTS if 'iterator' or 'out' is NULL.
 * @return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN if the Sentinel and Iterator
 *         have different element type sizes or belong to incompatible
 *         iteration domains.
 *
 * @post On success, '*out' is true if the iterator satisfies the Sentinel's
 *       termination condition and false otherwise.
 *
 * @note The generic Sentinel layer validates element-size compatibility before
 *       invoking the implementation-specific comparison callback.
 */
SentinelResult sentinel_equals(const Sentinel* sentinel, const Iterator* iterator, bool* out) {
    if(!sentinel || !sentinel->initialized)
        return SENTINEL_ERROR_INVALID_SENTINEL;

    if(!iterator || !iterator->initialized || !out)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    if(sentinel->type_size != iterator->type_size)
        return SENTINEL_ERROR_INCOMPATIBLE_DOMAIN;

    return sentinel->iface.equals(sentinel->context, iterator->context, out);
}

/**
 * @brief Obtains the element type size expected by a Sentinel.
 *
 * @param[in] sentinel Pointer to an initialized Sentinel.
 *
 * @return The expected element type size in bytes.
 * @return 0 if 'sentinel' is NULL or not initialized.
 */
size_t sentinel_get_type_size(const Sentinel* sentinel) {
    if(!sentinel || !sentinel->initialized)
        return 0;

    return sentinel->type_size;
}

/**
 * @brief Converts a numeric sentinel result code into a human-readable string.
 *
 * @param[in] result The result code to convert.
 * @return A human-readable description of the result code, or NULL for invalid inputs.
 *
 * @note The returned string is static and must not be freed.
 */
const char* sentinel_result_to_string(SentinelResult result) {
    switch(result) {
        case SENTINEL_SUCCESS:
            return "no error";
        case SENTINEL_ERROR_INVALID_SENTINEL:
            return "null or invalid sentinel";
        case SENTINEL_ERROR_INVALID_ARGUMENTS:
            return "invalid call argument";
        case SENTINEL_ERROR_INCOMPATIBLE_DOMAIN:
            return "incompatible iteration domain";
        default:
            return NULL;
    }
}