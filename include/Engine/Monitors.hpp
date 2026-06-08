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
    struct CursorTransformOptions
    {
        Vec2 windowLogicalPosition = Vec2::zero();
        Vec2 windowLogicalSize     = Vec2::zero();
        Vec2 windowPixelSize       = Vec2::zero();
        float fallbackScaleX       = 0.0001f;
        float fallbackScaleY       = 0.0001f;
    };

    struct CoordinateAffineTransform
    {
        Vec2 scale = Vec2::one();
        Vec2 offset = Vec2::zero();

        Vec2 transformLogicalToPhysical(const Vec2& logicalPoint) const
        {
            return {logicalPoint.x * scale.x + offset.x, logicalPoint.y * scale.y + offset.y};
        }

        Vec2 transformPhysicalToLogical(const Vec2& pixelPoint) const
        {
            const float safeScaleX = std::max(scale.x, 0.0001f);
            const float safeScaleY = std::max(scale.y, 0.0001f);

            return {(pixelPoint.x - offset.x) / safeScaleX, (pixelPoint.y - offset.y) / safeScaleY};
        }
    };

    struct MonitorTransform
    {
        Vec2i logicalPosition = Vec2i::zero();
        Vec2i logicalSize     = Vec2i::zero();
        Vec2i pixelPosition  = Vec2i::zero();
        Vec2i pixelSize      = Vec2i::zero();
        Vec2i physicalSize   = Vec2i::zero();
        Vec2  pixelPerMeter  = {3779.527f, 3779.527f};
        Vec2  contentScale   = Vec2::one();
        CoordinateAffineTransform logicalToPhysicalTransform;
        CoordinateAffineTransform physicalToLogicalTransform;

        Vec2 logicalToPhysical(const Vec2& logicalPoint) const
        {
            return logicalToPhysicalTransform.transformLogicalToPhysical(logicalPoint);
        }

        Vec2 physicalToLogical(const Vec2& pixelPoint) const
        {
            return physicalToLogicalTransform.transformPhysicalToLogical(pixelPoint);
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

        transform.logicalToPhysicalTransform.scale = {std::max(transform.contentScale.x, 0.0001f),
                                                    std::max(transform.contentScale.y, 0.0001f)};
        transform.logicalToPhysicalTransform.offset = {
            static_cast<float>(transform.pixelPosition.x) -
                static_cast<float>(transform.logicalPosition.x) * transform.logicalToPhysicalTransform.scale.x,
            static_cast<float>(transform.pixelPosition.y) -
                static_cast<float>(transform.logicalPosition.y) * transform.logicalToPhysicalTransform.scale.y};
        transform.physicalToLogicalTransform.scale = transform.logicalToPhysicalTransform.scale;
        transform.physicalToLogicalTransform.offset = transform.logicalToPhysicalTransform.offset;

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
        return findMonitorTransformForPoint(logicalPoint, false);
    }

    std::optional<MonitorTransform> getMonitorTransformForPhysicalPoint(Vec2 pixelPoint) const
    {
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

    template <typename Fn>
    void forEachMonitorTransform(Fn&& callback) const
    {
        std::lock_guard lock{m_mutex};
        if (!m_enumerator)
            return;

        const int monitorCount = m_enumerator->getMonitorsCount();
        for (int index = 0; index < monitorCount; ++index)
        {
            MonitorTransform transform;
            if (!buildMonitorTransform(index, transform))
                continue;

            callback(transform);
        }
    }

    Vec2 normalizeWindowCursor(const Vec2& cursorPos, const CursorTransformOptions& options) const
    {
        const Vec2 logicalSize = options.windowLogicalSize;
        if (logicalSize.x <= 0.f || logicalSize.y <= 0.f)
            return cursorPos;

        const auto isInsideRect = [](const Vec2& point, const Vec2& size) {
            return point.x >= 0.f && point.y >= 0.f && point.x <= size.x && point.y <= size.y;
        };

        const auto pointToRectDistanceSq = [](const Vec2& point, const Vec2& size) {
            const float clampedX = std::min(std::max(point.x, 0.f), size.x);
            const float clampedY = std::min(std::max(point.y, 0.f), size.y);
            const float dx      = point.x - clampedX;
            const float dy      = point.y - clampedY;
            return dx * dx + dy * dy;
        };

        const bool logicalInside = isInsideRect(cursorPos, logicalSize);
        if (logicalInside && options.windowPixelSize.x <= 0.f && options.windowPixelSize.y <= 0.f)
            return cursorPos;

        const Vec2 inferredPixelSize = {
            options.windowPixelSize.x > 0.f ? static_cast<float>(options.windowPixelSize.x) : logicalSize.x,
            options.windowPixelSize.y > 0.f ? static_cast<float>(options.windowPixelSize.y) : logicalSize.y};

        const bool hasPixelWindowMetrics =
            options.windowPixelSize.x > 0.f && options.windowPixelSize.y > 0.f && options.windowPixelSize.x > 1.f &&
            options.windowPixelSize.y > 1.f;
        const bool cursorInsidePhysical = isInsideRect(cursorPos, inferredPixelSize);
        const bool cursorLooksPhysical = hasPixelWindowMetrics && !logicalInside && cursorInsidePhysical;
        if (!cursorLooksPhysical && !logicalInside)
        {
            return {std::clamp(cursorPos.x, 0.f, logicalSize.x), std::clamp(cursorPos.y, 0.f, logicalSize.y)};
        }

        if (!cursorLooksPhysical && logicalInside)
            return cursorPos;

        Vec2 cursorBest = cursorPos;
        float bestDistance = std::numeric_limits<float>::infinity();
        bool foundCandidate = false;

        forEachMonitorTransform([&](const MonitorTransform& monitor) {
            const Vec2 originInPixel = monitor.logicalToPhysical(options.windowLogicalPosition);
            const Vec2 cursorFromPixel = monitor.physicalToLogical(originInPixel + cursorPos) - options.windowLogicalPosition;

            const bool inside = isInsideRect(cursorFromPixel, logicalSize);
            if (inside)
            {
                cursorBest = cursorFromPixel;
                foundCandidate = true;
                bestDistance = 0.f;
                return;
            }

            const float candidateDistance = pointToRectDistanceSq(cursorFromPixel, logicalSize);
            if (candidateDistance < bestDistance)
            {
                bestDistance = candidateDistance;
                cursorBest = cursorFromPixel;
            }
        });

        if (foundCandidate)
            return cursorBest;

        if (!logicalInside)
        {
            const float logicalDistance = pointToRectDistanceSq(cursorPos, logicalSize);
            const float fallbackDistance = pointToRectDistanceSq(cursorBest, logicalSize);
            if (fallbackDistance < logicalDistance)
                return cursorBest;
        }

        return cursorPos;
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
