/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include <stdlib.h>
#include <stddef.h>
#include "platform/os.h"
#include "system_allocator.h"
#include "allocator/allocator.h"

#if defined(ARC_PLATFORM_WINDOWS)
#include <malloc.h>
#endif

static void* arc_malloc(size_t size) {
    return malloc(size);
}

static void* arc_calloc(size_t n, size_t size) {
    return calloc(n, size);
}

static void* arc_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

static void* arc_aligned_alloc(size_t alignment, size_t size) {
#if defined(ARC_PLATFORM_LINUX) || defined(ARC_PLATFORM_MACOS)
    void* ptr = NULL;

    if(posix_memalign(&ptr, alignment, size) != 0)
        return NULL;

    return ptr;

#elif defined(ARC_PLATFORM_WINDOWS)
    return _aligned_malloc(size, alignment);
#endif
}

static void arc_aligned_free(void* ptr) {
#if defined(ARC_PLATFORM_LINUX) || defined(ARC_PLATFORM_MACOS)
    free(ptr);
    
#elif defined(ARC_PLATFORM_WINDOWS)
    _aligned_free(ptr);

#endif
}

static void arc_free(void* ptr) {
    free(ptr);
}

static AllocResult make_success(void* ptr) {
    return (AllocResult){
        .ok     = true,
        .as.ptr = ptr,
    };
}

static AllocResult make_failure(AllocError error) {
    return (AllocResult){
        .ok       = false,
        .as.error = error,
    };
}

static AllocResult system_alloc(void* context, size_t size) {
    (void)context;

    if(size == 0)
        return make_success(NULL);

    void* ptr = arc_malloc(size);

    if(!ptr)
        return make_failure(ALLOC_ERROR_OUT_OF_MEMORY);

    return make_success(ptr);
}

static AllocResult system_zero_alloc(void* context, size_t num, size_t size) {
    (void)context;

    if(num == 0 || size == 0)
        return make_success(NULL);

    if(num > SIZE_MAX / size)
        return make_failure(ALLOC_ERROR_INVALID_ARGUMENTS);

    void* ptr = arc_calloc(num, size);

    if(!ptr)
        return make_failure(ALLOC_ERROR_OUT_OF_MEMORY);

    return make_success(ptr);
}

static AllocResult system_realloc(void* context, void* ptr, size_t new_size) {
    (void)context;

    if(!ptr || new_size == 0)
        return make_failure(ALLOC_ERROR_INVALID_ARGUMENTS);

    void* tmp = arc_realloc(ptr, new_size);

    if(!tmp)
        return make_failure(ALLOC_ERROR_OUT_OF_MEMORY);

    return make_success(tmp);
}

static AllocResult system_aligned_alloc(void* context, size_t alignment, size_t size) {
    (void)context;

    if(alignment == 0 || (alignment & (alignment - 1)) != 0)
        return make_failure(ALLOC_ERROR_INVALID_ALIGNMENT);

    if(size == 0)
        return make_success(NULL);

    size_t effective_alignment = alignment;

    if(effective_alignment < sizeof(void*))
        effective_alignment = sizeof(void*);

    void* ptr = arc_aligned_alloc(effective_alignment, size);

    if(!ptr)
        return make_failure(ALLOC_ERROR_OUT_OF_MEMORY);

    return make_success(ptr);
}

static void system_free(void* context, void* ptr) {
    (void)context;

    arc_free(ptr);
}

static void system_aligned_free(void* context, void* ptr) {
    (void)context;

    if(!ptr)
        return;

    arc_aligned_free(ptr);
}

bool system_allocator_init(Allocator* out) {
    if(!out)
        return false;

    const AllocatorInterface iface = {
        .mem_alloc         = system_alloc,
        .mem_zero_alloc    = system_zero_alloc,
        .mem_realloc       = system_realloc,
        .mem_aligned_alloc = system_aligned_alloc,
        .mem_free          = system_free,
        .mem_aligned_free  = system_aligned_free,
        .mem_clear         = NULL,
    };

    const AllocatorDesc desc = {
        .context = NULL,
        .iface   = iface,
    };

    return allocator_init(out, &desc);
}
