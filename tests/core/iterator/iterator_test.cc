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

struct IteratorTestContext {
    size_t position;
    ptrdiff_t last_offset;
    void* value;

    IteratorResult next_result;
    IteratorResult previous_result;
    IteratorResult advance_result;
    IteratorResult distance_result;
    IteratorResult get_result;
    IteratorResult equals_result;
    IteratorResult clone_result;
    IteratorResult destroy_result;

    ptrdiff_t distance;
    bool equals;

    size_t next_calls;
    size_t previous_calls;
    size_t advance_calls;
    size_t distance_calls;
    size_t get_calls;
    size_t equals_calls;
};

struct TestAllocatorContext {
    size_t alloc_calls;
    size_t free_calls;
};

static IteratorResult test_next(void* context) {
    IteratorTestContext* ctx = static_cast<IteratorTestContext*>(context);

    ctx->next_calls++;

    if(ctx->next_result != ITERATOR_SUCCESS)
        return ctx->next_result;

    ctx->position++;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_previous(void* context) {
    IteratorTestContext* ctx = static_cast<IteratorTestContext*>(context);

    ctx->previous_calls++;

    if(ctx->previous_result != ITERATOR_SUCCESS)
        return ctx->previous_result;

    ctx->position--;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_advance(void* context, ptrdiff_t offset) {
    IteratorTestContext* ctx = static_cast<IteratorTestContext*>(context);

    ctx->advance_calls++;
    ctx->last_offset = offset;

    if(ctx->advance_result != ITERATOR_SUCCESS)
        return ctx->advance_result;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_distance(const void* start, const void* end, ptrdiff_t* out) {
    IteratorTestContext* start_ctx =
        const_cast<IteratorTestContext*>(static_cast<const IteratorTestContext*>(start));

    const IteratorTestContext* end_ctx =
        static_cast<const IteratorTestContext*>(end);

    start_ctx->distance_calls++;

    if(start_ctx->distance_result != ITERATOR_SUCCESS)
        return start_ctx->distance_result;

    *out = end_ctx->distance - start_ctx->distance;

    return ITERATOR_SUCCESS;
}

static IteratorResult other_distance(const void* start, const void* end, ptrdiff_t* out) {
    (void)start;
    (void)end;
    (void)out;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_get(const void* context, const void** out) {
    IteratorTestContext* ctx =
        const_cast<IteratorTestContext*>(static_cast<const IteratorTestContext*>(context));

    ctx->get_calls++;

    if(ctx->get_result != ITERATOR_SUCCESS)
        return ctx->get_result;

    *out = ctx->value;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_equals(const void* a, const void* b, bool* out) {
    IteratorTestContext* a_ctx =
        const_cast<IteratorTestContext*>(static_cast<const IteratorTestContext*>(a));

    const IteratorTestContext* b_ctx =
        static_cast<const IteratorTestContext*>(b);

    a_ctx->equals_calls++;

    if(a_ctx->equals_result != ITERATOR_SUCCESS)
        return a_ctx->equals_result;

    *out = a_ctx->equals == b_ctx->equals;

    return ITERATOR_SUCCESS;
}

static IteratorResult other_equals(const void* a, const void* b, bool* out) {
    (void)a;
    (void)b;

    *out = false;

    return ITERATOR_SUCCESS;
}

static AllocResult test_mem_alloc(void* context, size_t size) {
    TestAllocatorContext* allocator_context =
        static_cast<TestAllocatorContext*>(context);

    allocator_context->alloc_calls++;

    AllocResult result{};

    if(size == 0) {
        result.ok     = true;
        result.as.ptr = nullptr;
        return result;
    }

    void* ptr = std::malloc(size);

    if(!ptr) {
        result.ok       = false;
        result.as.error = ALLOC_ERROR_OUT_OF_MEMORY;
        return result;
    }

    result.ok     = true;
    result.as.ptr = ptr;

    return result;
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

    if(!allocator_init(&allocator, &desc))
        return Allocator{};

    return allocator;
}

static IteratorResult test_clone(const void* context, void** out, Allocator* allocator) {
    if(!out || !allocator)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    *out = nullptr;

    const IteratorTestContext* source =
        static_cast<const IteratorTestContext*>(context);

    if(!source)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    if(source->clone_result != ITERATOR_SUCCESS)
        return source->clone_result;

    AllocResult allocation = allocator_alloc(allocator, sizeof(IteratorTestContext));

    if(!allocation.ok)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    IteratorTestContext* clone =
        static_cast<IteratorTestContext*>(allocation.as.ptr);

    *clone = *source;
    *out   = clone;

    return ITERATOR_SUCCESS;
}

static IteratorResult test_destroy(void* context, Allocator* allocator) {
    if(!context || !allocator)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    IteratorTestContext* iterator_context =
        static_cast<IteratorTestContext*>(context);

    if(iterator_context->destroy_result != ITERATOR_SUCCESS)
        return iterator_context->destroy_result;

    if(allocator_free(allocator, iterator_context) != RELEASE_SUCCESS)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    return ITERATOR_SUCCESS;
}

static IteratorDesc make_forward_desc(IteratorTestContext* context) {
    return IteratorDesc{
        .context   = context,
        .type_size = sizeof(int),
        .iface =
            IteratorInterface{
                .next     = test_next,
                .previous = nullptr,
                .advance  = nullptr,
                .distance = nullptr,
                .get      = test_get,
                .equals   = test_equals,
                .clone    = nullptr,
                .destroy  = nullptr,
            },
    };
}

static IteratorDesc make_bidirectional_desc(IteratorTestContext* context) {
    return IteratorDesc{
        .context   = context,
        .type_size = sizeof(int),
        .iface =
            IteratorInterface{
                .next     = test_next,
                .previous = test_previous,
                .advance  = nullptr,
                .distance = nullptr,
                .get      = test_get,
                .equals   = test_equals,
                .clone    = nullptr,
                .destroy  = nullptr,
            },
    };
}

static IteratorDesc make_random_access_desc(IteratorTestContext* context) {
    return IteratorDesc{
        .context   = context,
        .type_size = sizeof(int),
        .iface =
            IteratorInterface{
                .next     = test_next,
                .previous = test_previous,
                .advance  = test_advance,
                .distance = test_distance,
                .get      = test_get,
                .equals   = test_equals,
                .clone    = nullptr,
                .destroy  = nullptr,
            },
    };
}

static IteratorDesc make_cloneable_random_access_desc(IteratorTestContext* context) {
    IteratorDesc desc = make_random_access_desc(context);

    desc.iface.clone   = test_clone;
    desc.iface.destroy = test_destroy;

    return desc;
}

/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

TEST(IteratorInit, NullOutputFails) {
    IteratorDesc desc = make_forward_desc(nullptr);

    EXPECT_FALSE(iterator_init(nullptr, &desc));
}

TEST(IteratorInit, NullDescriptorFails) {
    Iterator iterator{};

    EXPECT_FALSE(iterator_init(&iterator, nullptr));
    EXPECT_FALSE(iterator.initialized);
    EXPECT_EQ(iterator.category, ITERATOR_CATEGORY_INVALID);
}

TEST(IteratorInit, ZeroTypeSizeFails) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    desc.type_size = 0;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
    EXPECT_FALSE(iterator.initialized);
}

TEST(IteratorInit, CloneWithoutDestroyFails) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    desc.iface.clone = test_clone;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
}

TEST(IteratorInit, DestroyWithoutCloneFails) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    desc.iface.destroy = test_destroy;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
}

TEST(IteratorInit, MissingNextFails) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    desc.iface.next = nullptr;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
    EXPECT_FALSE(iterator.initialized);
}

TEST(IteratorInit, MissingGetFails) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    desc.iface.get = nullptr;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
    EXPECT_FALSE(iterator.initialized);
}

TEST(IteratorInit, MissingEqualsFails) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    desc.iface.equals = nullptr;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
    EXPECT_FALSE(iterator.initialized);
}

TEST(IteratorInit, AdvanceWithoutDistanceFails) {
    Iterator iterator{};
    IteratorDesc desc = make_bidirectional_desc(nullptr);

    desc.iface.advance = test_advance;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
}

TEST(IteratorInit, DistanceWithoutAdvanceFails) {
    Iterator iterator{};
    IteratorDesc desc = make_bidirectional_desc(nullptr);

    desc.iface.distance = test_distance;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
}

TEST(IteratorInit, RandomAccessWithoutPreviousFails) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    desc.iface.advance  = test_advance;
    desc.iface.distance = test_distance;

    EXPECT_FALSE(iterator_init(&iterator, &desc));
}

TEST(IteratorInit, ForwardIteratorSucceeds) {
    IteratorTestContext context{};
    Iterator iterator{};

    IteratorDesc desc = make_forward_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(iterator.initialized);
    EXPECT_FALSE(iterator.owns_context);
    EXPECT_EQ(iterator.type_size, sizeof(int));
    EXPECT_EQ(iterator.category, ITERATOR_CATEGORY_FORWARD);
    EXPECT_EQ(iterator.context, &context);
    EXPECT_EQ(iterator.cloning_allocator, nullptr);
}

TEST(IteratorInit, BidirectionalIteratorSucceeds) {
    IteratorTestContext context{};
    Iterator iterator{};

    IteratorDesc desc = make_bidirectional_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(iterator.initialized);
    EXPECT_FALSE(iterator.owns_context);
    EXPECT_EQ(iterator.type_size, sizeof(int));
    EXPECT_EQ(iterator.category, ITERATOR_CATEGORY_BIDIRECTIONAL);
    EXPECT_EQ(iterator.context, &context);
    EXPECT_EQ(iterator.cloning_allocator, nullptr);
}

TEST(IteratorInit, RandomAccessIteratorSucceeds) {
    IteratorTestContext context{};
    Iterator iterator{};

    IteratorDesc desc = make_random_access_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(iterator.initialized);
    EXPECT_FALSE(iterator.owns_context);
    EXPECT_EQ(iterator.type_size, sizeof(int));
    EXPECT_EQ(iterator.category, ITERATOR_CATEGORY_RANDOM_ACCESS);
    EXPECT_EQ(iterator.context, &context);
    EXPECT_EQ(iterator.cloning_allocator, nullptr);
}

TEST(IteratorInit, CopiesEntireInterface) {
    Iterator iterator{};
    IteratorDesc desc = make_random_access_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(iterator.iface.next, desc.iface.next);
    EXPECT_EQ(iterator.iface.previous, desc.iface.previous);
    EXPECT_EQ(iterator.iface.advance, desc.iface.advance);
    EXPECT_EQ(iterator.iface.distance, desc.iface.distance);
    EXPECT_EQ(iterator.iface.get, desc.iface.get);
    EXPECT_EQ(iterator.iface.equals, desc.iface.equals);
    EXPECT_EQ(iterator.iface.clone, desc.iface.clone);
    EXPECT_EQ(iterator.iface.destroy, desc.iface.destroy);
}

/* -------------------------------------------------------------------------- */
/* Type size                                                                  */
/* -------------------------------------------------------------------------- */

TEST(IteratorTypeSize, NullIteratorReturnsZero) {
    EXPECT_EQ(iterator_get_type_size(nullptr), 0U);
}

TEST(IteratorTypeSize, UninitializedIteratorReturnsZero) {
    Iterator iterator{};

    EXPECT_EQ(iterator_get_type_size(&iterator), 0U);
}

TEST(IteratorTypeSize, ReturnsConfiguredTypeSize) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    desc.type_size = sizeof(uint64_t);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(iterator_get_type_size(&iterator), sizeof(uint64_t));
}

/* -------------------------------------------------------------------------- */
/* Category                                                                   */
/* -------------------------------------------------------------------------- */

TEST(IteratorCategory, NullIteratorReturnsFalse) {
    EXPECT_FALSE(
        iterator_is_at_least(nullptr, ITERATOR_CATEGORY_FORWARD)
    );
}

TEST(IteratorCategory, UninitializedIteratorReturnsFalse) {
    Iterator iterator{};

    EXPECT_FALSE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_FORWARD)
    );
}

TEST(IteratorCategory, InvalidRequiredCategoryReturnsFalse) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_FALSE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_INVALID)
    );
}

TEST(IteratorCategory, CategoryAboveRandomAccessReturnsFalse) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_FALSE(
        iterator_is_at_least(
            &iterator,
            static_cast<IteratorCategory>(ITERATOR_CATEGORY_RANDOM_ACCESS + 1)
        )
    );
}

TEST(IteratorCategory, ForwardIsAtLeastForward) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_FORWARD)
    );
}

TEST(IteratorCategory, ForwardIsNotBidirectional) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_FALSE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_BIDIRECTIONAL)
    );
}

TEST(IteratorCategory, BidirectionalIsAtLeastForward) {
    Iterator iterator{};
    IteratorDesc desc = make_bidirectional_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_FORWARD)
    );
}

TEST(IteratorCategory, BidirectionalIsAtLeastBidirectional) {
    Iterator iterator{};
    IteratorDesc desc = make_bidirectional_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_BIDIRECTIONAL)
    );
}

TEST(IteratorCategory, BidirectionalIsNotRandomAccess) {
    Iterator iterator{};
    IteratorDesc desc = make_bidirectional_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_FALSE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_RANDOM_ACCESS)
    );
}

TEST(IteratorCategory, RandomAccessSatisfiesAllCategories) {
    Iterator iterator{};
    IteratorDesc desc = make_random_access_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_FORWARD)
    );

    EXPECT_TRUE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_BIDIRECTIONAL)
    );

    EXPECT_TRUE(
        iterator_is_at_least(&iterator, ITERATOR_CATEGORY_RANDOM_ACCESS)
    );
}

/* -------------------------------------------------------------------------- */
/* Next                                                                       */
/* -------------------------------------------------------------------------- */

TEST(IteratorNext, NullIteratorReturnsInvalidIterator) {
    EXPECT_EQ(
        iterator_next(nullptr),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorNext, UninitializedIteratorReturnsInvalidIterator) {
    Iterator iterator{};

    EXPECT_EQ(
        iterator_next(&iterator),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorNext, ForwardsOperationToCallback) {
    IteratorTestContext context{};
    Iterator iterator{};

    IteratorDesc desc = make_forward_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(iterator_next(&iterator), ITERATOR_SUCCESS);
    EXPECT_EQ(context.next_calls, 1U);
    EXPECT_EQ(context.position, 1U);
}

TEST(IteratorNext, PropagatesCallbackResult) {
    IteratorTestContext context{};
    context.next_result = ITERATOR_ERROR_OUT_OF_RANGE;

    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(
        iterator_next(&iterator),
        ITERATOR_ERROR_OUT_OF_RANGE
    );

    EXPECT_EQ(context.next_calls, 1U);
}

/* -------------------------------------------------------------------------- */
/* Previous                                                                   */
/* -------------------------------------------------------------------------- */

TEST(IteratorPrevious, NullIteratorReturnsInvalidIterator) {
    EXPECT_EQ(
        iterator_previous(nullptr),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorPrevious, ForwardIteratorReturnsUnsupportedOperation) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(
        iterator_previous(&iterator),
        ITERATOR_ERROR_UNSUPPORTED_OPERATION
    );
}

TEST(IteratorPrevious, ForwardsOperationToCallback) {
    IteratorTestContext context{};
    context.position = 5;

    Iterator iterator{};
    IteratorDesc desc = make_bidirectional_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(iterator_previous(&iterator), ITERATOR_SUCCESS);
    EXPECT_EQ(context.previous_calls, 1U);
    EXPECT_EQ(context.position, 4U);
}

TEST(IteratorPrevious, PropagatesCallbackResult) {
    IteratorTestContext context{};
    context.previous_result = ITERATOR_ERROR_OUT_OF_RANGE;

    Iterator iterator{};
    IteratorDesc desc = make_bidirectional_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(
        iterator_previous(&iterator),
        ITERATOR_ERROR_OUT_OF_RANGE
    );
}

/* -------------------------------------------------------------------------- */
/* Advance                                                                    */
/* -------------------------------------------------------------------------- */

TEST(IteratorAdvance, NullIteratorReturnsInvalidIterator) {
    EXPECT_EQ(
        iterator_advance(nullptr, 1),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorAdvance, ForwardIteratorReturnsUnsupportedOperation) {
    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(
        iterator_advance(&iterator, 1),
        ITERATOR_ERROR_UNSUPPORTED_OPERATION
    );
}

TEST(IteratorAdvance, BidirectionalIteratorReturnsUnsupportedOperation) {
    Iterator iterator{};
    IteratorDesc desc = make_bidirectional_desc(nullptr);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(
        iterator_advance(&iterator, 1),
        ITERATOR_ERROR_UNSUPPORTED_OPERATION
    );
}

TEST(IteratorAdvance, ForwardsOffsetToCallback) {
    IteratorTestContext context{};
    Iterator iterator{};

    IteratorDesc desc = make_random_access_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(iterator_advance(&iterator, -7), ITERATOR_SUCCESS);

    EXPECT_EQ(context.advance_calls, 1U);
    EXPECT_EQ(context.last_offset, -7);
}

TEST(IteratorAdvance, PropagatesCallbackResult) {
    IteratorTestContext context{};
    context.advance_result = ITERATOR_ERROR_OUT_OF_RANGE;

    Iterator iterator{};
    IteratorDesc desc = make_random_access_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(
        iterator_advance(&iterator, 500),
        ITERATOR_ERROR_OUT_OF_RANGE
    );
}

/* -------------------------------------------------------------------------- */
/* Distance                                                                   */
/* -------------------------------------------------------------------------- */

TEST(IteratorDistance, NullStartReturnsInvalidArguments) {
    Iterator iterator{};
    ptrdiff_t distance = 0;

    EXPECT_EQ(
        iterator_distance(nullptr, &iterator, &distance),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );
}

TEST(IteratorDistance, NullEndReturnsInvalidArguments) {
    Iterator iterator{};
    ptrdiff_t distance = 0;

    EXPECT_EQ(
        iterator_distance(&iterator, nullptr, &distance),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );
}

TEST(IteratorDistance, NullOutputReturnsInvalidArguments) {
    Iterator iterator{};

    EXPECT_EQ(
        iterator_distance(&iterator, &iterator, nullptr),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );
}

TEST(IteratorDistance, UninitializedIteratorReturnsInvalidIterator) {
    Iterator start{};
    Iterator end{};
    ptrdiff_t distance = 0;

    EXPECT_EQ(
        iterator_distance(&start, &end, &distance),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorDistance, ForwardIteratorReturnsUnsupportedOperation) {
    Iterator start{};
    Iterator end{};

    IteratorDesc start_desc = make_forward_desc(nullptr);
    IteratorDesc end_desc   = make_forward_desc(nullptr);

    ASSERT_TRUE(iterator_init(&start, &start_desc));
    ASSERT_TRUE(iterator_init(&end, &end_desc));

    ptrdiff_t distance = 0;

    EXPECT_EQ(
        iterator_distance(&start, &end, &distance),
        ITERATOR_ERROR_UNSUPPORTED_OPERATION
    );
}

TEST(IteratorDistance, DifferentTypeSizesAreIncompatible) {
    IteratorTestContext start_context{};
    IteratorTestContext end_context{};

    Iterator start{};
    Iterator end{};

    IteratorDesc start_desc = make_random_access_desc(&start_context);
    IteratorDesc end_desc   = make_random_access_desc(&end_context);

    end_desc.type_size = sizeof(double);

    ASSERT_TRUE(iterator_init(&start, &start_desc));
    ASSERT_TRUE(iterator_init(&end, &end_desc));

    ptrdiff_t distance = 0;

    EXPECT_EQ(
        iterator_distance(&start, &end, &distance),
        ITERATOR_ERROR_INCOMPATIBLE_ITERATORS
    );

    EXPECT_EQ(start_context.distance_calls, 0U);
}

TEST(IteratorDistance, DifferentImplementationsAreIncompatible) {
    IteratorTestContext start_context{};
    IteratorTestContext end_context{};

    Iterator start{};
    Iterator end{};

    IteratorDesc start_desc = make_random_access_desc(&start_context);
    IteratorDesc end_desc   = make_random_access_desc(&end_context);

    end_desc.iface.distance = other_distance;

    ASSERT_TRUE(iterator_init(&start, &start_desc));
    ASSERT_TRUE(iterator_init(&end, &end_desc));

    ptrdiff_t distance = 0;

    EXPECT_EQ(
        iterator_distance(&start, &end, &distance),
        ITERATOR_ERROR_INCOMPATIBLE_ITERATORS
    );
}

TEST(IteratorDistance, ForwardsOperationToCallback) {
    IteratorTestContext start_context{};
    IteratorTestContext end_context{};

    start_context.distance = 4;
    end_context.distance   = 11;

    Iterator start{};
    Iterator end{};

    IteratorDesc start_desc = make_random_access_desc(&start_context);
    IteratorDesc end_desc   = make_random_access_desc(&end_context);

    ASSERT_TRUE(iterator_init(&start, &start_desc));
    ASSERT_TRUE(iterator_init(&end, &end_desc));

    ptrdiff_t distance = 0;

    EXPECT_EQ(
        iterator_distance(&start, &end, &distance),
        ITERATOR_SUCCESS
    );

    EXPECT_EQ(distance, 7);
    EXPECT_EQ(start_context.distance_calls, 1U);
}

TEST(IteratorDistance, PropagatesCallbackResult) {
    IteratorTestContext start_context{};
    IteratorTestContext end_context{};

    start_context.distance_result = ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    Iterator start{};
    Iterator end{};

    IteratorDesc start_desc = make_random_access_desc(&start_context);
    IteratorDesc end_desc   = make_random_access_desc(&end_context);

    ASSERT_TRUE(iterator_init(&start, &start_desc));
    ASSERT_TRUE(iterator_init(&end, &end_desc));

    ptrdiff_t distance = 0;

    EXPECT_EQ(
        iterator_distance(&start, &end, &distance),
        ITERATOR_ERROR_INCOMPATIBLE_ITERATORS
    );
}

/* -------------------------------------------------------------------------- */
/* Get                                                                        */
/* -------------------------------------------------------------------------- */

TEST(IteratorGet, NullIteratorReturnsInvalidIterator) {
    const void* value = nullptr;

    EXPECT_EQ(
        iterator_get(nullptr, &value),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorGet, NullOutputReturnsInvalidArguments) {
    IteratorTestContext context{};
    Iterator iterator{};

    IteratorDesc desc = make_forward_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(
        iterator_get(&iterator, nullptr),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );
}

TEST(IteratorGet, UninitializedIteratorReturnsInvalidIterator) {
    Iterator iterator{};
    const void* value = nullptr;

    EXPECT_EQ(
        iterator_get(&iterator, &value),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorGet, ForwardsOperationToCallback) {
    int expected = 42;

    IteratorTestContext context{};
    context.value = &expected;

    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    const void* value = nullptr;

    EXPECT_EQ(iterator_get(&iterator, &value), ITERATOR_SUCCESS);

    EXPECT_EQ(value, &expected);
    EXPECT_EQ(context.get_calls, 1U);
}

TEST(IteratorGet, PropagatesCallbackResult) {
    IteratorTestContext context{};
    context.get_result = ITERATOR_ERROR_OUT_OF_RANGE;

    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    const void* value = nullptr;

    EXPECT_EQ(
        iterator_get(&iterator, &value),
        ITERATOR_ERROR_OUT_OF_RANGE
    );
}

/* -------------------------------------------------------------------------- */
/* Equals                                                                     */
/* -------------------------------------------------------------------------- */

TEST(IteratorEquals, NullFirstIteratorReturnsInvalidArguments) {
    Iterator iterator{};
    bool equal = false;

    EXPECT_EQ(
        iterator_equals(nullptr, &iterator, &equal),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );
}

TEST(IteratorEquals, NullSecondIteratorReturnsInvalidArguments) {
    Iterator iterator{};
    bool equal = false;

    EXPECT_EQ(
        iterator_equals(&iterator, nullptr, &equal),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );
}

TEST(IteratorEquals, NullOutputReturnsInvalidArguments) {
    Iterator iterator{};

    EXPECT_EQ(
        iterator_equals(&iterator, &iterator, nullptr),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );
}

TEST(IteratorEquals, UninitializedIteratorReturnsInvalidIterator) {
    Iterator a{};
    Iterator b{};

    bool equal = false;

    EXPECT_EQ(
        iterator_equals(&a, &b, &equal),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorEquals, DifferentTypeSizesAreIncompatible) {
    IteratorTestContext a_context{};
    IteratorTestContext b_context{};

    Iterator a{};
    Iterator b{};

    IteratorDesc a_desc = make_forward_desc(&a_context);
    IteratorDesc b_desc = make_forward_desc(&b_context);

    b_desc.type_size = sizeof(double);

    ASSERT_TRUE(iterator_init(&a, &a_desc));
    ASSERT_TRUE(iterator_init(&b, &b_desc));

    bool equal = false;

    EXPECT_EQ(
        iterator_equals(&a, &b, &equal),
        ITERATOR_ERROR_INCOMPATIBLE_ITERATORS
    );

    EXPECT_EQ(a_context.equals_calls, 0U);
}

TEST(IteratorEquals, DifferentImplementationsAreIncompatible) {
    IteratorTestContext a_context{};
    IteratorTestContext b_context{};

    Iterator a{};
    Iterator b{};

    IteratorDesc a_desc = make_forward_desc(&a_context);
    IteratorDesc b_desc = make_forward_desc(&b_context);

    b_desc.iface.equals = other_equals;

    ASSERT_TRUE(iterator_init(&a, &a_desc));
    ASSERT_TRUE(iterator_init(&b, &b_desc));

    bool equal = false;

    EXPECT_EQ(
        iterator_equals(&a, &b, &equal),
        ITERATOR_ERROR_INCOMPATIBLE_ITERATORS
    );
}

TEST(IteratorEquals, ForwardsOperationToCallback) {
    IteratorTestContext a_context{};
    IteratorTestContext b_context{};

    a_context.equals = true;
    b_context.equals = true;

    Iterator a{};
    Iterator b{};

    IteratorDesc a_desc = make_forward_desc(&a_context);
    IteratorDesc b_desc = make_forward_desc(&b_context);

    ASSERT_TRUE(iterator_init(&a, &a_desc));
    ASSERT_TRUE(iterator_init(&b, &b_desc));

    bool equal = false;

    EXPECT_EQ(
        iterator_equals(&a, &b, &equal),
        ITERATOR_SUCCESS
    );

    EXPECT_TRUE(equal);
    EXPECT_EQ(a_context.equals_calls, 1U);
}

TEST(IteratorEquals, PropagatesCallbackResult) {
    IteratorTestContext a_context{};
    IteratorTestContext b_context{};

    a_context.equals_result = ITERATOR_ERROR_INCOMPATIBLE_ITERATORS;

    Iterator a{};
    Iterator b{};

    IteratorDesc a_desc = make_forward_desc(&a_context);
    IteratorDesc b_desc = make_forward_desc(&b_context);

    ASSERT_TRUE(iterator_init(&a, &a_desc));
    ASSERT_TRUE(iterator_init(&b, &b_desc));

    bool equal = false;

    EXPECT_EQ(
        iterator_equals(&a, &b, &equal),
        ITERATOR_ERROR_INCOMPATIBLE_ITERATORS
    );
}

/* -------------------------------------------------------------------------- */
/* Clone                                                                      */
/* -------------------------------------------------------------------------- */

TEST(IteratorClone, NullOutputReturnsInvalidArguments) {
    IteratorTestContext context{};
    Iterator iterator{};
    IteratorDesc desc = make_cloneable_random_access_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    EXPECT_EQ(
        iterator_clone(&iterator, nullptr, &allocator),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );
}

TEST(IteratorClone, SelfAliasReturnsInvalidArguments) {
    IteratorTestContext context{};
    Iterator iterator{};
    IteratorDesc desc = make_cloneable_random_access_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    EXPECT_EQ(
        iterator_clone(&iterator, &iterator, &allocator),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );

    EXPECT_TRUE(iterator.initialized);
    EXPECT_EQ(iterator.context, &context);
}

TEST(IteratorClone, NullSourceReturnsInvalidIteratorAndResetsOutput) {
    Iterator out{
        .initialized = true,
    };

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    EXPECT_EQ(
        iterator_clone(nullptr, &out, &allocator),
        ITERATOR_ERROR_INVALID_ITERATOR
    );

    EXPECT_FALSE(out.initialized);
}

TEST(IteratorClone, UninitializedSourceReturnsInvalidIterator) {
    Iterator source{};
    Iterator out{};

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    EXPECT_EQ(
        iterator_clone(&source, &out, &allocator),
        ITERATOR_ERROR_INVALID_ITERATOR
    );

    EXPECT_FALSE(out.initialized);
}

TEST(IteratorClone, NullAllocatorReturnsInvalidArguments) {
    IteratorTestContext context{};
    Iterator source{};
    Iterator out{};

    IteratorDesc desc = make_cloneable_random_access_desc(&context);
    ASSERT_TRUE(iterator_init(&source, &desc));

    EXPECT_EQ(
        iterator_clone(&source, &out, nullptr),
        ITERATOR_ERROR_INVALID_ARGUMENTS
    );

    EXPECT_FALSE(out.initialized);
}

TEST(IteratorClone, UnsupportedIteratorReturnsUnsupportedOperation) {
    IteratorTestContext context{};
    Iterator source{};
    Iterator out{};

    IteratorDesc desc = make_random_access_desc(&context);
    ASSERT_TRUE(iterator_init(&source, &desc));

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    EXPECT_EQ(
        iterator_clone(&source, &out, &allocator),
        ITERATOR_ERROR_UNSUPPORTED_OPERATION
    );

    EXPECT_FALSE(out.initialized);
}

TEST(IteratorClone, PropagatesCallbackFailureAndLeavesOutputInvalid) {
    IteratorTestContext context{};
    context.clone_result = ITERATOR_ERROR_OUT_OF_RANGE;

    Iterator source{};
    Iterator out{};

    IteratorDesc desc = make_cloneable_random_access_desc(&context);
    ASSERT_TRUE(iterator_init(&source, &desc));

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    EXPECT_EQ(
        iterator_clone(&source, &out, &allocator),
        ITERATOR_ERROR_OUT_OF_RANGE
    );

    EXPECT_FALSE(out.initialized);
    EXPECT_EQ(allocator_context.alloc_calls, 0U);
}

TEST(IteratorClone, CreatesIndependentOwnedIterator) {
    IteratorTestContext context{};
    context.position = 7;

    Iterator source{};
    Iterator clone{};

    IteratorDesc desc = make_cloneable_random_access_desc(&context);
    ASSERT_TRUE(iterator_init(&source, &desc));

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    ASSERT_EQ(
        iterator_clone(&source, &clone, &allocator),
        ITERATOR_SUCCESS
    );

    EXPECT_TRUE(clone.initialized);
    EXPECT_TRUE(clone.owns_context);
    EXPECT_EQ(clone.type_size, source.type_size);
    EXPECT_EQ(clone.category, source.category);
    EXPECT_NE(clone.context, source.context);
    EXPECT_EQ(clone.cloning_allocator, &allocator);
    EXPECT_EQ(allocator_context.alloc_calls, 1U);

    IteratorTestContext* cloned_context =
        static_cast<IteratorTestContext*>(clone.context);

    EXPECT_EQ(cloned_context->position, context.position);

    ASSERT_EQ(iterator_next(&clone), ITERATOR_SUCCESS);

    EXPECT_EQ(cloned_context->position, 8U);
    EXPECT_EQ(context.position, 7U);

    ASSERT_EQ(iterator_destroy(&clone), ITERATOR_SUCCESS);
    EXPECT_EQ(allocator_context.free_calls, 1U);
}

/* -------------------------------------------------------------------------- */
/* Destroy                                                                    */
/* -------------------------------------------------------------------------- */

TEST(IteratorDestroy, NullIteratorReturnsInvalidIterator) {
    EXPECT_EQ(
        iterator_destroy(nullptr),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorDestroy, UninitializedIteratorReturnsInvalidIterator) {
    Iterator iterator{};

    EXPECT_EQ(
        iterator_destroy(&iterator),
        ITERATOR_ERROR_INVALID_ITERATOR
    );
}

TEST(IteratorDestroy, BorrowedIteratorInvalidatesWrapperOnly) {
    IteratorTestContext context{};
    context.position = 3;

    Iterator iterator{};
    IteratorDesc desc = make_forward_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_EQ(iterator_destroy(&iterator), ITERATOR_SUCCESS);

    EXPECT_FALSE(iterator.initialized);
    EXPECT_FALSE(iterator.owns_context);
    EXPECT_EQ(iterator.context, nullptr);
    EXPECT_EQ(context.position, 3U);
}

TEST(IteratorDestroy, OwnedIteratorReleasesContextAndInvalidatesWrapper) {
    IteratorTestContext context{};

    Iterator source{};
    Iterator clone{};

    IteratorDesc desc = make_cloneable_random_access_desc(&context);
    ASSERT_TRUE(iterator_init(&source, &desc));

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    ASSERT_EQ(iterator_clone(&source, &clone, &allocator), ITERATOR_SUCCESS);
    ASSERT_TRUE(clone.initialized);
    ASSERT_TRUE(clone.owns_context);

    EXPECT_EQ(iterator_destroy(&clone), ITERATOR_SUCCESS);

    EXPECT_FALSE(clone.initialized);
    EXPECT_FALSE(clone.owns_context);
    EXPECT_EQ(clone.context, nullptr);
    EXPECT_EQ(clone.cloning_allocator, nullptr);
    EXPECT_EQ(allocator_context.free_calls, 1U);
}

TEST(IteratorDestroy, CallbackFailurePreservesOwnedIteratorForRetry) {
    IteratorTestContext context{};
    context.destroy_result = ITERATOR_ERROR_OUT_OF_RANGE;

    Iterator source{};
    Iterator clone{};

    IteratorDesc desc = make_cloneable_random_access_desc(&context);
    ASSERT_TRUE(iterator_init(&source, &desc));

    TestAllocatorContext allocator_context{};
    Allocator allocator = make_test_allocator(&allocator_context);
    ASSERT_TRUE(allocator.initialized);

    ASSERT_EQ(iterator_clone(&source, &clone, &allocator), ITERATOR_SUCCESS);

    void* cloned_context = clone.context;

    EXPECT_EQ(
        iterator_destroy(&clone),
        ITERATOR_ERROR_OUT_OF_RANGE
    );

    EXPECT_TRUE(clone.initialized);
    EXPECT_TRUE(clone.owns_context);
    EXPECT_EQ(clone.context, cloned_context);
    EXPECT_EQ(clone.cloning_allocator, &allocator);
    EXPECT_EQ(allocator_context.free_calls, 0U);

    static_cast<IteratorTestContext*>(clone.context)->destroy_result =
        ITERATOR_SUCCESS;

    EXPECT_EQ(iterator_destroy(&clone), ITERATOR_SUCCESS);
    EXPECT_FALSE(clone.initialized);
    EXPECT_EQ(allocator_context.free_calls, 1U);
}

/* -------------------------------------------------------------------------- */
/* Category strings                                                           */
/* -------------------------------------------------------------------------- */

TEST(IteratorCategoryString, Invalid) {
    EXPECT_STREQ(
        iterator_category_to_string(ITERATOR_CATEGORY_INVALID),
        "invalid"
    );
}

TEST(IteratorCategoryString, Forward) {
    EXPECT_STREQ(
        iterator_category_to_string(ITERATOR_CATEGORY_FORWARD),
        "forward"
    );
}

TEST(IteratorCategoryString, Bidirectional) {
    EXPECT_STREQ(
        iterator_category_to_string(ITERATOR_CATEGORY_BIDIRECTIONAL),
        "bidirectional"
    );
}

TEST(IteratorCategoryString, RandomAccess) {
    EXPECT_STREQ(
        iterator_category_to_string(ITERATOR_CATEGORY_RANDOM_ACCESS),
        "random access"
    );
}

TEST(IteratorCategoryString, InvalidValueReturnsNull) {
    EXPECT_EQ(
        iterator_category_to_string(static_cast<IteratorCategory>(999)),
        nullptr
    );
}

/* -------------------------------------------------------------------------- */
/* Result strings                                                             */
/* -------------------------------------------------------------------------- */

TEST(IteratorResultString, Success) {
    EXPECT_STREQ(
        iterator_result_to_string(ITERATOR_SUCCESS),
        "no error"
    );
}

TEST(IteratorResultString, InvalidIterator) {
    EXPECT_STREQ(
        iterator_result_to_string(ITERATOR_ERROR_INVALID_ITERATOR),
        "null or invalid iterator"
    );
}

TEST(IteratorResultString, InvalidArguments) {
    EXPECT_STREQ(
        iterator_result_to_string(ITERATOR_ERROR_INVALID_ARGUMENTS),
        "invalid call argument"
    );
}

TEST(IteratorResultString, UnsupportedOperation) {
    EXPECT_STREQ(
        iterator_result_to_string(ITERATOR_ERROR_UNSUPPORTED_OPERATION),
        "unsupported operation"
    );
}

TEST(IteratorResultString, OutOfRange) {
    EXPECT_STREQ(
        iterator_result_to_string(ITERATOR_ERROR_OUT_OF_RANGE),
        "out of range access"
    );
}

TEST(IteratorResultString, IncompatibleIterators) {
    EXPECT_STREQ(
        iterator_result_to_string(ITERATOR_ERROR_INCOMPATIBLE_ITERATORS),
        "incompatible iterators"
    );
}

TEST(IteratorResultString, InvalidValueReturnsNull) {
    EXPECT_EQ(
        iterator_result_to_string(static_cast<IteratorResult>(999)),
        nullptr
    );
}
