/* MIT License

Copyright (c) 2024 Adrian Cruceru - https://github.com/AdrianCX/pico_https

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <string.h>
#include <time.h>
#include <atomic>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "pico_logger.h"
#include "general_config.h"

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "analog_microphone_handler.h"

#define CAPTURE_DEPTH 1024
#define CAPTURE_RING_BITS 11

uint16_t __not_in_flash("adc") capture_buf1[CAPTURE_DEPTH] __attribute__((aligned(2048)));
uint16_t __not_in_flash("adc") capture_buf2[CAPTURE_DEPTH] __attribute__((aligned(2048)));

volatile uint32_t counter_dma1 = 0;
volatile uint32_t counter_dma2 = 0;

int dma_chan1 = 0;
int dma_chan2 = 0;

void dma_handler1() {
    ++counter_dma1;
    // Clear the interrupt request.
    dma_hw->ints0 = 1u << dma_chan1;
}

void dma_handler2() {
    ++counter_dma2;

    dma_hw->ints0 = 1u << dma_chan2;
}

AnalogMicrophoneHandler& AnalogMicrophoneHandler::getInstance() {
    static AnalogMicrophoneHandler instance;
    return instance;
}

void AnalogMicrophoneHandler::registerHandler(MicrophoneCallback *callback)
{
    trace("AnalogMicrophoneHandler::registerHandler: callback=%p\n", callback);
    m_callbacks.insert(callback);
}
void AnalogMicrophoneHandler::unregisterHandler(MicrophoneCallback *callback)
{
    trace("AnalogMicrophoneHandler::unregisterHandler: callback=%p\n", callback);
    m_callbacks.erase(callback);
}

bool AnalogMicrophoneHandler::start()
{
    trace("AnalogMicrophoneHandler::start: this=%p, m_active=%d\n", this, m_active);
    if (m_active)
    {
        return true;
    }

    m_active = true;

    adc_gpio_init(ANALOG_MICROPHONE_GPIO);
    adc_init();
    adc_select_input(ANALOG_MICROPHONE_GPIO - 26);

    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
        false    // Don't shift each sample to 8 bits when pushing to FIFO
    );

    float clk_div = (clock_get_hz(clk_adc) / (1.0 * SAMPLE_RATE)) - 1;
    adc_set_clkdiv(clk_div);

    adc_run(true);

    // Use two DMAs chained to avoid reconfiguring DMA on interrupt.
    dma_chan1 = dma_claim_unused_channel(true);
    dma_chan2 = dma_claim_unused_channel(true);

    {
        dma_channel_config dma_channel_cfg = dma_channel_get_default_config(dma_chan2);

        channel_config_set_transfer_data_size(&dma_channel_cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&dma_channel_cfg, false);
        channel_config_set_write_increment(&dma_channel_cfg, true);

        channel_config_set_ring(&dma_channel_cfg, true, CAPTURE_RING_BITS);
        channel_config_set_dreq(&dma_channel_cfg, DREQ_ADC);

        channel_config_set_chain_to(&dma_channel_cfg, dma_chan1);

        dma_channel_set_irq1_enabled(dma_chan2, true);
        irq_set_exclusive_handler(DMA_IRQ_1, dma_handler2);
        irq_set_enabled(DMA_IRQ_1, true);

        dma_channel_configure(dma_chan2, &dma_channel_cfg, capture_buf2, &adc_hw->fifo, CAPTURE_DEPTH, false);
    }

    {
        dma_channel_config dma_channel_cfg = dma_channel_get_default_config(dma_chan1);
        
        channel_config_set_transfer_data_size(&dma_channel_cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&dma_channel_cfg, false);
        channel_config_set_write_increment(&dma_channel_cfg, true);
        
        channel_config_set_ring(&dma_channel_cfg, true, CAPTURE_RING_BITS);
        channel_config_set_dreq(&dma_channel_cfg, DREQ_ADC);

        channel_config_set_chain_to(&dma_channel_cfg, dma_chan2);
        
        irq_set_enabled(DMA_IRQ_0, true);
        irq_set_exclusive_handler(DMA_IRQ_0, dma_handler1);
        dma_channel_set_irq0_enabled(dma_chan1, true);

        dma_channel_configure(dma_chan1, &dma_channel_cfg, &capture_buf1[0], &adc_hw->fifo, CAPTURE_DEPTH, true);
    }

    return true;
}


void AnalogMicrophoneHandler::stop()
{
    if (m_active)
    {
        // Atomically abort both channels.
        dma_hw->abort = (1 << dma_chan1) | (1 << dma_chan2);

        // Wait for all aborts to complete
        while (dma_hw->abort) tight_loop_contents();

        // Wait for channels to not be busy.
        while (dma_hw->ch[dma_chan1].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS) tight_loop_contents();
        while (dma_hw->ch[dma_chan2].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS) tight_loop_contents();
    }
}

void AnalogMicrophoneHandler::update()
{
    uint32_t last_dma1 = counter_dma1;

    if (last_dma1 != m_dma1)
    {
        m_dma1 = last_dma1;
        memcpy(&m_samples[0], &capture_buf1[0], CAPTURE_DEPTH*2);
        reportBuffer();
    }

    uint32_t last_dma2 = counter_dma2;
    if (last_dma2 != m_dma2)
    {
        m_dma2 = last_dma2;
        memcpy(&m_samples[0], &capture_buf2[0], CAPTURE_DEPTH*2);
        reportBuffer();
    }
}

void AnalogMicrophoneHandler::reportBuffer()
{
    for (int i=0;i<CAPTURE_DEPTH;++i)
    {
        m_samples[i] = m_samples[i] - m_bias;
    }
    
    auto callbacks = m_callbacks;
    for (auto i : callbacks)
    {
        if (!i->audio_callback(&m_samples[0], CAPTURE_DEPTH))
        {
            unregisterHandler(i);
        }
    }
}
