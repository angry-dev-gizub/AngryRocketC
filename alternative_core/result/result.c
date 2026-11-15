/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENCE file in the project root for license information.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "allocator/allocator.h"
#include "result.h"

void arc_abort(const char* format, ...) {
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fputc('\n', stderr);
    abort();
}

/** @brief Validates a packed failure error. */
static bool error_is_failure(uint32_t error) {
    uint32_t severity = (error >> 16U) & 0xFFU;

    return error != 0 && (error & 0xFF000000U) == 0 &&
           (severity == ARC_SEVERITY_LOG || severity == ARC_SEVERITY_WARNING || severity == ARC_SEVERITY_ERROR ||
            severity == ARC_SEVERITY_FATAL);
}

/** @brief Checks the structural validity of a Residue. */
static bool residue_is_valid(const arc_residue_t* residue) {
    if(!residue)
        return false;
    if(residue->kind == ARC_RESIDUE_KIND_NONE)
        return residue->ptr == NULL && residue->allocator == NULL;
    if(residue->kind != ARC_RESIDUE_KIND_ALLOCATION || !residue->ptr || !residue->allocator)
        return false;

    const arc_allocator_t* allocator = residue->allocator;
    return arc_allocator_is_valid(allocator) && arc_allocator_has_features(allocator, ARC_ALLOCATOR_FEATURE_FREE);
}

/**
 * @brief Creates a successful Result.
 *
 * Constructs the canonical successful Result state.
 *
 * The returned Result satisfies:
 * - 'ok' is true;
 * - 'error' is ARC_ERROR_NONE (numeric value zero);
 * - 'residue.kind' is ARC_RESIDUE_KIND_NONE;
 * - 'residue.ptr' is NULL;
 * - 'residue.allocator' is NULL.
 *
 * @return A valid successful Result.
 *
 * @post The returned Result satisfies the invariants documented for
 *       arc_result_t.
 *
 * @note A successful Result contains no nonzero error value or residual
 *       ownership.
 */
arc_result_t arc_result_success(void) {
    return (arc_result_t){.ok = true, .error = ARC_ERROR_NONE, .residue = {0}};
}

arc_result_t arc_result_failure(uint32_t error) {
    if(!error_is_failure(error))
        arc_abort("Arc result: invalid failure error 0x%08X", (unsigned)error);

    return (arc_result_t){.ok = false, .error = error, .residue = {0}};
}

arc_result_t arc_result_failure_with_residue(uint32_t error, arc_residue_t residue) {
    if(!error_is_failure(error))
        arc_abort("Arc result: invalid failure error 0x%08X", (unsigned)error);
    if(residue.kind != ARC_RESIDUE_KIND_ALLOCATION || !residue_is_valid(&residue))
        arc_abort("Arc result: invalid allocation residue");

    return (arc_result_t){.ok = false, .error = error, .residue = residue};
}

bool arc_residue_destroy(arc_residue_t* residue) {
    if(!residue_is_valid(residue))
        return false;
    if(residue->kind == ARC_RESIDUE_KIND_NONE)
        return true;

    arc_result_t result = arc_allocator_free(residue->allocator, residue->ptr);
    if(!result.ok) {
        if(result.residue.kind != ARC_RESIDUE_KIND_NONE &&
           (result.residue.kind != ARC_RESIDUE_KIND_ALLOCATION || result.residue.ptr != residue->ptr ||
            result.residue.allocator != residue->allocator))
            arc_abort("Arc residue: free returned incompatible residual ownership");
        return false;
    }

    *residue = (arc_residue_t){0};
    return true;
}

bool arc_error_format(uint32_t error, char* buffer, size_t buffer_size) {
    if(buffer && buffer_size)
        buffer[0] = '\0';
    if(!buffer || !buffer_size || (error & 0xFF000000U))
        return false;

    const char* severity = arc_error_severity_to_string(error);
    const char* domain   = arc_error_domain_to_string(error);
    const char* code     = "test";//TODO arc_error_code_to_string(error);
    int written =
        snprintf(buffer, buffer_size, "error=0x%08X severity=%s(%u) domain=%s(%u) code=%s(%u)", (unsigned)error,
                 severity ? severity : "unknown", (unsigned)((error >> 16U) & 0xFFU), domain ? domain : "unknown",
                 (unsigned)((error >> 8U) & 0xFFU), code ? code : "unknown", (unsigned)(error & 0xFFU));
    if(written < 0 || (size_t)written >= buffer_size) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

const char* arc_error_domain_to_string(uint32_t error) {
    uint32_t domain = (error >> 8U) & 0xFFU;
    if(domain == 0)
        return "common";
    if(domain == ARC_DOMAIN_ALLOCATOR)
        return "allocator";
    return NULL;
}

const char* arc_error_code_to_string(uint32_t error);

const char* arc_error_severity_to_string(uint32_t error) {
    uint32_t severity = (error >> 16U) & 0xFFU;
    switch(severity) {
        case ARC_SEVERITY_NONE:
            return "none";
        case ARC_SEVERITY_LOG:
            return "log";
        case ARC_SEVERITY_WARNING:
            return "warning";
        case ARC_SEVERITY_ERROR:
            return "error";
        case ARC_SEVERITY_FATAL:
            return "fatal";
        default:
            return NULL;
    }
}

const char* arc_residue_kind_to_string(arc_residue_kind_t kind) {
    switch(kind) {
        case ARC_RESIDUE_KIND_NONE:
            return "none";
        case ARC_RESIDUE_KIND_ALLOCATION:
            return "allocation";
        default:
            return NULL;
    }
}
