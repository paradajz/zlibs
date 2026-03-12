/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/drivers/usb/usb_hw.h"

#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>

using namespace zlibs::drivers::usb;

namespace
{
    LOG_MODULE_REGISTER(zlibs_drivers_usb, CONFIG_ZLIBS_LOG_LEVEL);

    USBD_DEVICE_DEFINE(usb_device,
                       nullptr,
                       CONFIG_ZLIBS_DRIVERS_USB_VID,
                       CONFIG_ZLIBS_DRIVERS_USB_PID);

    USBD_DESC_LANG_DEFINE(usb_lang);
    USBD_DESC_MANUFACTURER_DEFINE(usb_mfr, CONFIG_ZLIBS_DRIVERS_USB_MANUFACTURER);
    USBD_DESC_PRODUCT_DEFINE(usb_product, CONFIG_ZLIBS_DRIVERS_USB_PRODUCT);
    IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(usb_sn)));

    USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
    USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

    constexpr uint8_t COMPOSITE_DEVICE_SUBCLASS = 0x02;
    constexpr uint8_t COMPOSITE_DEVICE_PROTOCOL = 0x01;
    constexpr uint8_t ATTRIBUTES                = (IS_ENABLED(CONFIG_ZLIBS_DRIVERS_USB_SELF_POWERED) ? USB_SCD_SELF_POWERED : 0) |
                                                  (IS_ENABLED(CONFIG_ZLIBS_DRIVERS_USB_REMOTE_WAKEUP) ? USB_SCD_REMOTE_WAKEUP : 0);

    constexpr const char* BLOCKLIST[] = {
        "dfu_dfu",
        nullptr,
    };

    USBD_CONFIGURATION_DEFINE(usb_fs_config,
                              ATTRIBUTES,
                              CONFIG_ZLIBS_DRIVERS_USB_MAX_POWER,
                              &fs_cfg_desc);

    USBD_CONFIGURATION_DEFINE(usb_hs_config,
                              ATTRIBUTES,
                              CONFIG_ZLIBS_DRIVERS_USB_MAX_POWER,
                              &hs_cfg_desc);

    void usb_fix_code_triple(usbd_context* context, const usbd_speed speed)
    {
        // Always use class code information from interface descriptors.
        if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
            IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
            IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) ||
            IS_ENABLED(CONFIG_USBD_MIDI2_CLASS) ||
            IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS) ||
            IS_ENABLED(CONFIG_USBD_VIDEO_CLASS))
        {
            /*
             * Class with multiple interfaces have an Interface
             * Association Descriptor available, use an appropriate triple
             * to indicate it.
             */
            usbd_device_set_code_triple(context,
                                        speed,
                                        USB_BCC_MISCELLANEOUS,
                                        COMPOSITE_DEVICE_SUBCLASS,
                                        COMPOSITE_DEVICE_PROTOCOL);
        }
        else
        {
            // Use interface descriptors for class information.
            usbd_device_set_code_triple(context, speed, 0, 0, 0);
        }
    }

    usbd_context* usb_setup_device()
    {
        int ret = usbd_add_descriptor(&usb_device, &usb_lang);

        if (ret)
        {
            LOG_ERR("Failed to initialize language descriptor (%d)", ret);
            return nullptr;
        }

        ret = usbd_add_descriptor(&usb_device, &usb_mfr);

        if (ret)
        {
            LOG_ERR("Failed to initialize manufacturer descriptor (%d)", ret);
            return nullptr;
        }

        ret = usbd_add_descriptor(&usb_device, &usb_product);

        if (ret)
        {
            LOG_ERR("Failed to initialize product descriptor (%d)", ret);
            return nullptr;
        }

        IF_ENABLED(CONFIG_HWINFO, (ret = usbd_add_descriptor(&usb_device, &usb_sn);))

        if (ret)
        {
            LOG_ERR("Failed to initialize SN descriptor (%d)", ret);
            return nullptr;
        }

        if (USBD_SUPPORTS_HIGH_SPEED && usbd_caps_speed(&usb_device) == USBD_SPEED_HS)
        {
            ret = usbd_add_configuration(&usb_device, USBD_SPEED_HS, &usb_hs_config);

            if (ret)
            {
                LOG_ERR("Failed to add High-Speed configuration");
                return nullptr;
            }

            ret = usbd_register_all_classes(&usb_device, USBD_SPEED_HS, 1, BLOCKLIST);

            if (ret)
            {
                LOG_ERR("Failed to add register classes");
                return nullptr;
            }

            usb_fix_code_triple(&usb_device, USBD_SPEED_HS);
        }

        ret = usbd_add_configuration(&usb_device, USBD_SPEED_FS, &usb_fs_config);

        if (ret)
        {
            LOG_ERR("Failed to add Full-Speed configuration");
            return nullptr;
        }

        ret = usbd_register_all_classes(&usb_device, USBD_SPEED_FS, 1, BLOCKLIST);

        if (ret)
        {
            LOG_ERR("Failed to add register classes");
            return nullptr;
        }

        usb_fix_code_triple(&usb_device, USBD_SPEED_FS);
        usbd_self_powered(&usb_device, ATTRIBUTES & USB_SCD_SELF_POWERED);

        return &usb_device;
    }
}    // namespace

UsbHw::UsbHw(const device* device)
    : _device(device)
{}

UsbHw& UsbHw::instance(const device* device)
{
    static UsbHw instance(device);

    if ((device != nullptr) && (instance._device != device))
    {
        LOG_WRN("USB hardware wrapper is a singleton, reusing device %s instead of %s",
                instance._device != nullptr ? instance._device->name : "<null>",
                device->name);
    }

    return instance;
}

bool UsbHw::init()
{
    if (_device == nullptr)
    {
        LOG_ERR("Null device cannot be configured for USB communication");
        return false;
    }

    if (!device_is_ready(_device))
    {
        LOG_ERR("USB device %s not ready", _device->name);
        return false;
    }

    usb_device.dev = _device;

    if (usb_setup_device() == nullptr)
    {
        return false;
    }

    int ret = usbd_init(&usb_device);

    if (ret)
    {
        LOG_ERR("Failed to initialize device support");
        return false;
    }

    if (usbd_enable(&usb_device))
    {
        LOG_ERR("Failed to enable device support");
        return false;
    }

    return true;
}

bool UsbHw::deinit()
{
    int ret = 0;

    if (usb_device.status.enabled)
    {
        ret = usbd_disable(&usb_device);

        if (ret)
        {
            LOG_ERR("Failed to disable USB device support (%d)", ret);
            return false;
        }
    }

    if (usb_device.status.initialized)
    {
        ret = usbd_shutdown(&usb_device);

        if (ret)
        {
            LOG_ERR("Failed to shutdown USB device support (%d)", ret);
            return false;
        }
    }

    usb_device.dev = nullptr;

    return true;
}
