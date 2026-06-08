#include "Engine/WorldSamplingSubsystem.hpp"

#include "Engine/Graphics/FramebufferOGL.hpp"
#include "Engine/Graphics/ShaderOGL.hpp"
#include "Engine/Graphics/ScreenSpaceQuadOGL.hpp"
#include "Engine/Graphics/TextureOGL.hpp"
#include "Engine/ScreenShoot.hpp"
#include "Game/GameData.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace
{
inline Vec2 computeCapturePositionX(const PhysicComponent& comp, const Vec2 prevToNewWinPos, const GameData& data)
{
    const float xPadding = prevToNewWinPos.x < 0.f ? prevToNewWinPos.x : 0.f;
    return Vec2(comp.getRect().getPosition().x + comp.getRect().getSize().x / 2.f + xPadding -
                    static_cast<float>(data.footBasementWidth) / 2.f,
                comp.getRect().getPosition().y + comp.getRect().getSize().y + 1 +
                    (prevToNewWinPos.y < 0.f ? prevToNewWinPos.y : 0.f) -
                    static_cast<float>(data.footBasementHeight) / 2.f);
}

inline Vec2i computeCaptureSize(const Vec2 prevToNewWinPos, const GameData& data)
{
    return Vec2i(static_cast<int>(fabs(prevToNewWinPos.x) + data.footBasementWidth),
                 static_cast<int>(fabs(prevToNewWinPos.y) + data.footBasementHeight));
}

inline int pixelIndex(int x, int y, int width, int channels)
{
    return (y * width + x) * channels;
}
}

WorldSamplingSubsystem::WorldSamplingSubsystem(MonitorTopologyCache* monitorTopology, double surfaceSampleIntervalSeconds)
    : m_surfaceSampleIntervalSeconds(surfaceSampleIntervalSeconds), m_monitorTopology(monitorTopology)
{
}

void WorldSamplingSubsystem::setMonitorTopologyCache(MonitorTopologyCache* monitorTopology)
{
    std::lock_guard lock{m_mutex};
    m_monitorTopology          = monitorTopology;
}

void WorldSamplingSubsystem::setSurfaceSampleInterval(double intervalSeconds)
{
    std::lock_guard lock{m_mutex};
    m_surfaceSampleIntervalSeconds = intervalSeconds;
}

void WorldSamplingSubsystem::update(double nowSeconds)
{
    if (!m_monitorTopology)
        return;

    if (m_monitorTopology->refreshIfNeeded(nowSeconds))
    {
        std::lock_guard lock{m_mutex};
        m_topologyState.monitorSnapshot = m_monitorTopology->getSnapshot();
    }
}

std::vector<MonitorTopologyItem> WorldSamplingSubsystem::getMonitorTopologySnapshot() const
{
    std::lock_guard lock{m_mutex};
    return m_topologyState.monitorSnapshot;
}

bool WorldSamplingSubsystem::hasValidSample(const PhysicComponent& comp) const
{
    std::lock_guard lock{m_mutex};
    auto            it = m_surfaceSamples.find(&comp);
    return it != m_surfaceSamples.end() && it->second.valid;
}

void WorldSamplingSubsystem::onMonitorTopologyChanged()
{
    std::lock_guard lock{m_mutex};
    m_surfaceSamples.clear();
    if (m_monitorTopology)
    {
        m_topologyState.monitorSnapshot = m_monitorTopology->getSnapshot();
    }
}

bool WorldSamplingSubsystem::checkSurfaceCollision(PhysicComponent& comp, Vec2 prevToNewWinPos, Vec2& newPos, GameData& data)
{
    if (prevToNewWinPos.sqrLength() == 0.f)
        return false;

    bool shouldSample = false;
    {
        std::lock_guard lock{m_mutex};
        auto&            sample = m_surfaceSamples[&comp];
        shouldSample = !sample.valid ||
                      (m_surfaceSampleIntervalSeconds > 0.0 &&
                       data.timeAcc - sample.lastSampleTime > m_surfaceSampleIntervalSeconds);
    }

    if (shouldSample && !refreshCollisionSample(data, comp, prevToNewWinPos, data.timeAcc))
        return false;

    std::lock_guard lock{m_mutex};
    const auto& sample = m_surfaceSamples[&comp];
    return testCollisionWithCachedSurface(sample, comp, prevToNewWinPos, newPos, data);
}

bool WorldSamplingSubsystem::refreshCollisionSample(GameData& data, const PhysicComponent& comp, Vec2 prevToNewWinPos,
                                                  double nowSeconds)
{
#ifdef USE_OPENGL_API
    if (!data.edgeDetectionShaders.size())
        return false;

    if (data.debugEdgeDetection && (!data.pFullScreenQuad || !data.pFramebuffer))
        return false;

    const Vec2 capturePosLogical = computeCapturePositionX(comp, prevToNewWinPos, data);
    const Vec2i captureSizeLogical = computeCaptureSize(prevToNewWinPos, data);
    Vec2       captureScale = getMonitorScaleForPosition(capturePosLogical);
    if (captureScale.x <= 0.f || captureScale.y <= 0.f)
        captureScale = Vec2::one();

    Vec2i captureSizePhysical;
    Vec2  capturePositionPhysical = logicalToPhysical(capturePosLogical, captureScale);

    if (data.debugEdgeDetection)
    {
        capturePositionPhysical.x = 0.f;
        capturePositionPhysical.y = 0.f;
        captureSizePhysical.x =
            std::max(1, static_cast<int>(std::round(static_cast<float>(data.window->getSize().x) * captureScale.x)));
        captureSizePhysical.y =
            std::max(1, static_cast<int>(std::round(static_cast<float>(data.window->getSize().y) * captureScale.y)));
    }
    else
    {
        captureSizePhysical.x = std::max(1, static_cast<int>(std::round(static_cast<float>(captureSizeLogical.x) * captureScale.x)));
        captureSizePhysical.y = std::max(1, static_cast<int>(std::round(static_cast<float>(captureSizeLogical.y) * captureScale.y)));
    }

    if (captureSizePhysical.x <= 0 || captureSizePhysical.y <= 0)
        return false;

    const int screenShootPosX = std::max(0, static_cast<int>(std::floor(capturePositionPhysical.x)));
    const int screenShootPosY = std::max(0, static_cast<int>(std::floor(capturePositionPhysical.y)));
    ScreenShoot screenshoot(screenShootPosX, screenShootPosY, captureSizePhysical.x, captureSizePhysical.y);
    const ScreenShoot::Data& pxlData = screenshoot.get();

    auto collisionTexture = std::make_unique<Texture>(pxlData.bits, pxlData.width, pxlData.height, 4);
    auto edgeTexture     = std::make_unique<Texture>(pxlData.width, pxlData.height, 4);

    glDisable(GL_BLEND);
    glViewport(0, 0, pxlData.width, pxlData.height);

    if (data.edgeDetectionShaders.size() == 1)
    {
        data.pFramebuffer->bind();
        data.pFramebuffer->attachTexture(*edgeTexture);

        data.edgeDetectionShaders[0]->use();
        data.edgeDetectionShaders[0]->setInt("uTexture", 0);
        data.pFullScreenQuad->use();
        collisionTexture->use();
        data.pFullScreenQuad->draw();
    }
    else
    {
        data.pFramebuffer->bind();
        data.pFramebuffer->attachTexture(*collisionTexture);

        data.edgeDetectionShaders[0]->use();
        data.edgeDetectionShaders[0]->setInt("uTexture", 0);
        collisionTexture->use();
        data.pFullScreenQuad->use();
        data.pFullScreenQuad->draw();

        data.pFramebuffer->bind();
        data.pFramebuffer->attachTexture(*edgeTexture);

        data.edgeDetectionShaders[1]->use();
        data.edgeDetectionShaders[1]->setInt("uTexture", 0);
        data.edgeDetectionShaders[1]->setVec2("resolution", static_cast<float>(pxlData.width),
                                              static_cast<float>(pxlData.height));
        collisionTexture->use();
        data.pFullScreenQuad->use();
        data.pFullScreenQuad->draw();
    }

    std::vector<unsigned char> pixels;
    edgeTexture->getPixels(pixels);

    std::lock_guard lock{m_mutex};
    auto& sample = m_surfaceSamples[&comp];
    sample.capturePosition = {screenShootPosX, screenShootPosY};
    sample.captureWidth    = pxlData.width;
    sample.captureHeight   = pxlData.height;
    sample.captureScale    = captureScale;
    sample.channels        = edgeTexture->getChannelsCount();
    sample.pixels          = std::move(pixels);
    sample.valid           = true;
    sample.lastSampleTime  = nowSeconds;

    data.pCollisionTexture     = std::move(collisionTexture);
    data.pEdgeDetectionTexture = std::move(edgeTexture);
    return true;
#endif

    return false;
}

bool WorldSamplingSubsystem::testCollisionWithCachedSurface(const SurfaceCollisionSample& sample, const PhysicComponent& comp,
                                                           Vec2 prevToNewWinPos, Vec2& newPos, const GameData& data) const
{
    if (!sample.valid || sample.pixels.empty() || sample.channels <= 0)
        return false;

    const Vec2 scaledMovement = prevToNewWinPos * sample.captureScale;
    const bool iterationOnX = fabs(scaledMovement.x) > fabs(scaledMovement.y);
    Vec2       prevToNewWinPosDir;
    if (iterationOnX)
    {
        const float length = sqrtf(scaledMovement.x * scaledMovement.x);
        prevToNewWinPosDir = length > 0.f ? (scaledMovement / length) : Vec2::zero();
    }
    else
    {
        const float length = sqrtf(scaledMovement.y * scaledMovement.y);
        prevToNewWinPosDir = length > 0.f ? (scaledMovement / length) : Vec2::zero();
    }

    const int basementWidth = std::max(1, static_cast<int>(std::round(data.footBasementWidth * sample.captureScale.x)));
    const int basementHeight =
        std::max(1, static_cast<int>(std::round(data.footBasementHeight * sample.captureScale.y)));

    float row    = prevToNewWinPosDir.y < 0.f ? sample.captureHeight - basementHeight : 0.f;
    float column = prevToNewWinPosDir.x < 0.f ? sample.captureWidth - basementWidth : 0.f;

    const int iterationCount = iterationOnX ? sample.captureWidth - basementWidth : sample.captureHeight - basementHeight;
    if (iterationCount <= 0)
        return false;

    for (int i = 0; i < iterationCount + 1; i++)
    {
        float count = 0.f;
        for (int y = 0; y < basementHeight; y++)
        {
            for (int x = 0; x < basementWidth; x++)
            {
                const int pixelX = static_cast<int>(column) + x;
                const int pixelY = static_cast<int>(row) + y;

                if (pixelX < 0 || pixelY < 0 || pixelX >= sample.captureWidth || pixelY >= sample.captureHeight)
                    continue;

                const int index = pixelIndex(pixelX, sample.captureHeight - 1 - pixelY, sample.captureWidth, sample.channels);
                if (index >= 0 && index < static_cast<int>(sample.pixels.size()) && sample.pixels[index] == 255)
                    count += 1.f;
            }
        }

        count /= static_cast<float>(basementWidth * basementHeight);
        if (count > data.collisionPixelRatioStopMovement)
        {
            const float denomX = std::max(sample.captureScale.x, 0.0001f);
            const float denomY = std::max(sample.captureScale.y, 0.0001f);
            newPos = comp.getRect().getPosition() + Vec2(column / denomX, row / denomY);
            return true;
        }

        row += prevToNewWinPosDir.y;
        column += prevToNewWinPosDir.x;
    }

    return false;
}

Vec2 WorldSamplingSubsystem::getMonitorScaleForPosition(const Vec2& position, const Vec2 defaultScale) const
{
    if (!m_monitorTopology)
        return defaultScale;

    const auto monitor = m_monitorTopology->findMonitorForLogicalPoint(position);
    if (monitor.has_value())
        return monitor->contentScale;

    if (!m_topologyState.monitorSnapshot.empty())
        return m_topologyState.monitorSnapshot.front().contentScale;

    return defaultScale;
}

Vec2 WorldSamplingSubsystem::logicalToPhysical(const Vec2& logicalPosition, const Vec2 defaultScale) const
{
    if (!m_monitorTopology)
        return logicalPosition * defaultScale;

    const auto monitor = m_monitorTopology->findMonitorForLogicalPoint(logicalPosition);
    if (monitor.has_value())
        return monitor->logicalToPhysical(logicalPosition);

    return logicalPosition * defaultScale;
}

Vec2 WorldSamplingSubsystem::getPixelPerMeterForPosition(const Vec2& logicalPosition, const Vec2 defaultPixelPerMeter) const
{
    if (!m_monitorTopology)
        return defaultPixelPerMeter;

    const auto monitor = m_monitorTopology->findMonitorForLogicalPoint(logicalPosition);
    if (monitor.has_value() && monitor->pixelPerMeter.x > 0.f && monitor->pixelPerMeter.y > 0.f &&
        std::isfinite(monitor->pixelPerMeter.x) && std::isfinite(monitor->pixelPerMeter.y))
    {
        return monitor->pixelPerMeter;
    }

    const auto snapshot = getMonitorTopologySnapshot();
    if (!snapshot.empty() && snapshot.front().pixelPerMeter.x > 0.f && snapshot.front().pixelPerMeter.y > 0.f &&
        std::isfinite(snapshot.front().pixelPerMeter.x) && std::isfinite(snapshot.front().pixelPerMeter.y))
    {
        return snapshot.front().pixelPerMeter;
    }

    return defaultPixelPerMeter;
}
