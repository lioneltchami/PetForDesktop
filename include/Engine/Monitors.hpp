#pragma once

#include "Engine/Platform/IWindowEnumerator.hpp"
#include "Engine/Platform/PlatformServices.hpp"
#include "Engine/Vector2.hpp"

#include <cmath>
#include <functional>
#include <memory>
#include <mutex>

class Monitors
{
protected:
    std::unique_ptr<IWindowEnumerator> m_enumerator;
    std::function<void()>              m_onTopologyChanged;
    mutable std::mutex                 m_mutex;

public:
    Monitors() : m_enumerator(PlatformServices::createWindowEnumerator())
    {
    }

    explicit Monitors(std::unique_ptr<IWindowEnumerator> enumerator) : m_enumerator(std::move(enumerator))
    {
    }

    void setTopologyChangedCallback(std::function<void()> callback)
    {
        std::lock_guard lock{m_mutex};
        m_onTopologyChanged = std::move(callback);
    }

    void setImplementation(std::unique_ptr<IWindowEnumerator> enumerator)
    {
        std::lock_guard lock{m_mutex};
        m_enumerator = std::move(enumerator);
    }

    void init()
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            m_enumerator = PlatformServices::createWindowEnumerator();
        if (m_enumerator)
            m_enumerator->init();
    }

    void onMonitorConnectionChanged(void* monitor, int event)
    {
        std::function<void()> callback;
        {
            std::lock_guard lock{m_mutex};
            if (!m_enumerator)
            {
                m_enumerator = PlatformServices::createWindowEnumerator();
                if (!m_enumerator)
                    return;
            }

            m_enumerator->onMonitorConnectionChanged(monitor, event);
            callback = m_onTopologyChanged;
        }

        if (callback)
            callback();
    }

    void clearTopologyChangedCallback() noexcept
    {
        std::lock_guard lock{m_mutex};
        m_onTopologyChanged = nullptr;
    }

    Vec2i getMainMonitorWorkingArea(Vec2i& position, Vec2i& size) const
    {
        position = Vec2i::zero();
        size     = Vec2i::zero();
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return Vec2i::zero();
        m_enumerator->getMainMonitorWorkingArea(position, size);
        return position;
    }

    Vec2i getMonitorsSize() const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return Vec2i::zero();
        return m_enumerator->getMonitorsSize();
    }

    Vec2 getMonitorScale(int index, Vec2 defaultScale = Vec2::one()) const
    {
        Vec2 scale = defaultScale;
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return scale;

        m_enumerator->getMonitorContentScale(index, scale);
        if (scale.x <= 0.f || scale.y <= 0.f || !std::isfinite(scale.x) || !std::isfinite(scale.y))
            return defaultScale;

        return scale;
    }

    void getMonitorPosition(int index, Vec2i& position) const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return;
        m_enumerator->getMonitorPosition(index, position);
    }

    void getMonitorSize(int index, Vec2i& size) const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return;
        m_enumerator->getMonitorSize(index, size);
    }

    Vec2i getMonitorPhysicalSize() const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return Vec2i::zero();
        return m_enumerator->getMonitorPhysicalSize();
    }

    Vec2i getMonitorPhysicalSize(int index) const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return Vec2i::zero();
        return m_enumerator->getMonitorPhysicalSize(index);
    }

    int getMonitorsCount() const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return 0;
        return m_enumerator->getMonitorsCount();
    }
};
