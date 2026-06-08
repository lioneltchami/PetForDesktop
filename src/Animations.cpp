#include "Game/Animations.hpp"
#include "Game/Pet.hpp"

void PetJumpNode::onUpdate(GameData& blackBoard, double dt)
{
    AnimationNode::onUpdate(blackBoard, dt);

    if (spriteAnimator.isDone()) // Enter only for jump begin because don't loop.
    {
        AnimationMotionLogic::applyJumpImpulse(pet.getPhysicComponent(), baseDir, static_cast<int>(pet.getSide()), vThrust,
                                               hThrust, blackBoard.gravity);
    }
}

void GrabNode::onEnter(GameData& blackBoard)
{
    AnimationNode::onEnter(blackBoard);
    pet.setIsGrab(true);
}

void GrabNode::onExit(GameData& blackBoard)
{
    AnimationNode::onExit(blackBoard);
    pet.setIsGrab(false);
}

void MovementDirectionNode::onEnter(GameData& blackBoard)
{
    AnimationNode::onEnter(blackBoard);
    baseDir = AnimationMotionLogic::pickDirection(directions);
    pet.setSide(AnimationMotionLogic::shouldFaceRight(baseDir) ? Pet::ESide::right : Pet::ESide::left);
    AnimationMotionLogic::applyMovementEnter(pet.getPhysicComponent(), baseDir, applyGravity);
}

void MovementDirectionNode::onExit(GameData& blackBoard)
{
    AnimationNode::onExit(blackBoard);
    AnimationMotionLogic::applyMovementExit(pet.getPhysicComponent(), baseDir);
}
