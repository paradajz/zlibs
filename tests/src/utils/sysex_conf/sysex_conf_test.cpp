/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/sysex_conf/sysex_conf.h"

#include <array>

using namespace ::testing;
using namespace zlibs::utils::sysex_conf;

namespace
{
    namespace midi = zlibs::utils::midi;

    LOG_MODULE_REGISTER(sysex_conf_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr std::array<uint8_t, 2> sysex_param(uint16_t value)
    {
        const Split14Bit SPLIT(value);
        return { SPLIT.high(), SPLIT.low() };
    }

    constexpr uint8_t  SYS_EX_CONF_M_ID_0               = 0x00;
    constexpr uint8_t  SYS_EX_CONF_M_ID_1               = 0x53;
    constexpr uint8_t  SYS_EX_CONF_M_ID_2               = 0x43;
    constexpr uint16_t SECTION_0_PARAMETERS             = 10;
    constexpr uint16_t SECTION_1_PARAMETERS             = 6;
    constexpr uint16_t SECTION_2_PARAMETERS             = 33;
    constexpr uint16_t SECTION_0_MIN                    = 0;
    constexpr uint16_t SECTION_0_MAX                    = 50;
    constexpr uint16_t SECTION_1_MIN                    = 0;
    constexpr uint16_t SECTION_1_MAX                    = 0;
    constexpr uint16_t SECTION_2_MIN                    = 0;
    constexpr uint16_t SECTION_2_MAX                    = 0;
    constexpr uint8_t  TEST_BLOCK_ID                    = 0;
    constexpr uint8_t  TEST_SECTION_SINGLE_PART_ID      = 0;
    constexpr uint8_t  TEST_SECTION_MULTIPLE_PARTS_ID   = 2;
    constexpr uint8_t  TEST_SECTION_NOMINMAX            = 1;
    constexpr uint16_t TEST_INDEX_ID                    = 5;
    constexpr uint8_t  TEST_MSG_PART_VALID              = 0;
    constexpr uint8_t  TEST_MSG_PART_INVALID            = 10;
    constexpr uint16_t TEST_INVALID_PARAMETER_B0S0      = 15;
    constexpr uint16_t TEST_NEW_VALUE_VALID             = 25;
    constexpr uint16_t TEST_NEW_VALUE_INVALID           = 100;
    constexpr uint16_t TEST_VALUE_GET                   = 3;
    constexpr uint8_t  CUSTOM_REQUEST_ID_VALID          = 54;
    constexpr uint8_t  CUSTOM_REQUEST_ID_INVALID        = 55;
    constexpr uint8_t  CUSTOM_REQUEST_ID_ERROR_READ     = 56;
    constexpr uint8_t  CUSTOM_REQUEST_ID_NO_CONN_CHECK  = 57;
    constexpr uint16_t CUSTOM_REQUEST_VALUE             = 1;
    constexpr uint16_t CUSTOM_REQUEST_OVERFLOW_ATTEMPTS = MAX_MESSAGE_SIZE;

    class SysExConfTest : public Test
    {
        public:
        SysExConfTest()  = default;
        ~SysExConfTest() = default;

        protected:
        void SetUp() override
        {
            LOG_INF("Running test %s",
                    UnitTest::GetInstance()->current_test_info()->name());
            sys_ex_conf.set_layout(TEST_LAYOUT);
            sys_ex_conf.setup_custom_requests(custom_requests);
        }

        void TearDown() override
        {
            LOG_INF("Test %s complete",
                    UnitTest::GetInstance()->current_test_info()->name());
        }

        const ManufacturerId M_ID = { SYS_EX_CONF_M_ID_0, SYS_EX_CONF_M_ID_1, SYS_EX_CONF_M_ID_2 };

        static constexpr auto TEST_BLOCK = make_block(std::array<Section, 3>{
            Section{
                SECTION_0_PARAMETERS,
                SECTION_0_MIN,
                SECTION_0_MAX,
            },
            Section{
                SECTION_1_PARAMETERS,
                SECTION_1_MIN,
                SECTION_1_MAX,
            },
            Section{
                SECTION_2_PARAMETERS,
                SECTION_2_MIN,
                SECTION_2_MAX,
            },
        });

        static constexpr auto TEST_LAYOUT = make_layout(std::array<Block, 1>{
            Block(TEST_BLOCK),
        });

        std::vector<CustomRequest> custom_requests = {
            {
                .request_id      = CUSTOM_REQUEST_ID_VALID,
                .conn_open_check = true,
            },

            {
                .request_id      = CUSTOM_REQUEST_ID_NO_CONN_CHECK,
                .conn_open_check = false,
            },

            {
                .request_id      = CUSTOM_REQUEST_ID_ERROR_READ,
                .conn_open_check = true,
            }
        };

        class TestDataHandler : public DataHandler
        {
            public:
            TestDataHandler() = default;

            Status get(uint8_t block, uint8_t section, uint16_t index, uint16_t& value) override
            {
                value = TEST_VALUE_GET;

                if (get_results.empty())
                {
                    return Status::Ack;
                }

                Status ret_val = get_results.at(0);
                get_results.erase(get_results.begin());

                return ret_val;
            }

            Status set(uint8_t block, uint8_t section, uint16_t index, uint16_t new_value) override
            {
                if (set_results.empty())
                {
                    return Status::Ack;
                }

                Status ret_val = set_results.at(0);
                set_results.erase(set_results.begin());

                return ret_val;
            }

            Status custom_request(uint16_t request, CustomResponse& custom_response) override
            {
                switch (request)
                {
                case CUSTOM_REQUEST_ID_VALID:
                case CUSTOM_REQUEST_ID_NO_CONN_CHECK:
                {
                    if (force_custom_request_overflow)
                    {
                        for (std::size_t i = 0; i < CUSTOM_REQUEST_OVERFLOW_ATTEMPTS; i++)
                        {
                            custom_append_results.push_back(custom_response.append(CUSTOM_REQUEST_VALUE));
                        }
                    }
                    else
                    {
                        for (std::size_t i = 0; i < _custom_req_array.size(); i++)
                        {
                            custom_append_results.push_back(custom_response.append(_custom_req_array[i]));
                        }
                    }

                    return Status::Ack;
                }
                break;

                case CUSTOM_REQUEST_ID_ERROR_READ:
                default:
                    return Status::ErrorRead;
                }
            }

            void reset()
            {
                _response.clear();
                _response_active.clear();
                _response_ump.clear();
                get_results.clear();
                set_results.clear();
                custom_append_results.clear();
                force_custom_request_overflow = false;
            }

            std::size_t response_counter()
            {
                return _response.size();
            }

            std::vector<uint8_t> response(uint8_t index)
            {
                if (index >= _response.size())
                {
                    return {};
                }

                return _response.at(index);
            }

            std::size_t response_ump_counter()
            {
                return _response_ump.size();
            }

            midi_ump response_ump(uint8_t index)
            {
                if (index >= _response_ump.size())
                {
                    return {};
                }

                return _response_ump.at(index);
            }

            bool send_response(const midi_ump& packet) override
            {
                _response_ump.push_back(packet);

                const auto STATUS = midi::sysex7_status(packet);
                const auto SIZE   = midi::sysex7_payload_size(packet);

                if (midi::sysex7_starts_message(STATUS))
                {
                    _response_active.clear();
                    _response_active.push_back(SYSEX_START_BYTE);
                }

                for (size_t i = 0; i < SIZE; i++)
                {
                    _response_active.push_back(midi::read_sysex7_payload_byte(packet, i));
                }

                if (midi::sysex7_ends_message(STATUS))
                {
                    _response_active.push_back(SYSEX_END_BYTE);
                    _response.push_back(_response_active);
                    _response_active.clear();
                }

                return true;
            }

            std::vector<Status> get_results                   = {};
            std::vector<Status> set_results                   = {};
            std::vector<bool>   custom_append_results         = {};
            bool                force_custom_request_overflow = false;

            private:
            std::vector<std::vector<uint8_t>> _response;
            std::vector<uint8_t>              _response_active;
            std::vector<midi_ump>             _response_ump;
            std::vector<uint8_t>              _custom_req_array = { 1 };
        };

        template<typename T>
        void verify_response(const std::vector<uint8_t>& source, T status, const std::vector<uint8_t>* data = nullptr)
        {
            std::size_t size = source.size();

            if ((data != nullptr) && source.size())
            {
                size--;    // Skip the last byte (`0xF7`); custom data starts here.
            }

            for (std::size_t i = 0; i < size; i++)
            {
                if (i != 4)    // Status byte.
                {
                    ASSERT_EQ(source.at(i), data_handler.response(data_handler.response_counter() - 1).at(i));
                }
                else
                {
                    ASSERT_EQ(static_cast<uint8_t>(status), data_handler.response(data_handler.response_counter() - 1).at(i));
                }
            }

            // Now verify the data.
            if ((data != nullptr) && source.size())
            {
                for (std::size_t i = size; i < (size + data->size()); i++)
                {
                    ASSERT_EQ(data->at(i - size), data_handler.response(data_handler.response_counter() - 1).at(i));
                }

                ASSERT_EQ(0xF7, data_handler.response(data_handler.response_counter() - 1).at(size + data->size()));
            }
        }

        void handle_packet(const std::vector<uint8_t>& source)
        {
            ASSERT_GE(source.size(), 2);
            ASSERT_EQ(SYSEX_START_BYTE, source.front());
            ASSERT_EQ(SYSEX_END_BYTE, source.back());

            const size_t PAYLOAD_END = source.size() - 1;
            size_t       offset      = 1;
            bool         sent_packet = false;

            do
            {
                const size_t REMAINING_PAYLOAD = PAYLOAD_END - offset;
                const size_t CHUNK_SIZE        = REMAINING_PAYLOAD > midi::SYSEX7_DATA64_MAX_PAYLOAD_SIZE ? midi::SYSEX7_DATA64_MAX_PAYLOAD_SIZE : REMAINING_PAYLOAD;
                const bool   FIRST_CHUNK       = (offset == 1);
                const bool   LAST_CHUNK        = ((offset + CHUNK_SIZE) == PAYLOAD_END);

                midi::SysEx7Status status = midi::SysEx7Status::Continue;

                if (FIRST_CHUNK && LAST_CHUNK)
                {
                    status = midi::SysEx7Status::Complete;
                }
                else if (FIRST_CHUNK)
                {
                    status = midi::SysEx7Status::Start;
                }
                else if (LAST_CHUNK)
                {
                    status = midi::SysEx7Status::End;
                }

                midi_ump packet = {};
                packet.data[0]  = midi::sysex7_header(0, status, CHUNK_SIZE);

                for (size_t i = 0; i < CHUNK_SIZE; i++)
                {
                    midi::write_sysex7_payload_byte(packet, i, source.at(offset + i));
                }

                sys_ex_conf.handle_packet(packet);

                offset += CHUNK_SIZE;
                sent_packet = true;
            } while (offset < PAYLOAD_END || !sent_packet);
        }

        void open_conn()
        {
            // Send the open-connection request.
            handle_packet(CONN_OPEN);

            // SysEx configuration should be enabled now.
            ASSERT_TRUE(sys_ex_conf.is_configuration_enabled());

            data_handler.reset();
        }

        const std::vector<uint8_t> CONN_OPEN = {
            // Request used to enable SysEx configuration.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(SpecialRequest::ConnOpen),
            0xF7
        };

        const std::vector<uint8_t> CONN_CLOSE = {
            // Request used to disable SysEx configuration.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(SpecialRequest::ConnClose),
            0xF7
        };

        const std::vector<uint8_t> ERROR_STATUS = {
            // Get-single message with an invalid status byte for a request message.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Ack),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> ERROR_WISH = {
            // Wish byte set to an invalid value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            0x04,
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> ERROR_MESSAGE_LENGTH = {
            // Message intentionally one byte too long.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> ERROR_BLOCK = {
            // Block byte set to an invalid value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            0x41,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> ERROR_SECTION = {
            // Section byte set to an invalid value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            0x61,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> ERROR_INDEX = {
            // Index byte set to an invalid value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(0x7F)[0],
            sysex_param(0x7F)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> ERROR_PART = {
            // Part byte set to an invalid value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_INVALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> ERROR_AMOUNT = {
            // Amount byte set to an invalid value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            0x02,
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> CUSTOM_REQ = {
            // Custom request with a user-defined custom ID.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            CUSTOM_REQUEST_ID_VALID,
            0xF7
        };

        const std::vector<uint8_t> CUSTOM_REQ_ERROR_READ = {
            // Custom request with a user-defined custom ID.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            CUSTOM_REQUEST_ID_ERROR_READ,
            0xF7
        };

        const std::vector<uint8_t> CUSTOM_REQ_INVALID = {
            // Custom request with a non-existent custom ID.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            CUSTOM_REQUEST_ID_INVALID,
            0xF7
        };

        const std::vector<uint8_t> CUSTOM_REQ_NO_CONN_CHECK = {
            // Custom request with a custom ID that ignores the connection check.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            CUSTOM_REQUEST_ID_NO_CONN_CHECK,
            0xF7
        };

        const std::vector<uint8_t> SHORT_MESSAGE1 = {
            // Short message that should be ignored by the protocol, variant 1.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0xF7
        };

        const std::vector<uint8_t> SHORT_MESSAGE2 = {
            // Short message that should be ignored by the protocol, variant 2.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            0xF7
        };

        const std::vector<uint8_t> SHORT_MESSAGE3 = {
            // Short message that should trigger `MESSAGE_LENGTH`.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            0xF7
        };

        const std::vector<uint8_t> GET_SINGLE_VALID = {
            // Valid get-single command.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> GET_SINGLE_PART1 = {
            // Get-single command with part ID set to `1`.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            1,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_MULTIPLE_PARTS_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_SINGLE_PART1 = {
            // Set-single command with part ID set to `1`.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            1,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_MULTIPLE_PARTS_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> GET_SINGLE_INVALID_SYS_EX_ID = {
            // Get-single command with invalid SysEx IDs.
            0xF0,
            SYS_EX_CONF_M_ID_2,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_0,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> GET_ALL_VALID_1PART = {
            // Valid get-all command.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(0)[0],
            sysex_param(0)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> GET_ALL_VALID_ALL_PARTS_7_F = {
            // Valid get-all command for all parts (`0x7F` variant).
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0x7F,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_MULTIPLE_PARTS_ID,
            sysex_param(0)[0],
            sysex_param(0)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> GET_ALL_VALID_ALL_PARTS_7_E = {
            // Valid get-all command for all parts (`0x7E` variant).
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0x7E,
            static_cast<uint8_t>(Wish::Get),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_MULTIPLE_PARTS_ID,
            sysex_param(0)[0],
            sysex_param(0)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> GET_SPECIAL_REQ_BYTES_PER_VAL = {
            // Built-in special request that returns the number of bytes per value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            0x00,
            0x00,
            0x02,
            0xF7
        };

        const std::vector<uint8_t> GET_SPECIAL_REQ_PARAM_PER_MSG = {
            // Built-in special request that returns the number of parameters per message.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            0x00,
            0x00,
            0x03,
            0xF7
        };

        const std::vector<uint8_t> SET_SINGLE_VALID = {
            // Valid set-single command.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(TEST_NEW_VALUE_VALID)[0],
            sysex_param(TEST_NEW_VALUE_VALID)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_SINGLE_INVALID_NEW_VALUE = {
            // Set-single command with an invalid new value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_ALL_VALID = {
            // Valid set-all command.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0x00,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_ALL_ALL_PARTS = {
            // Set-all command with the all-parts modifier (invalid request).
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0x7F,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_SINGLE_NO_MIN_MAX1 = {
            // Valid set-single command for a section without min/max checking, variant 1.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_NOMINMAX,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_SINGLE_NO_MIN_MAX2 = {
            // Valid set-single command for a section without min/max checking, variant 2.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_NOMINMAX,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(50)[0],
            sysex_param(50)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_SINGLE_NO_MIN_MAX3 = {
            // Valid set-single command for a section without min/max checking, variant 3.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_NOMINMAX,
            sysex_param(TEST_INDEX_ID)[0],
            sysex_param(TEST_INDEX_ID)[1],
            sysex_param(127)[0],
            sysex_param(127)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_SINGLE_INVALID_PARAM = {
            // Set-single command with an invalid parameter index.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_INVALID_PARAMETER_B0S0)[0],
            sysex_param(TEST_INVALID_PARAMETER_B0S0)[1],
            sysex_param(0x00)[0],
            sysex_param(0x00)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_ALLNVALID_NEW_VAL = {
            // Set-all command with an invalid new value.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            TEST_MSG_PART_VALID,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            sysex_param(TEST_NEW_VALUE_INVALID)[0],
            sysex_param(TEST_NEW_VALUE_INVALID)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_ALL_MORE_PARTS1 = {
            // Set-all command for a section with more parts, part 0.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0x00,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_MULTIPLE_PARTS_ID,
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x02)[0],
            sysex_param(0x02)[1],
            sysex_param(0x03)[0],
            sysex_param(0x03)[1],
            sysex_param(0x04)[0],
            sysex_param(0x04)[1],
            sysex_param(0x05)[0],
            sysex_param(0x05)[1],
            sysex_param(0x06)[0],
            sysex_param(0x06)[1],
            sysex_param(0x07)[0],
            sysex_param(0x07)[1],
            sysex_param(0x08)[0],
            sysex_param(0x08)[1],
            sysex_param(0x09)[0],
            sysex_param(0x09)[1],
            sysex_param(0x0A)[0],
            sysex_param(0x0A)[1],
            sysex_param(0x0B)[0],
            sysex_param(0x0B)[1],
            sysex_param(0x0C)[0],
            sysex_param(0x0C)[1],
            sysex_param(0x0D)[0],
            sysex_param(0x0D)[1],
            sysex_param(0x0E)[0],
            sysex_param(0x0E)[1],
            sysex_param(0x0F)[0],
            sysex_param(0x0F)[1],
            sysex_param(0x10)[0],
            sysex_param(0x10)[1],
            sysex_param(0x11)[0],
            sysex_param(0x11)[1],
            sysex_param(0x12)[0],
            sysex_param(0x12)[1],
            sysex_param(0x13)[0],
            sysex_param(0x13)[1],
            sysex_param(0x14)[0],
            sysex_param(0x14)[1],
            sysex_param(0x15)[0],
            sysex_param(0x15)[1],
            sysex_param(0x16)[0],
            sysex_param(0x16)[1],
            sysex_param(0x17)[0],
            sysex_param(0x17)[1],
            sysex_param(0x18)[0],
            sysex_param(0x18)[1],
            sysex_param(0x19)[0],
            sysex_param(0x19)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            0xF7
        };

        const std::vector<uint8_t> SET_ALL_MORE_PARTS2 = {
            // Set-all command for a section with more parts, part 1.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0x01,
            static_cast<uint8_t>(Wish::Set),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_MULTIPLE_PARTS_ID,
            sysex_param(0x01)[0],
            sysex_param(0x01)[1],
            0xF7
        };

        const std::vector<uint8_t> BACKUP_ALL = {
            // Backup-all command.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0x7F,
            static_cast<uint8_t>(Wish::Backup),
            static_cast<uint8_t>(Amount::All),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(0)[0],
            sysex_param(0)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        const std::vector<uint8_t> BACKUP_SINGLE_INV_PART = {
            // Backup-single command with an invalid part set.
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0x03,
            static_cast<uint8_t>(Wish::Backup),
            static_cast<uint8_t>(Amount::Single),
            TEST_BLOCK_ID,
            TEST_SECTION_SINGLE_PART_ID,
            sysex_param(0)[0],
            sysex_param(0)[1],
            sysex_param(0)[0],
            sysex_param(0)[1],
            0xF7
        };

        TestDataHandler data_handler;
        SysExConf       sys_ex_conf = SysExConf(data_handler, M_ID);
    };

}    // namespace

TEST_F(SysExConfTest, Init)
{
    open_conn();

    // Close the connection.
    handle_packet(CONN_CLOSE);

    // SysEx configuration should be disabled now.
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Check the response.
    verify_response(CONN_CLOSE, Status::Ack);
}

TEST_F(SysExConfTest, UmpCompleteSysExMessage)
{
    handle_packet(CONN_OPEN);

    ASSERT_TRUE(sys_ex_conf.is_configuration_enabled());
    ASSERT_EQ(1, data_handler.response_counter());
    ASSERT_EQ(1, data_handler.response_ump_counter());

    verify_response(CONN_OPEN, Status::Ack);

    const auto RESPONSE     = data_handler.response(0);
    const auto RESPONSE_UMP = data_handler.response_ump(0);
    const auto UMP_STATUS   = midi::sysex7_status(RESPONSE_UMP);
    const auto UMP_SIZE     = midi::sysex7_payload_size(RESPONSE_UMP);

    ASSERT_EQ(UMP_MT_DATA_64, UMP_MT(RESPONSE_UMP));
    ASSERT_EQ(midi::SysEx7Status::Complete, UMP_STATUS);
    ASSERT_EQ(RESPONSE.size() - 2, UMP_SIZE);

    for (size_t i = 0; i < UMP_SIZE; i++)
    {
        ASSERT_EQ(RESPONSE.at(i + 1), midi::read_sysex7_payload_byte(RESPONSE_UMP, i));
    }
}

TEST_F(SysExConfTest, UmpMultiPacketSysExMessage)
{
    open_conn();

    handle_packet(GET_SINGLE_VALID);

    const std::vector<uint8_t> DATA = {
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
    };

    ASSERT_EQ(1, data_handler.response_counter());
    ASSERT_GT(data_handler.response_ump_counter(), 1);
    verify_response(GET_SINGLE_VALID, Status::Ack, &DATA);
}

TEST_F(SysExConfTest, ErrorConnClosed)
{
    // The configuration should be closed initially.
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // Send a valid get message. `Status::ErrorConnection` should be reported because the connection is closed.
    handle_packet(GET_SINGLE_VALID);

    // Check the response.
    verify_response(GET_SINGLE_VALID, Status::ErrorConnection);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorStatus)
{
    open_conn();

    // Send a message with an invalid status byte. `Status::ErrorStatus` should be reported.
    handle_packet(ERROR_STATUS);

    // Check the response.
    verify_response(ERROR_STATUS, Status::ErrorStatus);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorWish)
{
    open_conn();

    // Send a message with an invalid wish byte.
    handle_packet(ERROR_WISH);

    // Check the response.
    verify_response(ERROR_WISH, Status::ErrorWish);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorAmount)
{
    open_conn();

    // Send a message with an invalid amount byte. `Status::ErrorAmount` should be reported.
    handle_packet(ERROR_AMOUNT);

    // Check the response.
    verify_response(ERROR_AMOUNT, Status::ErrorAmount);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorBlock)
{
    open_conn();

    // Send a message with an invalid block byte. `Status::ErrorBlock` should be reported.
    handle_packet(ERROR_BLOCK);

    // Check the response.
    verify_response(ERROR_BLOCK, Status::ErrorBlock);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorSection)
{
    open_conn();

    // Send a message with an invalid section byte. `Status::ErrorSection` should be reported.
    handle_packet(ERROR_SECTION);

    // Check the response.
    verify_response(ERROR_SECTION, Status::ErrorSection);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorPart)
{
    open_conn();

    // Send a message with an invalid part byte. `Status::ErrorPart` should be reported.
    handle_packet(ERROR_PART);

    // Check the response.
    verify_response(ERROR_PART, Status::ErrorPart);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the number of received messages.
    data_handler.reset();

    /*
     * Send a get-single request for a section that normally contains more
     * parts, but set the message part byte to `1`. `Status::ErrorPart` should
     * be reported because the part must always be `0` when the amount is
     * single.
     */

    handle_packet(GET_SINGLE_PART1);

    // Check the response.
    verify_response(GET_SINGLE_PART1, Status::ErrorPart);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the number of received messages.
    data_handler.reset();

    // The same outcome is expected for a set-single message with part `1`.
    handle_packet(SET_SINGLE_PART1);

    // Check the response.
    verify_response(SET_SINGLE_PART1, Status::ErrorPart);
}

TEST_F(SysExConfTest, ErrorIndex)
{
    open_conn();

    // Send a message with an invalid index byte. `Status::ErrorIndex` should be reported.
    handle_packet(ERROR_INDEX);

    // Check the response.
    verify_response(ERROR_INDEX, Status::ErrorIndex);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorMessageLength)
{
    open_conn();

    // Send a message with an invalid length. `Status::ErrorMessageLength` should be reported.
    handle_packet(ERROR_MESSAGE_LENGTH);

    // Check the response.
    verify_response(ERROR_MESSAGE_LENGTH, Status::ErrorMessageLength);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorNewValue)
{
    open_conn();

    // Send an invalid set message. `Status::ErrorNewValue` should be reported.
    handle_packet(SET_SINGLE_INVALID_NEW_VALUE);

    // Check the response.
    verify_response(SET_SINGLE_INVALID_NEW_VALUE, Status::ErrorNewValue);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorWrite)
{
    open_conn();

    // Configure the set handler to return an error. The status byte should be `Status::ErrorWrite`.

    data_handler.set_results.push_back(Status::ErrorWrite);

    // Send a valid set message.
    handle_packet(SET_SINGLE_VALID);

    // Check the response.
    verify_response(SET_SINGLE_VALID, Status::ErrorWrite);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the number of received messages.
    data_handler.reset();
    data_handler.set_results.push_back(Status::ErrorWrite);

    handle_packet(SET_ALL_VALID);

    // Check the response.
    verify_response(SET_ALL_VALID, Status::ErrorWrite);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorWriteIgnoreUserErrorMode)
{
    open_conn();
    sys_ex_conf.set_user_error_ignore_mode(true);

    /*
     * Configure the set handler to return an error. The status byte should
     * still be `Status::Ack` because ignore mode is enabled.
     */

    data_handler.set_results.push_back(Status::ErrorWrite);

    // Send a valid set message.
    handle_packet(SET_SINGLE_VALID);

    // Check the response.
    verify_response(SET_SINGLE_VALID, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the number of received messages.
    data_handler.reset();
    data_handler.set_results.push_back(Status::ErrorWrite);

    handle_packet(SET_ALL_VALID);

    // Check the response.
    verify_response(SET_ALL_VALID, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorRead)
{
    open_conn();

    // Configure the get handler to return an error. The status byte should be `Status::ErrorRead`.

    data_handler.get_results.push_back(Status::ErrorRead);

    handle_packet(GET_SINGLE_VALID);

    // Check the response.
    verify_response(GET_SINGLE_VALID, Status::ErrorRead);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();
    data_handler.get_results.push_back(Status::ErrorRead);

    // Test a get-all request. `Status::ErrorRead` should be reported again.
    handle_packet(GET_ALL_VALID_1PART);

    // Check the response.
    verify_response(GET_ALL_VALID_1PART, Status::ErrorRead);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorReadIgnoreUserErrorMode)
{
    open_conn();
    sys_ex_conf.set_user_error_ignore_mode(true);

    /*
     * Configure the get handler to return an error. The status byte should
     * still be `Status::Ack` because ignore mode is enabled.
     */

    // The user error should be ignored.
    data_handler.get_results.push_back(Status::ErrorRead);

    handle_packet(GET_SINGLE_VALID);

    // The read value should be set to `0`.
    std::vector<uint8_t> data = {
        sysex_param(0)[0],
        sysex_param(0)[1],
    };

    // Check the response.
    verify_response(GET_SINGLE_VALID, Status::Ack, &data);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();
    data_handler.get_results.push_back(Status::ErrorRead);
    data_handler.get_results.push_back(Status::Ack);
    data_handler.get_results.push_back(Status::ErrorRead);

    // Test a get-all request.
    handle_packet(GET_ALL_VALID_1PART);

    data = {
        sysex_param(0)[0],
        sysex_param(0)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(0)[0],
        sysex_param(0)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
    };

    // Check the response.
    verify_response(GET_ALL_VALID_1PART, Status::Ack, &data);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorCustom)
{
    open_conn();

    // Configure the set handler to return a custom error.
    uint8_t error = 63;
    data_handler.set_results.push_back(static_cast<Status>(error));

    // Send a valid set message.
    handle_packet(SET_SINGLE_VALID);

    // Check the response.
    verify_response(SET_SINGLE_VALID, error);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the number of received messages.
    data_handler.reset();
    data_handler.set_results.push_back(static_cast<Status>(error));

    handle_packet(SET_ALL_VALID);

    // Check the response.
    verify_response(SET_ALL_VALID, error);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, SetSingle)
{
    open_conn();

    // Send a valid set message.
    handle_packet(SET_SINGLE_VALID);

    // Check the response.
    verify_response(SET_SINGLE_VALID, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Send a set-single command with an invalid parameter index.
    handle_packet(SET_SINGLE_INVALID_PARAM);

    // Check the response.
    verify_response(SET_SINGLE_INVALID_PARAM, Status::ErrorIndex);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    /*
     * Test a block whose minimum and maximum values are the same. In this
     * case, `Status::ErrorNewValue` should never be reported for any value.
     */
    handle_packet(SET_SINGLE_NO_MIN_MAX1);

    // Check the response.
    verify_response(SET_SINGLE_NO_MIN_MAX1, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    handle_packet(SET_SINGLE_NO_MIN_MAX2);

    // Check the response.
    verify_response(SET_SINGLE_NO_MIN_MAX2, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    handle_packet(SET_SINGLE_NO_MIN_MAX3);

    // Check the response.
    verify_response(SET_SINGLE_NO_MIN_MAX3, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();
}

TEST_F(SysExConfTest, SetAll)
{
    open_conn();

    // Send a set-all request.
    handle_packet(SET_ALL_VALID);

    // Check the response.
    verify_response(SET_ALL_VALID, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Send a set-all message for a section with more parts.
    handle_packet(SET_ALL_MORE_PARTS1);

    // Check the response.
    verify_response(SET_ALL_MORE_PARTS1, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Send a set-all request with the part byte set to `0x01`.
    handle_packet(SET_ALL_MORE_PARTS2);

    // Check the response.
    verify_response(SET_ALL_MORE_PARTS2, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    /*
     * Send a set-all request for all parts and verify that the status byte is
     * `Status::ErrorPart`.
     */
    handle_packet(SET_ALL_ALL_PARTS);

    // Check the response.
    verify_response(SET_ALL_ALL_PARTS, Status::ErrorPart);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    /*
     * Send a set-all request with an invalid value.
     * The status byte should be `Status::ErrorNewValue`.
     */
    handle_packet(SET_ALLNVALID_NEW_VAL);

    // Check the response.
    verify_response(SET_ALLNVALID_NEW_VAL, Status::ErrorNewValue);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();
}

TEST_F(SysExConfTest, GetSingle)
{
    open_conn();

    // Send a get-single request.
    handle_packet(GET_SINGLE_VALID);

    const std::vector<uint8_t> DATA = {
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
    };

    // Check the response.
    verify_response(GET_SINGLE_VALID, Status::Ack, &DATA);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, GetAll)
{
    open_conn();

    // Send a get-all request.
    handle_packet(GET_ALL_VALID_1PART);

    const std::vector<uint8_t> DATA = {
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
    };

    // Check the response.
    verify_response(GET_ALL_VALID_1PART, Status::Ack, &DATA);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    /*
     * Send the same request for all parts.
     * Two messages are expected.
     */
    handle_packet(GET_ALL_VALID_ALL_PARTS_7_F);

    // Check the number of received messages.
    ASSERT_EQ(2, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    /*
     * Send the same message with part `0x7E`.
     * In this case, the last message should be a `Status::Ack` message.
     */
    handle_packet(GET_ALL_VALID_ALL_PARTS_7_E);

    // Check the last response.
    ASSERT_EQ(0xF0, data_handler.response(data_handler.response_counter() - 1)[0]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_0, data_handler.response(data_handler.response_counter() - 1)[1]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_1, data_handler.response(data_handler.response_counter() - 1)[2]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_2, data_handler.response(data_handler.response_counter() - 1)[3]);
    ASSERT_EQ(static_cast<uint8_t>(Status::Ack), data_handler.response(data_handler.response_counter() - 1)[4]);
    ASSERT_EQ(0x7E, data_handler.response(data_handler.response_counter() - 1)[5]);
    ASSERT_EQ(0x00, data_handler.response(data_handler.response_counter() - 1)[6]);
    ASSERT_EQ(0x01, data_handler.response(data_handler.response_counter() - 1)[7]);
    ASSERT_EQ(TEST_BLOCK_ID, data_handler.response(data_handler.response_counter() - 1)[8]);
    ASSERT_EQ(TEST_SECTION_MULTIPLE_PARTS_ID, data_handler.response(data_handler.response_counter() - 1)[9]);

    // Check the number of received messages.
    ASSERT_EQ(3, data_handler.response_counter());
}

TEST_F(SysExConfTest, CustomReq)
{
    open_conn();

    // Send a valid custom request message.
    handle_packet(CUSTOM_REQ);

    ASSERT_EQ(1, data_handler.custom_append_results.size());
    ASSERT_TRUE(data_handler.custom_append_results[0]);

    std::vector<uint8_t> data = {
        sysex_param(CUSTOM_REQUEST_VALUE)[0],
        sysex_param(CUSTOM_REQUEST_VALUE)[1],
    };

    // Check the response.
    verify_response(CUSTOM_REQ, Status::Ack, &data);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    /*
     * Send a custom request that should make the custom request handler return
     * `false`. In this case, `Status::ErrorRead` should be reported.
     */
    handle_packet(CUSTOM_REQ_ERROR_READ);

    ASSERT_EQ(0, data_handler.custom_append_results.size());

    // Check the response.
    verify_response(CUSTOM_REQ_ERROR_READ, Status::ErrorRead);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Send a non-existent custom request message. `Status::ErrorWish` should be reported.
    handle_packet(CUSTOM_REQ_INVALID);

    ASSERT_EQ(0, data_handler.custom_append_results.size());

    // Check the response.
    verify_response(CUSTOM_REQ_INVALID, Status::ErrorWish);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Disable configuration.
    handle_packet(CONN_CLOSE);

    // Verify that the connection is closed.
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Send a valid custom request message. `Status::ErrorConnection` should be reported.
    handle_packet(CUSTOM_REQ);

    ASSERT_EQ(0, data_handler.custom_append_results.size());

    // Check the response.
    verify_response(CUSTOM_REQ, Status::ErrorConnection);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Close the SysEx connection.
    handle_packet(CONN_CLOSE);

    // SysEx configuration should be disabled now.
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    /*
     * Send custom request `0`.
     * `Status::ErrorConnection` should be returned because the connection is closed.
     */
    handle_packet(CUSTOM_REQ);

    ASSERT_EQ(0, data_handler.custom_append_results.size());

    // Check the response.
    verify_response(CUSTOM_REQ, Status::ErrorConnection);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Send another custom request whose flag ignores connection status. `Status::Ack` should be reported.
    handle_packet(CUSTOM_REQ_NO_CONN_CHECK);

    ASSERT_EQ(1, data_handler.custom_append_results.size());
    ASSERT_TRUE(data_handler.custom_append_results[0]);

    data = {
        sysex_param(CUSTOM_REQUEST_VALUE)[0],
        sysex_param(CUSTOM_REQUEST_VALUE)[1],
    };

    // Check the response.
    verify_response(CUSTOM_REQ_NO_CONN_CHECK, Status::Ack, &data);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Open the connection again.
    handle_packet(CONN_OPEN);

    // Check the response.
    verify_response(CONN_OPEN, Status::Ack);

    // Verify that the connection is open.
    ASSERT_TRUE(sys_ex_conf.is_configuration_enabled());

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Try defining illegal custom requests.
    std::vector<CustomRequest> custom_requests_invalid = {
        {
            .request_id      = 0,
            .conn_open_check = true,
        },

        {
            .request_id      = 1,
            .conn_open_check = true,
        },

        {
            .request_id      = 2,
            .conn_open_check = true,
        },

        {
            .request_id      = 3,
            .conn_open_check = true,
        },

        {
            .request_id      = 4,
            .conn_open_check = true,
        },

        {
            .request_id      = 5,
            .conn_open_check = true,
        }
    };

    /*
     * `setup_custom_requests()` should return `false` because the pointed
     * structure defines special requests that are already used internally.
     */
    ASSERT_FALSE(sys_ex_conf.setup_custom_requests(custom_requests_invalid));
}

TEST_F(SysExConfTest, CustomReqAppendOverflow)
{
    open_conn();

    data_handler.force_custom_request_overflow = true;

    handle_packet(CUSTOM_REQ);

    bool seen_success = false;
    bool seen_failure = false;

    for (std::size_t i = 0; i < data_handler.custom_append_results.size(); i++)
    {
        if (data_handler.custom_append_results[i])
        {
            seen_success = true;
        }
        else
        {
            seen_failure = true;
        }
    }

    ASSERT_EQ(CUSTOM_REQUEST_OVERFLOW_ATTEMPTS, data_handler.custom_append_results.size());
    ASSERT_TRUE(seen_success);
    ASSERT_TRUE(seen_failure);

    ASSERT_EQ(1, data_handler.response_counter());
    ASSERT_EQ(0xF0, data_handler.response(data_handler.response_counter() - 1)[0]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_0, data_handler.response(data_handler.response_counter() - 1)[1]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_1, data_handler.response(data_handler.response_counter() - 1)[2]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_2, data_handler.response(data_handler.response_counter() - 1)[3]);
    ASSERT_EQ(static_cast<uint8_t>(Status::ErrorRead), data_handler.response(data_handler.response_counter() - 1)[4]);
    ASSERT_EQ(TEST_MSG_PART_VALID, data_handler.response(data_handler.response_counter() - 1)[5]);
    ASSERT_EQ(CUSTOM_REQUEST_ID_VALID, data_handler.response(data_handler.response_counter() - 1)[6]);
    // `append()` keeps one byte reserved for the final terminator and appends values in 2-byte chunks.
    ASSERT_EQ((MAX_MESSAGE_SIZE - 1), data_handler.response(data_handler.response_counter() - 1).size());
    ASSERT_EQ(SYSEX_END_BYTE, data_handler.response(data_handler.response_counter() - 1).back());
}

TEST_F(SysExConfTest, IgnoreMessage)
{
    open_conn();

    /*
     * Verify that no action is taken when the SysEx IDs in the message do not
     * match. A short message is any message missing one or more required bytes.
     */
    handle_packet(SHORT_MESSAGE1);

    // If no action was taken, `response_counter` should be `0`. Check the number of received messages.
    ASSERT_EQ(0, data_handler.response_counter());

    // Send another variant of the short message.
    handle_packet(SHORT_MESSAGE2);

    // Check the number of received messages.
    ASSERT_EQ(0, data_handler.response_counter());

    // Send a message with an invalid SysEx ID.
    handle_packet(GET_SINGLE_INVALID_SYS_EX_ID);

    // Check the number of received messages.
    ASSERT_EQ(0, data_handler.response_counter());

    // Send a short message that should return `Status::ErrorMessageLength`.
    handle_packet(SHORT_MESSAGE3);

    // Check the response.
    verify_response(SHORT_MESSAGE3, Status::ErrorMessageLength);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, CustomMessage)
{
    open_conn();

    // Construct a custom message and verify that the output matches.
    std::vector<uint16_t> values = { 0x05, 0x06, 0x07 };

    sys_ex_conf.send_custom_message(values);

    // Check the response.
    ASSERT_EQ(0xF0, data_handler.response(data_handler.response_counter() - 1)[0]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_0, data_handler.response(data_handler.response_counter() - 1)[1]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_1, data_handler.response(data_handler.response_counter() - 1)[2]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_2, data_handler.response(data_handler.response_counter() - 1)[3]);
    ASSERT_EQ(static_cast<uint8_t>(Status::Ack), data_handler.response(data_handler.response_counter() - 1)[4]);
    ASSERT_EQ(0x00, data_handler.response(data_handler.response_counter() - 1)[5]);
    ASSERT_EQ(0x05, data_handler.response(data_handler.response_counter() - 1)[6]);
    ASSERT_EQ(0x06, data_handler.response(data_handler.response_counter() - 1)[7]);
    ASSERT_EQ(0x07, data_handler.response(data_handler.response_counter() - 1)[8]);
    ASSERT_EQ(0xF7, data_handler.response(data_handler.response_counter() - 1)[9]);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Construct the same message again with `Status::Request` as the status byte.
    sys_ex_conf.send_custom_message(values, false);

    // Check the response.
    ASSERT_EQ(0xF0, data_handler.response(data_handler.response_counter() - 1)[0]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_0, data_handler.response(data_handler.response_counter() - 1)[1]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_1, data_handler.response(data_handler.response_counter() - 1)[2]);
    ASSERT_EQ(SYS_EX_CONF_M_ID_2, data_handler.response(data_handler.response_counter() - 1)[3]);
    ASSERT_EQ(static_cast<uint8_t>(Status::Request), data_handler.response(data_handler.response_counter() - 1)[4]);
    ASSERT_EQ(0x00, data_handler.response(data_handler.response_counter() - 1)[5]);
    ASSERT_EQ(0x05, data_handler.response(data_handler.response_counter() - 1)[6]);
    ASSERT_EQ(0x06, data_handler.response(data_handler.response_counter() - 1)[7]);
    ASSERT_EQ(0x07, data_handler.response(data_handler.response_counter() - 1)[8]);
    ASSERT_EQ(0xF7, data_handler.response(data_handler.response_counter() - 1)[9]);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, Backup)
{
    open_conn();

    // Send a backup-all request.
    handle_packet(BACKUP_ALL);

    // Check whether the status byte is set to `Status::Request`.
    ASSERT_EQ(static_cast<uint8_t>(Status::Request), data_handler.response(data_handler.response_counter() - 1)[4]);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Now send the received response back.
    handle_packet(data_handler.response(0));

    // Check whether the status byte is set to `Status::Ack`.
    ASSERT_EQ(static_cast<uint8_t>(Status::Ack), data_handler.response(data_handler.response_counter() - 1)[4]);

    // Reset the message count.
    data_handler.reset();

    // Send a backup-single request with an incorrect part.
    handle_packet(BACKUP_SINGLE_INV_PART);

    // Check the response.
    verify_response(BACKUP_SINGLE_INV_PART, Status::ErrorPart);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, SpecialRequest)
{
    open_conn();

    std::vector<uint8_t> data;

    // Test all preconfigured special requests and verify that they return the correct value.

    // Bytes-per-value request.
    handle_packet(GET_SPECIAL_REQ_BYTES_PER_VAL);

    data = {
        sysex_param(2)[0],
        sysex_param(2)[1],
    };

    // Check the response.
    verify_response(GET_SPECIAL_REQ_BYTES_PER_VAL, Status::Ack, &data);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Params-per-message request.
    handle_packet(GET_SPECIAL_REQ_PARAM_PER_MSG);

    data = {
        sysex_param(PARAMS_PER_MESSAGE)[0],
        sysex_param(PARAMS_PER_MESSAGE)[1],
    };

    // Check the response.
    verify_response(GET_SPECIAL_REQ_PARAM_PER_MSG, Status::Ack, &data);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    /*
     * Try those same requests again, but without a prior open-connection
     * request. The status byte must be `Status::ErrorConnection`.
     */

    // Close the connection first.
    handle_packet(CONN_CLOSE);

    // The configuration should be closed now.
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Bytes-per-value request.
    handle_packet(GET_SPECIAL_REQ_BYTES_PER_VAL);

    // Check the response.
    verify_response(GET_SPECIAL_REQ_BYTES_PER_VAL, Status::ErrorConnection);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Params-per-message request.
    handle_packet(GET_SPECIAL_REQ_PARAM_PER_MSG);

    // Check the response.
    verify_response(GET_SPECIAL_REQ_PARAM_PER_MSG, Status::ErrorConnection);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Try closing the configuration when it is already closed.
    handle_packet(CONN_CLOSE);

    // Check the response.
    verify_response(CONN_CLOSE, Status::ErrorConnection);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Send an open-connection request.
    handle_packet(CONN_OPEN);

    // SysEx configuration should be enabled now.
    ASSERT_TRUE(sys_ex_conf.is_configuration_enabled());

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();

    // Disable configuration again.
    handle_packet(CONN_CLOSE);

    // Check the response.
    verify_response(CONN_CLOSE, Status::Ack);

    // Check the number of received messages.
    ASSERT_EQ(1, data_handler.response_counter());

    // Reset the message count.
    data_handler.reset();
}
