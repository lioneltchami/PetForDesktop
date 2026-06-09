#pragma once

#include "Engine/Rect.hpp"
#include "Engine/ClassUtility.hpp"
#include <functional>

struct InteractionComponent
{
protected:
    Rect& m_rect;

public:

    std::function<void()> onMouseOver;
    std::function<void()> onLeftPressOver;
    std::function<void()> onLeftReleaseOver;
    std::function<void()> onRightPressOver;
    std::function<void()> onRightReleaseOver;

    bool isLeftSelected   = false;
    bool isRightSelected  = false;

    // One frame
    bool isMouseOver      = false;
    bool isLeftPressOver  = false;
    bool isLeftRelease    = false;
    bool isRightPressOver = false;
    bool isRightRelease   = false;

public:
    [[nodiscard]] inline Rect& getRect() noexcept
    {
        return m_rect;
    }

    [[nodiscard]] inline const Rect& getRect() const noexcept
    {
        return m_rect;
    }

    InteractionComponent(Rect& rect) : m_rect{rect}
    {
    }
};
