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
#ifndef ARDUCAM_HANDLER_H
#define ARDUCAM_HANDLER_H

#include "ArducamCamera.h"

#define ARDUCAM_BUFFER_SIZE 1460

class ArducamHandler {
public:
    static ArducamHandler& getInstance();

    bool requestCapture(int8_t resolution, int8_t quality, int8_t brightness, int8_t contrast, int8_t evlevel, int8_t saturation, int8_t sharpness);

    bool isActive() { return m_frameTransferActive; }

    uint32_t readBuffer(uint8_t *buffer, uint32_t size);
    
    
protected:
    ArducamHandler();
    virtual ~ArducamHandler() {};

    ArducamCamera m_camera;

    bool m_frameTransferActive = false;
    bool m_initialized = false;

    int8_t m_resolution = 0;
    int8_t m_quality = 0;
    int8_t m_contrast = 0;
    int8_t m_evlevel = 0;
    int8_t m_saturation = 0;
    int8_t m_sharpness = 0;
    int8_t m_brightness = 0;
};
#endif