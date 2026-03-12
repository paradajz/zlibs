/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/drivers/uart/uart_hw.h"

#include <zephyr/logging/log.h>

using namespace zlibs::drivers::uart;

namespace
{
    LOG_MODULE_REGISTER(zlibs_drivers_uart, CONFIG_ZLIBS_LOG_LEVEL);
}    // namespace

UartHw::UartHw(const device* device)
    : _device(device)
{}

UartHw::~UartHw()
{
    shutdown();
}

bool UartHw::init(const Config& config)
{
    const zlibs::utils::misc::LockGuard lock(_mutex);

    if (_device == nullptr)
    {
        LOG_ERR("Null device cannot be configured for UART communication");
        return false;
    }

    if (!device_is_ready(_device))
    {
        LOG_ERR("UART device %s not ready", _device->name);
        return false;
    }

    if (_initialized)
    {
        LOG_ERR("UART device %s is already initialized", _device->name);
        return false;
    }

    if (!configure(config))
    {
        return false;
    }

    _initialized = true;
    uart_irq_rx_enable(_device);

    LOG_INF("UART successfully initialized");

    return true;
}

bool UartHw::deinit()
{
    return shutdown();
}

bool UartHw::shutdown()
{
    const zlibs::utils::misc::LockGuard lock(_mutex);
    const unsigned int                  irq_key = irq_lock();

    if (!_initialized)
    {
        // Nothing to do.
        irq_unlock(irq_key);
        return true;
    }

    int callback_result = 0;

    if ((_device != nullptr) && device_is_ready(_device))
    {
        uart_irq_rx_disable(_device);
        uart_irq_tx_disable(_device);

        callback_result = uart_irq_callback_user_data_set(_device, nullptr, nullptr);
    }

    _initialized = false;
    _loopback    = false;
    _rx_buffer   = nullptr;
    _tx_buffer   = nullptr;

    irq_unlock(irq_key);

    if (callback_result != 0)
    {
        LOG_WRN("Failed to unregister UART callback for device %s: %d", _device->name, callback_result);
    }

    return true;
}

bool UartHw::configure(const Config& config)
{
    uart_config uart_config = {};
    uart_config.baudrate    = config.baudrate;
    uart_config.stop_bits   = config.stop_bits;
    uart_config.parity      = UART_CFG_PARITY_NONE;
    uart_config.data_bits   = UART_CFG_DATA_BITS_8;
    uart_config.flow_ctrl   = UART_CFG_FLOW_CTRL_NONE;

    if (uart_configure(_device, &uart_config) != 0)
    {
        LOG_ERR("Configuration failed for UART device: %s", _device->name);
        return false;
    }

    const int callback_result = uart_irq_callback_user_data_set(_device, [](const device*, void* user_data)
                                                                {
                                                                    auto self = static_cast<UartHw*>(user_data);

                                                                    if (self == nullptr)
                                                                    {
                                                                        return;
                                                                    }

                                                                    self->isr();
                                                                },
                                                                this);

    if (callback_result != 0)
    {
        LOG_ERR("Failed to register UART callback for device %s: %d", _device->name, callback_result);
        return false;
    }

    _loopback  = config.loopback;
    _rx_buffer = &config.rx_buffer;
    _tx_buffer = &config.tx_buffer;

    return true;
}

bool UartHw::write(uint8_t data)
{
    const zlibs::utils::misc::LockGuard lock(_mutex);

    if (!_initialized || (_tx_buffer == nullptr))
    {
        LOG_ERR("UART device not configured");
        return false;
    }

    if (!_tx_buffer->insert(data))
    {
        LOG_WRN_RATELIMIT("UART TX buffer full on device %s", _device->name);
        return false;
    }

    uart_irq_tx_enable(_device);

    return true;
}

std::optional<uint8_t> UartHw::read()
{
    const zlibs::utils::misc::LockGuard lock(_mutex);

    if (!_initialized || (_rx_buffer == nullptr))
    {
        LOG_ERR("UART device not configured");
        return {};
    }

    return _rx_buffer->remove();
}

void UartHw::isr()
{
    if (!_initialized || (_rx_buffer == nullptr) || (_tx_buffer == nullptr))
    {
        return;
    }

    while (true)
    {
        const int update_result = uart_irq_update(_device);

        if (update_result <= 0)
        {
            break;
        }

        const int pending_result = uart_irq_is_pending(_device);

        if (pending_result <= 0)
        {
            break;
        }

        const int rx_ready = uart_irq_rx_ready(_device);

        if (rx_ready < 0)
        {
            break;
        }

        if (rx_ready > 0)
        {
            while (true)
            {
                uint8_t   data           = 0;
                const int bytes_received = uart_fifo_read(_device, &data, 1);

                if (bytes_received <= 0)
                {
                    break;
                }

                if (!_rx_buffer->insert(data))
                {
                    // Overflow; nothing to do.
                }

                if (_loopback)
                {
                    if (_tx_buffer->insert(data))
                    {
                        uart_irq_tx_enable(_device);
                    }
                    else
                    {
                        LOG_WRN_RATELIMIT("UART TX buffer full while looping back on device %s", _device->name);
                    }
                }
            }
        }

        const int tx_ready = uart_irq_tx_ready(_device);

        if (tx_ready < 0)
        {
            break;
        }

        if (tx_ready > 0)
        {
            const auto tx_data = _tx_buffer->peek();

            if (!tx_data.has_value())
            {
                uart_irq_tx_disable(_device);
                continue;
            }

            const uint8_t data          = tx_data.value();
            const int     bytes_written = uart_fifo_fill(_device, &data, 1);

            if (bytes_written < 0)
            {
                uart_irq_tx_disable(_device);
                break;
            }

            if (bytes_written == 0)
            {
                break;
            }

            if (bytes_written > 0)
            {
                _tx_buffer->remove();
            }
        }
    }
}
