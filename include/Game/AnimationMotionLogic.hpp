#pragma once

#include "Engine/PhysicComponent.hpp"
#include "Engine/Utilities.hpp"
#include "Engine/Vector2.hpp"

#include <vector>

namespace AnimationMotionLogic
{
inline Vec2 pickDirection(const std::vector<Vec2>& directions)
{
    if (directions.empty())
        return Vec2::zero();

    return directions[static_cast<std::size_t>(randNum(0, static_cast<int>(directions.size()) - 1))];
}

inline bool shouldFaceRight(const Vec2& baseDir)
{
    return baseDir.dot(Vec2::right()) > 0.f;
}

inline void applyMovementEnter(PhysicComponent& comp, const Vec2& baseDir, bool applyGravity)
{
    comp.applyGravity = applyGravity;
    comp.continuousVelocity += baseDir;
}

inline void applyMovementExit(PhysicComponent& comp, const Vec2& baseDir)
{
    comp.applyGravity = true;
    comp.continuousVelocity -= baseDir;
}

inline void applyJumpImpulse(PhysicComponent& comp, const Vec2& baseDir, int sideIndex, float verticalThrust,
                             float horizontalThrust, const Vec2& gravity)
{
    comp.velocity += baseDir * ((static_cast<float>(sideIndex) * 2.f) - 1.f) * horizontalThrust - gravity * verticalThrust;
    comp.isGrounded = false;
}
} // namespace AnimationMotionLogic
