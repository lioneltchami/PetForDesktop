#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>

#include "Engine/MonitorTopologyCache.hpp"
#include "Engine/PhysicComponent.hpp"
#include "Engine/Vector2.hpp"

class GameData;

class WorldSamplingSubsystem
{
protected:
    struct SurfaceCollisionSample
    {
        std::vector<unsigned char> pixels;
        Vec2i capturePosition = Vec2i::zero();
        int   captureWidth    = 0;
        int   captureHeight   = 0;
        Vec2  captureScale    = Vec2::one();
        int   channels        = 0;
        bool  valid           = false;
        double lastSampleTime  = 0.0;
    };

struct MonitorTopologyState
    {
        std::vector<MonitorTopologyItem> monitorSnapshot;
    };

    double                                 m_surfaceSampleIntervalSeconds;
    MonitorTopologyCache*                  m_monitorTopology;
    mutable std::mutex                     m_mutex;
    MonitorTopologyState                   m_topologyState;
    std::unordered_map<const PhysicComponent*, SurfaceCollisionSample> m_surfaceSamples;

    bool refreshCollisionSample(GameData& data, const PhysicComponent& comp, Vec2 prevToNewWinPos, double nowSeconds);

    bool testCollisionWithCachedSurface(const SurfaceCollisionSample& sample, const PhysicComponent& comp,
                                       Vec2 prevToNewWinPos, Vec2& newPos, const GameData& data) const;

public:
    explicit WorldSamplingSubsystem(MonitorTopologyCache* monitorTopology = nullptr,
                                   double surfaceSampleIntervalSeconds = 1.0 / 12.0);

    void setMonitorTopologyCache(MonitorTopologyCache* monitorTopology);

    void update(double nowSeconds);

    void setSurfaceSampleInterval(double intervalSeconds);

    bool hasValidSample(const PhysicComponent& comp) const;
    void onMonitorTopologyChanged();

    Vec2 getMonitorScaleForPosition(const Vec2& position, const Vec2 defaultScale = Vec2::one()) const;

    std::vector<MonitorTopologyItem> getMonitorTopologySnapshot() const;

    bool checkSurfaceCollision(PhysicComponent& comp, Vec2 prevToNewWinPos, Vec2& newPos, GameData& data);
};
