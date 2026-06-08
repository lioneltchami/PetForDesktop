#pragma once

#include "Engine/Vector2.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <mutex>
#include <vector>

class Monitors;

struct MonitorTopologyItem
{
    Vec2i position;      // Logical desktop coordinates
    Vec2i size;          // Logical desktop size
    Vec2i pixelPosition; // Physical pixel coordinates
    Vec2i pixelSize;     // Physical pixel size
    Vec2  contentScale = Vec2::one();

    bool containsLogicalPoint(const Vec2& logicalPoint) const
    {
        return logicalPoint.x >= static_cast<float>(position.x) && logicalPoint.x < static_cast<float>(position.x + size.x) &&
               logicalPoint.y >= static_cast<float>(position.y) && logicalPoint.y < static_cast<float>(position.y + size.y);
    }

    Vec2 logicalToPhysical(const Vec2& logicalPoint) const
    {
        const float safeScaleX = std::max(contentScale.x, 0.0001f);
        const float safeScaleY = std::max(contentScale.y, 0.0001f);

        return {(logicalPoint.x - static_cast<float>(position.x)) * safeScaleX + static_cast<float>(pixelPosition.x),
                (logicalPoint.y - static_cast<float>(position.y)) * safeScaleY + static_cast<float>(pixelPosition.y)};
    }

    Vec2 physicalToLogical(const Vec2& pixelPoint) const
    {
        const float safeScaleX = std::max(contentScale.x, 0.0001f);
        const float safeScaleY = std::max(contentScale.y, 0.0001f);

        return {(pixelPoint.x - static_cast<float>(pixelPosition.x)) / safeScaleX + static_cast<float>(position.x),
                (pixelPoint.y - static_cast<float>(pixelPosition.y)) / safeScaleY + static_cast<float>(position.y)};
    }

    bool containsPhysicalPoint(const Vec2& pixelPoint) const
    {
        return containsLogicalPoint(physicalToLogical(pixelPoint));
    }
};

class MonitorTopologyCache
{
protected:
    const Monitors* m_monitors = nullptr;
    Monitors*       m_hotplugMonitors = nullptr;
    std::function<void()> m_topologyChangedCallback;
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

    void attachHotplugBridge(Monitors* monitors, std::function<void()> onTopologyChanged = {});

    void detachHotplugBridge();

    void markDirty();

    bool refreshIfNeeded(double nowSeconds);

    void forceRefresh(double nowSeconds);

    std::vector<MonitorTopologyItem> getSnapshot() const;

    std::optional<MonitorTopologyItem> findMonitorForLogicalPoint(const Vec2& logicalPoint) const;

    std::optional<MonitorTopologyItem> findMonitorForPhysicalPoint(const Vec2& pixelPoint) const;
};
