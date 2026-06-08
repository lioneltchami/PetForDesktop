#include "Engine/Settings.hpp"
#include "Engine/Log.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <errno.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>

#ifndef _WIN32
inline int fopen_s(FILE** file, const char* filename, const char* mode)
{
    *file = fopen(filename, mode);
    if (!*file)
        return errno;
    return 0;
}
#endif

namespace
{
constexpr int kMinFPS = 1;
constexpr int kMaxFPS = 500;
constexpr int kMinPhysicsFrameRate = 1;
constexpr int kMaxPhysicsFrameRate = 500;
constexpr int kMinScale = 1;
constexpr int kMaxScale = 10;
constexpr float kMinTextScale = 0.3f;
constexpr float kMaxTextScale = 2.f;
constexpr float kMinGravity = -2000.f;
constexpr float kMaxGravity = 2000.f;
constexpr float kMinBounciness = 0.f;
constexpr float kMaxBounciness = 1.f;
constexpr float kMinFriction = 0.f;
constexpr float kMaxFriction = 1.f;
constexpr float kMinCollisionRatio = 0.f;
constexpr float kMaxCollisionRatio = 1.f;
constexpr float kMinPositiveFloat = 0.0001f;
constexpr int kMinFootBasement = 1;

constexpr std::array<std::string_view, 9> kAllowedSections = {"Game", "Physic", "GamePlay", "Window", "Style",
                                                           "Accessibility", "Debug", "Graphics", "Display"};

constexpr std::array<std::string_view, 2> kGameSectionKeys = {"FPS", "RandomSeed"};
constexpr std::array<std::string_view, 11> kPhysicsSectionKeys = {
    "PhysicFrameRate",
    "Bounciness",
    "GravityX",
    "GravityY",
    "Friction",
    "ContinuousCollisionMaxVelocity",
    "FootBasementWidth",
    "FootBasementHeight",
    "CollisionPixelRatioStopMovement",
    "IsGroundedDetection",
    "InputReleaseImpulse"};
constexpr std::array<std::string_view, 1> kGamePlaySectionKeys = {"CoyoteTimeCursorMovement"};
constexpr std::array<std::string_view, 5> kWindowSectionKeys = {"FullScreenWindow", "ShowWindow", "ShowFrameBufferBackground",
                                                                "UseForwardWindow", "UseMousePassThoughWindow"};
constexpr std::array<std::string_view, 1> kStyleSectionKeys = {"Theme"};
constexpr std::array<std::string_view, 2> kAccessibilitySectionKeys = {"Scale", "TextScale"};
constexpr std::array<std::string_view, 1> kDebugSectionKeys = {"ShowEdgeDetection"};

inline bool isAllowedSection(std::string_view sectionName)
{
    return std::find(kAllowedSections.begin(), kAllowedSections.end(), sectionName) != kAllowedSections.end();
}

void addValidationIssue(Setting::ValidationReport& report, const std::string& filePath, const std::string& section,
                       const std::string& field, const std::string& message, bool error)
{
    Setting::ValidationIssue issue{
        .filePath = filePath, .section = section, .field = field, .message = message, .severity = error ? "error" : "warning"};

    if (error)
    {
        report.valid  = false;
        report.errors.emplace_back(std::move(issue));
    }
    else
    {
        report.warnings.emplace_back(std::move(issue));
    }
}

template <typename T>
bool readScalar(const YAML::Node& section, const char* key, T& value)
{
    const YAML::Node entry = section[key];
    if (!entry)
        return false;

    try
    {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
        {
            const T typedValue = entry.as<T>();
            if (!std::isfinite(typedValue))
                return false;
            value = typedValue;
            return true;
        }
        else
        {
            value = entry.as<T>();
            return true;
        }
    }
    catch (...)
    {
        return false;
    }
}

template <typename T>
bool readScalarWithReport(const YAML::Node& section, const char* sectionName, const char* key, T& value,
                         Setting::ValidationReport& report, const std::string& filePath, bool isRequired = false)
{
    const YAML::Node entry = section[key];
    if (!entry)
    {
        if (isRequired)
            addValidationIssue(report, filePath, sectionName, key, "Missing required setting value", true);
        return false;
    }

    if (!readScalar(section, key, value))
    {
        addValidationIssue(report, filePath, sectionName, key, "Invalid type or malformed value", true);
        return false;
    }

    return true;
}

template <size_t N>
bool inArray(std::string_view key, const std::array<std::string_view, N>& allowed)
{
    for (const auto& candidate : allowed)
    {
        if (key == candidate)
            return true;
    }

    return false;
}

template <size_t N>
void warnUnknownKeys(const YAML::Node& section, std::string_view sectionName, const std::string& filePath,
                    const std::array<std::string_view, N>& allowed, Setting::ValidationReport& report)
{
    if (!section.IsMap())
        return;

    for (const auto& item : section)
    {
        const auto keyNode = item.first;
        if (!keyNode)
            continue;

        const std::string key = keyNode.as<std::string>("");
        if (key.empty())
            continue;

        if (!inArray(std::string_view{key}, allowed))
        {
            addValidationIssue(report, filePath, std::string(sectionName), key, "Unknown key, using defaults", false);
            warning((std::string("Unknown setting key '") + key + "' in section '" + std::string(sectionName) +
                     "', using defaults.")
                        .c_str());
        }
    }
}

void warnUnknownSections(const YAML::Node& root, const std::string& filePath, Setting::ValidationReport& report)
{
    if (root.IsMap())
    {
        for (const auto& item : root)
        {
            if (!item.first.IsScalar())
                continue;

            const std::string name = item.first.as<std::string>("");
            if (!name.empty() && !isAllowedSection(name))
            {
                addValidationIssue(report, filePath, name, std::string(), "Unknown section, ignoring", false);
                warning((std::string("Unknown settings section '") + name + "', ignoring.").c_str());
            }
        }
        return;
    }

    if (root.IsSequence())
    {
        for (const auto& entry : root)
        {
            if (!entry.IsMap())
                continue;

            for (const auto& item : entry)
            {
                if (!item.first.IsScalar())
                    continue;

                const std::string name = item.first.as<std::string>("");
                if (!name.empty() && !isAllowedSection(name))
                {
                    addValidationIssue(report, filePath, name, std::string(), "Unknown section, ignoring", false);
                    warning((std::string("Unknown settings section '") + name + "', ignoring.").c_str());
                }
            }
        }
    }
}

bool getSection(const YAML::Node& root, const char* sectionName, YAML::Node& section)
{
    if (root.IsMap())
    {
        const YAML::Node candidate = root[sectionName];
        if (!candidate)
            return false;

        section = candidate;
        return true;
    }

    if (!root.IsSequence())
        return false;

    for (const auto& entry : root)
    {
        if (!entry.IsMap())
            continue;

        const YAML::Node candidate = entry[sectionName];
        if (!candidate)
            continue;

        section = candidate;
        return true;
    }

    return false;
}

void clampAndNormalize(GameData& data)
{
    auto clampFloat = [](float value, float minValue, float maxValue, float fallback) {
        if (!std::isfinite(value))
            return fallback;

        return std::clamp(value, minValue, maxValue);
    };

    auto clampInt = [](int value, int minValue, int maxValue) {
        return std::clamp(value, minValue, maxValue);
    };

    data.FPS       = clampInt(data.FPS, kMinFPS, kMaxFPS);
    data.scale     = clampInt(data.scale, kMinScale, kMaxScale);
    data.textScale = clampFloat(data.textScale, kMinTextScale, kMaxTextScale, 1.f);

    data.physicFrameRate = clampInt(data.physicFrameRate, kMinPhysicsFrameRate, kMaxPhysicsFrameRate);

    data.gravity.x = clampFloat(data.gravity.x, kMinGravity, kMaxGravity, 0.f);
    data.gravity.y = clampFloat(data.gravity.y, kMinGravity, kMaxGravity, 9.81f);
    if (data.gravity.sqrLength() > 0.f && std::isfinite(data.gravity.x) && std::isfinite(data.gravity.y))
        data.gravityDir = data.gravity.normalized();
    else
        data.gravityDir = {0.f, 1.f};

    data.bounciness = clampFloat(data.bounciness, kMinBounciness, kMaxBounciness, 0.6f);
    data.friction   = clampFloat(data.friction, kMinFriction, kMaxFriction, 0.85f);
    data.continuousCollisionMaxSqrVelocity =
        std::max(kMinPositiveFloat, clampFloat(std::sqrt(data.continuousCollisionMaxSqrVelocity),
                                                kMinPositiveFloat, 200.f, kMinPositiveFloat));
    data.continuousCollisionMaxSqrVelocity = data.continuousCollisionMaxSqrVelocity * data.continuousCollisionMaxSqrVelocity;

    data.footBasementWidth = std::max(kMinFootBasement, data.footBasementWidth);
    data.footBasementHeight = std::max(kMinFootBasement, data.footBasementHeight);

    data.collisionPixelRatioStopMovement = clampFloat(data.collisionPixelRatioStopMovement, kMinCollisionRatio, kMaxCollisionRatio,
                                                    0.3f);
    data.isGroundedDetection = std::max(kMinPositiveFloat, data.isGroundedDetection);
    data.releaseImpulse     = std::max(kMinPositiveFloat, data.releaseImpulse);
    data.coyoteTimeCursorPos = std::max(kMinPositiveFloat, data.coyoteTimeCursorPos);

    if (data.styleName.empty())
        data.styleName = "PetForDesktop";
}

void applyDefaults(GameData& data)
{
    data.FPS        = 60;
    data.scale      = 2;
    data.textScale  = 1.f;
    data.randomSeed = -1;

    data.physicFrameRate = 60;
    data.bounciness     = 0.6f;
    data.gravity        = {0.f, 9.81f};
    data.friction       = 0.85f;
    data.continuousCollisionMaxSqrVelocity = 1600.f;
    data.collisionPixelRatioStopMovement   = 0.3f;
    data.isGroundedDetection               = 1.f;
    data.releaseImpulse                    = 1.f;
    data.footBasementWidth                 = 6;
    data.footBasementHeight                = 2;
    data.coyoteTimeCursorPos               = 0.05f;

    data.fullScreenWindow          = true;
    data.showWindow                = false;
    data.showFrameBufferBackground = false;
    data.useForwardWindow          = true;
    data.useMousePassThoughWindow  = true;

    data.styleName                 = "PetForDesktop";
    data.debugEdgeDetection        = false;
}
}

bool Setting::importFile(const char* src, GameData& data, ValidationReport& report)
{
    report = ValidationReport{};
    const std::string srcPath = src ? src : "";

    applyDefaults(data);

    YAML::Node root;
    try
    {
        if (!src)
            throw std::runtime_error("Settings source path is null");
        root = YAML::LoadFile(src);
    }
    catch (...)
    {
        addValidationIssue(report, srcPath, std::string(), std::string(), "Unable to read settings file", true);
        return false;
    }

    if (!root.IsMap() && !root.IsSequence())
    {
        addValidationIssue(report, srcPath, std::string(), std::string(), "Invalid settings file format", true);
        return false;
    }

    warnUnknownSections(root, srcPath, report);

    YAML::Node section;
    if (getSection(root, "Game", section))
    {
        warnUnknownKeys(section, "Game", srcPath, kGameSectionKeys, report);

        readScalarWithReport(section, "Game", "FPS", data.FPS, report, srcPath);
        readScalarWithReport(section, "Game", "RandomSeed", data.randomSeed, report, srcPath);
    }

    if (getSection(root, "Physic", section))
    {
        warnUnknownKeys(section, "Physic", srcPath, kPhysicsSectionKeys, report);

        readScalarWithReport(section, "Physic", "PhysicFrameRate", data.physicFrameRate, report, srcPath);
        readScalarWithReport(section, "Physic", "Bounciness", data.bounciness, report, srcPath);
        readScalarWithReport(section, "Physic", "GravityX", data.gravity.x, report, srcPath);
        readScalarWithReport(section, "Physic", "GravityY", data.gravity.y, report, srcPath);
        readScalarWithReport(section, "Physic", "Friction", data.friction, report, srcPath);
        readScalarWithReport(section, "Physic", "CollisionPixelRatioStopMovement", data.collisionPixelRatioStopMovement, report,
                             srcPath);
        readScalarWithReport(section, "Physic", "IsGroundedDetection", data.isGroundedDetection, report, srcPath);
        readScalarWithReport(section, "Physic", "InputReleaseImpulse", data.releaseImpulse, report, srcPath);

        float maxVelocity = 0.f;
        if (readScalarWithReport(section, "Physic", "ContinuousCollisionMaxVelocity", maxVelocity, report, srcPath))
            data.continuousCollisionMaxSqrVelocity = maxVelocity;

        int basementWidth = data.footBasementWidth;
        if (readScalarWithReport(section, "Physic", "FootBasementWidth", basementWidth, report, srcPath))
            data.footBasementWidth = basementWidth;

        int basementHeight = data.footBasementHeight;
        if (readScalarWithReport(section, "Physic", "FootBasementHeight", basementHeight, report, srcPath))
            data.footBasementHeight = basementHeight;
    }

    if (getSection(root, "GamePlay", section))
    {
        warnUnknownKeys(section, "GamePlay", srcPath, kGamePlaySectionKeys, report);

        readScalarWithReport(section, "GamePlay", "CoyoteTimeCursorMovement", data.coyoteTimeCursorPos, report, srcPath);
    }

    if (getSection(root, "Window", section))
    {
        warnUnknownKeys(section, "Window", srcPath, kWindowSectionKeys, report);

        bool value = false;
        if (readScalarWithReport(section, "Window", "FullScreenWindow", value, report, srcPath))
            data.fullScreenWindow = value;

        if (readScalarWithReport(section, "Window", "ShowWindow", value, report, srcPath))
            data.showWindow = value;

        if (readScalarWithReport(section, "Window", "ShowFrameBufferBackground", value, report, srcPath))
            data.showFrameBufferBackground = value;

        if (readScalarWithReport(section, "Window", "UseForwardWindow", value, report, srcPath))
            data.useForwardWindow = value;

        if (readScalarWithReport(section, "Window", "UseMousePassThoughWindow", value, report, srcPath))
            data.useMousePassThoughWindow = value;
    }

    if (getSection(root, "Style", section))
    {
        warnUnknownKeys(section, "Style", srcPath, kStyleSectionKeys, report);

        std::string theme;
        if (readScalarWithReport(section, "Style", "Theme", theme, report, srcPath))
            data.styleName = theme;
    }

    if (getSection(root, "Accessibility", section))
    {
        warnUnknownKeys(section, "Accessibility", srcPath, kAccessibilitySectionKeys, report);

        int scale = data.scale;
        if (readScalarWithReport(section, "Accessibility", "Scale", scale, report, srcPath))
            data.scale = scale;

        readScalarWithReport(section, "Accessibility", "TextScale", data.textScale, report, srcPath);
    }

    if (getSection(root, "Debug", section))
    {
        warnUnknownKeys(section, "Debug", srcPath, kDebugSectionKeys, report);

        bool debugValue = false;
        if (readScalarWithReport(section, "Debug", "ShowEdgeDetection", debugValue, report, srcPath))
            data.debugEdgeDetection = debugValue;
    }

    Setting::sanitize(data);
    return report.valid;
}

void Setting::importFile(const char* src, GameData& data)
{
    ValidationReport report;
    const std::string srcPath = src ? src : "";
    if (!importFile(src, data, report))
    {
        warning(std::string("Invalid setting file: ") + srcPath + ", using defaults where needed");
        for (const auto& warningItem : report.warnings)
            warning((warningItem.section + ": " + warningItem.field + ": " + warningItem.message).c_str());
        for (const auto& errorItem : report.errors)
            logf("Error in settings (%s): %s\n", errorItem.section.c_str(), (errorItem.section + ": " + errorItem.field + ": " + errorItem.message).c_str());
    }
    else if (!report.warnings.empty())
    {
        for (const auto& warningItem : report.warnings)
            warning((warningItem.section + ": " + warningItem.field + ": " + warningItem.message).c_str());
    }
}

void Setting::exportFile(const char* dest, GameData& data)
{
    Setting::clampForRuntime(data);

    const std::filesystem::path destinationPath(dest);
    const std::filesystem::path tempPath = destinationPath.string() + ".tmp";

    std::ofstream output(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output)
    {
        logf("Could not open temporary file \"%s\" for settings write\n", tempPath.string().c_str());
        return;
    }

    YAML::Emitter out;
    out << YAML::BeginSeq;
    std::string section;
    {
        section = "Game";
        out << YAML::BeginMap;
        out << YAML::Block << section;
        out << YAML::BeginMap;
        out << YAML::Key << "FPS" << YAML::Value << data.FPS;
        out << YAML::Key << "RandomSeed" << YAML::Value << data.randomSeed;
        out << YAML::EndMap;
        out << YAML::EndMap;
    }

    {
        section = "Physic";
        out << YAML::BeginMap;
        out << section;
        out << YAML::BeginMap;
        out << YAML::Key << "PhysicFrameRate" << YAML::Value << data.physicFrameRate;
        out << YAML::Key << "Bounciness" << YAML::Value << data.bounciness;
        out << YAML::Key << "GravityX" << YAML::Value << data.gravity.x;
        out << YAML::Key << "GravityY" << YAML::Value << data.gravity.y;
        out << YAML::Key << "Friction" << YAML::Value << data.friction;
        const float continuousCollisionMaxVelocity = std::sqrt(data.continuousCollisionMaxSqrVelocity);
        out << YAML::Key << "ContinuousCollisionMaxVelocity" << YAML::Value << continuousCollisionMaxVelocity;
        out << YAML::Key << "FootBasementWidth" << YAML::Value << data.footBasementWidth;
        out << YAML::Key << "FootBasementHeight" << YAML::Value << data.footBasementHeight;
        out << YAML::Key << "CollisionPixelRatioStopMovement" << YAML::Value << data.collisionPixelRatioStopMovement;
        out << YAML::Key << "IsGroundedDetection" << YAML::Value << data.isGroundedDetection;
        out << YAML::Key << "InputReleaseImpulse" << YAML::Value << data.releaseImpulse;
        out << YAML::EndMap;
        out << YAML::EndMap;
    }

    {
        section = "GamePlay";
        out << YAML::BeginMap;
        out << section;
        out << YAML::BeginMap;
        out << YAML::Key << "CoyoteTimeCursorMovement" << YAML::Value << data.coyoteTimeCursorPos;
        out << YAML::EndMap;
        out << YAML::EndMap;
    }

    {
        section = "Window";
        out << YAML::BeginMap;
        out << section;
        out << YAML::BeginMap;
        out << YAML::Key << "FullScreenWindow" << YAML::Value << data.fullScreenWindow;
        out << YAML::Key << "ShowWindow" << YAML::Value << data.showWindow;
        out << YAML::Key << "ShowFrameBufferBackground" << YAML::Value << data.showFrameBufferBackground;
        out << YAML::Key << "UseForwardWindow" << YAML::Value << data.useForwardWindow;
        out << YAML::Key << "UseMousePassThoughWindow" << YAML::Value << data.useMousePassThoughWindow;
        out << YAML::EndMap;
        out << YAML::EndMap;
    }

    {
        section = "Style";
        out << YAML::BeginMap;
        out << section;
        out << YAML::BeginMap;
        out << YAML::Key << "Theme" << YAML::Value << data.styleName;
        out << YAML::EndMap;
        out << YAML::EndMap;
    }

    {
        section = "Accessibility";
        out << YAML::BeginMap;
        out << section;
        out << YAML::BeginMap;
        out << YAML::Key << "Scale" << YAML::Value << data.scale;
        out << YAML::Key << "TextScale" << YAML::Value << data.textScale;
        out << YAML::EndMap;
        out << YAML::EndMap;
    }

    {
        section = "Debug";
        out << YAML::BeginMap;
        out << section;
        out << YAML::BeginMap;
        out << YAML::Key << "ShowEdgeDetection" << YAML::Value << data.debugEdgeDetection;
        out << YAML::EndMap;
        out << YAML::EndMap;
    }

    out << YAML::EndSeq;
    output << out.c_str();
    output.close();

    if (!output)
    {
        logf("Failed to write settings temporary file \"%s\"\n", tempPath.string().c_str());
        std::error_code cleanupError;
        std::filesystem::remove(tempPath, cleanupError);
        return;
    }

    std::error_code removeError;
    std::filesystem::remove(destinationPath, removeError);

    std::error_code renameError;
    std::filesystem::rename(tempPath, destinationPath, renameError);
    if (renameError)
    {
        logf("Failed to swap settings file \"%s\" atomically: %s\n", destinationPath.string().c_str(),
             renameError.message().c_str());
        std::error_code cleanupError;
        std::filesystem::remove(tempPath, cleanupError);
    }
}

bool Setting::sanitize(GameData& data)
{
    clampAndNormalize(data);
    return true;
}

void Setting::clampForRuntime(GameData& data)
{
    clampAndNormalize(data);
}
