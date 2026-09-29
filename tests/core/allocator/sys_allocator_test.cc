/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file sys_allocator_test.cc
 *
 * @author Nelson Somé
 *
 * @brief Tests for the system allocator.
 *
 * These tests verify system allocator initialization, supported features,
 * allocation behavior, zero-initialized allocation, reallocation,
 * aligned allocation, release operations, and error handling.
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <allocator/allocator.h>
#include <allocator/allocators/system_allocator.h>

/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that allocator initialization rejects a NULL output pointer.
 */
TEST(SystemAllocatorInit, RejectsNullOutput) {
    EXPECT_FALSE(system_allocator_init(nullptr));
}

/**
 * @test Verifies successful initialization with a valid allocator pointer.
 */
TEST(SystemAllocatorInit, InitializesValidAllocator) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    EXPECT_TRUE(allocator.initialized);
    EXPECT_EQ(allocator.context, nullptr);
}

/* -------------------------------------------------------------------------- */
/* Feature discovery                                                          */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that the system allocator reports all supported features.
 */
TEST(SystemAllocatorFeatures, DiscoversSupportedFeatures) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocatorFeatureFlags expected =
        ALLOCATOR_FEATURE_ZERO_ALLOC |
        ALLOCATOR_FEATURE_ALIGNED_ALLOC |
        ALLOCATOR_FEATURE_REALLOC |
        ALLOCATOR_FEATURE_FREE |
        ALLOCATOR_FEATURE_ALIGNED_FREE;

    EXPECT_TRUE(
        allocator_has_features(
            &allocator,
            expected));
}

/**
 * @test Verifies that the system allocator does not report clear support.
 */
TEST(SystemAllocatorFeatures, DoesNotSupportClear) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    EXPECT_FALSE(
        allocator_has_features(
            &allocator,
            ALLOCATOR_FEATURE_CLEAR));
}

/* -------------------------------------------------------------------------- */
/* Allocation                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that normal allocation succeeds for a non-zero size.
 */
TEST(SystemAllocatorAlloc, AllocatesMemory) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_alloc(
            &allocator,
            64);

    ASSERT_TRUE(result.ok);
    ASSERT_NE(result.as.ptr, nullptr);

    EXPECT_EQ(
        allocator_free(
            &allocator,
            result.as.ptr),
        RELEASE_SUCCESS);
}

/**
 * @test Verifies that zero-size allocation succeeds as a no-op.
 */
TEST(SystemAllocatorAlloc, ZeroSizeIsSuccessfulNoOp) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_alloc(
            &allocator,
            0);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.as.ptr, nullptr);
}

/**
 * @test Verifies that allocated memory can be written to and read back.
 */
TEST(SystemAllocatorAlloc, AllocatedMemoryIsUsable) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_alloc(
            &allocator,
            sizeof(int));

    ASSERT_TRUE(result.ok);
    ASSERT_NE(result.as.ptr, nullptr);

    auto* value =
        static_cast<int*>(result.as.ptr);

    *value = 42;

    EXPECT_EQ(*value, 42);

    EXPECT_EQ(
        allocator_free(
            &allocator,
            result.as.ptr),
        RELEASE_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* Zero allocation                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that zero allocation returns zero-initialized memory.
 */
TEST(SystemAllocatorZeroAlloc, ReturnsZeroInitializedMemory) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    constexpr size_t count = 16;

    const AllocResult result =
        allocator_zero_alloc(
            &allocator,
            count,
            sizeof(int));

    ASSERT_TRUE(result.ok);
    ASSERT_NE(result.as.ptr, nullptr);

    auto* values =
        static_cast<int*>(result.as.ptr);

    for(size_t i = 0; i < count; ++i)
        EXPECT_EQ(values[i], 0);

    EXPECT_EQ(
        allocator_free(
            &allocator,
            result.as.ptr),
        RELEASE_SUCCESS);
}

/**
 * @test Verifies that a zero element count results in a successful no-op.
 */
TEST(SystemAllocatorZeroAlloc, ZeroCountIsSuccessfulNoOp) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_zero_alloc(
            &allocator,
            0,
            sizeof(int));

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.as.ptr, nullptr);
}

/**
 * @test Verifies that a zero element size results in a successful no-op.
 */
TEST(SystemAllocatorZeroAlloc, ZeroSizeIsSuccessfulNoOp) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_zero_alloc(
            &allocator,
            16,
            0);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.as.ptr, nullptr);
}

/* -------------------------------------------------------------------------- */
/* Reallocation                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that reallocation can grow an allocation.
 */
TEST(SystemAllocatorRealloc, GrowsAllocation) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    AllocResult allocation =
        allocator_alloc(
            &allocator,
            32);

    ASSERT_TRUE(allocation.ok);

    const AllocResult resized =
        allocator_realloc(
            &allocator,
            allocation.as.ptr,
            128);

    ASSERT_TRUE(resized.ok);
    ASSERT_NE(resized.as.ptr, nullptr);

    EXPECT_EQ(
        allocator_free(
            &allocator,
            resized.as.ptr),
        RELEASE_SUCCESS);
}

/**
 * @test Verifies that reallocation preserves the contents of the original block.
 */
TEST(SystemAllocatorRealloc, PreservesExistingData) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    AllocResult allocation =
        allocator_alloc(
            &allocator,
            4 * sizeof(int));

    ASSERT_TRUE(allocation.ok);
    ASSERT_NE(allocation.as.ptr, nullptr);

    auto* values =
        static_cast<int*>(allocation.as.ptr);

    values[0] = 10;
    values[1] = 20;
    values[2] = 30;
    values[3] = 40;

    const AllocResult resized =
        allocator_realloc(
            &allocator,
            values,
            8 * sizeof(int));

    ASSERT_TRUE(resized.ok);
    ASSERT_NE(resized.as.ptr, nullptr);

    auto* resized_values =
        static_cast<int*>(resized.as.ptr);

    EXPECT_EQ(resized_values[0], 10);
    EXPECT_EQ(resized_values[1], 20);
    EXPECT_EQ(resized_values[2], 30);
    EXPECT_EQ(resized_values[3], 40);

    EXPECT_EQ(
        allocator_free(
            &allocator,
            resized.as.ptr),
        RELEASE_SUCCESS);
}

/**
 * @test Verifies that realloc rejects a NULL pointer.
 */
TEST(SystemAllocatorRealloc, RejectsNullPointer) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_realloc(
            &allocator,
            nullptr,
            128);

    ASSERT_FALSE(result.ok);

    EXPECT_EQ(
        result.as.error,
        ALLOC_ERROR_INVALID_ARGUMENTS);
}

/**
 * @test Verifies that realloc rejects a zero new size.
 */
TEST(SystemAllocatorRealloc, RejectsZeroSize) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult allocation =
        allocator_alloc(
            &allocator,
            64);

    ASSERT_TRUE(allocation.ok);
    ASSERT_NE(allocation.as.ptr, nullptr);

    const AllocResult result =
        allocator_realloc(
            &allocator,
            allocation.as.ptr,
            0);

    ASSERT_FALSE(result.ok);

    EXPECT_EQ(
        result.as.error,
        ALLOC_ERROR_INVALID_ARGUMENTS);

    EXPECT_EQ(
        allocator_free(
            &allocator,
            allocation.as.ptr),
        RELEASE_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* Aligned allocation                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that aligned allocation returns memory satisfying the
 *       requested alignment.
 */
TEST(SystemAllocatorAlignedAlloc, ReturnsRequestedAlignment) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    constexpr size_t alignment = 64;

    const AllocResult result =
        allocator_aligned_alloc(
            &allocator,
            alignment,
            128);

    ASSERT_TRUE(result.ok);
    ASSERT_NE(result.as.ptr, nullptr);

    const auto address =
        reinterpret_cast<std::uintptr_t>(
            result.as.ptr);

    EXPECT_EQ(
        address % alignment,
        0U);

    EXPECT_EQ(
        allocator_aligned_free(
            &allocator,
            result.as.ptr),
        RELEASE_SUCCESS);
}

/**
 * @test Verifies that zero-size aligned allocation succeeds as a no-op.
 */
TEST(SystemAllocatorAlignedAlloc, ZeroSizeIsSuccessfulNoOp) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_aligned_alloc(
            &allocator,
            64,
            0);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.as.ptr, nullptr);
}

/**
 * @test Verifies that zero alignment is rejected.
 */
TEST(SystemAllocatorAlignedAlloc, RejectsZeroAlignment) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_aligned_alloc(
            &allocator,
            0,
            128);

    ASSERT_FALSE(result.ok);

    EXPECT_EQ(
        result.as.error,
        ALLOC_ERROR_INVALID_ALIGNMENT);
}

/**
 * @test Verifies that non-power-of-two alignment is rejected.
 */
TEST(SystemAllocatorAlignedAlloc, RejectsNonPowerOfTwoAlignment) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult result =
        allocator_aligned_alloc(
            &allocator,
            3,
            128);

    ASSERT_FALSE(result.ok);

    EXPECT_EQ(
        result.as.error,
        ALLOC_ERROR_INVALID_ALIGNMENT);
}

/* -------------------------------------------------------------------------- */
/* Release operations                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @test Verifies that freeing a valid allocation succeeds.
 */
TEST(SystemAllocatorFree, FreesMemory) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    const AllocResult allocation =
        allocator_alloc(
            &allocator,
            64);

    ASSERT_TRUE(allocation.ok);
    ASSERT_NE(allocation.as.ptr, nullptr);

    EXPECT_EQ(
        allocator_free(
            &allocator,
            allocation.as.ptr),
        RELEASE_SUCCESS);
}

/**
 * @test Verifies that freeing NULL is a successful no-op.
 */
TEST(SystemAllocatorFree, NullPointerIsSuccessfulNoOp) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    EXPECT_EQ(
        allocator_free(
            &allocator,
            nullptr),
        RELEASE_SUCCESS);
}

/**
 * @test Verifies that aligned-free accepts NULL as a no-op.
 */
TEST(SystemAllocatorAlignedFree, NullPointerIsSuccessfulNoOp) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    EXPECT_EQ(
        allocator_aligned_free(
            &allocator,
            nullptr),
        RELEASE_SUCCESS);
}

/**
 * @test Verifies that clear is unsupported by the system allocator.
 */
TEST(SystemAllocatorClear, IsUnsupported) {
    Allocator allocator{};

    ASSERT_TRUE(system_allocator_init(&allocator));

    EXPECT_EQ(
        allocator_clear(&allocator),
        RELEASE_ERROR_UNSUPPORTED_OPERATION);
}
