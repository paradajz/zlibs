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
         * @brief Constructs a MIDI instance for deferred transport binding.
         *
         * Derived transport wrappers must call `bind_transport()` before the
         * instance is used.
         */
        Base() = default;

        virtual ~Base() = default;

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
            return *_transport;
        }

        /**
         * @brief Returns whether this instance is initialized.
         *
         * @return `true` when initialized, otherwise `false`.
         */
        bool initialized();

        /**
         * @brief Returns whether the underlying transport backend is available.
         *
         * @return `true` when the backend is supported, otherwise `false`.
         */
        bool supported();

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
         *
         * @return `true` if the thru route was registered, otherwise `false`.
         */
        bool register_thru_interface(Thru& interface_ref);

        /**
         * @brief Unregisters one thru sink interface.
         *
         * @param interface_ref Thru interface to unregister.
         */
        void unregister_thru_interface(Thru& interface_ref);

        protected:
        /**
         * @brief Enables a thru route from this MIDI source to a destination.
         *
         * Implementations can use this hook to prepare route-specific source
         * resources before software forwarding is registered. Returning `false`
         * rejects the route and prevents registration.
         *
         * @param destination Destination thru sink being routed from this source.
         *
         * @return `true` if the route can be enabled, otherwise `false`.
         */
        virtual bool enable_thru_route(Thru& destination);

        /**
         * @brief Disables a thru route from this MIDI source to a destination.
         *
         * Implementations can use this hook to release source-side resources
         * prepared in `enable_thru_route()`.
         *
         * @param destination Destination thru sink being unrouted from this source.
         */
        virtual void disable_thru_route(Thru& destination);

        /**
         * @brief Returns whether this source bypasses software forwarding to a destination.
         *
         * A source can return `true` when `enable_thru_route()` activated an
         * alternate forwarding path, such as hardware loopback. In that case the
         * route remains registered, but `Base` skips software forwarding while
         * the bypass is active.
         *
         * @param destination Destination thru sink being routed from this source.
         *
         * @return `true` if software forwarding should be skipped, otherwise `false`.
         */
        virtual bool has_thru_bypass(Thru& destination);

        /**
         * @brief Binds this MIDI instance to a transport backend.
         *
         * This is used by concrete transport wrappers when `Base` is a virtual
         * base class and therefore cannot be initialized by an intermediate
         * transport constructor.
         *
         * @param transport Transport implementation.
         */
        void bind_transport(Transport& transport)
        {
            _transport = &transport;
        }

        private:
        Transport*                                                     _transport       = nullptr;
        std::array<Thru*, CONFIG_ZLIBS_UTILS_MIDI_MAX_THRU_INTERFACES> _thru_interfaces = {};
        bool                                                           _initialized     = false;
        mutable zlibs::utils::misc::Mutex                              _tx_mutex;
        mutable zlibs::utils::misc::Mutex                              _thru_mutex;

        /**
         * @brief Forwards a packet to all registered thru interfaces.
         */
        void thru(const midi_ump& packet);
    };
}    // namespace zlibs::utils::midi
