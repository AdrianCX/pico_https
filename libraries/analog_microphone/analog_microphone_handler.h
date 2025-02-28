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
#ifndef ANALOG_MICROPHONE_HANDLER_H
#define ANALOG_MICROPHONE_HANDLER_H

#include <set>

#include "general_config.h"

#define CAPTURE_DEPTH 1024
#define CAPTURE_RING_BITS 11

class MicrophoneCallback
{
public:
    virtual bool audio_callback(int16_t *samples, size_t size) = 0;
};

class AnalogMicrophoneHandler {
public:
    static AnalogMicrophoneHandler& getInstance();

    bool start();
    void stop();

    void registerHandler(MicrophoneCallback *callback);
    void unregisterHandler(MicrophoneCallback *callback);

    void update();
    
protected:
    virtual ~AnalogMicrophoneHandler() {};
    void reportBuffer();

    bool m_active = false;
    std::set<MicrophoneCallback *> m_callbacks;
    int16_t m_samples[CAPTURE_DEPTH] = {0};

    uint32_t m_dma1 = 0;
    uint32_t m_dma2 = 0;

    static constexpr float m_biasVoltage = 1.25;
    static constexpr int16_t m_bias = ((int16_t)((m_biasVoltage * 4095) / 3.3));

};
#endif
