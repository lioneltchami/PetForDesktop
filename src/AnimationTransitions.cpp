#include "Game/AnimationTransitions.hpp"
#include "Engine/Utilities.hpp"
#include "Game/Animations.hpp"
#include "Game/Pet.hpp"

bool IsGroundedTransition::canTransition(GameData& blackBoard)
{
    return AnimationTransitionLogic::shouldTransitionWhenGrounded(pet.getPhysicComponent().isGrounded);
}

bool IsNotGroundedTransition::canTransition(GameData& blackBoard)
{
    return AnimationTransitionLogic::shouldTransitionWhenNotGrounded(pet.getPhysicComponent().isGrounded);
}

bool AnimationEndTransition::canTransition(GameData& blackBoard)
{
    return AnimationTransitionLogic::shouldTransitionWhenAnimationDone(static_cast<AnimationNode*>(pOwner)->IsAnimationDone());
}

bool StartLeftClicTransition::canTransition(GameData& blackBoard)
{
    return AnimationTransitionLogic::shouldTransitionOnLeftPressOver(pet.getInteractionComponent().isLeftPressOver);
};

bool TouchScreenEdgeTransition::canTransition(GameData& blackBoard)
{
    return AnimationTransitionLogic::shouldTransitionOnTouchScreenEdge(pet.getPhysicComponent().touchScreenEdge);
}

void EndLeftClicTransition::onEnter(GameData& blackBoard)
{
    leftWasPressed = pet.getInteractionComponent().isLeftPressOver;
};

bool EndLeftClicTransition::canTransition(GameData& blackBoard)
{
    return AnimationTransitionLogic::updateEndLeftClickState(pet.getInteractionComponent().isLeftPressOver,
                                                              blackBoard.leftButtonEvent, leftWasPressed);
}
