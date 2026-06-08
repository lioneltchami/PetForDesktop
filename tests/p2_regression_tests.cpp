#include "Engine/Monitors.hpp"
#include "Engine/Platform/IWindowEnumerator.hpp"
#include "Engine/Settings.hpp"
#include "Engine/Updater.hpp"

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
        (void)index;
        return Vec2i::zero();
    }
};

bool near(float lhs, float rhs, float eps = 1e-4f)
{
    return std::fabs(lhs - rhs) < eps;
}

bool test_monitor_scaling_transform()
{
    auto monitorEnumerator = std::make_unique<FakeWindowEnumerator>();
    monitorEnumerator->addMonitor({{0, 0}, {1920, 1080}, {1.f, 1.f}});      // 100%
    monitorEnumerator->addMonitor({{1920, 0}, {1920, 1080}, {1.25f, 1.25f}}); // 125%
    monitorEnumerator->addMonitor({{3840, -1200}, {1536, 1536}, {1.5f, 1.5f}}); // 150%

    Monitors monitors(std::move(monitorEnumerator));

    // 100% DPI conversion
    Vec2 logicalToPhysical100 = monitors.logicalToPhysical({100.f, 25.f});
    if (!near(logicalToPhysical100.x, 100.f) || !near(logicalToPhysical100.y, 25.f))
        return false;

    // 125% conversion
    Vec2 logicalToPhysical125 = monitors.logicalToPhysical({1536.f + 40.f, 100.f});
    if (!near(logicalToPhysical125.x, 1920.f + (40.f * 1.25f)) || !near(logicalToPhysical125.y, 125.f))
        return false;

    // 150% conversion
    Vec2 logicalToPhysical150 = monitors.logicalToPhysical({2600.f, -760.f});
    if (!near(logicalToPhysical150.x, 3840.f + (2600.f - 2560.f) * 1.5f) ||
        !near(logicalToPhysical150.y, -1200.f + (-760.f + 800.f) * 1.5f))
        return false;

    // Physical-to-logical inverse for mixed scale
    const Vec2 physicalPoint{1920.f, 125.f};
    Vec2 physicalToLogical = monitors.physicalToLogical(physicalPoint);
    if (!near(physicalToLogical.x, 1536.f) || !near(physicalToLogical.y, 100.f))
        return false;

    // Logical index nearest fallback for mixed layout and gap fallback
    if (monitors.getMonitorIndexForLogicalPoint(Vec2i{1960, 100}) != 0)
        return false;
    if (monitors.getMonitorIndexForLogicalPoint(Vec2i{2050, 100}) != 1)
        return false;

    return true;
}

bool test_monitor_fallback_and_selection()
{
    auto monitorEnumerator = std::make_unique<FakeWindowEnumerator>();
    monitorEnumerator->addMonitor({{0, 0}, {1920, 1080}, {1.f, 1.f}});
    monitorEnumerator->addMonitor({{3000, 0}, {1600, 1200}, {1.5f, 1.5f}});
    monitorEnumerator->setPrimaryMonitor(1);

    Monitors monitors(std::move(monitorEnumerator));

    Vec2i monitorPos;
    Vec2i monitorSize;
    monitors.getMainMonitorWorkingArea(monitorPos, monitorSize);

    if (monitorPos.x != 2000 || monitorPos.y != 0 || monitorSize.x != 1067 || monitorSize.y != 800)
        return false;

    if (monitors.getMonitorIndexForLogicalPoint(Vec2i{1500, 200}) != 0)
        return false;

    if (monitors.getMonitorIndexForLogicalPoint(Vec2i{2500, 200}) != 1)
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
    UpdateMetadata metadata;
    std::string error;

    // Good manifest-like payload
    if (!Updater::parseManifestForTest("{\"package\":\"https://example.com/app-v1.exe\",\"checksum\":\"abcdef\"}", metadata))
        return false;

    // Invalid checksum length
    if (Updater::validateMetadataEnvelopeForTest(metadata, error))
        return false;

    metadata.tag = "1.0.0";
    metadata.checksum = std::string(64, 'a');
    metadata.checksumAlgorithm = "sha256";
    metadata.packageName = "app-v1.exe";
    metadata.packageUrl = "https://example.com/app-v1.exe";
    if (!Updater::validateMetadataEnvelopeForTest(metadata, error))
        return false;

    metadata.signatureAlgorithm = "bcrypt";
    if (Updater::validateMetadataEnvelopeForTest(metadata, error))
        return false;

    metadata.signatureAlgorithm = "sha256";
    metadata.signature = std::string(64, 'a');
    if (!Updater::verifySignedMetadataForTest(metadata, error))
        return false;

    metadata.signature = std::string(64, 'b');
    if (Updater::verifySignedMetadataForTest(metadata, error))
        return false;

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
