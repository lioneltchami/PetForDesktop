#include "Engine/WorldSamplingSubsystem.hpp"

#include "Engine/Graphics/FramebufferOGL.hpp"
#include "Engine/Graphics/ShaderOGL.hpp"
#include "Engine/Graphics/ScreenSpaceQuadOGL.hpp"
#include "Engine/Graphics/TextureOGL.hpp"
#include "Engine/ScreenShoot.hpp"
#include "Game/GameData.hpp"

#include <cmath>
#include <memory>
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

    Vec2 capturePos = computeCapturePositionX(comp, prevToNewWinPos, data);
    Vec2i captureSize = computeCaptureSize(prevToNewWinPos, data);

    if (data.debugEdgeDetection)
    {
        capturePos.x   = 0.f;
        capturePos.y   = 0.f;
        captureSize.x  = data.window->getSize().x;
        captureSize.y  = data.window->getSize().y;
    }

    const int screenShootPosX = static_cast<int>(capturePos.x);
    const int screenShootPosY = static_cast<int>(capturePos.y);
    if (captureSize.x <= 0 || captureSize.y <= 0)
        return false;

    ScreenShoot screenshoot(screenShootPosX, screenShootPosY, captureSize.x, captureSize.y);
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

    const bool iterationOnX = fabs(prevToNewWinPos.x) > fabs(prevToNewWinPos.y);
    Vec2       prevToNewWinPosDir;
    if (iterationOnX)
        prevToNewWinPosDir = prevToNewWinPos / sqrtf(prevToNewWinPos.x * prevToNewWinPos.x);
    else
        prevToNewWinPosDir = prevToNewWinPos / sqrtf(prevToNewWinPos.y * prevToNewWinPos.y);

    float row    = prevToNewWinPosDir.y < 0.f ? sample.captureHeight - data.footBasementHeight : 0.f;
    float column = prevToNewWinPosDir.x < 0.f ? sample.captureWidth - data.footBasementWidth : 0.f;

    const int iterationCount = iterationOnX ? sample.captureWidth - data.footBasementWidth
                                           : sample.captureHeight - data.footBasementHeight;
    if (iterationCount <= 0)
        return false;

    for (int i = 0; i < iterationCount + 1; i++)
    {
        float count = 0;
        for (int y = 0; y < data.footBasementHeight; y++)
        {
            for (int x = 0; x < data.footBasementWidth; x++)
            {
                const int pixelX = static_cast<int>(column) + x;
                const int pixelY = static_cast<int>(row) + y;

                if (pixelX < 0 || pixelY < 0 || pixelX >= sample.captureWidth || pixelY >= sample.captureHeight)
                    continue;

                const int index = pixelIndex(pixelX, sample.captureHeight - 1 - pixelY, sample.captureWidth,
                                            sample.channels);
                if (index >= 0 && index < static_cast<int>(sample.pixels.size()) &&
                    sample.pixels[index] == 255)
                    count += 1.f;
            }
        }

        count /= data.footBasementWidth * data.footBasementHeight;
        if (count > data.collisionPixelRatioStopMovement)
        {
            newPos = comp.getRect().getPosition() + Vec2(column, row);
            return true;
        }

        row += prevToNewWinPosDir.y;
        column += prevToNewWinPosDir.x;
    }

    return false;
}
