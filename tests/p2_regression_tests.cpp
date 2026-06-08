#include "Engine/Monitors.hpp"
#include "Engine/Platform/IWindowEnumerator.hpp"
#include "Engine/MonitorTopologyCache.hpp"
#include "Engine/Settings.hpp"
#include "Engine/Updater.hpp"

class Window;
class Framebuffer;
class Shader;
class Texture;
class ScreenSpaceQuad;
class InteractionSystem;
class WorldSamplingSubsystem;
class ContextualMenu;
class SettingMenu;
class UpdateMenu;
class Pet;

class Window{};
class Framebuffer{};
class Shader{};
class Texture{};
class ScreenSpaceQuad{};
class InteractionSystem{};
class WorldSamplingSubsystem{};
class ContextualMenu{};
class SettingMenu{};
class UpdateMenu{};
class Pet{};

#include "yaml-cpp/yaml.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
struct FakeMonitorSpec
{
    Vec2i pixelPosition = Vec2i::zero();
    Vec2i pixelSize     = Vec2i::zero();
    Vec2i physicalSize  = Vec2i::zero();
    Vec2  contentScale  = Vec2::one();
};

class FakeWindowEnumerator : public IWindowEnumerator
{
    std::vector<FakeMonitorSpec> m_monitors;
    int                           m_primary = 0;

    static float clampScale(float scale)
    {
        return std::max(scale, 0.01f);
    }

public:
    void addMonitor(const FakeMonitorSpec& spec)
    {
        m_monitors.emplace_back(spec);
    }

    void setPrimaryMonitor(int index)
    {
        m_primary = index;
    }

    void init() override
    {
    }

    void onMonitorConnectionChanged(void* monitor, int event) override
    {
        (void)monitor;
        (void)event;
    }

    int getMonitorsCount() const override
    {
        return static_cast<int>(m_monitors.size());
    }

    int getPrimaryMonitorIndex() const override
    {
        if (m_monitors.empty())
            return -1;

        if (m_primary < 0 || m_primary >= static_cast<int>(m_monitors.size()))
            return 0;

        return m_primary;
    }

    void getMainMonitorWorkingArea(Vec2i& position, Vec2i& size) const override
    {
        const int primary = getPrimaryMonitorIndex();
        if (primary < 0)
        {
            position = Vec2i::zero();
            size     = Vec2i::zero();
            return;
        }

        getMonitorPosition(primary, position);
        getMonitorSize(primary, size);
    }

    Vec2i getMonitorsSize() const override
    {
        Vec2i size = Vec2i::zero();
        for (int i = 0; i < getMonitorsCount(); ++i)
        {
            Vec2i monitorPos;
            Vec2i monitorSize;
            getMonitorPosition(i, monitorPos);
            getMonitorSize(i, monitorSize);

            size.x = std::max(size.x, monitorPos.x + monitorSize.x);
            size.y = std::max(size.y, monitorPos.y + monitorSize.y);
        }

        return size;
    }

    void getMonitorPixelPosition(int index, Vec2i& position) const override
    {
        if (index < 0 || index >= static_cast<int>(m_monitors.size()))
        {
            position = Vec2i::zero();
            return;
        }

        position = m_monitors[index].pixelPosition;
    }

    void getMonitorPixelSize(int index, Vec2i& size) const override
    {
        if (index < 0 || index >= static_cast<int>(m_monitors.size()))
        {
            size = Vec2i::zero();
            return;
        }

        size = m_monitors[index].pixelSize;
    }

    void getMonitorContentScale(int index, Vec2& scale) const override
    {
        if (index < 0 || index >= static_cast<int>(m_monitors.size()))
        {
            scale = Vec2::one();
            return;
        }

        scale = m_monitors[index].contentScale;
    }

    void getMonitorPosition(int index, Vec2i& position) const override
    {
        if (index < 0 || index >= static_cast<int>(m_monitors.size()))
        {
            position = Vec2i::zero();
            return;
        }

        const auto& monitor = m_monitors[index];
        const float sx      = clampScale(monitor.contentScale.x);
        const float sy      = clampScale(monitor.contentScale.y);
        position.x = static_cast<int>(std::floor(static_cast<float>(monitor.pixelPosition.x) / sx));
        position.y = static_cast<int>(std::floor(static_cast<float>(monitor.pixelPosition.y) / sy));
    }

    void getMonitorSize(int index, Vec2i& size) const override
    {
        if (index < 0 || index >= static_cast<int>(m_monitors.size()))
        {
            size = Vec2i::zero();
            return;
        }

        const auto& monitor = m_monitors[index];
        const float sx      = clampScale(monitor.contentScale.x);
        const float sy      = clampScale(monitor.contentScale.y);

        const float left   = std::floor(static_cast<float>(monitor.pixelPosition.x) / sx);
        const float top    = std::floor(static_cast<float>(monitor.pixelPosition.y) / sy);
        const float right  = std::ceil((static_cast<float>(monitor.pixelPosition.x) + monitor.pixelSize.x) / sx);
        const float bottom = std::ceil((static_cast<float>(monitor.pixelPosition.y) + monitor.pixelSize.y) / sy);

        size.x = std::max(1, static_cast<int>(right - left));
        size.y = std::max(1, static_cast<int>(bottom - top));
    }

    Vec2i getMonitorPhysicalSize() const override
    {
        return Vec2i::zero();
    }

    Vec2i getMonitorPhysicalSize(int index) const override
    {
        if (index < 0 || index >= static_cast<int>(m_monitors.size()))
            return Vec2i::zero();

        return m_monitors[index].physicalSize;
    }
};

bool near(float lhs, float rhs, float eps = 1e-4f)
{
    return std::fabs(lhs - rhs) < eps;
}

bool test_monitor_scaling_transform()
{
    auto monitorEnumerator = std::make_unique<FakeWindowEnumerator>();
    monitorEnumerator->addMonitor({{0, 0}, {1920, 1080}, {400, 225}, {1.f, 1.f}});      // 100%
    monitorEnumerator->addMonitor({{1920, 0}, {1920, 1080}, {384, 216}, {1.25f, 1.25f}}); // 125%
    monitorEnumerator->addMonitor({{3840, -1200}, {1536, 1536}, {307, 173}, {1.5f, 1.5f}}); // 150%

    Monitors monitors(std::move(monitorEnumerator));

    const auto fail = [](const char* step) {
        std::cout << "monitor_scaling fail: " << step << "\n";
        return false;
    };

    // 100% DPI conversion
    Vec2 logicalToPhysical100 = monitors.logicalToPhysical({100.f, 25.f});
    if (!near(logicalToPhysical100.x, 100.f) || !near(logicalToPhysical100.y, 25.f))
    {
        std::cout << "monitor_scaling logicalToPhysical100 => {" << logicalToPhysical100.x << ", " << logicalToPhysical100.y << "}\n";
        return fail("logicalToPhysical100");
    }

    // 125% conversion
    Vec2 logicalToPhysical125 = monitors.logicalToPhysical({2000.f, 100.f});
    if (!near(logicalToPhysical125.x, 1920.f + (2000.f - 1536.f) * 1.25f) || !near(logicalToPhysical125.y, 125.f))
    {
        std::cout << "monitor_scaling logicalToPhysical125 => {" << logicalToPhysical125.x << ", " << logicalToPhysical125.y << "}\n";
        return fail("logicalToPhysical125");
    }

    // 150% conversion
    Vec2 logicalToPhysical150 = monitors.logicalToPhysical({2600.f, -760.f});
    if (!near(logicalToPhysical150.x, 3840.f + (2600.f - 2560.f) * 1.5f) ||
        !near(logicalToPhysical150.y, -1200.f + (-760.f + 800.f) * 1.5f))
    {
        std::cout << "monitor_scaling logicalToPhysical150 => {" << logicalToPhysical150.x << ", " << logicalToPhysical150.y << "}\n";
        return fail("logicalToPhysical150");
    }

    // Physical-to-logical inverse for mixed scale
    const Vec2 physicalPoint{1920.f, 125.f};
    Vec2 physicalToLogical = monitors.physicalToLogical(physicalPoint);
    if (!near(physicalToLogical.x, 1536.f) || !near(physicalToLogical.y, 100.f))
    {
        std::cout << "monitor_scaling physicalToLogical => {" << physicalToLogical.x << ", " << physicalToLogical.y << "}\n";
        std::cout << "monitor_scaling physicalToLogical 1919 => {" << monitors.physicalToLogical({1919.f, 100.f}).x << ", "
                  << monitors.physicalToLogical({1919.f, 100.f}).y << "}\n";
        std::cout << "monitor_scaling physicalToLogical 1921 => {" << monitors.physicalToLogical({1921.f, 100.f}).x << ", "
                  << monitors.physicalToLogical({1921.f, 100.f}).y << "}\n";
        return fail("physicalToLogical");
    }

    // Logical index nearest fallback for mixed layout and gap fallback
    if (monitors.getMainMonitorIndex(Vec2i{1500, 100}) != 0)
        return fail("mainMonitorIndex(1500)");
    if (monitors.getMainMonitorIndex(Vec2i{2050, 100}) != 1)
        return fail("mainMonitorIndex(2050)");

    // Per monitor metric check (pixel-per-meter)
    constexpr float mmToMeter = 0.001f;
    const float ppm1ExpectedX = 1920.f / (400.f * mmToMeter);
    const float ppm1ExpectedY = 1080.f / (225.f * mmToMeter);
    Vec2 ppm1 = monitors.getPixelPerMeterForLogicalPoint({100.f, 100.f});
    if (!near(ppm1.x, ppm1ExpectedX, 2.f) || !near(ppm1.y, ppm1ExpectedY, 2.f))
        return fail("ppm1");

    const float ppm2ExpectedX = 1920.f / (384.f * mmToMeter);
    const float ppm2ExpectedY = 1080.f / (216.f * mmToMeter);
    Vec2 ppm2 = monitors.getPixelPerMeterForLogicalPoint({2100.f, 100.f});
    if (!near(ppm2.x, ppm2ExpectedX, 2.f) || !near(ppm2.y, ppm2ExpectedY, 2.f))
        return fail("ppm2");

    const float ppm3ExpectedX = 1536.f / (307.f * mmToMeter);
    const float ppm3ExpectedY = 1536.f / (173.f * mmToMeter);
    Vec2 ppm3 = monitors.getPixelPerMeterForLogicalPoint({3400.f, -700.f});
    if (!near(ppm3.x, ppm3ExpectedX, 3.f) || !near(ppm3.y, ppm3ExpectedY, 3.f))
        return fail("ppm3");

    return true;
}

bool test_monitor_fallback_and_selection()
{
    auto monitorEnumerator = std::make_unique<FakeWindowEnumerator>();
    monitorEnumerator->addMonitor({{0, 0}, {1920, 1080}, {400, 225}, {1.f, 1.f}});
    monitorEnumerator->addMonitor({{3000, 0}, {1600, 1200}, {256, 144}, {1.5f, 1.5f}});
    monitorEnumerator->setPrimaryMonitor(1);

    Monitors monitors(std::move(monitorEnumerator));

    Vec2i monitorPos;
    Vec2i monitorSize;
    monitors.getMainMonitorWorkingArea(monitorPos, monitorSize);

    if (monitorPos.x != 2000 || monitorPos.y != 0 || monitorSize.x != 1067 || monitorSize.y != 800)
        return false;

    if (monitors.getMainMonitorIndex(Vec2i{1500, 200}) != 0)
        return false;

    if (monitors.getMainMonitorIndex(Vec2i{2500, 200}) != 1)
        return false;

    if (monitors.getPrimaryMonitorIndex() != 1)
        return false;

    Vec2i physicalSize = monitors.getMonitorPhysicalSize(1);
    if (physicalSize.x != 256 || physicalSize.y != 144)
        return false;

    return true;
}

bool test_settings_validation_hardening()
{
    std::filesystem::path tmpPath = std::filesystem::temp_directory_path() / "pet-settings-validation.yaml";
    {
        std::ofstream out(tmpPath);
        out << "Game:\n";
        out << "  FPS: bad-value\n";
        out << "  RandomSeed: 42\n";
        out << "  UnknownKey: 9\n";
        out << "Physic:\n";
        out << "  Bounciness: 2.0\n";
        out << "  FootBasementWidth: -4\n";
        out << "UnknownSection:\n";
        out << "  Foo: 1\n";
    }

    GameData data{};
    Setting::ValidationReport report;
    const bool loaded = Setting::instance().importFile(tmpPath.string().c_str(), data, report);

    if (loaded)
        return false;

    if (report.errors.empty())
        return false;

    if (data.FPS != 60)
        return false;

    if (data.randomSeed != 42)
        return false;

    if (data.bounciness > 1.f)
        return false;

    return true;
}

bool test_update_metadata_validation()
{
    const auto fail = [](const char* step) {
        std::cout << "update_metadata_validation fail: " << step << "\n";
        return false;
    };

    UpdateMetadata metadata;
    std::string error;

    // Good manifest-like payload
    if (!Updater::parseManifestForTest("{\"package\":\"https://github.com/example/app-v1.exe\",\"checksum\":\"abcdef\"}", metadata))
        return fail("parse valid manifest");

    if (metadata.checksumAlgorithm != "sha256")
        return fail("default checksum algorithm should be sha256");
    if (metadata.packageUrl != "https://github.com/example/app-v1.exe")
        return fail("default package URL parsed");

    // Invalid host or malformed package destination must fail manifest parse.
    if (Updater::parseManifestForTest("{\"package\":\"https://not-trusted.example.com/app-v1.exe\",\"checksum\":\"a\"}",
                                      metadata))
        return fail("untrusted host accepted");

    // Invalid package destination but with no package key should still be parseable payload metadata.
    if (!Updater::parseManifestForTest("{\"checksum\":\"a\"}", metadata))
    {
        return fail("checksum-only parse");
    }
    if (metadata.checksum != "a")
        return fail("checksum-only parse value");

    // Invalid checksum length
    if (Updater::validateMetadataEnvelopeForTest(metadata, error))
    {
        return fail("invalid checksum length accepted");
    }

    metadata.tag = "1.0.0";
    metadata.checksum = std::string(64, 'a');
    metadata.checksumAlgorithm = "sha256";
    metadata.signatureAlgorithm = "sha256";
    metadata.packageName = "app-v1.exe";
    metadata.packageUrl = "https://github.com/example/app-v1.exe";
    if (!Updater::validateMetadataEnvelopeForTest(metadata, error))
    {
        return fail("valid envelope");
    }

    metadata.signature = std::string(64, 'a');
    metadata.signatureAlgorithm = "bcrypt";
    if (Updater::validateMetadataEnvelopeForTest(metadata, error))
        return fail("unsupported signature algorithm accepted");

    metadata.signatureAlgorithm = "sha256";
    metadata.signature = std::string(64, 'a');
    if (!Updater::verifySignedMetadataForTest(metadata, error))
        return fail("valid sha256 signature rejected");

    metadata.signature = std::string(64, 'b');
    if (Updater::verifySignedMetadataForTest(metadata, error))
        return fail("invalid sha256 signature accepted");

    return true;
}

bool test_cursor_normalization_for_mixed_scale()
{
    auto monitorEnumerator = std::make_unique<FakeWindowEnumerator>();
    monitorEnumerator->addMonitor({{0, 0}, {1920, 1080}, {400, 225}, {1.f, 1.f}});
    monitorEnumerator->addMonitor({{1920, 0}, {1920, 1080}, {384, 216}, {1.25f, 1.25f}});
    monitorEnumerator->addMonitor({{3840, 0}, {1536, 1536}, {307, 173}, {1.5f, 1.5f}});

    Monitors monitors(std::move(monitorEnumerator));

    const auto fail = [](const char* step) {
        std::cout << "cursor_normalization fail: " << step << "\n";
        return false;
    };

    const Vec2 logicalWindowPos{1921.f, 24.f};
    const Vec2 logicalWindowSize{320.f, 180.f};
    const Vec2 physicalWindowSize{static_cast<float>(static_cast<int>(logicalWindowSize.x * 1.25f)),
                                 static_cast<float>(static_cast<int>(logicalWindowSize.y * 1.25f))};

    auto normalize = [&](const Vec2& cursor) {
        return monitors.normalizeWindowCursor(cursor,
                                             {logicalWindowPos, logicalWindowSize, physicalWindowSize, 0.0001f, 0.0001f});
    };

    const Vec2 logicalInput{120.f, 40.f};
    const Vec2 logicalResult = normalize(logicalInput);
    if (!near(logicalResult.x, logicalInput.x) || !near(logicalResult.y, logicalInput.y))
        return fail("logical passthrough");

    const Vec2 physicalInput{logicalInput.x * 1.25f, logicalInput.y * 1.25f};
    const Vec2 physicalResult = normalize(physicalInput);
    if (!near(physicalResult.x, logicalInput.x) || !near(physicalResult.y, logicalInput.y))
        return fail("physical to logical conversion");

    // outside physical bounds but closest to logical should clamp/keep fallback path.
    const Vec2 outsidePhysical{logicalWindowSize.x * 1.25f + 40.f, logicalWindowSize.y * 1.25f + 16.f};
    const Vec2 outsideResult = normalize(outsidePhysical);
    if (!std::isfinite(outsideResult.x) || !std::isfinite(outsideResult.y) || outsideResult.x < 0.f || outsideResult.y < 0.f)
        return fail("outside cursor fallback");

    return true;
}
} // namespace

int main()
{
    const std::vector<std::pair<const char*, bool (*)()>> tests = {
        {"monitor_scaling_transform", test_monitor_scaling_transform},
        {"monitor_fallback_and_selection", test_monitor_fallback_and_selection},
        {"settings_validation_hardening", test_settings_validation_hardening},
        {"update_metadata_validation", test_update_metadata_validation},
        {"cursor_normalization_for_mixed_scale", test_cursor_normalization_for_mixed_scale},
    };

    int failed = 0;
    for (const auto& [name, fn] : tests)
    {
        const bool ok = fn();
        std::cout << "[" << (ok ? "PASS" : "FAIL") << "] " << name << "\n";
        if (!ok)
            ++failed;
    }

    if (failed)
    {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
