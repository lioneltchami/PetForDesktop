#pragma once

#include "Engine/ClassUtility.hpp"
#include "Engine/Vector2.hpp"
#include "Engine/Rect.hpp"

class PhysicComponent
{
protected:
    Rect& m_rect;

public:
    [[nodiscard]] inline Rect& getRect() noexcept
    {
        return m_rect;
    }

    [[nodiscard]] inline const Rect& getRect() const noexcept
    {
        return m_rect;
    }

    Vec2  velocity            = {0.f, 0.f};
    Vec2  continuousVelocity  = {0.f, 0.f};
    bool  applyGravity        = true;
    bool  touchScreenEdge     = false;
    bool  isOnBottomOfWindow  = false;
    bool  isGrounded          = false;

    PhysicComponent(Rect& rect) : m_rect{rect}
    {

    }
};
