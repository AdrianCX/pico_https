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

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "arducam_handler.h"
#include "request_handler.h"
#include "pico_logger.h"

#include "general_config.h"

ArducamHandler& ArducamHandler::getInstance() {
    static ArducamHandler instance;
    return instance;
}

ArducamHandler::ArducamHandler()
{
}


bool ArducamHandler::requestCapture(int8_t resolution, int8_t quality, int8_t brightness, int8_t contrast, int8_t evlevel, int8_t saturation, int8_t sharpness)
{
    trace("ArducamHandler::requestCapture()", this);
    if (m_frameTransferActive)
    {
        return true;
    }

    if (!m_created)
    {
        m_camera = createArducamCamera(CAMERA_CS_PIN);
        begin(&m_camera);
        
        m_created = true;
    }
    
    if (!m_initialized || quality != m_quality)
    {
        if (setImageQuality(&m_camera, (IMAGE_QUALITY)quality) == -1)
        {
            trace("ArducamHandler::requestCapture: this=%p, failed setting image quality: %d", this, quality);
            return false;
        }
        m_quality = quality;
    }

    if (!m_initialized || brightness != m_brightness)
    {
        if (setBrightness(&m_camera, (CAM_BRIGHTNESS_LEVEL)brightness) == -1)
        {
            trace("ArducamHandler::requestCapture: this=%p, failed setting brightness: %d", this, brightness);
            return false;
        }
        m_brightness = brightness;
    }

    if (!m_initialized || contrast != m_contrast)
    {
        if (setContrast(&m_camera, (CAM_CONTRAST_LEVEL)contrast) == -1)
        {
            trace("ArducamHandler::requestCapture: this=%p, failed setting contrast: %d", this, contrast);
            return false;
        }
        m_contrast = contrast;
    }

    if (!m_initialized || evlevel != m_evlevel)
    {
        if (setEV(&m_camera, (CAM_EV_LEVEL)evlevel) == -1)
        {
            trace("ArducamHandler::requestCapture: this=%p, failed setting EV level: %d", this, evlevel);
            return false;
        }

        m_evlevel = evlevel;
    }

    if (!m_initialized || saturation != m_saturation)
    {
        if (setSaturation(&m_camera, (CAM_STAURATION_LEVEL)saturation) == -1)
        {
            trace("ArducamHandler::requestCapture: this=%p, failed setting saturation: %d", this, saturation);
            return false;
        }
        m_saturation = saturation;
    }

    if (!m_initialized || sharpness != m_sharpness)
    {
        if (setSharpness(&m_camera, (CAM_SHARPNESS_LEVEL)sharpness) == -1)
        {
            trace("ArducamHandler::requestCapture: this=%p, failed setting sharpness: %d", this, sharpness);
            return false;
        }

        m_sharpness = sharpness;
    }

    if (takePicture(&m_camera, (CAM_IMAGE_MODE)resolution, CAM_IMAGE_PIX_FMT_JPG) == -1)
    {
        trace("ArducamHandler::requestCapture: this=%p, failed requesting picture with resolution %d", this, resolution);
        return false;
    }

    m_initialized = true;
    m_frameTransferActive = true;
    return true;
}

uint32_t ArducamHandler::readBuffer(uint8_t *buffer, uint32_t size)
{
    uint32_t rlen = 0;
    
    int bytesToRead = m_camera.receivedLength < size ? m_camera.receivedLength : size;
    if (bytesToRead > 0)
    {
        rlen = readBuff(&m_camera, buffer, bytesToRead);
    }

    m_frameTransferActive = (m_camera.receivedLength != 0);
    return rlen;
}
