/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/midi/midi.h"

namespace zlibs::utils::midi
{
    /**
     * @brief No-op transport used when a concrete transport backend is disabled.
     */
    class Null : public virtual Base
    {
        public:
        template<typename... Args>
        explicit Null(Args&&... /*args*/)
        {
            bind_transport(_transport);
        }

        private:
        class Transport : public midi::Transport
        {
            public:
            bool init() override
            {
                return true;
            }

            bool deinit() override
            {
                return true;
            }

            bool supported() override
            {
                return false;
            }

            bool write([[maybe_unused]] const midi_ump& packet) override
            {
                return false;
            }

            std::optional<midi_ump> read() override
            {
                return {};
            }
        };

        protected:
        Transport _transport = {};
    };
}    // namespace zlibs::utils::midi
