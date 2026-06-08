#pragma once

#include "Engine/WorldSamplingSubsystem.hpp"

#include "Engine/Vector2.hpp"
#include "Engine/PhysicComponent.hpp"
#include "Engine/InteractionComponent.hpp"
#include "Engine/Rect.hpp"
#include "Game/GameData.hpp"

#include <cmath>

class PhysicSystem
{
protected:
    GameData& data;

public:
    PhysicSystem(GameData& data) : data{data}
    {
    }

    bool checkIsGrounded(const PhysicComponent& comp)
    {
        float velocityLength     = comp.velocity.length();

        if (velocityLength < FLT_EPSILON)
            return true;

        float dotGravityVelocity = (- data.gravityDir).dot(comp.velocity / velocityLength);
        return dotGravityVelocity > 0.8 && velocityLength < data.isGroundedDetection;
    }

    static bool isRectDisjointRectB(const Vec2i posA, const Vec2i sizeA, const Vec2i posB, const Vec2i sizeB)
    {
        // Check if the rectangles are disjoint (i.e. do not overlap)
        return posA.x + sizeA.x < posB.x || posA.x > posB.x + sizeB.x || posA.y + sizeA.y < posB.y ||
               posA.y > posB.y + sizeB.y;
    }

    static bool isRectAInsideRectB(const Vec2i posA, const Vec2i sizeA, const Vec2i posB, const Vec2i sizeB)
    {
        return posA.x > posB.x && posA.x + sizeA.x < posB.x + sizeB.x && posA.y > posB.y && posA.y + sizeA.y < posB.y + sizeB.y;
    }

    void computeMonitorCollisions(PhysicComponent& comp)
    {
        const std::vector<MonitorTopologyItem> monitorsTopology =
            data.worldSampling ? data.worldSampling->getMonitorTopologySnapshot() : std::vector<MonitorTopologyItem>{};
        if (monitorsTopology.empty())
            return;
        bool               isOutside          = true;
        int                screenOverlapCount = 0;

        // 1: Check if pet is outside of all monitors
        for (const auto& monitorItem : monitorsTopology)
        {
            bool isOutsideOfCurrentMonitor =
                isRectDisjointRectB(comp.getRect().getPosition(), comp.getRect().getSize(), monitorItem.position, monitorItem.size);
            bool iInsideOfCurrentMonitor =
                isRectAInsideRectB(comp.getRect().getPosition(), comp.getRect().getSize(), monitorItem.position, monitorItem.size);

            screenOverlapCount += !iInsideOfCurrentMonitor && !isOutsideOfCurrentMonitor;

            isOutside &= isOutsideOfCurrentMonitor;
        }

        // 2: If pet is outside need correction
        float minSqrDistance    = FLT_MAX;
        comp.isOnBottomOfWindow = false;
        Vec2 reelPositionCorrection;

        // Check if only one screen overlap is not perfect but cover the majority of cases
        comp.touchScreenEdge = isOutside || screenOverlapCount == 1;
        if (comp.touchScreenEdge)
        {
        for (const auto& monitorItem : monitorsTopology)
        {
            Vec2 positionCorrection = comp.getRect().getPosition();
            bool isOnBottom         = false;

            if (comp.getRect().getCornerMin().x <= monitorItem.position.x)
            {
                positionCorrection.x = monitorItem.position.x;
            }
            else if (comp.getRect().getCornerMax().x >= monitorItem.position.x + monitorItem.size.x)
            {
                positionCorrection.x = monitorItem.position.x + monitorItem.size.x - comp.getRect().getSize().x;
            }

            if (comp.getRect().getCornerMin().y <= monitorItem.position.y)
            {
                positionCorrection.y = monitorItem.position.y;
            }
            else if (comp.getRect().getCornerMax().y >= monitorItem.position.y + monitorItem.size.y)
            {
                positionCorrection.y = monitorItem.position.y + monitorItem.size.y - comp.getRect().getSize().y;
                isOnBottom           = true;
            }

                float currentSqrDistance = (positionCorrection - comp.getRect().getPosition()).sqrLength();
                if (currentSqrDistance < minSqrDistance)
                {
                    comp.isOnBottomOfWindow = isOnBottom;
                    minSqrDistance          = currentSqrDistance;
                    reelPositionCorrection  = positionCorrection;
                }
            }

            if (minSqrDistance > FLT_EPSILON)
                comp.velocity =
                    comp.velocity.reflect((reelPositionCorrection - comp.getRect().getPosition()).normalized()) * data.bounciness;

            comp.isGrounded = (comp.isOnBottomOfWindow &&
                               comp.velocity.sqrLength() < data.isGroundedDetection * data.isGroundedDetection) ||
                              checkIsGrounded(comp);
            comp.velocity *= !comp.isGrounded; // reset velocity if is grounded

            comp.getRect().setPosition(reelPositionCorrection);
        }
    }

    bool CatpureScreenCollision(const PhysicComponent& comp, const Vec2 prevToNewWinPos, Vec2& newPos)
    {
        if (!data.worldSampling)
            return false;

        return data.worldSampling->checkSurfaceCollision(const_cast<PhysicComponent&>(comp), prevToNewWinPos, newPos, data);
    }

    void update(PhysicComponent& comp, InteractionComponent& interactionComp, double deltaTime)
    {
        // Apply gravity if not selected
        if (interactionComp.isLeftSelected)
        {
            Vec2 movement = {data.deltaCursorPosX, data.deltaCursorPosY};
            comp.getRect().setPosition(comp.getRect().getPosition() + movement);

            data.deltaCursorPosX = 0;
            data.deltaCursorPosY = 0;
        }
        else
        {
            // Acc = Sum of force / Mass
            // G is already an acceleration
            const Vec2 acc = data.gravity * comp.applyGravity * !comp.isGrounded;

            // V = Acc * Time
            comp.velocity += acc * (float)deltaTime;

            const Vec2 prevWinPos = comp.getRect().getPosition();
            // Pos = PrevPos + V * Time
            const Vec2 newWinPos = comp.getRect().getPosition() + ((comp.continuousVelocity + comp.velocity) * (1.f - data.friction) *
                                                  data.pixelPerMeter * (float)deltaTime);
            
            const Vec2 prevToNewWinPos = newWinPos - prevWinPos;
            const float sqrDistMovement    = prevToNewWinPos.sqrLength();
            if ((sqrDistMovement <= data.continuousCollisionMaxSqrVelocity && prevToNewWinPos.y > 0.f) ||
                data.debugEdgeDetection)
            {
                Vec2 newPos;
                if (CatpureScreenCollision(comp, prevToNewWinPos, newPos))
                {
                    Vec2 collisionPos = newPos;
                    comp.getRect().setPosition(collisionPos);
                    comp.velocity     = comp.velocity.reflect(Vec2::up()) * data.bounciness;

                    // check if is grounded
                    comp.isGrounded = checkIsGrounded(comp);
                    comp.velocity *= !comp.isGrounded; // reset velocity if is grounded
                }
                else
                {
                    comp.getRect().setPosition(newWinPos);
                }
            }
            else
            {
                // Update is grounded
                if (comp.isGrounded && !comp.isOnBottomOfWindow)
                {
                    Vec2 newPos;
                    Vec2 footBasement((float)data.footBasementWidth, (float)data.footBasementHeight);
                    comp.isGrounded = CatpureScreenCollision(comp, footBasement, newPos);
                }

                comp.getRect().setPosition(newWinPos);
            }

            // Apply monitor collision
            if (sqrDistMovement > FLT_EPSILON)
                computeMonitorCollisions(comp);
        }
    }
};
