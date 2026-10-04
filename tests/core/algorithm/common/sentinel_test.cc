/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

#include <gtest/gtest.h>

#include <iterator/iterator.h>
#include <algorithm/common/sentinel.h>

/* -------------------------------------------------------------------------- */
/* Test helpers                                                               */
/* -------------------------------------------------------------------------- */

struct SentinelTestContext {
    bool comparison_result;
    SentinelResult result;
    size_t calls;
};

struct IteratorTestContext {
    size_t position;
};

static IteratorResult iterator_test_next(void* context) {
    if(!context)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    IteratorTestContext* iterator_context =
        static_cast<IteratorTestContext*>(context);

    iterator_context->position++;

    return ITERATOR_SUCCESS;
}

static IteratorResult iterator_test_get(const void* context, const void** out) {
    if(!context || !out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    *out = context;

    return ITERATOR_SUCCESS;
}

static IteratorResult iterator_test_equals(
    const void* a,
    const void* b,
    bool* out
) {
    if(!a || !b || !out)
        return ITERATOR_ERROR_INVALID_ARGUMENTS;

    const IteratorTestContext* lhs =
        static_cast<const IteratorTestContext*>(a);

    const IteratorTestContext* rhs =
        static_cast<const IteratorTestContext*>(b);

    *out = lhs->position == rhs->position;

    return ITERATOR_SUCCESS;
}

static Iterator make_test_iterator(
    IteratorTestContext* context,
    size_t type_size
) {
    Iterator iterator{};

    IteratorDesc desc{
        .context   = context,
        .type_size = type_size,
        .iface =
            IteratorInterface{
                .next     = iterator_test_next,
                .previous = nullptr,
                .advance  = nullptr,
                .distance = nullptr,
                .get      = iterator_test_get,
                .equals   = iterator_test_equals,
                .clone    = nullptr,
                .destroy  = nullptr,
            },
    };

    EXPECT_TRUE(iterator_init(&iterator, &desc));

    return iterator;
}

static SentinelResult sentinel_test_equals(
    const void* sentinel_context,
    const void* iterator_context,
    bool* out
) {
    if(!out)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    SentinelTestContext* context =
        const_cast<SentinelTestContext*>(
            static_cast<const SentinelTestContext*>(sentinel_context)
        );

    context->calls++;

    if(context->result != SENTINEL_SUCCESS)
        return context->result;

    if(!iterator_context)
        return SENTINEL_ERROR_INVALID_ARGUMENTS;

    *out = context->comparison_result;

    return SENTINEL_SUCCESS;
}

static Sentinel make_test_sentinel(
    SentinelTestContext* context,
    size_t type_size
) {
    Sentinel sentinel{};

    SentinelDesc desc{
        .context   = context,
        .type_size = type_size,
        .iface =
            SentinelInterface{
                .equals = sentinel_test_equals,
            },
    };

    EXPECT_EQ(
        sentinel_init(&sentinel, &desc),
        SENTINEL_SUCCESS
    );

    return sentinel;
}

/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

TEST(SentinelInit, NullOutputReturnsInvalidArguments) {
    SentinelDesc desc{
        .context   = nullptr,
        .type_size = sizeof(int),
        .iface =
            SentinelInterface{
                .equals = sentinel_test_equals,
            },
    };

    EXPECT_EQ(
        sentinel_init(nullptr, &desc),
        SENTINEL_ERROR_INVALID_ARGUMENTS
    );
}

TEST(SentinelInit, NullDescriptorReturnsInvalidArguments) {
    Sentinel sentinel{
        .initialized = true,
    };

    EXPECT_EQ(
        sentinel_init(&sentinel, nullptr),
        SENTINEL_ERROR_INVALID_ARGUMENTS
    );
}

TEST(SentinelInit, ZeroTypeSizeFails) {
    Sentinel sentinel{
        .initialized = true,
    };

    SentinelDesc desc{
        .context   = nullptr,
        .type_size = 0,
        .iface =
            SentinelInterface{
                .equals = sentinel_test_equals,
            },
    };

    EXPECT_EQ(
        sentinel_init(&sentinel, &desc),
        SENTINEL_ERROR_INVALID_ARGUMENTS
    );

    EXPECT_FALSE(sentinel.initialized);
    EXPECT_EQ(sentinel.type_size, 0U);
    EXPECT_EQ(sentinel.context, nullptr);
    EXPECT_EQ(sentinel.iface.equals, nullptr);
}

TEST(SentinelInit, MissingEqualsFails) {
    Sentinel sentinel{
        .initialized = true,
    };

    SentinelDesc desc{
        .context   = nullptr,
        .type_size = sizeof(int),
        .iface =
            SentinelInterface{
                .equals = nullptr,
            },
    };

    EXPECT_EQ(
        sentinel_init(&sentinel, &desc),
        SENTINEL_ERROR_INVALID_ARGUMENTS
    );

    EXPECT_FALSE(sentinel.initialized);
}

TEST(SentinelInit, ValidDescriptorInitializesSentinel) {
    SentinelTestContext context{};

    Sentinel sentinel{};

    SentinelDesc desc{
        .context   = &context,
        .type_size = sizeof(int),
        .iface =
            SentinelInterface{
                .equals = sentinel_test_equals,
            },
    };

    EXPECT_EQ(
        sentinel_init(&sentinel, &desc),
        SENTINEL_SUCCESS
    );

    EXPECT_TRUE(sentinel.initialized);
    EXPECT_EQ(sentinel.type_size, sizeof(int));
    EXPECT_EQ(sentinel.context, &context);
    EXPECT_EQ(sentinel.iface.equals, sentinel_test_equals);
}

TEST(SentinelInit, NullContextIsAllowed) {
    Sentinel sentinel{};

    SentinelDesc desc{
        .context   = nullptr,
        .type_size = sizeof(int),
        .iface =
            SentinelInterface{
                .equals = sentinel_test_equals,
            },
    };

    EXPECT_EQ(
        sentinel_init(&sentinel, &desc),
        SENTINEL_SUCCESS
    );

    EXPECT_TRUE(sentinel.initialized);
    EXPECT_EQ(sentinel.context, nullptr);
}

TEST(SentinelInit, FailureResetsOutput) {
    Sentinel sentinel{
        .initialized = true,
        .type_size   = sizeof(double),
        .context     = reinterpret_cast<void*>(0x1),
        .iface =
            SentinelInterface{
                .equals = sentinel_test_equals,
            },
    };

    SentinelDesc desc{
        .context   = nullptr,
        .type_size = 0,
        .iface =
            SentinelInterface{
                .equals = sentinel_test_equals,
            },
    };

    EXPECT_EQ(
        sentinel_init(&sentinel, &desc),
        SENTINEL_ERROR_INVALID_ARGUMENTS
    );

    EXPECT_FALSE(sentinel.initialized);
    EXPECT_EQ(sentinel.type_size, 0U);
    EXPECT_EQ(sentinel.context, nullptr);
    EXPECT_EQ(sentinel.iface.equals, nullptr);
}

/* -------------------------------------------------------------------------- */
/* Equality                                                                   */
/* -------------------------------------------------------------------------- */

TEST(SentinelEquals, NullSentinelReturnsInvalidSentinel) {
    IteratorTestContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(int));

    bool equal = false;

    EXPECT_EQ(
        sentinel_equals(nullptr, &iterator, &equal),
        SENTINEL_ERROR_INVALID_SENTINEL
    );
}

TEST(SentinelEquals, UninitializedSentinelReturnsInvalidSentinel) {
    Sentinel sentinel{};

    IteratorTestContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(int));

    bool equal = false;

    EXPECT_EQ(
        sentinel_equals(&sentinel, &iterator, &equal),
        SENTINEL_ERROR_INVALID_SENTINEL
    );
}

TEST(SentinelEquals, NullIteratorReturnsInvalidArguments) {
    SentinelTestContext sentinel_context{};
    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(int));

    bool equal = false;

    EXPECT_EQ(
        sentinel_equals(&sentinel, nullptr, &equal),
        SENTINEL_ERROR_INVALID_ARGUMENTS
    );
}

TEST(SentinelEquals, UninitializedIteratorReturnsInvalidArguments) {
    SentinelTestContext sentinel_context{};
    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(int));

    Iterator iterator{};
    bool equal = false;

    EXPECT_EQ(
        sentinel_equals(&sentinel, &iterator, &equal),
        SENTINEL_ERROR_INVALID_ARGUMENTS
    );
}

TEST(SentinelEquals, NullOutputReturnsInvalidArguments) {
    SentinelTestContext sentinel_context{};
    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(int));

    IteratorTestContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(int));

    EXPECT_EQ(
        sentinel_equals(&sentinel, &iterator, nullptr),
        SENTINEL_ERROR_INVALID_ARGUMENTS
    );
}

TEST(SentinelEquals, TypeSizeMismatchReturnsIncompatibleDomain) {
    SentinelTestContext sentinel_context{};
    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(int));

    IteratorTestContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(double));

    bool equal = false;

    EXPECT_EQ(
        sentinel_equals(&sentinel, &iterator, &equal),
        SENTINEL_ERROR_INCOMPATIBLE_DOMAIN
    );

    EXPECT_EQ(sentinel_context.calls, 0U);
}

TEST(SentinelEquals, CallbackReturnsTrue) {
    SentinelTestContext sentinel_context{
        .comparison_result = true,
        .result            = SENTINEL_SUCCESS,
        .calls             = 0,
    };

    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(int));

    IteratorTestContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(int));

    bool equal = false;

    EXPECT_EQ(
        sentinel_equals(&sentinel, &iterator, &equal),
        SENTINEL_SUCCESS
    );

    EXPECT_TRUE(equal);
    EXPECT_EQ(sentinel_context.calls, 1U);
}

TEST(SentinelEquals, CallbackReturnsFalse) {
    SentinelTestContext sentinel_context{
        .comparison_result = false,
        .result            = SENTINEL_SUCCESS,
        .calls             = 0,
    };

    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(int));

    IteratorTestContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(int));

    bool equal = true;

    EXPECT_EQ(
        sentinel_equals(&sentinel, &iterator, &equal),
        SENTINEL_SUCCESS
    );

    EXPECT_FALSE(equal);
    EXPECT_EQ(sentinel_context.calls, 1U);
}

TEST(SentinelEquals, CallbackErrorIsPropagated) {
    SentinelTestContext sentinel_context{
        .comparison_result = false,
        .result            = SENTINEL_ERROR_INCOMPATIBLE_DOMAIN,
        .calls             = 0,
    };

    Sentinel sentinel =
        make_test_sentinel(&sentinel_context, sizeof(int));

    IteratorTestContext iterator_context{};
    Iterator iterator =
        make_test_iterator(&iterator_context, sizeof(int));

    bool equal = false;

    EXPECT_EQ(
        sentinel_equals(&sentinel, &iterator, &equal),
        SENTINEL_ERROR_INCOMPATIBLE_DOMAIN
    );

    EXPECT_EQ(sentinel_context.calls, 1U);
}

/* -------------------------------------------------------------------------- */
/* Type size                                                                  */
/* -------------------------------------------------------------------------- */

TEST(SentinelTypeSize, NullSentinelReturnsZero) {
    EXPECT_EQ(
        sentinel_get_type_size(nullptr),
        0U
    );
}

TEST(SentinelTypeSize, UninitializedSentinelReturnsZero) {
    Sentinel sentinel{};

    EXPECT_EQ(
        sentinel_get_type_size(&sentinel),
        0U
    );
}

TEST(SentinelTypeSize, ReturnsConfiguredTypeSize) {
    SentinelTestContext context{};
    Sentinel sentinel =
        make_test_sentinel(&context, sizeof(uint64_t));

    EXPECT_EQ(
        sentinel_get_type_size(&sentinel),
        sizeof(uint64_t)
    );
}

/* -------------------------------------------------------------------------- */
/* Result strings                                                             */
/* -------------------------------------------------------------------------- */

TEST(SentinelResultString, Success) {
    EXPECT_STREQ(
        sentinel_result_to_string(SENTINEL_SUCCESS),
        "no error"
    );
}

TEST(SentinelResultString, InvalidSentinel) {
    EXPECT_STREQ(
        sentinel_result_to_string(SENTINEL_ERROR_INVALID_SENTINEL),
        "null or invalid sentinel"
    );
}

TEST(SentinelResultString, InvalidArguments) {
    EXPECT_STREQ(
        sentinel_result_to_string(SENTINEL_ERROR_INVALID_ARGUMENTS),
        "invalid call argument"
    );
}

TEST(SentinelResultString, IncompatibleDomain) {
    EXPECT_STREQ(
        sentinel_result_to_string(SENTINEL_ERROR_INCOMPATIBLE_DOMAIN),
        "incompatible iteration domain"
    );
}

TEST(SentinelResultString, InvalidValueReturnsNull) {
    EXPECT_EQ(
        sentinel_result_to_string(
            static_cast<SentinelResult>(999)
        ),
        nullptr
    );
}