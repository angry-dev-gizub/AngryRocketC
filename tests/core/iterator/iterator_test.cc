/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include <gtest/gtest.h>

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

    ptrdiff_t distance;
    bool equals;

    size_t next_calls;
    size_t previous_calls;
    size_t advance_calls;
    size_t distance_calls;
    size_t get_calls;
    size_t equals_calls;
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

static IteratorResult test_get(const void* context, void** out) {
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

static IteratorDesc make_forward_desc(IteratorTestContext* context) {
    return IteratorDesc{
        .context = context,
        .iface =
            IteratorInterface{
                .next     = test_next,
                .previous = nullptr,
                .advance  = nullptr,
                .distance = nullptr,
                .get      = test_get,
                .equals   = test_equals,
            },
    };
}

static IteratorDesc make_bidirectional_desc(IteratorTestContext* context) {
    return IteratorDesc{
        .context = context,
        .iface =
            IteratorInterface{
                .next     = test_next,
                .previous = test_previous,
                .advance  = nullptr,
                .distance = nullptr,
                .get      = test_get,
                .equals   = test_equals,
            },
    };
}

static IteratorDesc make_random_access_desc(IteratorTestContext* context) {
    return IteratorDesc{
        .context = context,
        .iface =
            IteratorInterface{
                .next     = test_next,
                .previous = test_previous,
                .advance  = test_advance,
                .distance = test_distance,
                .get      = test_get,
                .equals   = test_equals,
            },
    };
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
    EXPECT_EQ(iterator.category, ITERATOR_CATEGORY_FORWARD);
    EXPECT_EQ(iterator.context, &context);
}

TEST(IteratorInit, BidirectionalIteratorSucceeds) {
    IteratorTestContext context{};
    Iterator iterator{};

    IteratorDesc desc = make_bidirectional_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(iterator.initialized);
    EXPECT_EQ(iterator.category, ITERATOR_CATEGORY_BIDIRECTIONAL);
    EXPECT_EQ(iterator.context, &context);
}

TEST(IteratorInit, RandomAccessIteratorSucceeds) {
    IteratorTestContext context{};
    Iterator iterator{};

    IteratorDesc desc = make_random_access_desc(&context);

    ASSERT_TRUE(iterator_init(&iterator, &desc));

    EXPECT_TRUE(iterator.initialized);
    EXPECT_EQ(iterator.category, ITERATOR_CATEGORY_RANDOM_ACCESS);
    EXPECT_EQ(iterator.context, &context);
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
    void* value = nullptr;

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
    void* value = nullptr;

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

    void* value = nullptr;

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

    void* value = nullptr;

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