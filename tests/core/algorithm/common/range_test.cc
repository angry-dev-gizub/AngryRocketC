/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include <gtest/gtest.h>

#include <cstdlib>

#include <allocator/allocator.h>
#include <iterator/iterator.h>
#include <algorithm/common/range.h>
#include <algorithm/common/sentinel.h>

/* -------------------------------------------------------------------------- */
/* Test helpers                                                               */
/* -------------------------------------------------------------------------- */

struct TestAllocatorContext {
    size_t alloc_calls;
    size_t free_calls;
    bool fail_alloc;
};

struct TestIteratorContext {
    size_t position;
    int value;

    IteratorResult clone_result;
    IteratorResult destroy_result;
};

struct TestSentinelContext {
    size_t boundary;

    SentinelResult clone_result;
    SentinelResult destroy_result;
};

static AllocResult test_mem_alloc(void* context, size_t size) {
    TestAllocatorContext* allocator_context =
        static_cast<TestAllocatorContext*>(context);

    allocator_context->alloc_calls++;

    if(allocator_context->fail_alloc) {
        return AllocResult{
            .ok = false,
            .as =
                {
                    .error = ALLOC_ERROR_OUT_OF_MEMORY,
                },
        };
    }

    if(size == 0) {
        return AllocResult{
            .ok = true,
            .as =
                {
                    .ptr = nullptr,
                },
        };
    }

    void* ptr = std::malloc(size);

    if(!ptr) {
        return AllocResult{
            .ok = false,
            .as =
                {
                    .error = ALLOC_ERROR_OUT_OF_MEMORY,
                },
        };
    }

    return AllocResult{
        .ok = true,
        .as =
            {
                .ptr = ptr,
            },
    };
}

static void test_mem_free(void* context, void* ptr) {
    TestAllocatorContext* allocator_context =
        static_cast<TestAllocatorContext*>(context);

    allocator_context->free_calls++;
    std::free(ptr);
}

static Allocator make_test_allocator(TestAllocatorContext* context) {
    Allocator allocator{};

    AllocatorDesc desc{
        .context = context,
        .iface =
            AllocatorInterface{
                .mem_alloc         = test_mem_alloc,
                .mem_aligned_alloc = nullptr,
                .mem_zero_alloc    = nullptr,
                .mem_realloc       = nullptr,
                .mem_aligned_free  = nullptr,
                .mem_free          = test_mem_free,
                .mem_clear         = nullptr,
            },
    };

    EXPECT_TRUE(allocator_init(&allocator, &desc));

    return allocator;
}

/* -------------------------------------------------------------------------- */
/* Iterator implementation used by Range tests                                */
/* -------------------------------------------------------------------------- */

static IteratorResult test_iterator_next(void* context) {
    if(!context)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    TestIteratorContext* iterator_context =
        static_cast<TestIteratorContext*>(context);

    iterator_context->position++;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_iterator_get(const void* context, const void** out) {
    if(!context || !out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    const TestIteratorContext* iterator_context =
        static_cast<const TestIteratorContext*>(context);

    *out = &iterator_context->value;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_iterator_equals(
    const void* a,
    const void* b,
    bool* out
) {
    if(!a || !b || !out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    const TestIteratorContext* lhs =
        static_cast<const TestIteratorContext*>(a);

    const TestIteratorContext* rhs =
        static_cast<const TestIteratorContext*>(b);

    *out = lhs->position == rhs->position;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_iterator_clone(
    const void* context,
    void** out,
    Allocator* allocator
) {
    if(!context || !out || !allocator)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    *out = nullptr;

    const TestIteratorContext* source =
        static_cast<const TestIteratorContext*>(context);

    if(source->clone_result != ITERATOR_SUCCESS)
        return source->clone_result;

    AllocResult allocation =
        allocator_alloc(allocator, sizeof(TestIteratorContext));

    if(!allocation.ok)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    TestIteratorContext* clone =
        static_cast<TestIteratorContext*>(allocation.as.ptr);

    *clone = *source;
    *out   = clone;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_iterator_destroy(
    void* context,
    Allocator* allocator
) {
    if(!context || !allocator)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    TestIteratorContext* iterator_context =
        static_cast<TestIteratorContext*>(context);

    if(iterator_context->destroy_result != ITERATOR_SUCCESS)
        return iterator_context->destroy_result;

    if(allocator_free(allocator, iterator_context) != RELEASE_SUCCESS)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    return ITERATOR_SUCCESS;
}

static Iterator make_test_iterator(
    TestIteratorContext* context,
    size_t type_size = sizeof(int),
    bool cloning_supported = true
) {
    Iterator iterator{};

    IteratorDesc desc{
        .context   = context,
        .type_size = type_size,
        .iface =
            IteratorInterface{
                .next     = test_iterator_next,
                .previous = nullptr,
                .advance  = nullptr,
                .distance = nullptr,
                .get      = test_iterator_get,
                .equals   = test_iterator_equals,
                .clone    = cloning_supported ? test_iterator_clone : nullptr,
                .destroy  = cloning_supported ? test_iterator_destroy : nullptr,
            },
    };

    EXPECT_TRUE(iterator_init(&iterator, &desc));

    return iterator;
}

/* -------------------------------------------------------------------------- */
/* Sentinel implementation used by Range tests                                */
/* -------------------------------------------------------------------------- */

static SentinelResult test_sentinel_equals(
    const void* sentinel_context,
    const void* iterator_context,
    bool* out
) {
    if(!sentinel_context || !iterator_context || !out)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    const TestSentinelContext* sentinel =
        static_cast<const TestSentinelContext*>(sentinel_context);

    const TestIteratorContext* iterator =
        static_cast<const TestIteratorContext*>(iterator_context);

    *out = iterator->position == sentinel->boundary;

    return SENTINEL_SUCCESS;
}

static SentinelResult test_sentinel_clone(
    const void* context,
    void** out,
    Allocator* allocator
) {
    if(!context || !out || !allocator)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = nullptr;

    const TestSentinelContext* source =
        static_cast<const TestSentinelContext*>(context);

    if(source->clone_result != SENTINEL_SUCCESS)
        return source->clone_result;

    AllocResult allocation =
        allocator_alloc(allocator, sizeof(TestSentinelContext));

    if(!allocation.ok)
        return SENTINEL_ERROR_ALLOCATOR_ERROR;

    TestSentinelContext* clone =
        static_cast<TestSentinelContext*>(allocation.as.ptr);

    *clone = *source;
    *out   = clone;

    return SENTINEL_SUCCESS;
}

static SentinelResult test_sentinel_destroy(
    void* context,
    Allocator* allocator
) {
    if(!context || !allocator)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    TestSentinelContext* sentinel_context =
        static_cast<TestSentinelContext*>(context);

    if(sentinel_context->destroy_result != SENTINEL_SUCCESS)
        return sentinel_context->destroy_result;

    if(allocator_free(allocator, sentinel_context) != RELEASE_SUCCESS)
        return SENTINEL_ERROR_ALLOCATOR_ERROR;

    return SENTINEL_SUCCESS;
}

static Sentinel make_test_sentinel(
    TestSentinelContext* context,
    size_t type_size = sizeof(int),
    bool cloning_supported = true
) {
    Sentinel sentinel{};

    SentinelDesc desc{
        .context   = context,
        .type_size = type_size,
        .iface =
            SentinelInterface{
                .equals  = test_sentinel_equals,
                .clone   = cloning_supported ? test_sentinel_clone : nullptr,
                .destroy = cloning_supported ? test_sentinel_destroy : nullptr,
            },
    };

    EXPECT_EQ(
        sentinel_init(&sentinel, &desc),
        SENTINEL_SUCCESS
    );

    return sentinel;
}

/* -------------------------------------------------------------------------- */
/* range_init                                                                 */
/* -------------------------------------------------------------------------- */

TEST(RangeInit, NullOutputReturnsInvalidArguments) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    EXPECT_EQ(
        range_init(nullptr, &iterator, &sentinel, &allocator),
        RANGE_ERROR_INVALID_ARGUMENTS
    );

    EXPECT_EQ(allocator_context.alloc_calls, 0U);
    EXPECT_EQ(allocator_context.free_calls, 0U);
}

TEST(RangeInit, NullIteratorReturnsInvalidIteratorAndResetsOutput) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{
        .initialized = true,
    };

    EXPECT_EQ(
        range_init(&range, nullptr, &sentinel, &allocator),
        RANGE_ERROR_INVALID_ITERATOR
    );

    EXPECT_FALSE(range.initialized);
    EXPECT_FALSE(iterator_is_valid(&range.begin));
    EXPECT_FALSE(sentinel_is_valid(&range.end));
}

TEST(RangeInit, InvalidIteratorReturnsInvalidIterator) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    Iterator iterator{};

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_INVALID_ITERATOR
    );

    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeInit, NullSentinelReturnsInvalidSentinelAndResetsOutput) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    Range range{
        .initialized = true,
    };

    EXPECT_EQ(
        range_init(&range, &iterator, nullptr, &allocator),
        RANGE_ERROR_INVALID_SENTINEL
    );

    EXPECT_FALSE(range.initialized);
    EXPECT_FALSE(iterator_is_valid(&range.begin));
    EXPECT_FALSE(sentinel_is_valid(&range.end));
}

TEST(RangeInit, InvalidSentinelReturnsInvalidSentinel) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    Sentinel sentinel{};
    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_INVALID_SENTINEL
    );

    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeInit, NullAllocatorReturnsInvalidAllocator) {
    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, nullptr),
        RANGE_ERROR_INVALID_ALLOCATOR
    );

    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeInit, UninitializedAllocatorReturnsInvalidAllocator) {
    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Allocator allocator{};
    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_INVALID_ALLOCATOR
    );

    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeInit, MismatchedTypeSizesReturnIncompatibleTypes) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(int));

    TestSentinelContext sentinel_context{};
    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(long long));

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_INCOMPATIBLE_TYPES
    );

    EXPECT_FALSE(range_is_valid(&range));
    EXPECT_EQ(allocator_context.alloc_calls, 0U);
}

TEST(RangeInit, IteratorWithoutCloneReturnsIteratorError) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(int), false);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_ITERATOR_ERROR
    );

    EXPECT_FALSE(range_is_valid(&range));
    EXPECT_EQ(allocator_context.alloc_calls, 0U);
}

TEST(RangeInit, IteratorCloneFailureReturnsIteratorError) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{
        .clone_result = ITERATOR_ERROR_OUT_OF_RANGE,
    };

    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_ITERATOR_ERROR
    );

    EXPECT_FALSE(range_is_valid(&range));
    EXPECT_EQ(allocator_context.alloc_calls, 0U);
}

TEST(RangeInit, SentinelWithoutCloneReturnsSentinelErrorAndRollsBackIterator) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(int), false);

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_SENTINEL_ERROR
    );

    EXPECT_FALSE(range_is_valid(&range));
    EXPECT_FALSE(iterator_is_valid(&range.begin));
    EXPECT_FALSE(sentinel_is_valid(&range.end));

    EXPECT_EQ(allocator_context.alloc_calls, 1U);
    EXPECT_EQ(allocator_context.free_calls, 1U);
}

TEST(RangeInit, SentinelCloneFailureRollsBackIterator) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{
        .clone_result = SENTINEL_ERROR_INVALID_ARGUMENTS,
    };

    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_SENTINEL_ERROR
    );

    EXPECT_FALSE(range_is_valid(&range));
    EXPECT_FALSE(iterator_is_valid(&range.begin));
    EXPECT_FALSE(sentinel_is_valid(&range.end));

    EXPECT_EQ(allocator_context.alloc_calls, 1U);
    EXPECT_EQ(allocator_context.free_calls, 1U);
}

TEST(RangeInit, FailedRollbackPreservesOwnedIteratorForLaterCleanup) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{
        .destroy_result = ITERATOR_ERROR_INVALID_ARGUMENTS,
    };

    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{
        .clone_result = SENTINEL_ERROR_INVALID_ARGUMENTS,
    };

    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_ITERATOR_ERROR
    );

    EXPECT_FALSE(range.initialized);
    EXPECT_FALSE(range_is_valid(&range));

    ASSERT_TRUE(iterator_is_valid(&range.begin));
    EXPECT_TRUE(range.begin.owns_context);
    EXPECT_FALSE(sentinel_is_valid(&range.end));

    EXPECT_EQ(allocator_context.alloc_calls, 1U);
    EXPECT_EQ(allocator_context.free_calls, 0U);

    TestIteratorContext* owned_context =
        static_cast<TestIteratorContext*>(range.begin.context);

    ASSERT_NE(owned_context, nullptr);
    owned_context->destroy_result = ITERATOR_SUCCESS;

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_SUCCESS
    );

    EXPECT_EQ(allocator_context.free_calls, 1U);
    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeInit, AllocatorFailureDuringIteratorCloneReturnsIteratorError) {
    TestAllocatorContext allocator_context{
        .fail_alloc = true,
    };

    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    EXPECT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_ERROR_ITERATOR_ERROR
    );

    EXPECT_FALSE(range_is_valid(&range));
    EXPECT_EQ(allocator_context.alloc_calls, 1U);
    EXPECT_EQ(allocator_context.free_calls, 0U);
}

TEST(RangeInit, SuccessCreatesOwnedIndependentEndpoints) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{
        .position = 3,
        .value = 42,
    };

    TestSentinelContext sentinel_context{
        .boundary = 9,
    };

    Iterator iterator = make_test_iterator(&iterator_context);
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    EXPECT_TRUE(range.initialized);
    EXPECT_TRUE(range_is_valid(&range));

    EXPECT_TRUE(range.begin.owns_context);
    EXPECT_TRUE(range.end.owns_context);

    EXPECT_NE(range.begin.context, iterator.context);
    EXPECT_NE(range.end.context, sentinel.context);

    ASSERT_NE(range.begin.context, nullptr);
    ASSERT_NE(range.end.context, nullptr);

    const TestIteratorContext* cloned_iterator =
        static_cast<const TestIteratorContext*>(range.begin.context);

    const TestSentinelContext* cloned_sentinel =
        static_cast<const TestSentinelContext*>(range.end.context);

    EXPECT_EQ(cloned_iterator->position, 3U);
    EXPECT_EQ(cloned_iterator->value, 42);
    EXPECT_EQ(cloned_sentinel->boundary, 9U);

    EXPECT_EQ(range.begin.cloning_allocator, &allocator);
    EXPECT_EQ(range.end.cloning_allocator, &allocator);

    EXPECT_EQ(allocator_context.alloc_calls, 2U);
    EXPECT_EQ(allocator_context.free_calls, 0U);

    EXPECT_EQ(range_destroy(&range), RANGE_SUCCESS);

    EXPECT_EQ(allocator_context.free_calls, 2U);
}

TEST(RangeInit, SourceContextChangesDoNotModifyOwnedEndpointContexts) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{
        .position = 2,
        .value = 11,
    };

    TestSentinelContext sentinel_context{
        .boundary = 7,
    };

    Iterator iterator = make_test_iterator(&iterator_context);
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    iterator_context.position = 100;
    iterator_context.value    = 999;
    sentinel_context.boundary = 200;

    const TestIteratorContext* cloned_iterator =
        static_cast<const TestIteratorContext*>(range.begin.context);

    const TestSentinelContext* cloned_sentinel =
        static_cast<const TestSentinelContext*>(range.end.context);

    ASSERT_NE(cloned_iterator, nullptr);
    ASSERT_NE(cloned_sentinel, nullptr);

    EXPECT_EQ(cloned_iterator->position, 2U);
    EXPECT_EQ(cloned_iterator->value, 11);
    EXPECT_EQ(cloned_sentinel->boundary, 7U);

    EXPECT_EQ(range_destroy(&range), RANGE_SUCCESS);
}

TEST(RangeInit, DestroyingBorrowedSourceWrappersDoesNotInvalidateRange) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    TestSentinelContext sentinel_context{
        .boundary = 4,
    };

    Iterator iterator = make_test_iterator(&iterator_context);
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    ASSERT_EQ(
        iterator_destroy(&iterator),
        ITERATOR_SUCCESS
    );

    ASSERT_EQ(
        sentinel_destroy(&sentinel),
        SENTINEL_SUCCESS
    );

    EXPECT_TRUE(range_is_valid(&range));

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_SUCCESS
    );
}

/* -------------------------------------------------------------------------- */
/* range_is_valid                                                             */
/* -------------------------------------------------------------------------- */

TEST(RangeIsValid, NullRangeIsInvalid) {
    EXPECT_FALSE(range_is_valid(nullptr));
}

TEST(RangeIsValid, ZeroInitializedRangeIsInvalid) {
    Range range{};

    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeIsValid, InitializedRangeWithInvalidBeginIsInvalid) {
    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{
        .initialized = true,
        .begin = Iterator{},
        .end = sentinel,
    };

    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeIsValid, InitializedRangeWithInvalidEndIsInvalid) {
    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    Range range{
        .initialized = true,
        .begin = iterator,
        .end = Sentinel{},
    };

    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeIsValid, SuccessfullyInitializedRangeIsValid) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    EXPECT_TRUE(range_is_valid(&range));

    EXPECT_EQ(range_destroy(&range), RANGE_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* range_get_type_size                                                        */
/* -------------------------------------------------------------------------- */

TEST(RangeGetTypeSize, NullRangeReturnsZero) {
    EXPECT_EQ(range_get_type_size(nullptr), 0U);
}

TEST(RangeGetTypeSize, InvalidRangeReturnsZero) {
    Range range{};

    EXPECT_EQ(range_get_type_size(&range), 0U);
}

TEST(RangeGetTypeSize, ValidRangeReturnsEndpointTypeSize) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(long long));

    TestSentinelContext sentinel_context{};
    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(long long));

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    EXPECT_EQ(
        range_get_type_size(&range),
        sizeof(long long)
    );

    EXPECT_EQ(range_destroy(&range), RANGE_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* range_destroy                                                              */
/* -------------------------------------------------------------------------- */

TEST(RangeDestroy, NullRangeReturnsInvalidRange) {
    EXPECT_EQ(
        range_destroy(nullptr),
        RANGE_ERROR_INVALID_RANGE
    );
}

TEST(RangeDestroy, ZeroInitializedRangeReturnsInvalidRange) {
    Range range{};

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_ERROR_INVALID_RANGE
    );

    EXPECT_FALSE(range.initialized);
}

TEST(RangeDestroy, ValidRangeDestroysBothOwnedEndpoints) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator iterator = make_test_iterator(&iterator_context);

    TestSentinelContext sentinel_context{};
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    ASSERT_EQ(allocator_context.alloc_calls, 2U);

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_SUCCESS
    );

    EXPECT_EQ(allocator_context.free_calls, 2U);

    EXPECT_FALSE(range.initialized);
    EXPECT_FALSE(iterator_is_valid(&range.begin));
    EXPECT_FALSE(sentinel_is_valid(&range.end));
}

TEST(RangeDestroy, IteratorDestroyFailurePreservesBothEndpoints) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{
        .destroy_result = ITERATOR_ERROR_INVALID_ARGUMENTS,
    };

    TestSentinelContext sentinel_context{};

    Iterator iterator = make_test_iterator(&iterator_context);
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_ERROR_ITERATOR_ERROR
    );

    EXPECT_TRUE(iterator_is_valid(&range.begin));
    EXPECT_TRUE(sentinel_is_valid(&range.end));
    EXPECT_EQ(allocator_context.free_calls, 0U);

    TestIteratorContext* owned_iterator =
        static_cast<TestIteratorContext*>(range.begin.context);

    ASSERT_NE(owned_iterator, nullptr);
    owned_iterator->destroy_result = ITERATOR_SUCCESS;

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_SUCCESS
    );

    EXPECT_EQ(allocator_context.free_calls, 2U);
}

TEST(RangeDestroy, SentinelDestroyFailurePreservesSentinelAfterBeginIsDestroyed) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};

    TestSentinelContext sentinel_context{
        .destroy_result = SENTINEL_ERROR_INVALID_ARGUMENTS,
    };

    Iterator iterator = make_test_iterator(&iterator_context);
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_ERROR_SENTINEL_ERROR
    );

    EXPECT_FALSE(iterator_is_valid(&range.begin));
    EXPECT_TRUE(sentinel_is_valid(&range.end));
    EXPECT_EQ(allocator_context.free_calls, 1U);

    TestSentinelContext* owned_sentinel =
        static_cast<TestSentinelContext*>(range.end.context);

    ASSERT_NE(owned_sentinel, nullptr);
    owned_sentinel->destroy_result = SENTINEL_SUCCESS;

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_SUCCESS
    );

    EXPECT_EQ(allocator_context.free_calls, 2U);
    EXPECT_FALSE(range_is_valid(&range));
}

TEST(RangeDestroy, CanCleanUpIteratorOnlyPartialRange) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{};
    Iterator source = make_test_iterator(&iterator_context);

    Iterator owned_iterator{};

    ASSERT_EQ(
        iterator_clone(&source, &owned_iterator, &allocator),
        ITERATOR_SUCCESS
    );

    Range range{
        .initialized = false,
        .begin = owned_iterator,
        .end = Sentinel{},
    };

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_SUCCESS
    );

    EXPECT_EQ(allocator_context.free_calls, 1U);
    EXPECT_FALSE(iterator_is_valid(&range.begin));
}

TEST(RangeDestroy, CanCleanUpSentinelOnlyPartialRange) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestSentinelContext sentinel_context{};
    Sentinel source = make_test_sentinel(&sentinel_context);

    Sentinel owned_sentinel{};

    ASSERT_EQ(
        sentinel_clone(&source, &owned_sentinel, &allocator),
        SENTINEL_SUCCESS
    );

    Range range{
        .initialized = false,
        .begin = Iterator{},
        .end = owned_sentinel,
    };

    EXPECT_EQ(
        range_destroy(&range),
        RANGE_SUCCESS
    );

    EXPECT_EQ(allocator_context.free_calls, 1U);
    EXPECT_FALSE(sentinel_is_valid(&range.end));
}

/* -------------------------------------------------------------------------- */
/* Endpoint behavior                                                          */
/* -------------------------------------------------------------------------- */

TEST(RangeEndpoints, StoredBeginRemainsStableWhenWorkingCloneMoves) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{
        .position = 0,
        .value = 123,
    };

    TestSentinelContext sentinel_context{
        .boundary = 3,
    };

    Iterator iterator = make_test_iterator(&iterator_context);
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    Iterator current{};

    ASSERT_EQ(
        iterator_clone(&range.begin, &current, &allocator),
        ITERATOR_SUCCESS
    );

    ASSERT_EQ(
        iterator_next(&current),
        ITERATOR_SUCCESS
    );

    const TestIteratorContext* stored_begin =
        static_cast<const TestIteratorContext*>(range.begin.context);

    const TestIteratorContext* working =
        static_cast<const TestIteratorContext*>(current.context);

    ASSERT_NE(stored_begin, nullptr);
    ASSERT_NE(working, nullptr);

    EXPECT_EQ(stored_begin->position, 0U);
    EXPECT_EQ(working->position, 1U);

    EXPECT_EQ(iterator_destroy(&current), ITERATOR_SUCCESS);
    EXPECT_EQ(range_destroy(&range), RANGE_SUCCESS);
}

TEST(RangeEndpoints, ClonedBeginCanTraverseUntilOwnedSentinel) {
    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);

    TestIteratorContext iterator_context{
        .position = 0,
        .value = 5,
    };

    TestSentinelContext sentinel_context{
        .boundary = 3,
    };

    Iterator iterator = make_test_iterator(&iterator_context);
    Sentinel sentinel = make_test_sentinel(&sentinel_context);

    Range range{};

    ASSERT_EQ(
        range_init(&range, &iterator, &sentinel, &allocator),
        RANGE_SUCCESS
    );

    Iterator current{};

    ASSERT_EQ(
        iterator_clone(&range.begin, &current, &allocator),
        ITERATOR_SUCCESS
    );

    size_t visited = 0;

    while(true) {
        bool at_end = false;

        ASSERT_EQ(
            sentinel_equals(&range.end, &current, &at_end),
            SENTINEL_SUCCESS
        );

        if(at_end)
            break;

        visited++;

        ASSERT_EQ(
            iterator_next(&current),
            ITERATOR_SUCCESS
        );
    }

    EXPECT_EQ(visited, 3U);

    EXPECT_EQ(iterator_destroy(&current), ITERATOR_SUCCESS);
    EXPECT_EQ(range_destroy(&range), RANGE_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* range_result_to_string                                                     */
/* -------------------------------------------------------------------------- */

TEST(RangeResultToString, ReturnsExpectedStrings) {
    EXPECT_STREQ(
        range_result_to_string(RANGE_SUCCESS),
        "no error"
    );

    EXPECT_STREQ(
        range_result_to_string(RANGE_ERROR_INVALID_RANGE),
        "null or invalid range"
    );

    EXPECT_STREQ(
        range_result_to_string(RANGE_ERROR_INVALID_ITERATOR),
        "null or invalid iterator endpoint"
    );

    EXPECT_STREQ(
        range_result_to_string(RANGE_ERROR_INVALID_SENTINEL),
        "null or invalid sentinel endpoint"
    );

    EXPECT_STREQ(
        range_result_to_string(RANGE_ERROR_INVALID_ALLOCATOR),
        "null or invalid allocator"
    );

    EXPECT_STREQ(
        range_result_to_string(RANGE_ERROR_INCOMPATIBLE_TYPES),
        "iterator and sentinel type sizes do not match"
    );

    EXPECT_STREQ(
        range_result_to_string(RANGE_ERROR_ITERATOR_ERROR),
        "iterator endpoint operation failed"
    );

    EXPECT_STREQ(
        range_result_to_string(RANGE_ERROR_SENTINEL_ERROR),
        "sentinel endpoint operation failed"
    );
}

TEST(RangeResultToString, InvalidValueReturnsNull) {
    EXPECT_EQ(
        range_result_to_string(static_cast<RangeResult>(-1)),
        nullptr
    );

    EXPECT_EQ(
        range_result_to_string(static_cast<RangeResult>(999)),
        nullptr
    );
}
