/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file errorpack.h
 *
 * @author Nelson Somé
 *
 * @brief Utility macro for error packing.
 *
 * Provides a common numeric representation for error values across Arc Core.
 * Each value combines an 8-bit severity, an 8-bit domain, and an 8-bit code
 * into a value that can be stored in uint32_t.
 *
 * The numeric layout is 0x00SSDDCC, where SS represents the severity,
 * DD represents the domain, and CC represents the code within that domain.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_ERROR_PACK_H_
#define ARC_ERROR_PACK_H_

#include <stdint.h>

/**
 * @brief Packs a severity, domain, and code into a single error value.
 *
 * @param[in] severity Integer severity identifier, expected to be in the range 0 to 255.
 * @param[in] domain Integer domain identifier, expected to be in the range 0 to 255.
 * @param[in] code Integer code within the domain, expected to be in the range 0 to 255.
 * @return The packed value, with numeric layout 0x00SSDDCC.
 *
 * @note Arguments must identify a combination defined by the error API.
 *       Packing an arbitrary combination does not make it a recognized error.
 *       Arguments must not contain conflicting side effects.
 *
 * @note With integer constant-expression arguments, the result is an integer
 *       constant expression. Packing (1U, 2U, 3U) produces 0x00010203.
 */
#define ARC_ERROR_PACK(severity, domain, code)                                                                         \
    ((((uint32_t)(severity) & 0xFFU) << 16U) | (((uint32_t)(domain) & 0xFFU) << 8U) | ((uint32_t)(code) & 0xFFU))

/**
 * @brief Extracts the severity field from a complete packed error value.
 *
 * @param[in] error Complete packed error value with numeric layout 0x00SSDDCC.
 * @return The value of bits 16 through 23, converted to uint8_t.
 *
 * @note The argument is evaluated once. No validity or identifier-recognition
 *       check is performed. Other fields, including reserved bits, are ignored.
 *       With an integer constant-expression argument, the result is an integer
 *       constant expression. Extracting from zero returns zero.
 */
#define ARC_ERROR_SEVERITY(error) ((uint8_t)(((uint32_t)(error) >> 16U) & 0xFFU))

/**
 * @brief Extracts the domain field from a complete packed error value.
 *
 * @param[in] error Complete packed error value with numeric layout 0x00SSDDCC.
 * @return The value of bits 8 through 15, converted to uint8_t.
 *
 * @note The argument is evaluated once. No validity or identifier-recognition
 *       check is performed. Other fields, including reserved bits, are ignored.
 *       With an integer constant-expression argument, the result is an integer
 *       constant expression. Extracting from zero returns zero.
 */
#define ARC_ERROR_DOMAIN(error) ((uint8_t)(((uint32_t)(error) >> 8U) & 0xFFU))

/**
 * @brief Extracts the code field from a complete packed error value.
 *
 * @param[in] error Complete packed error value with numeric layout 0x00SSDDCC.
 * @return The value of bits 0 through 7, converted to uint8_t.
 *
 * @note The argument is evaluated once. No validity or identifier-recognition
 *       check is performed. Other fields, including reserved bits, are ignored.
 *       With an integer constant-expression argument, the result is an integer
 *       constant expression. Extracting from zero returns zero.
 */
#define ARC_ERROR_CODE(error) ((uint8_t)((uint32_t)(error) & 0xFFU))

#endif /* ARC_ERROR_PACK_H_ */
