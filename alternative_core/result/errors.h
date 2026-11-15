/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

/**
 * @file errors.h
 *
 * @author Nelson Somé
 *
 * @brief Shared severity, domain, and error constants.
 *
 * Defines the symbolic values used to classify errors across Arc Core.
 * Complete error values are constructed with ARC_ERROR_PACK and can be
 * stored in uint32_t. Severity and domain identifiers alone are not complete
 * error values.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_ERRORS_H_
#define ARC_ERRORS_H_

#include <stdint.h>

#include "errorpack.h"

/**
 * @brief Severity identifiers used to classify reported conditions.
 *
 * Severity identifiers occupy the severity field of a packed value. They are
 * individual values, not flags, and must not be combined using bitwise OR.
 *
 * @attention Severity alone does not document the validity of an operation's
 *            outputs. Each operation must specify its success and failure
 *            conditions, valid outputs, and whether processing may continue.
 *
 * @note A severity constant does not emit a message, terminate the process,
 *       or perform recovery. Those actions belong to the consuming code.
 */
#define ARC_SEVERITY_NONE ((uint8_t)0U)    /**< No severity assigned. */
#define ARC_SEVERITY_LOG ((uint8_t)1U)     /**< Informational condition. */
#define ARC_SEVERITY_WARNING ((uint8_t)2U) /**< Warning condition. */
#define ARC_SEVERITY_ERROR ((uint8_t)3U)   /**< Error condition. */
#define ARC_SEVERITY_FATAL ((uint8_t)4U)   /**< Condition classified as fatal. */

/**
 * @brief Domain identifiers used to group error codes.
 *
 * A domain identifies the group in which a code is defined. The same code
 * number may be used in different domains without referring to the same error.
 * Domain identifiers are individual values and must not be combined as flags.
 *
 * @note Common errors may be returned by multiple subsystems. Their domain
 *       identifies the shared definition, not the subsystem that returned it.
 */

/** Errors shared across subsystems. */
#define ARC_DOMAIN_COMMON ((uint8_t)0U)

/**
 * @brief Error values shared across Arc Core subsystems.
 *
 * ARC_ERROR_NONE has numeric value zero and represents the absence of an error.
 * Each remaining constant combines its severity, domain, and domain-local code.
 * Compare complete packed values when identifying a specific constant.
 */

/** No error. */
#define ARC_ERROR_NONE ARC_ERROR_PACK(ARC_SEVERITY_NONE, ARC_DOMAIN_COMMON, 0U)
/** An argument does not satisfy the operation's requirements. */
#define ARC_ERROR_INVALID_ARGUMENT ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_COMMON, 1U)
/** Unsupported operation. */
#define ARC_ERROR_UNSUPPORTED_OPERATION ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_COMMON, 2U)
/** Invalid descriptor. */
#define ARC_ERROR_INVALID_DESCRIPTOR ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_COMMON, 3U)
/** Unknown failure. */
#define ARC_ERROR_UNKNOWN ARC_ERROR_PACK(ARC_SEVERITY_ERROR, ARC_DOMAIN_COMMON, 255U)

#endif /* ARC_ERRORS_H_ */
