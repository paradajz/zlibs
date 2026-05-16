/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/midi/midi1.h"
#include "zlibs/utils/midi/transport/null/transport_null.h"

using namespace ::testing;
using namespace zlibs::utils::midi;

namespace
{
    LOG_MODULE_REGISTER(midi_null_test, CONFIG_ZLIBS_LOG_LEVEL);

    class MidiNullTransportTest : public Test
    {
        protected:
        Null _transport;
    };
}    // namespace

TEST_F(MidiNullTransportTest, InitAndDeinitSucceed)
{
    EXPECT_TRUE(_transport.init());
    EXPECT_TRUE(_transport.deinit());
}

TEST_F(MidiNullTransportTest, SupportedReturnsFalse)
{
    EXPECT_FALSE(_transport.supported());
}

TEST_F(MidiNullTransportTest, WriteFails)
{
    EXPECT_FALSE(_transport.send(midi1::note_on(0, 0, 60, 100)));
}

TEST_F(MidiNullTransportTest, ReadReturnsNoPacket)
{
    EXPECT_FALSE(_transport.read().has_value());
}
