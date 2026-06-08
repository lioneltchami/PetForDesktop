#pragma once

#include "Engine/WorldSamplingSubsystem.hpp"

#include "Engine/Vector2.hpp"
#include "Engine/PhysicComponent.hpp"
#include "Engine/InteractionComponent.hpp"
#include "Engine/Rect.hpp"
#include "Game/GameData.hpp"

#include <cfloat>
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

    static bool isRectDisjointRectB(const Vec2 posA, const Vec2 sizeA, const Vec2 posB, const Vec2 sizeB)
    {
        // Check if the rectangles are disjoint (i.e. do not overlap)
        constexpr float kEpsilon = 0.0001f;

        return posA.x + sizeA.x <= posB.x + kEpsilon || posA.x >= posB.x + sizeB.x - kEpsilon ||
               posA.y + sizeA.y <= posB.y + kEpsilon || posA.y >= posB.y + sizeB.y - kEpsilon;
    }

    static bool isRectAInsideRectB(const Vec2 posA, const Vec2 sizeA, const Vec2 posB, const Vec2 sizeB)
    {
        constexpr float kEpsilon = 0.0001f;

        return posA.x >= posB.x - kEpsilon && posA.x + sizeA.x <= posB.x + sizeB.x + kEpsilon &&
               posA.y >= posB.y - kEpsilon && posA.y + sizeA.y <= posB.y + sizeB.y + kEpsilon;
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
            const Vec2 monitorPos{static_cast<float>(monitorItem.position.x), static_cast<float>(monitorItem.position.y)};
            const Vec2 monitorSize{static_cast<float>(monitorItem.size.x), static_cast<float>(monitorItem.size.y)};

            const bool isOutsideOfCurrentMonitor =
                isRectDisjointRectB(comp.getRect().getPosition(), comp.getRect().getSize(), monitorPos, monitorSize);
            const bool isInsideOfCurrentMonitor =
                isRectAInsideRectB(comp.getRect().getPosition(), comp.getRect().getSize(), monitorPos, monitorSize);

            screenOverlapCount += !isInsideOfCurrentMonitor && !isOutsideOfCurrentMonitor;
            isOutside &= isOutsideOfCurrentMonitor;
        }

        // 2: If pet is outside need correction
        Vec2       reelPositionCorrection = comp.getRect().getPosition();
        comp.isOnBottomOfWindow = false;

        // Check if only one screen overlap is not perfect but cover the majority of cases
        comp.touchScreenEdge = isOutside || screenOverlapCount <= 1;
        if (comp.touchScreenEdge)
        {
            float bestDistance = FLT_MAX;
            bool bestOnBottom = false;

            for (const auto& monitorItem : monitorsTopology)
            {
                const Vec2 monitorPos{static_cast<float>(monitorItem.position.x), static_cast<float>(monitorItem.position.y)};
                const Vec2 monitorSize{static_cast<float>(monitorItem.size.x), static_cast<float>(monitorItem.size.y)};

                Vec2 positionCorrection = comp.getRect().getPosition();
                bool isOnBottom         = false;

                if (comp.getRect().getCornerMin().x <= monitorPos.x)
                {
                    positionCorrection.x = monitorPos.x;
                }
                else if (comp.getRect().getCornerMax().x >= monitorPos.x + monitorSize.x)
                {
                    positionCorrection.x = monitorPos.x + monitorSize.x - comp.getRect().getSize().x;
                }

                if (comp.getRect().getCornerMin().y <= monitorPos.y)
                {
                    positionCorrection.y = monitorPos.y;
                }
                else if (comp.getRect().getCornerMax().y >= monitorPos.y + monitorSize.y)
                {
                    positionCorrection.y = monitorPos.y + monitorSize.y - comp.getRect().getSize().y;
                    isOnBottom           = true;
                }

                const float currentSqrDistance = (positionCorrection - comp.getRect().getPosition()).sqrLength();
                if (currentSqrDistance < bestDistance)
                {
                    bestDistance     = currentSqrDistance;
                    reelPositionCorrection = positionCorrection;
                    bestOnBottom     = isOnBottom;
                }
            }

            if (bestDistance < FLT_MAX)
            {
                comp.isOnBottomOfWindow = bestOnBottom;
                comp.velocity =
                    comp.velocity.reflect((reelPositionCorrection - comp.getRect().getPosition()).normalized()) * data.bounciness;

                comp.isGrounded = (comp.isOnBottomOfWindow &&
                                   comp.velocity.sqrLength() < data.isGroundedDetection * data.isGroundedDetection) ||
                                  checkIsGrounded(comp);
                comp.velocity *= !comp.isGrounded; // reset velocity if is grounded

                comp.getRect().setPosition(reelPositionCorrection);
            }
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
        Vec2 localPixelPerMeter = data.pixelPerMeter;
        if (data.worldSampling)
        {
            const Vec2 motionProbePoint = comp.getRect().getPosition() + comp.getRect().getSize() * 0.5f;
            localPixelPerMeter = data.worldSampling->getPixelPerMeterForPosition(motionProbePoint, localPixelPerMeter);
            if (localPixelPerMeter.x <= 0.f || localPixelPerMeter.y <= 0.f ||
                !std::isfinite(localPixelPerMeter.x) || !std::isfinite(localPixelPerMeter.y))
            {
                localPixelPerMeter = data.pixelPerMeter;
            }
        }

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
                                                  localPixelPerMeter * (float)deltaTime);
            
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
