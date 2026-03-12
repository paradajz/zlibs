/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_setting.h"

#include "zlibs/utils/misc/meta.h"
#include "zlibs/utils/misc/noncopyable.h"
#include "zlibs/utils/misc/string.h"

#include <zephyr/settings/settings.h>

namespace zlibs::utils::settings
{
    /**
     * @brief Manager for a compile-time list of persisted settings.
     *
     * @tparam SubtreeName Settings subtree name used for load/save operations.
     * @tparam Settings Setting descriptor types.
     */
    template<zlibs::utils::misc::StringLiteral SubtreeName, SettingData... Settings>
        requires misc::AllUnique<Settings...>
    class SettingsManager : public misc::NonCopyableOrMovable
    {
        public:
        SettingsManager()  = default;
        ~SettingsManager() = default;

        /**
         * @brief Initializes Zephyr settings subsystem and loads stored values.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init()
        {
            if (settings_subsys_init() != 0)
            {
                return false;
            }

            return settings_load_subtree_direct(SubtreeName.c_str(), settings_load, this) == 0;
        }

        /**
         * @brief Updates a setting value and optionally persists it.
         *
         * @tparam T Setting descriptor type.
         * @param value New setting payload.
         * @param retention When `true`, value is written to Zephyr settings storage.
         *
         * @return `true` if update succeeds, otherwise `false`.
         */
        template<SettingData T>
        bool update(const typename T::Value& value, bool retention = true)
        {
            auto& setting = std::get<BaseSetting<T>>(_settings);
            bool  ret     = setting.update(value);

            if (ret && retention)
            {
                static constexpr auto PATH = misc::string_join(SubtreeName,
                                                               "/",
                                                               T::Properties::KEY);

                ret = settings_save_one(PATH.c_str(), &setting.value(), sizeof(typename T::Value)) == 0;
            }

            return ret;
        }

        /**
         * @brief Returns the current value of a setting.
         *
         * @tparam T Setting descriptor type.
         * @return Current setting value.
         */
        template<SettingData T>
        const typename T::Value& value() const
        {
            return std::get<BaseSetting<T>>(_settings).value();
        }

        /**
         * @brief Restores all non-retained settings to defaults.
         *
         * @return `true` if all applicable resets succeed, otherwise `false`.
         */
        bool reset()
        {
            bool ret = true;

            (
                [&]
                {
                    if (ret && !Settings::Properties::KEEP_ON_RESET)
                    {
                        ret = update<Settings>(Settings::Properties::DEFAULT_VALUE);
                    }
                }(),
                ...);

            return ret;
        }

        private:
        /** @brief Tuple storing one `BaseSetting` instance per managed setting descriptor. */
        using SettingsTuple = std::tuple<BaseSetting<Settings>...>;

        SettingsTuple _settings = {};

        /**
         * @brief Zephyr settings loader callback for one subtree entry.
         *
         * @param key Setting key relative to `SubtreeName`.
         * @param len Serialized data size.
         * @param cb Zephyr settings read callback.
         * @param cb_arg Callback argument forwarded to `cb`.
         * @param param User parameter carrying `SettingsManager` instance.
         *
         * @return `0` on success, otherwise a negative error code.
         */
        static int settings_load(const char*      key,
                                 size_t           len,
                                 settings_read_cb cb,
                                 void*            cb_arg,
                                 void*            param)
        {
            auto self = static_cast<SettingsManager*>(param);

            if (self == nullptr)
            {
                return -ENOENT;
            }

            int  rc    = 0;
            bool found = false;

            (
                [&]
                {
                    if (found)
                    {
                        return;
                    }

                    const char* next = nullptr;

                    if (settings_name_steq(key, std::string_view(Settings::Properties::KEY).data(), &next) && !next)
                    {
                        found         = true;
                        auto& setting = std::get<BaseSetting<Settings>>(self->_settings);

                        if (len == sizeof(typename Settings::Value))
                        {
                            rc = cb(cb_arg, &setting.value(), sizeof(typename Settings::Value));
                        }
                    }
                }(),
                ...);

            return (rc >= 0 && found) ? 0 : -ENOENT;
        }
    };
}    // namespace zlibs::utils::settings
