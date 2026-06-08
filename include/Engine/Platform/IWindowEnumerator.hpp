#pragma once

#include "Engine/Vector2.hpp"

class IWindowEnumerator
{
public:
    virtual ~IWindowEnumerator() = default;

    virtual void init() = 0;

    virtual void onMonitorConnectionChanged(void* monitor, int event) = 0;

    virtual int getMonitorsCount() const = 0;

    virtual void getMainMonitorWorkingArea(Vec2i& position, Vec2i& size) const = 0;

    virtual Vec2i getMonitorsSize() const = 0;

    virtual void getMonitorPixelPosition(int index, Vec2i& position) const = 0;

    virtual void getMonitorPixelSize(int index, Vec2i& size) const = 0;

    virtual void getMonitorContentScale(int index, Vec2& scale) const = 0;

    virtual void getMonitorPosition(int index, Vec2i& position) const = 0;

    virtual void getMonitorSize(int index, Vec2i& size) const = 0;

    virtual Vec2i getMonitorPhysicalSize() const = 0;

    virtual Vec2i getMonitorPhysicalSize(int index) const = 0;
};
