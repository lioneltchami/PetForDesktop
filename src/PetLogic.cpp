#include "Game/PetLogic.hpp"

#include <cmath>

namespace PetLogic
{
Vec2 computeReleaseVelocity(const GameData& datas, const Vec2& deltaCursorAcc, const Vec2& localPixelPerMeter)
{
    if (datas.coyoteTimeCursorPos <= 0.f || localPixelPerMeter.x <= 0.f || localPixelPerMeter.y <= 0.f ||
        !std::isfinite(localPixelPerMeter.x) || !std::isfinite(localPixelPerMeter.y))
    {
        return Vec2::zero();
    }

    return deltaCursorAcc / datas.coyoteTimeCursorPos / localPixelPerMeter * datas.releaseImpulse;
}

void applyPauseState(bool flag, bool& isPaused, StateMachine& animator, const std::shared_ptr<StateMachine::Node>& pauseNode,
                     const std::shared_ptr<StateMachine::Node>& firstNode, PhysicComponent& physicComponent)
{
    if (isPaused == flag)
        return;

    isPaused = flag;
    const std::shared_ptr<StateMachine::Node>& targetNode = isPaused ? pauseNode : firstNode;
    assert(targetNode != nullptr);

    animator.setCurrent(targetNode);
    if (animator.getCurrent())
        animator.getCurrent()->canUseTransition = !isPaused;

    physicComponent.velocity           = {0.f, 0.f};
    physicComponent.continuousVelocity = {0.f, 0.f};
}
} // namespace PetLogic
