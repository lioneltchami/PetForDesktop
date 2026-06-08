#pragma once

#include "Engine/Platform/IWindowEnumerator.hpp"
#include "Engine/Platform/PlatformServices.hpp"
#include "Engine/Vector2.hpp"

#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <limits>

class Monitors
{
public:
    struct MonitorTransform
    {
        Vec2i logicalPosition = Vec2i::zero();
        Vec2i logicalSize     = Vec2i::zero();
        Vec2i pixelPosition  = Vec2i::zero();
        Vec2i pixelSize      = Vec2i::zero();
        Vec2i physicalSize   = Vec2i::zero();
        Vec2  pixelPerMeter  = {3779.527f, 3779.527f};
        Vec2  contentScale   = Vec2::one();

        Vec2 logicalToPhysical(const Vec2& logicalPoint) const
        {
            const float safeScaleX = std::max(contentScale.x, 0.0001f);
            const float safeScaleY = std::max(contentScale.y, 0.0001f);

            return {(logicalPoint.x - static_cast<float>(logicalPosition.x)) * safeScaleX + static_cast<float>(pixelPosition.x),
                    (logicalPoint.y - static_cast<float>(logicalPosition.y)) * safeScaleY + static_cast<float>(pixelPosition.y)};
        }

        Vec2 physicalToLogical(const Vec2& pixelPoint) const
        {
            const float safeScaleX = std::max(contentScale.x, 0.0001f);
            const float safeScaleY = std::max(contentScale.y, 0.0001f);

            return {(pixelPoint.x - static_cast<float>(pixelPosition.x)) / safeScaleX + static_cast<float>(logicalPosition.x),
                    (pixelPoint.y - static_cast<float>(pixelPosition.y)) / safeScaleY + static_cast<float>(logicalPosition.y)};
        }

        bool containsLogicalPoint(const Vec2& point) const
        {
            return point.x >= static_cast<float>(logicalPosition.x) &&
                   point.x < static_cast<float>(logicalPosition.x + logicalSize.x) &&
                   point.y >= static_cast<float>(logicalPosition.y) &&
                   point.y < static_cast<float>(logicalPosition.y + logicalSize.y);
        }

        bool containsPhysicalPoint(const Vec2& point) const
        {
            return point.x >= static_cast<float>(pixelPosition.x) && point.x < static_cast<float>(pixelPosition.x + pixelSize.x) &&
                   point.y >= static_cast<float>(pixelPosition.y) && point.y < static_cast<float>(pixelPosition.y + pixelSize.y);
        }
    };

protected:
    std::unique_ptr<IWindowEnumerator> m_enumerator;
    std::function<void()>              m_onTopologyChanged;
    mutable std::mutex                 m_mutex;

    bool buildMonitorTransform(int index, MonitorTransform& transform) const
    {
        if (!m_enumerator)
            return false;

        m_enumerator->getMonitorPixelPosition(index, transform.pixelPosition);
        m_enumerator->getMonitorPixelSize(index, transform.pixelSize);
        m_enumerator->getMonitorContentScale(index, transform.contentScale);
        transform.physicalSize = m_enumerator->getMonitorPhysicalSize(index);
        m_enumerator->getMonitorPosition(index, transform.logicalPosition);
        m_enumerator->getMonitorSize(index, transform.logicalSize);

        if (transform.logicalSize.x <= 0 || transform.logicalSize.y <= 0 || transform.pixelSize.x <= 0 ||
            transform.pixelSize.y <= 0)
            return false;

        if (transform.contentScale.x <= 0.f || transform.contentScale.y <= 0.f ||
            !std::isfinite(transform.contentScale.x) || !std::isfinite(transform.contentScale.y))
            transform.contentScale = Vec2::one();

        if (transform.physicalSize.x > 0 && transform.physicalSize.y > 0)
        {
            transform.pixelPerMeter = {static_cast<float>(transform.pixelSize.x) /
                                          (static_cast<float>(transform.physicalSize.x) * 0.001f),
                                      static_cast<float>(transform.pixelSize.y) /
                                          (static_cast<float>(transform.physicalSize.y) * 0.001f)};
            if (transform.pixelPerMeter.x <= 0.f || !std::isfinite(transform.pixelPerMeter.x) ||
                transform.pixelPerMeter.y <= 0.f || !std::isfinite(transform.pixelPerMeter.y))
            {
                transform.pixelPerMeter = {3779.527f * transform.contentScale.x, 3779.527f * transform.contentScale.y};
            }
        }
        else
        {
            transform.pixelPerMeter = {3779.527f * transform.contentScale.x, 3779.527f * transform.contentScale.y};
        }

        return true;
    }

    static float pointToRectDistanceSq(const Vec2& point, const Vec2& minPoint, const Vec2& maxPoint)
    {
        const float clampedX = std::min(std::max(point.x, minPoint.x), maxPoint.x);
        const float clampedY = std::min(std::max(point.y, minPoint.y), maxPoint.y);
        const float dx = point.x - clampedX;
        const float dy = point.y - clampedY;
        return dx * dx + dy * dy;
    }

    std::optional<MonitorTransform> findMonitorTransformForPoint(const Vec2& point,
                                                                const bool preferPhysicalContainment = false) const
    {
        std::lock_guard lock{m_mutex};
        const int monitorCount = m_enumerator ? m_enumerator->getMonitorsCount() : 0;
        if (monitorCount <= 0)
            return std::nullopt;

        std::optional<MonitorTransform> bestTransform;
        float                          bestDistance = std::numeric_limits<float>::infinity();

        for (int i = 0; i < monitorCount; ++i)
        {
            MonitorTransform transform;
            if (!buildMonitorTransform(i, transform))
                continue;

            const bool pointInMonitor =
                preferPhysicalContainment ? transform.containsPhysicalPoint(point) : transform.containsLogicalPoint(point);
            if (pointInMonitor)
                return transform;

            const Vec2 minPoint = preferPhysicalContainment
                                      ? Vec2{static_cast<float>(transform.pixelPosition.x),
                                             static_cast<float>(transform.pixelPosition.y)}
                                      : Vec2{static_cast<float>(transform.logicalPosition.x),
                                             static_cast<float>(transform.logicalPosition.y)};
            const Vec2 maxPoint =
                preferPhysicalContainment
                    ? Vec2{static_cast<float>(transform.pixelPosition.x + transform.pixelSize.x),
                           static_cast<float>(transform.pixelPosition.y + transform.pixelSize.y)}
                    : Vec2{static_cast<float>(transform.logicalPosition.x + transform.logicalSize.x),
                           static_cast<float>(transform.logicalPosition.y + transform.logicalSize.y)};

            const float distance = pointToRectDistanceSq(point, minPoint, maxPoint);
            if (!bestTransform || distance < bestDistance)
            {
                bestDistance  = distance;
                bestTransform = transform;
            }
        }

        if (!bestTransform)
            return std::nullopt;

        return bestTransform;
    }

    int findMonitorIndexForLogicalPoint(const Vec2i& logicalPoint) const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return -1;

        const Vec2 logicalPointAsFloat{static_cast<float>(logicalPoint.x), static_cast<float>(logicalPoint.y)};
        const int monitorCount = m_enumerator->getMonitorsCount();
        if (monitorCount <= 0)
            return -1;

        int   bestIndex   = -1;
        float bestDistance = std::numeric_limits<float>::infinity();

        for (int i = 0; i < m_enumerator->getMonitorsCount(); ++i)
        {
            MonitorTransform monitorTransformCandidate;
            if (!buildMonitorTransform(i, monitorTransformCandidate))
                continue;

            if (monitorTransformCandidate.containsLogicalPoint(logicalPointAsFloat))
                return i;

            const Vec2 minPoint{static_cast<float>(monitorTransformCandidate.logicalPosition.x),
                               static_cast<float>(monitorTransformCandidate.logicalPosition.y)};
            const Vec2 maxPoint{
                static_cast<float>(monitorTransformCandidate.logicalPosition.x + monitorTransformCandidate.logicalSize.x),
                static_cast<float>(monitorTransformCandidate.logicalPosition.y + monitorTransformCandidate.logicalSize.y)};
            const float distance = pointToRectDistanceSq(logicalPointAsFloat, minPoint, maxPoint);
            if (bestIndex < 0 || distance < bestDistance)
            {
                bestIndex   = i;
                bestDistance = distance;
            }
        }

        return bestIndex;
    }

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

    int getMainMonitorIndex(Vec2i referencePosition) const
    {
        return findMonitorIndexForLogicalPoint(referencePosition);
    }

    int getPrimaryMonitorIndex() const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return -1;

        const int primaryMonitor = m_enumerator->getPrimaryMonitorIndex();
        if (primaryMonitor >= 0)
            return primaryMonitor;

        const int monitorCount = m_enumerator->getMonitorsCount();
        return monitorCount > 0 ? 0 : -1;
    }

    int getMainMonitorIndex() const
    {
        return getPrimaryMonitorIndex();
    }

    std::optional<MonitorTransform> getMonitorTransformForLogicalPoint(Vec2 logicalPoint) const
    {
        std::lock_guard lock{m_mutex};
        return findMonitorTransformForPoint(logicalPoint, false);
    }

    std::optional<MonitorTransform> getMonitorTransformForPhysicalPoint(Vec2 pixelPoint) const
    {
        std::lock_guard lock{m_mutex};
        return findMonitorTransformForPoint(pixelPoint, true);
    }

    Vec2 logicalToPhysical(const Vec2& logicalPoint, const Vec2 fallbackScale = Vec2::one()) const
    {
        const auto transform = getMonitorTransformForLogicalPoint(logicalPoint);
        if (!transform)
            return logicalPoint * fallbackScale;

        return transform->logicalToPhysical(logicalPoint);
    }

    Vec2 physicalToLogical(const Vec2& pixelPoint, const Vec2 fallbackScale = Vec2::one()) const
    {
        const auto transform = getMonitorTransformForPhysicalPoint(pixelPoint);
        if (!transform)
            return pixelPoint * (Vec2{1.f / std::max(fallbackScale.x, 0.0001f), 1.f / std::max(fallbackScale.y, 0.0001f)});

        return transform->physicalToLogical(pixelPoint);
    }

    Vec2 getPixelPerMeterForLogicalPoint(Vec2 logicalPoint, Vec2 fallbackPixelPerMeter = {3779.527f, 3779.527f}) const
    {
        const auto transform = getMonitorTransformForLogicalPoint(logicalPoint);
        if (!transform)
            return fallbackPixelPerMeter;

        return transform->pixelPerMeter;
    }

    Vec2 getPixelPerMeterForPhysicalPoint(Vec2 pixelPoint, Vec2 fallbackPixelPerMeter = {3779.527f, 3779.527f}) const
    {
        const auto transform = getMonitorTransformForPhysicalPoint(pixelPoint);
        if (!transform)
            return fallbackPixelPerMeter;

        return transform->pixelPerMeter;
    }

    Vec2 getPixelPerMeterForMonitor(int index, Vec2 fallbackPixelPerMeter = {3779.527f, 3779.527f}) const
    {
        MonitorTransform transform;
        if (!buildMonitorTransform(index, transform))
            return fallbackPixelPerMeter;

        return transform.pixelPerMeter;
    }

    Vec2i getMainMonitorPhysicalSize() const
    {
        const int mainMonitor = getMainMonitorIndex();
        if (mainMonitor < 0)
            return Vec2i::zero();

        return getMonitorPhysicalSize(mainMonitor);
    }

    Vec2i getMainMonitorWorkingArea(Vec2i& position, Vec2i& size) const
    {
        position = Vec2i::zero();
        size     = Vec2i::zero();
        const int mainMonitor = getMainMonitorIndex();
        if (mainMonitor >= 0)
        {
            getMonitorPosition(mainMonitor, position);
            getMonitorSize(mainMonitor, size);
        }

        return position;
    }

    Vec2i getMonitorsSize() const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return Vec2i::zero();
        return m_enumerator->getMonitorsSize();
    }

    void getMonitorPixelPosition(int index, Vec2i& position) const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return;
        m_enumerator->getMonitorPixelPosition(index, position);
    }

    void getMonitorPixelSize(int index, Vec2i& size) const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return;
        m_enumerator->getMonitorPixelSize(index, size);
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
