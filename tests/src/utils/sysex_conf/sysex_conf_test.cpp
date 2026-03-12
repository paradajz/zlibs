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
                _response_counter = 0;
                _response.clear();
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

            void send_response(std::span<const uint8_t> response) override
            {
                std::vector<uint8_t> temp_response;

                for (size_t i = 0; i < response.size(); i++)
                {
                    temp_response.push_back(response[i]);
                }

                _response.push_back(temp_response);
            }

            std::vector<Status> get_results                   = {};
            std::vector<Status> set_results                   = {};
            std::vector<bool>   custom_append_results         = {};
            bool                force_custom_request_overflow = false;

            private:
            std::vector<std::vector<uint8_t>> _response;
            std::vector<uint8_t>              _custom_req_array = { 1 };

            std::size_t _response_counter = 0;
        };

        template<typename T>
        void verify_message(const std::vector<uint8_t>& source, T status, const std::vector<uint8_t>* data = nullptr)
        {
            std::size_t size = source.size();

            if ((data != nullptr) && source.size())
            {
                size--;    // skip last byte (0xF7) - custom data starts here
            }

            for (std::size_t i = 0; i < size; i++)
            {
                if (i != 4)    // status byte
                {
                    ASSERT_EQ(source.at(i), data_handler.response(data_handler.response_counter() - 1).at(i));
                }
                else
                {
                    ASSERT_EQ(static_cast<uint8_t>(status), data_handler.response(data_handler.response_counter() - 1).at(i));
                }
            }

            // now verify data
            if ((data != nullptr) && source.size())
            {
                for (std::size_t i = size; i < (size + data->size()); i++)
                {
                    ASSERT_EQ(data->at(i - size), data_handler.response(data_handler.response_counter() - 1).at(i));
                }

                ASSERT_EQ(0xF7, data_handler.response(data_handler.response_counter() - 1).at(size + data->size()));
            }
        }

        void handle_message(const std::vector<uint8_t>& source)
        {
            sys_ex_conf.handle_message(source);
        }

        void open_conn()
        {
            // send open connection request
            handle_message(CONN_OPEN);

            // sysex configuration should be enabled now
            ASSERT_TRUE(sys_ex_conf.is_configuration_enabled());

            data_handler.reset();
        }

        const std::vector<uint8_t> CONN_OPEN = {
            // request used to enable sysex configuration
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
            // request used to disable sysex configuration
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
            // get single message with invalid status byte for request message
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
            // wish byte set to invalid value
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
            // message intentionally one byte too long
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
            // block byte set to invalid value
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
            // section byte set to invalid value
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
            // index byte set to invalid value
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
            // part byte set to invalid value
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
            // amount byte set to invalid value
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
            // custom request with custom ID specified by user
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
            // custom request with custom ID specified by user
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
            // custom request with non-existing custom ID
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
            // custom request with non-existing custom ID
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
            // short message which should be ignored by the protocol, variant 1
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            SYS_EX_CONF_M_ID_2,
            static_cast<uint8_t>(Status::Request),
            0xF7
        };

        const std::vector<uint8_t> SHORT_MESSAGE2 = {
            // short message which should be ignored by the protocol, variant 2
            0xF0,
            SYS_EX_CONF_M_ID_0,
            SYS_EX_CONF_M_ID_1,
            0xF7
        };

        const std::vector<uint8_t> SHORT_MESSAGE3 = {
            // short message on which protocol should throw MESSAGE_LENGTH error
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
            // valid get single command
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
            // get single command with part id set to 1
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
            // set single command with part id set to 1
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
            // get single command with invalid sysex ids
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
            // valid get all command
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
            // valid get all command for all parts (7F variant)
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
            // valid get all command for all parts (7E variant)
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
            // built-in special request which returns number of bytes per value
            // configured in protocol
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
            // built-in special request which returns number of parameters per message
            // configured in protocol
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
            // valid set singe command
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
            // set single command - invalid new value
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
            // valid set all command
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
            // set all command with all parts modifier (invalid request)
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
            // valid set single command for section without min/max checking, variant
            // 1
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
            // valid set single command for section without min/max checking, variant
            // 2
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
            // valid set single command for section without min/max checking, variant
            // 3
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
            // set single command with invalid parameter index
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
            // set all command with invalid new value
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
            // set all command for section with more parts, part 0
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
            // set all command for section with more parts, part 1
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
            // backup all command
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
            // backup single command with invalid part set
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

    // close connection
    handle_message(CONN_CLOSE);

    // sysex configuration should be disabled now
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // check response
    verify_message(CONN_CLOSE, Status::Ack);
}

TEST_F(SysExConfTest, ErrorConnClosed)
{
    // configuration should be closed initially
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // send valid get message
    // since connection is closed, Status::ErrorConnection should be reported
    handle_message(GET_SINGLE_VALID);

    // check response
    verify_message(GET_SINGLE_VALID, Status::ErrorConnection);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorStatus)
{
    open_conn();

    // send message with invalid status byte
    // Status::ErrorStatus should be reported
    handle_message(ERROR_STATUS);

    // check response
    verify_message(ERROR_STATUS, Status::ErrorStatus);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorWish)
{
    open_conn();

    // send message with invalid wish byte
    handle_message(ERROR_WISH);

    // check response
    verify_message(ERROR_WISH, Status::ErrorWish);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorAmount)
{
    open_conn();

    // send message with invalid amount byte
    // Status::ErrorAmount should be reported
    handle_message(ERROR_AMOUNT);

    // check response
    verify_message(ERROR_AMOUNT, Status::ErrorAmount);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorBlock)
{
    open_conn();

    // send message with invalid block byte
    // Status::ErrorBlock should be reported
    handle_message(ERROR_BLOCK);

    // check response
    verify_message(ERROR_BLOCK, Status::ErrorBlock);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorSection)
{
    open_conn();

    // send message with invalid section byte
    // Status::ErrorSection should be reported
    handle_message(ERROR_SECTION);

    // check response
    verify_message(ERROR_SECTION, Status::ErrorSection);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorPart)
{
    open_conn();

    // send message with invalid index byte
    // Status::ErrorPart should be reported
    handle_message(ERROR_PART);

    // check response
    verify_message(ERROR_PART, Status::ErrorPart);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset number of received messages
    data_handler.reset();

    // send get single request with section which normally contains more parts,
    // however, set message part byte to 1
    // error part should be thrown because message part must always be at value 0
    // when the amount is single

    handle_message(GET_SINGLE_PART1);

    // check response
    verify_message(GET_SINGLE_PART1, Status::ErrorPart);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset number of received messages
    data_handler.reset();

    // same outcome is expected for set single message with part 1
    handle_message(SET_SINGLE_PART1);

    // check response
    verify_message(SET_SINGLE_PART1, Status::ErrorPart);
}

TEST_F(SysExConfTest, ErrorIndex)
{
    open_conn();

    // send message with invalid index byte
    // Status::ErrorIndex should be reported
    handle_message(ERROR_INDEX);

    // check response
    verify_message(ERROR_INDEX, Status::ErrorIndex);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorMessageLength)
{
    open_conn();

    // send message with invalid index byte
    // Status::ErrorMessageLength should be reported
    handle_message(ERROR_MESSAGE_LENGTH);

    // check response
    verify_message(ERROR_MESSAGE_LENGTH, Status::ErrorMessageLength);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorNewValue)
{
    open_conn();

    // send invalid set message
    // Status::ErrorNewValue should be reported
    handle_message(SET_SINGLE_INVALID_NEW_VALUE);

    // check response
    verify_message(SET_SINGLE_INVALID_NEW_VALUE, Status::ErrorNewValue);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorWrite)
{
    open_conn();

    // configure set function to return error
    // check if status byte is Status::ErrorWrite

    data_handler.set_results.push_back(Status::ErrorWrite);

    // send valid set message
    handle_message(SET_SINGLE_VALID);

    // check response
    verify_message(SET_SINGLE_VALID, Status::ErrorWrite);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset number of received messages
    data_handler.reset();
    data_handler.set_results.push_back(Status::ErrorWrite);

    handle_message(SET_ALL_VALID);

    // check response
    verify_message(SET_ALL_VALID, Status::ErrorWrite);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorWriteIgnoreUserErrorMode)
{
    open_conn();
    sys_ex_conf.set_user_error_ignore_mode(true);

    // configure set function to return error
    // check if status byte is Status::Ack due to the ignore mode being on

    data_handler.set_results.push_back(Status::ErrorWrite);

    // send valid set message
    handle_message(SET_SINGLE_VALID);

    // check response
    verify_message(SET_SINGLE_VALID, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset number of received messages
    data_handler.reset();
    data_handler.set_results.push_back(Status::ErrorWrite);

    handle_message(SET_ALL_VALID);

    // check response
    verify_message(SET_ALL_VALID, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorRead)
{
    open_conn();

    // configure get function to return error
    // check if status byte is Status::ErrorRead

    data_handler.get_results.push_back(Status::ErrorRead);

    handle_message(GET_SINGLE_VALID);

    // check response
    verify_message(GET_SINGLE_VALID, Status::ErrorRead);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();
    data_handler.get_results.push_back(Status::ErrorRead);

    // test get with all parameters
    // Status::ErrorRead should be reported again
    handle_message(GET_ALL_VALID_1PART);

    // check response
    verify_message(GET_ALL_VALID_1PART, Status::ErrorRead);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorReadIgnoreUserErrorMode)
{
    open_conn();
    sys_ex_conf.set_user_error_ignore_mode(true);

    // configure get function to return error
    // check if status byte is Status::Ack due to the ignore mode being on

    // user error should be ignored
    data_handler.get_results.push_back(Status::ErrorRead);

    handle_message(GET_SINGLE_VALID);

    // read value should be set to 0
    std::vector<uint8_t> data = {
        sysex_param(0)[0],
        sysex_param(0)[1],
    };

    // check response
    verify_message(GET_SINGLE_VALID, Status::Ack, &data);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();
    data_handler.get_results.push_back(Status::ErrorRead);
    data_handler.get_results.push_back(Status::Ack);
    data_handler.get_results.push_back(Status::ErrorRead);

    // test get with all parameters
    handle_message(GET_ALL_VALID_1PART);

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

    // check response
    verify_message(GET_ALL_VALID_1PART, Status::Ack, &data);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, ErrorCustom)
{
    open_conn();

    // configure set function to return custom error
    uint8_t error = 63;
    data_handler.set_results.push_back(static_cast<Status>(error));

    // send valid set message
    handle_message(SET_SINGLE_VALID);

    // check response
    verify_message(SET_SINGLE_VALID, error);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset number of received messages
    data_handler.reset();
    data_handler.set_results.push_back(static_cast<Status>(error));

    handle_message(SET_ALL_VALID);

    // check response
    verify_message(SET_ALL_VALID, error);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, SetSingle)
{
    open_conn();

    // send valid set message
    handle_message(SET_SINGLE_VALID);

    // check response
    verify_message(SET_SINGLE_VALID, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send set single command with invalid param index
    handle_message(SET_SINGLE_INVALID_PARAM);

    // check response
    verify_message(SET_SINGLE_INVALID_PARAM, Status::ErrorIndex);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // test block which has same min and max value
    // in this case, Status::ErrorNewValue should never be reported on any
    // value
    handle_message(SET_SINGLE_NO_MIN_MAX1);

    // check response
    verify_message(SET_SINGLE_NO_MIN_MAX1, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    handle_message(SET_SINGLE_NO_MIN_MAX2);

    // check response
    verify_message(SET_SINGLE_NO_MIN_MAX2, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    handle_message(SET_SINGLE_NO_MIN_MAX3);

    // check response
    verify_message(SET_SINGLE_NO_MIN_MAX3, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();
}

TEST_F(SysExConfTest, SetAll)
{
    open_conn();

    // send set all request
    handle_message(SET_ALL_VALID);

    // check response
    verify_message(SET_ALL_VALID, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send set all message for section with more parts
    handle_message(SET_ALL_MORE_PARTS1);

    // check response
    verify_message(SET_ALL_MORE_PARTS1, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send set all request with part byte being 0x01
    handle_message(SET_ALL_MORE_PARTS2);

    // check response
    verify_message(SET_ALL_MORE_PARTS2, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send set all requests for all parts and verify that status byte is set to
    // Status::ErrorPart
    handle_message(SET_ALL_ALL_PARTS);

    // check response
    verify_message(SET_ALL_ALL_PARTS, Status::ErrorPart);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send set all request with invalid value
    // status byte should be Status::ErrorNewValue
    handle_message(SET_ALLNVALID_NEW_VAL);

    // check response
    verify_message(SET_ALLNVALID_NEW_VAL, Status::ErrorNewValue);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();
}

TEST_F(SysExConfTest, GetSingle)
{
    open_conn();

    // send get single request
    handle_message(GET_SINGLE_VALID);

    const std::vector<uint8_t> DATA = {
        sysex_param(TEST_VALUE_GET)[0],
        sysex_param(TEST_VALUE_GET)[1],
    };

    // check response
    verify_message(GET_SINGLE_VALID, Status::Ack, &DATA);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, GetAll)
{
    open_conn();

    // send get all request
    handle_message(GET_ALL_VALID_1PART);

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

    // check response
    verify_message(GET_ALL_VALID_1PART, Status::Ack, &DATA);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // now send same request for all parts
    // we are expecting 2 messages now
    handle_message(GET_ALL_VALID_ALL_PARTS_7_F);

    // check number of received messages
    ASSERT_EQ(2, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // same message with part being 0x7E
    // in this case, last message should be Status::Ack message
    handle_message(GET_ALL_VALID_ALL_PARTS_7_E);

    // check last response
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

    // check number of received messages
    ASSERT_EQ(3, data_handler.response_counter());
}

TEST_F(SysExConfTest, CustomReq)
{
    open_conn();

    // send valid custom request message
    handle_message(CUSTOM_REQ);

    ASSERT_EQ(1, data_handler.custom_append_results.size());
    ASSERT_TRUE(data_handler.custom_append_results[0]);

    std::vector<uint8_t> data = {
        sysex_param(CUSTOM_REQUEST_VALUE)[0],
        sysex_param(CUSTOM_REQUEST_VALUE)[1],
    };

    // check response
    verify_message(CUSTOM_REQ, Status::Ack, &data);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send custom request message which should return false in custom request
    // handler in this case, Status::ErrorRead should be reported
    handle_message(CUSTOM_REQ_ERROR_READ);

    ASSERT_EQ(0, data_handler.custom_append_results.size());

    // check response
    verify_message(CUSTOM_REQ_ERROR_READ, Status::ErrorRead);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send non-existing custom request message
    // Status::ErrorWish should be reported
    handle_message(CUSTOM_REQ_INVALID);

    ASSERT_EQ(0, data_handler.custom_append_results.size());

    // check response
    verify_message(CUSTOM_REQ_INVALID, Status::ErrorWish);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // disable configuration
    handle_message(CONN_CLOSE);

    // verify that connection is closed
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send valid custom request message
    // Status::ErrorConnection should be reported
    handle_message(CUSTOM_REQ);

    ASSERT_EQ(0, data_handler.custom_append_results.size());

    // check response
    verify_message(CUSTOM_REQ, Status::ErrorConnection);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // close sysex connection
    handle_message(CONN_CLOSE);

    // sysex configuration should be disabled now
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send custom request 0
    // Status::ErrorConnection should be returned because connection is closed
    handle_message(CUSTOM_REQ);

    ASSERT_EQ(0, data_handler.custom_append_results.size());

    // check response
    verify_message(CUSTOM_REQ, Status::ErrorConnection);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send another custom request which has flag set to ignore connection status
    // Status::Ack should be reported
    handle_message(CUSTOM_REQ_NO_CONN_CHECK);

    ASSERT_EQ(1, data_handler.custom_append_results.size());
    ASSERT_TRUE(data_handler.custom_append_results[0]);

    data = {
        sysex_param(CUSTOM_REQUEST_VALUE)[0],
        sysex_param(CUSTOM_REQUEST_VALUE)[1],
    };

    // check response
    verify_message(CUSTOM_REQ_NO_CONN_CHECK, Status::Ack, &data);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // open connection again
    handle_message(CONN_OPEN);

    // check response
    verify_message(CONN_OPEN, Status::Ack);

    // verify that connection is opened
    ASSERT_TRUE(sys_ex_conf.is_configuration_enabled());

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // try defining illegal custom requests
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

    // setup_custom_requests should return false because special requests which
    // are already used internally are defined in pointed structure
    ASSERT_FALSE(sys_ex_conf.setup_custom_requests(custom_requests_invalid));
}

TEST_F(SysExConfTest, CustomReqAppendOverflow)
{
    open_conn();

    data_handler.force_custom_request_overflow = true;

    handle_message(CUSTOM_REQ);

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
    // append() keeps one byte reserved for final terminator and appends values in 2-byte chunks.
    ASSERT_EQ((MAX_MESSAGE_SIZE - 1), data_handler.response(data_handler.response_counter() - 1).size());
    ASSERT_EQ(SYSEX_END_BYTE, data_handler.response(data_handler.response_counter() - 1).back());
}

TEST_F(SysExConfTest, IgnoreMessage)
{
    open_conn();

    // verify that no action takes place when sysex ids in message don't match
    // short message is any message without every required byte
    handle_message(SHORT_MESSAGE1);

    // if no action took place, response_counter should be 0
    // check number of received messages
    ASSERT_EQ(0, data_handler.response_counter());

    // send another variant of short message
    handle_message(SHORT_MESSAGE2);

    // check number of received messages
    ASSERT_EQ(0, data_handler.response_counter());

    // send message with invalid SysEx ID
    handle_message(GET_SINGLE_INVALID_SYS_EX_ID);

    // check number of received messages
    ASSERT_EQ(0, data_handler.response_counter());

    // short message where Status::ErrorMessageLength should be returned
    handle_message(SHORT_MESSAGE3);

    // check response
    verify_message(SHORT_MESSAGE3, Status::ErrorMessageLength);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, CustomMessage)
{
    open_conn();

    // construct custom message and see if output matches
    std::vector<uint16_t> values = { 0x05, 0x06, 0x07 };

    sys_ex_conf.send_custom_message(values);

    // check response
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

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // construct same message again with Status::Request as status byte
    sys_ex_conf.send_custom_message(values, false);

    // check response
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

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, Backup)
{
    open_conn();

    // send backup all request
    handle_message(BACKUP_ALL);

    // check if status byte is set to Status::Request value
    ASSERT_EQ(static_cast<uint8_t>(Status::Request), data_handler.response(data_handler.response_counter() - 1)[4]);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // now, try to send received response back
    handle_message(data_handler.response(0));

    // check if status byte is set to Status::Ack value
    ASSERT_EQ(static_cast<uint8_t>(Status::Ack), data_handler.response(data_handler.response_counter() - 1)[4]);

    // reset message count
    data_handler.reset();

    // send backup/single request with incorrect part
    handle_message(BACKUP_SINGLE_INV_PART);

    // check response
    verify_message(BACKUP_SINGLE_INV_PART, Status::ErrorPart);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());
}

TEST_F(SysExConfTest, SpecialRequest)
{
    open_conn();

    std::vector<uint8_t> data;

    // test all pre-configured special requests and see if they return correct
    // value

    // bytes per value request
    handle_message(GET_SPECIAL_REQ_BYTES_PER_VAL);

    data = {
        sysex_param(2)[0],
        sysex_param(2)[1],
    };

    // check response
    verify_message(GET_SPECIAL_REQ_BYTES_PER_VAL, Status::Ack, &data);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // params per msg request
    handle_message(GET_SPECIAL_REQ_PARAM_PER_MSG);

    data = {
        sysex_param(PARAMS_PER_MESSAGE)[0],
        sysex_param(PARAMS_PER_MESSAGE)[1],
    };

    // check response
    verify_message(GET_SPECIAL_REQ_PARAM_PER_MSG, Status::Ack, &data);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // now try those same requests, but without prior open connection request
    // status byte must equal Status::ErrorConnection

    // close connection first
    handle_message(CONN_CLOSE);

    // configuration should be closed now
    ASSERT_FALSE(sys_ex_conf.is_configuration_enabled());

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // bytes per value request
    handle_message(GET_SPECIAL_REQ_BYTES_PER_VAL);

    // check response
    verify_message(GET_SPECIAL_REQ_BYTES_PER_VAL, Status::ErrorConnection);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // params per msg request
    handle_message(GET_SPECIAL_REQ_PARAM_PER_MSG);

    // check response
    verify_message(GET_SPECIAL_REQ_PARAM_PER_MSG, Status::ErrorConnection);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // try to close configuration which is already closed
    handle_message(CONN_CLOSE);

    // check response
    verify_message(CONN_CLOSE, Status::ErrorConnection);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // send open connection request
    handle_message(CONN_OPEN);

    // sysex configuration should be enabled now
    ASSERT_TRUE(sys_ex_conf.is_configuration_enabled());

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();

    // disable configuration again
    handle_message(CONN_CLOSE);

    // check response
    verify_message(CONN_CLOSE, Status::Ack);

    // check number of received messages
    ASSERT_EQ(1, data_handler.response_counter());

    // reset message count
    data_handler.reset();
}
