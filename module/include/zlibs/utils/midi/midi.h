/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "midi_common.h"
#include "midi1.h"
#include "midi2.h"
#include "zlibs/utils/midi/transport/transport_base.h"

#include "zlibs/utils/misc/mutex.h"

namespace zlibs::utils::midi
{
    /**
     * @brief Core transport-agnostic MIDI parser and transmitter.
     *
     * `send(const midi_ump&)` is thread-safe.
     */
    class Base
    {
        public:
        /**
         * @brief Constructs a MIDI instance over a transport backend.
         *
         * @param transport Transport implementation.
         */
        explicit Base(Transport& transport)
            : _transport(transport)
        {}

        /**
         * @brief Initializes MIDI parser/transmitter and underlying transport.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init();

        /**
         * @brief Deinitializes MIDI parser/transmitter and underlying transport.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool deinit();

        /**
         * @brief Returns the thru-facing interface of the underlying transport.
         *
         * @return Mutable reference to the transport thru sink.
         */
        Thru& thru_interface()
        {
            return _transport;
        }

        /**
         * @brief Returns whether this instance is initialized.
         *
         * @return `true` when initialized, otherwise `false`.
         */
        bool initialized();

        /**
         * @brief Sends one Universal MIDI Packet through the active transport.
         *
         * @param packet UMP to transmit.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send(const midi_ump& packet);

        /**
         * @brief Reads one UMP from transport.
         *
         * @return Read packet, or `std::nullopt` when no packet is available.
         */
        std::optional<midi_ump> read();

        /**
         * @brief Registers one thru sink interface.
         *
         * @param interface_ref Thru interface to register.
         */
        void register_thru_interface(Thru& interface_ref);

        /**
         * @brief Unregisters one thru sink interface.
         *
         * @param interface_ref Thru interface to unregister.
         */
        void unregister_thru_interface(Thru& interface_ref);

        private:
        Transport&                                                     _transport;
        std::array<Thru*, CONFIG_ZLIBS_UTILS_MIDI_MAX_THRU_INTERFACES> _thru_interfaces = {};
        bool                                                           _initialized     = false;
        mutable zlibs::utils::misc::Mutex                              _tx_mutex;

        /**
         * @brief Forwards a packet to all registered thru interfaces.
         */
        void thru(const midi_ump& packet);
    };
}    // namespace zlibs::utils::midi
