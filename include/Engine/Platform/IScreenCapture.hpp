#pragma once

#include <cstdint>

class IScreenCapture
{
public:
    struct Data
    {
        std::uint32_t width       = 0;
        std::uint32_t height      = 0;
        std::uint32_t bitPerPixel = 0;
        void*        bits        = nullptr;
    };

    virtual ~IScreenCapture() = default;

    virtual Data capture(int x, int y, int width, int height, bool saveIntoClipboard = false) = 0;

    virtual void release(Data& data) = 0;
};
