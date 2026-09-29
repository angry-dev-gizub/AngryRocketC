/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file system_allocator.h
 *
 * @author Nelson Somé
 *
 * @brief System allocator implementation.
 * 
 * System heap allocator.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_SYSTEM_ALLOCATOR_H_
#define ARC_SYSTEM_ALLOCATOR_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/** Forward declarations */
typedef struct Allocator Allocator;

/**
 * @brief Initializes an allocator backed by the system heap.
 *
 * @param[in, out] out Pointer to the allocator to initialize, must not be NULL.
 *
 * @return true if initialization succeeds, false otherwise.
 */
bool system_allocator_init(Allocator* out);

#ifdef __cplusplus
}
#endif

#endif /* ARC_SYSTEM_ALLOCATOR_H_ */
