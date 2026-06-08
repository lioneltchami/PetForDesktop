#pragma once

#include "Engine/Vector2.hpp"

#include <mutex>
#include <vector>

class Monitors;

struct MonitorTopologyItem
{
    Vec2i position;
    Vec2i size;
};

class MonitorTopologyCache
{
protected:
    const Monitors* m_monitors = nullptr;
    Monitors*       m_hotplugMonitors = nullptr;
    mutable std::mutex m_mutex;
    std::vector<MonitorTopologyItem> m_monitorsSnapshot;
    double m_sampleIntervalSeconds;
    double m_lastSampleTime;
    bool m_isDirty;

    void sampleNow();

public:
    explicit MonitorTopologyCache(const Monitors* monitors = nullptr, double sampleIntervalSeconds = 1.0 / 10.0)
        : m_monitors(monitors),
          m_sampleIntervalSeconds(sampleIntervalSeconds),
          m_lastSampleTime(0.0),
          m_isDirty(true)
    {
    }

    void bindMonitors(const Monitors* monitors);

    void attachHotplugBridge(Monitors* monitors);

    void detachHotplugBridge();

    void markDirty();

    bool refreshIfNeeded(double nowSeconds);

    void forceRefresh(double nowSeconds);

    std::vector<MonitorTopologyItem> getSnapshot() const;
};
