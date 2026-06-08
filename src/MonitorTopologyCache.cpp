#include "Engine/MonitorTopologyCache.hpp"

#include "Engine/Monitors.hpp"

#include <chrono>

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
        m_monitors->getMonitorPosition(i, topology.position);
        m_monitors->getMonitorSize(i, topology.size);
        if (topology.size.x == 0 || topology.size.y == 0)
            continue;
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
