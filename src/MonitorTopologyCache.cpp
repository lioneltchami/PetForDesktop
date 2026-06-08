#include "Engine/MonitorTopologyCache.hpp"

#include "Engine/Monitors.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
double nowSeconds()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}
}

void MonitorTopologyCache::bindMonitors(const Monitors* monitors)
{
    std::lock_guard lock{m_mutex};
    m_monitors = monitors;
    m_isDirty   = true;
}

void MonitorTopologyCache::attachHotplugBridge(Monitors* monitors, std::function<void()> onTopologyChanged)
{
    detachHotplugBridge();
    bindMonitors(monitors);
    {
        std::lock_guard lock{m_mutex};
        m_topologyChangedCallback = std::move(onTopologyChanged);
    }

    if (!monitors)
        return;

    monitors->setTopologyChangedCallback([this]() {
        forceRefresh(nowSeconds());
        std::function<void()> callback;
        {
            std::lock_guard lock{m_mutex};
            callback = m_topologyChangedCallback;
        }
        if (callback)
            callback();
    });
    m_hotplugMonitors = monitors;
}

void MonitorTopologyCache::detachHotplugBridge()
{
    Monitors* callbackMonitors = nullptr;
    {
        std::lock_guard lock{m_mutex};
        callbackMonitors      = m_hotplugMonitors;
        m_hotplugMonitors     = nullptr;
        m_topologyChangedCallback = nullptr;
        m_isDirty                 = true;
    }

    if (callbackMonitors)
        callbackMonitors->clearTopologyChangedCallback();
}

void MonitorTopologyCache::markDirty()
{
    std::lock_guard lock{m_mutex};
    m_isDirty = true;
}

void MonitorTopologyCache::sampleNow()
{
    m_monitorsSnapshot.clear();
    if (!m_monitors)
        return;

    const int monitorCount = m_monitors->getMonitorsCount();
    if (monitorCount <= 0)
        return;

    m_monitorsSnapshot.reserve(monitorCount);
    for (int i = 0; i < monitorCount; ++i)
    {
        MonitorTopologyItem topology;
        m_monitors->getMonitorPixelPosition(i, topology.pixelPosition);
        m_monitors->getMonitorPixelSize(i, topology.pixelSize);
        m_monitors->getMonitorScale(i, topology.contentScale);

        const float safeScaleX = std::max(topology.contentScale.x, 0.0001f);
        const float safeScaleY = std::max(topology.contentScale.y, 0.0001f);
        if (topology.pixelSize.x <= 0 || topology.pixelSize.y <= 0)
            continue;

        const int logicalLeft = static_cast<int>(std::floor(static_cast<float>(topology.pixelPosition.x) / safeScaleX));
        const int logicalTop  = static_cast<int>(std::floor(static_cast<float>(topology.pixelPosition.y) / safeScaleY));
        const int logicalRight = static_cast<int>(std::ceil(
            static_cast<float>(topology.pixelPosition.x + topology.pixelSize.x) / safeScaleX));
        const int logicalBottom = static_cast<int>(std::ceil(
            static_cast<float>(topology.pixelPosition.y + topology.pixelSize.y) / safeScaleY));

        topology.position.x = logicalLeft;
        topology.position.y = logicalTop;
        topology.size.x     = std::max(1, logicalRight - logicalLeft);
        topology.size.y     = std::max(1, logicalBottom - logicalTop);

        if (topology.contentScale.x <= 0.f || topology.contentScale.y <= 0.f)
            topology.contentScale = Vec2::one();

        m_monitorsSnapshot.emplace_back(topology);
    }
}

bool MonitorTopologyCache::refreshIfNeeded(double nowSeconds)
{
    std::lock_guard lock{m_mutex};
    if (!m_isDirty && m_sampleIntervalSeconds > 0.0 && nowSeconds - m_lastSampleTime < m_sampleIntervalSeconds)
        return false;

    sampleNow();
    m_lastSampleTime = nowSeconds;
    m_isDirty        = false;
    return true;
}

void MonitorTopologyCache::forceRefresh(double nowSeconds)
{
    markDirty();
    refreshIfNeeded(nowSeconds);
}

std::vector<MonitorTopologyItem> MonitorTopologyCache::getSnapshot() const
{
    std::lock_guard lock{m_mutex};
    return m_monitorsSnapshot;
}

std::optional<MonitorTopologyItem> MonitorTopologyCache::findMonitorForLogicalPoint(const Vec2& logicalPoint) const
{
    std::lock_guard lock{m_mutex};
    if (m_monitorsSnapshot.empty())
        return std::nullopt;

    for (const auto& monitor : m_monitorsSnapshot)
    {
        if (monitor.containsLogicalPoint(logicalPoint))
            return monitor;
    }

    return std::nullopt;
}

std::optional<MonitorTopologyItem> MonitorTopologyCache::findMonitorForPhysicalPoint(const Vec2& pixelPoint) const
{
    std::lock_guard lock{m_mutex};
    if (m_monitorsSnapshot.empty())
        return std::nullopt;

    for (const auto& monitor : m_monitorsSnapshot)
    {
        if (monitor.containsPhysicalPoint(pixelPoint))
            return monitor;
    }

    return std::nullopt;
}
