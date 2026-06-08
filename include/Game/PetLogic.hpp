#pragma once

#include "Engine/PhysicComponent.hpp"
#include "Engine/StateMachine.hpp"
#include "Engine/Vector2.hpp"
#include "Game/GameData.hpp"

namespace PetLogic
{
Vec2 computeReleaseVelocity(const GameData& datas, const Vec2& deltaCursorAcc, const Vec2& localPixelPerMeter);
void applyPauseState(bool flag, bool& isPaused, StateMachine& animator, const std::shared_ptr<StateMachine::Node>& pauseNode,
                     const std::shared_ptr<StateMachine::Node>& firstNode, PhysicComponent& physicComponent);
} // namespace PetLogic
