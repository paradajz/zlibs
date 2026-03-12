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
    const zlibs::utils::misc::LockGuard LOCK(_mutex);

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
    const zlibs::utils::misc::LockGuard LOCK(_mutex);
    const unsigned int                  IRQ_KEY = irq_lock();

    if (!_initialized)
    {
        // nothing to do
        irq_unlock(IRQ_KEY);
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
    _rx_buffer   = nullptr;
    _tx_buffer   = nullptr;

    irq_unlock(IRQ_KEY);

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

    const int CALLBACK_RESULT = uart_irq_callback_user_data_set(_device, [](const device*, void* user_data)
                                                                {
                                                                    auto self = static_cast<UartHw*>(user_data);

                                                                    if (self == nullptr)
                                                                    {
                                                                        return;
                                                                    }

                                                                    self->isr();
                                                                },
                                                                this);

    if (CALLBACK_RESULT != 0)
    {
        LOG_ERR("Failed to register UART callback for device %s: %d", _device->name, CALLBACK_RESULT);
        return false;
    }

    _rx_buffer = &config.rx_buffer;
    _tx_buffer = &config.tx_buffer;

    return true;
}

bool UartHw::write(uint8_t data)
{
    const zlibs::utils::misc::LockGuard LOCK(_mutex);

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
    const zlibs::utils::misc::LockGuard LOCK(_mutex);

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
        const int UPDATE_RESULT = uart_irq_update(_device);

        if (UPDATE_RESULT <= 0)
        {
            break;
        }

        const int PENDING_RESULT = uart_irq_is_pending(_device);

        if (PENDING_RESULT <= 0)
        {
            break;
        }

        const int RX_READY = uart_irq_rx_ready(_device);

        if (RX_READY < 0)
        {
            break;
        }

        if (RX_READY > 0)
        {
            while (true)
            {
                uint8_t   data           = 0;
                const int BYTES_RECEIVED = uart_fifo_read(_device, &data, 1);

                if (BYTES_RECEIVED <= 0)
                {
                    break;
                }

                if (!_rx_buffer->insert(data))
                {
                    // overflow, nothing to do?
                }
            }
        }

        const int TX_READY = uart_irq_tx_ready(_device);

        if (TX_READY < 0)
        {
            break;
        }

        if (TX_READY > 0)
        {
            const auto TX_DATA = _tx_buffer->peek();

            if (!TX_DATA.has_value())
            {
                uart_irq_tx_disable(_device);
                continue;
            }

            const uint8_t DATA          = TX_DATA.value();
            const int     BYTES_WRITTEN = uart_fifo_fill(_device, &DATA, 1);

            if (BYTES_WRITTEN < 0)
            {
                uart_irq_tx_disable(_device);
                break;
            }

            if (BYTES_WRITTEN == 0)
            {
                break;
            }

            if (BYTES_WRITTEN > 0)
            {
                _tx_buffer->remove();
            }
        }
    }
}
