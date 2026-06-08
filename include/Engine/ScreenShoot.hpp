#pragma once

#include "Engine/Platform/IScreenCapture.hpp"
#include "Engine/Platform/PlatformServices.hpp"

#include <memory>

class ScreenShoot
{
protected:
    IScreenCapture::Data        data;
    std::unique_ptr<IScreenCapture> m_capture;

public:
    using Data = IScreenCapture::Data;

    ScreenShoot(int x, int y, int w, int h, bool saveIntoClipboard = false)
    {
        m_capture = PlatformServices::createScreenCapture();
        if (m_capture)
            data = m_capture->capture(x, y, w, h, saveIntoClipboard);
    }

    ~ScreenShoot()
    {
        if (m_capture)
            m_capture->release(data);
    }

    const Data& get()
    {
        return data;
    }
};
