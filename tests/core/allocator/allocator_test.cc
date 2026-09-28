/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file allocator_test.cpp
 *
 * @author Nelson Somé
 *
 * @brief Tests for the generic Arc allocator API.
 *
 * These tests verify allocator initialization, feature discovery,
 * callback forwarding, error propagation, unsupported operations,
 * and utility functions.
 */

#include <gtest/gtest.h>

#include "allocator/allocator.h"

/* -------------------------------------------------------------------------- */
/* Test context                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Mutable state used by fake allocator callbacks.
 *
 * The context allows tests to verify that the allocator forwards its context
 * and operation arguments correctly.
 */
struct TestAllocatorContext {
    size_t last_size      = 0;
    size_t last_num       = 0;
    size_t last_alignment = 0;
    void* last_ptr        = nullptr;

    size_t alloc_calls         = 0;
    size_t zero_alloc_calls    = 0;
    size_t realloc_calls       = 0;
    size_t aligned_alloc_calls = 0;
    size_t free_calls          = 0;
    size_t aligned_free_calls  = 0;
    size_t clear_calls         = 0;
};

/* -------------------------------------------------------------------------- */
/* Fake callbacks                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Successful fake allocation callback.
 */
static AllocResult test_alloc(void* context, size_t size) {
    auto* state = static_cast<TestAllocatorContext*>(context);

    if(state) {
        state->last_size = size;
        ++state->alloc_calls;
    }

    static int storage;

    return AllocResult{
        .ok = true,
        .as = {.ptr = &storage},
    };
}

/**
 * @brief Successful fake zero-allocation callback.
 */
static AllocResult test_zero_alloc(void* context, size_t num, size_t size) {
    auto* state = static_cast<TestAllocatorContext*>(context);

    if(state) {
        state->last_num  = num;
        state->last_size = size;
        ++state->zero_alloc_calls;
    }

    static int storage;

    return AllocResult{
        .ok = true,
        .as = {.ptr = &storage},
    };
}

/**
 * @brief Successful fake reallocation callback.
 */
static AllocResult test_realloc(void* context, void* ptr, size_t new_size) {
    auto* state = static_cast<TestAllocatorContext*>(context);

    if(state) {
        state->last_ptr  = ptr;
        state->last_size = new_size;
        ++state->realloc_calls;
    }

    static int storage;

    return AllocResult{
        .ok = true,
        .as = {.ptr = &storage},
    };
}

/**
 * @brief Successful fake aligned-allocation callback.
 */
static AllocResult test_aligned_alloc(void* context, size_t alignment, size_t size) {
    auto* state = static_cast<TestAllocatorContext*>(context);

    if(state) {
        state->last_alignment = alignment;
        state->last_size      = size;
        ++state->aligned_alloc_calls;
    }

    static int storage;

    return AllocResult{
        .ok = true,
        .as = {.ptr = &storage},
    };
}

/**
 * @brief Fake free callback.
 */
static void test_free(void* context, void* ptr) {
    auto* state = static_cast<TestAllocatorContext*>(context);

    if(state) {
        state->last_ptr = ptr;
        ++state->free_calls;
    }
}

/**
 * @brief Fake aligned-free callback.
 */
static void test_aligned_free(void* context, void* ptr) {
    auto* state = static_cast<TestAllocatorContext*>(context);

    if(state) {
        state->last_ptr = ptr;
        ++state->aligned_free_calls;
    }
}

/**
 * @brief Fake clear callback.
 */
static void test_clear(void* context) {
    auto* state = static_cast<TestAllocatorContext*>(context);

    if(state)
        ++state->clear_calls;
}

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Creates a descriptor containing every supported allocator callback.
 */
static AllocatorDesc make_full_descriptor(TestAllocatorContext* context) {
    return AllocatorDesc{
        .context = context,
        .iface =
            {
                .mem_alloc         = test_alloc,
                .mem_aligned_alloc = test_aligned_alloc,
                .mem_zero_alloc    = test_zero_alloc,
                .mem_realloc       = test_realloc,
                .mem_aligned_free  = test_aligned_free,
                .mem_free          = test_free,
                .mem_clear         = test_clear,
            },
    };
}

/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that allocator initialization rejects a NULL output pointer.
 */
TEST(AllocatorInit, RejectsNullOutput) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    EXPECT_FALSE(allocator_init(nullptr, &desc));
}

/**
 * @test Verifies that allocator initialization rejects a NULL descriptor.
 */
TEST(AllocatorInit, RejectsNullDescriptor) {
    Allocator allocator{};

    EXPECT_FALSE(allocator_init(&allocator, nullptr));
    EXPECT_FALSE(allocator.initialized);
}

/**
 * @test Verifies that an allocator cannot be initialized without the mandatory
 *       allocation callback.
 */
TEST(AllocatorInit, RejectsMissingAllocationCallback) {
    const AllocatorDesc desc{
        .context = nullptr,
        .iface   = {},
    };

    Allocator allocator{};

    EXPECT_FALSE(allocator_init(&allocator, &desc));
    EXPECT_FALSE(allocator.initialized);
}

/**
 * @test Verifies successful initialization with a valid descriptor.
 */
TEST(AllocatorInit, InitializesValidAllocator) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};

    ASSERT_TRUE(allocator_init(&allocator, &desc));

    EXPECT_TRUE(allocator.initialized);
    EXPECT_EQ(allocator.context, &context);
}

/* -------------------------------------------------------------------------- */
/* Feature discovery                                                          */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that optional callbacks are converted into allocator features.
 */
TEST(AllocatorFeatures, DiscoversSupportedFeatures) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    EXPECT_TRUE(allocator_has_features(&allocator, ALLOCATOR_FEATURE_ZERO_ALLOC | ALLOCATOR_FEATURE_ALIGNED_ALLOC |
                                                       ALLOCATOR_FEATURE_REALLOC | ALLOCATOR_FEATURE_FREE |
                                                       ALLOCATOR_FEATURE_ALIGNED_FREE | ALLOCATOR_FEATURE_CLEAR));
}

/**
 * @test Verifies that unsupported optional operations are not reported as
 *       supported features.
 */
TEST(AllocatorFeatures, DoesNotReportMissingFeatures) {
    const AllocatorDesc desc{
        .context = nullptr,
        .iface =
            {
                .mem_alloc = test_alloc,
            },
    };

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    EXPECT_FALSE(allocator_has_features(&allocator, ALLOCATOR_FEATURE_REALLOC));

    EXPECT_FALSE(allocator_has_features(&allocator, ALLOCATOR_FEATURE_FREE));
}

/**
 * @test Verifies that querying features on an invalid allocator returns false.
 */
TEST(AllocatorFeatures, RejectsInvalidAllocator) {
    EXPECT_FALSE(allocator_has_features(nullptr, ALLOCATOR_FEATURE_FREE));

    Allocator allocator{};

    EXPECT_FALSE(allocator_has_features(&allocator, ALLOCATOR_FEATURE_FREE));
}

/* -------------------------------------------------------------------------- */
/* Allocation forwarding                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that allocator_alloc forwards the allocator context and size
 *       to the underlying allocation callback.
 */
TEST(AllocatorOperations, AllocForwardsArguments) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    const AllocResult result = allocator_alloc(&allocator, 128);

    ASSERT_TRUE(result.ok);
    EXPECT_NE(result.as.ptr, nullptr);

    EXPECT_EQ(context.alloc_calls, 1U);
    EXPECT_EQ(context.last_size, 128U);
}

/**
 * @test Verifies that allocator_zero_alloc forwards element count and size.
 */
TEST(AllocatorOperations, ZeroAllocForwardsArguments) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    const AllocResult result = allocator_zero_alloc(&allocator, 4, 32);

    ASSERT_TRUE(result.ok);

    EXPECT_EQ(context.zero_alloc_calls, 1U);
    EXPECT_EQ(context.last_num, 4U);
    EXPECT_EQ(context.last_size, 32U);
}

/**
 * @test Verifies that allocator_realloc forwards the pointer and new size.
 */
TEST(AllocatorOperations, ReallocForwardsArguments) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    int value = 0;

    const AllocResult result = allocator_realloc(&allocator, &value, 256);

    ASSERT_TRUE(result.ok);

    EXPECT_EQ(context.realloc_calls, 1U);
    EXPECT_EQ(context.last_ptr, &value);
    EXPECT_EQ(context.last_size, 256U);
}

/**
 * @test Verifies that allocator_aligned_alloc forwards alignment and size.
 */
TEST(AllocatorOperations, AlignedAllocForwardsArguments) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    const AllocResult result = allocator_aligned_alloc(&allocator, 64, 512);

    ASSERT_TRUE(result.ok);

    EXPECT_EQ(context.aligned_alloc_calls, 1U);
    EXPECT_EQ(context.last_alignment, 64U);
    EXPECT_EQ(context.last_size, 512U);
}

/* -------------------------------------------------------------------------- */
/* Release forwarding                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that allocator_free invokes the underlying free callback.
 */
TEST(AllocatorOperations, FreeForwardsPointer) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    int value = 0;

    EXPECT_EQ(allocator_free(&allocator, &value), RELEASE_SUCCESS);

    EXPECT_EQ(context.free_calls, 1U);
    EXPECT_EQ(context.last_ptr, &value);
}

/**
 * @test Verifies that allocator_aligned_free invokes the aligned-free callback.
 */
TEST(AllocatorOperations, AlignedFreeForwardsPointer) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    int value = 0;

    EXPECT_EQ(allocator_aligned_free(&allocator, &value), RELEASE_SUCCESS);

    EXPECT_EQ(context.aligned_free_calls, 1U);
    EXPECT_EQ(context.last_ptr, &value);
}

/**
 * @test Verifies that allocator_clear invokes the allocator's clear callback.
 */
TEST(AllocatorOperations, ClearInvokesCallback) {
    TestAllocatorContext context{};
    const AllocatorDesc desc = make_full_descriptor(&context);

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    EXPECT_EQ(allocator_clear(&allocator), RELEASE_SUCCESS);

    EXPECT_EQ(context.clear_calls, 1U);
}

/* -------------------------------------------------------------------------- */
/* Unsupported operations                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that missing optional allocation callbacks produce the
 *       documented unsupported-operation error.
 */
TEST(AllocatorOperations, MissingAllocationOperationIsUnsupported) {
    const AllocatorDesc desc{
        .context = nullptr,
        .iface =
            {
                .mem_alloc = test_alloc,
            },
    };

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    const AllocResult result = allocator_zero_alloc(&allocator, 4, 4);

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.as.error, ALLOC_ERROR_UNSUPPORTED_OPERATION);
}

/**
 * @test Verifies that missing release callbacks produce the documented
 *       unsupported-operation result.
 */
TEST(AllocatorOperations, MissingReleaseOperationIsUnsupported) {
    const AllocatorDesc desc{
        .context = nullptr,
        .iface =
            {
                .mem_alloc = test_alloc,
            },
    };

    Allocator allocator{};
    ASSERT_TRUE(allocator_init(&allocator, &desc));

    EXPECT_EQ(allocator_free(&allocator, nullptr), RELEASE_ERROR_UNSUPPORTED_OPERATION);

    EXPECT_EQ(allocator_clear(&allocator), RELEASE_ERROR_UNSUPPORTED_OPERATION);
}

/* -------------------------------------------------------------------------- */
/* Invalid allocator handling                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that allocation operations reject a NULL allocator.
 */
TEST(AllocatorOperations, AllocationRejectsNullAllocator) {
    const AllocResult result = allocator_alloc(nullptr, 32);

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.as.error, ALLOC_ERROR_INVALID_ALLOCATOR);
}

/**
 * @test Verifies that release operations reject a NULL allocator.
 */
TEST(AllocatorOperations, ReleaseRejectsNullAllocator) {
    EXPECT_EQ(allocator_free(nullptr, nullptr), RELEASE_ERROR_INVALID_ALLOCATOR);

    EXPECT_EQ(allocator_clear(nullptr), RELEASE_ERROR_INVALID_ALLOCATOR);
}

/* -------------------------------------------------------------------------- */
/* String conversion                                                          */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that known allocation errors have readable descriptions.
 */
TEST(AllocatorErrors, AllocationErrorsHaveStrings) {
    EXPECT_STREQ(alloc_error_to_string(ALLOC_ERROR_UNKNOWN), "unknown error");

    EXPECT_STREQ(alloc_error_to_string(ALLOC_ERROR_OUT_OF_MEMORY), "out of memory");

    EXPECT_STREQ(alloc_error_to_string(ALLOC_ERROR_INVALID_ALLOCATOR), "invalid or uninitialized allocator");

    EXPECT_STREQ(alloc_error_to_string(ALLOC_ERROR_INVALID_ALIGNMENT), "invalid alignment");

    EXPECT_STREQ(alloc_error_to_string(ALLOC_ERROR_INVALID_ARGUMENTS), "invalid call arguments");

    EXPECT_STREQ(alloc_error_to_string(ALLOC_ERROR_UNSUPPORTED_OPERATION), "operation not supported");
}

/**
 * @test Verifies that known release results have readable descriptions.
 */
TEST(AllocatorErrors, ReleaseResultsHaveStrings) {
    EXPECT_STREQ(release_result_to_string(RELEASE_SUCCESS), "no error");

    EXPECT_STREQ(release_result_to_string(RELEASE_ERROR_INVALID_ALLOCATOR), "invalid or uninitialized allocator");

    EXPECT_STREQ(release_result_to_string(RELEASE_ERROR_UNSUPPORTED_OPERATION), "operation not supported");
}

/**
 * @test Verifies that unknown error/result values return NULL.
 */
TEST(AllocatorErrors, InvalidValuesReturnNull) {
    EXPECT_EQ(alloc_error_to_string(static_cast<AllocError>(999)), nullptr);

    EXPECT_EQ(release_result_to_string(static_cast<ReleaseResult>(999)), nullptr);
}
