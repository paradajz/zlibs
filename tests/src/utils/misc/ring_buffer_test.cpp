/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/misc/ring_buffer.h"

using namespace ::testing;
using namespace zlibs::utils;

namespace
{
    LOG_MODULE_REGISTER(ring_buffer_test, CONFIG_ZLIBS_LOG_LEVEL);
}    // namespace

class RingBufferTest : public Test
{
    public:
    RingBufferTest()  = default;
    ~RingBufferTest() = default;

    protected:
    void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
    }

    void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
    }
};

TEST_F(RingBufferTest, EmptyBufferReportsNoData)
{
    misc::RingBuffer<8> buffer = {};

    ASSERT_TRUE(buffer.is_empty());
    ASSERT_FALSE(buffer.is_full());
    ASSERT_EQ(0, buffer.size());
    ASSERT_FALSE(buffer.peek().has_value());
    ASSERT_FALSE(buffer.remove().has_value());
}

TEST_F(RingBufferTest, BufferRejectsWriteWhenFull)
{
    misc::RingBuffer<8> buffer = {};

    for (uint8_t value = 0; value < 8; value++)
    {
        ASSERT_TRUE(buffer.insert(value));
    }

    ASSERT_TRUE(buffer.is_full());
    ASSERT_EQ(8, buffer.size());
    ASSERT_FALSE(buffer.insert(8));

    for (uint8_t value = 0; value < 8; value++)
    {
        ASSERT_EQ(value, buffer.peek().value());
        ASSERT_EQ(value, buffer.remove().value());
    }

    ASSERT_TRUE(buffer.is_empty());
}

TEST_F(RingBufferTest, OverwriteModeReplacesOldestEntryWhenFull)
{
    misc::RingBuffer<8, true> buffer = {};

    for (uint8_t value = 0; value < 8; value++)
    {
        ASSERT_TRUE(buffer.insert(value));
    }

    ASSERT_TRUE(buffer.is_full());
    ASSERT_EQ(8, buffer.size());
    ASSERT_TRUE(buffer.insert(8));
    ASSERT_TRUE(buffer.is_full());
    ASSERT_EQ(8, buffer.size());
    ASSERT_EQ(1, buffer.peek().value());

    for (uint8_t value = 1; value <= 8; value++)
    {
        ASSERT_EQ(value, buffer.peek().value());
        ASSERT_EQ(value, buffer.remove().value());
    }

    ASSERT_TRUE(buffer.is_empty());
}

TEST_F(RingBufferTest, BufferPreservesOrderAcrossWraparound)
{
    misc::RingBuffer<8> buffer = {};

    for (uint8_t value = 1; value <= 8; value++)
    {
        ASSERT_TRUE(buffer.insert(value));
    }

    for (uint8_t value = 1; value <= 3; value++)
    {
        ASSERT_EQ(value, buffer.remove().value());
    }

    for (uint8_t value = 9; value <= 11; value++)
    {
        ASSERT_TRUE(buffer.insert(value));
    }

    for (uint8_t value = 4; value <= 11; value++)
    {
        ASSERT_EQ(value, buffer.peek().value());
        ASSERT_EQ(value, buffer.remove().value());
    }

    ASSERT_TRUE(buffer.is_empty());
}

TEST_F(RingBufferTest, OverwriteModePreservesNewestValuesAcrossWraparound)
{
    misc::RingBuffer<8, true> buffer = {};

    for (uint8_t value = 1; value <= 8; value++)
    {
        ASSERT_TRUE(buffer.insert(value));
    }

    for (uint8_t value = 1; value <= 3; value++)
    {
        ASSERT_EQ(value, buffer.remove().value());
    }

    for (uint8_t value = 9; value <= 15; value++)
    {
        ASSERT_TRUE(buffer.insert(value));
    }

    ASSERT_TRUE(buffer.is_full());
    ASSERT_EQ(8, buffer.size());

    for (uint8_t value = 8; value <= 15; value++)
    {
        ASSERT_EQ(value, buffer.peek().value());
        ASSERT_EQ(value, buffer.remove().value());
    }

    ASSERT_TRUE(buffer.is_empty());
}

TEST_F(RingBufferTest, ResetClearsBufferedState)
{
    misc::RingBuffer<8> buffer = {};

    ASSERT_TRUE(buffer.insert(10));
    ASSERT_TRUE(buffer.insert(11));
    ASSERT_TRUE(buffer.insert(12));
    ASSERT_EQ(10, buffer.remove().value());

    buffer.reset();

    ASSERT_TRUE(buffer.is_empty());
    ASSERT_EQ(0, buffer.size());
    ASSERT_FALSE(buffer.peek().has_value());

    ASSERT_TRUE(buffer.insert(99));
    ASSERT_EQ(99, buffer.remove().value());
}

TEST_F(RingBufferTest, LargeBuffersTrackSizeWithoutIndexTruncation)
{
    static misc::RingBuffer<131072> buffer = {};

    buffer.reset();

    for (size_t value = 0; value < 65537; value++)
    {
        ASSERT_TRUE(buffer.insert(static_cast<uint8_t>(value)));
    }

    ASSERT_EQ(65537, buffer.size());
    ASSERT_EQ(0, buffer.peek().value());
    ASSERT_EQ(0, buffer.remove().value());
    ASSERT_EQ(65536, buffer.size());
}
