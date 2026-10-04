/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file range.h
 *
 * @author Nelson Somé
 *
 * @brief Generic range interface.
 *
 * A Range represents a sequence of elements that can be traversed using an
 * Iterator and terminated by a Sentinel.
 *
 * Range implementations must satisfy the contracts documented by this API.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_RANGE_H_
#define ARC_RANGE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declarations. */
typedef struct Iterator Iterator;
typedef struct Sentinel Sentinel;
typedef struct Range Range;

/**
 * @brief Generic range of elements.
 *
 * A Range represents a traversal domain beginning at an Iterator and
 * terminating when that Iterator satisfies a Sentinel.
 * 
 */
struct Range {
    /** Iterator representing the beginning of the range. */
    Iterator begin;

    /** Sentinel representing the traversal termination boundary. */
    Sentinel end;
};

/**
 * @brief Initializes a Range from an Iterator and Sentinel.
 *
 * @param[out] out Pointer to the Range to initialize. Must not be NULL.
 * @param[in] begin Pointer to an initialized Iterator representing the
 *                  beginning of the range. Must not be NULL.
 * @param[in] end Pointer to an initialized Sentinel representing the traversal
 *                termination boundary. Must not be NULL.
 *
 * @return true if successfully initialized, false otherwise.
 *
 * @attention 'begin' and 'end' must have matching element type sizes.
 *
 * @post On failure, 'out' is reset to an invalid Range state.
 */
bool range_init(Range* out, const Iterator* begin, const Sentinel* end);

#ifdef __cplusplus
}
#endif

#endif /* ARC_RANGE_H_ */
