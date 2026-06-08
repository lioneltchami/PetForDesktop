#pragma once

#include "Engine/Platform/IWindowEnumerator.hpp"
#include "Engine/Platform/PlatformServices.hpp"
#include "Engine/Vector2.hpp"

#include <memory>

class Monitors
{
protected:
    std::unique_ptr<IWindowEnumerator> m_enumerator;

public:
    Monitors() : m_enumerator(PlatformServices::createWindowEnumerator())
    {
    }

    explicit Monitors(std::unique_ptr<IWindowEnumerator> enumerator) : m_enumerator(std::move(enumerator))
    {
    }

    void setImplementation(std::unique_ptr<IWindowEnumerator> enumerator)
    {
        m_enumerator = std::move(enumerator);
    }

    void init()
    {
        if (!m_enumerator)
            m_enumerator = PlatformServices::createWindowEnumerator();
        if (m_enumerator)
            m_enumerator->init();
    }

    void onMonitorConnectionChanged(void* monitor, int event)
    {
        if (!m_enumerator)
            return;
        m_enumerator->onMonitorConnectionChanged(monitor, event);
    }

    void getMainMonitorWorkingArea(Vec2i& position, Vec2i& size) const
    {
        position = Vec2i::zero();
        size     = Vec2i::zero();
        if (!m_enumerator)
            return;
        m_enumerator->getMainMonitorWorkingArea(position, size);
    }

    Vec2i getMonitorsSize() const
    {
        if (!m_enumerator)
            return Vec2i::zero();
        return m_enumerator->getMonitorsSize();
    }

    void getMonitorPosition(int index, Vec2i& position) const
    {
        if (!m_enumerator)
            return;
        m_enumerator->getMonitorPosition(index, position);
    }

    void getMonitorSize(int index, Vec2i& size) const
    {
        if (!m_enumerator)
            return;
        m_enumerator->getMonitorSize(index, size);
    }

    Vec2i getMonitorPhysicalSize() const
    {
        if (!m_enumerator)
            return Vec2i::zero();
        return m_enumerator->getMonitorPhysicalSize();
    }

    int getMonitorsCount() const
    {
        if (!m_enumerator)
            return 0;
        return m_enumerator->getMonitorsCount();
    }
};
