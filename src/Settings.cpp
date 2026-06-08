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
#include <system_error>
#include <string>
#include <string_view>
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

constexpr std::array<std::string_view, 8> kAllowedSections = {"Game", "Physic", "GamePlay", "Window", "Style",
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

template <typename T, size_t N>
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
void warnUnknownKeys(const YAML::Node& section, std::string_view sectionName,
                    const std::array<std::string_view, N>& allowed)
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
            warning((std::string("Unknown setting key '") + key + "' in section '" + std::string(sectionName) +
                     "', using defaults.")
                        .c_str());
        }
    }
}

void warnUnknownSections(const YAML::Node& root)
{
    if (root.IsMap())
    {
        for (const auto& item : root)
        {
            if (!item.first.IsScalar())
                continue;

            const std::string name = item.first.as<std::string>("");
            if (!name.empty() && !isAllowedSection(name))
                warning((std::string("Unknown settings section '") + name + "', ignoring.").c_str());
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
                    warning((std::string("Unknown settings section '") + name + "', ignoring.").c_str());
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

void Setting::importFile(const char* src, GameData& data)
{
    applyDefaults(data);

    YAML::Node root;
    try
    {
        root = YAML::LoadFile(src);
    }
    catch (...)
    {
        warning(std::string("Could not read setting file: ") + src + ", using defaults");
        return;
    }

    if (!root.IsMap() && !root.IsSequence())
    {
        warning(std::string("Invalid setting file format: ") + src + ", using defaults");
        return;
    }

    warnUnknownSections(root);

    YAML::Node section;
    if (getSection(root, "Game", section))
    {
        warnUnknownKeys(section, "Game", kGameSectionKeys);

        readScalar(section, "FPS", data.FPS);
        readScalar(section, "RandomSeed", data.randomSeed);
    }

    if (getSection(root, "Physic", section))
    {
        warnUnknownKeys(section, "Physic", kPhysicsSectionKeys);

        int physicFrameRate;
        if (readScalar(section, "PhysicFrameRate", physicFrameRate))
            data.physicFrameRate = physicFrameRate;

        readScalar(section, "Bounciness", data.bounciness);
        readScalar(section, "GravityX", data.gravity.x);
        readScalar(section, "GravityY", data.gravity.y);
        readScalar(section, "Friction", data.friction);

        float maxVelocity = 0.f;
        if (readScalar(section, "ContinuousCollisionMaxVelocity", maxVelocity))
            data.continuousCollisionMaxSqrVelocity = maxVelocity;

        int basementWidth;
        if (readScalar(section, "FootBasementWidth", basementWidth))
            data.footBasementWidth = basementWidth;

        int basementHeight;
        if (readScalar(section, "FootBasementHeight", basementHeight))
            data.footBasementHeight = basementHeight;

        readScalar(section, "CollisionPixelRatioStopMovement", data.collisionPixelRatioStopMovement);
        readScalar(section, "IsGroundedDetection", data.isGroundedDetection);
        readScalar(section, "InputReleaseImpulse", data.releaseImpulse);
    }

    if (getSection(root, "GamePlay", section))
    {
        warnUnknownKeys(section, "GamePlay", kGamePlaySectionKeys);

        readScalar(section, "CoyoteTimeCursorMovement", data.coyoteTimeCursorPos);
    }

    if (getSection(root, "Window", section))
    {
        warnUnknownKeys(section, "Window", kWindowSectionKeys);

        bool value = false;
        if (readScalar(section, "FullScreenWindow", value))
            data.fullScreenWindow = value;

        if (readScalar(section, "ShowWindow", value))
            data.showWindow = value;

        if (readScalar(section, "ShowFrameBufferBackground", value))
            data.showFrameBufferBackground = value;

        if (readScalar(section, "UseForwardWindow", value))
            data.useForwardWindow = value;

        if (readScalar(section, "UseMousePassThoughWindow", value))
            data.useMousePassThoughWindow = value;
    }

    if (getSection(root, "Style", section))
    {
        warnUnknownKeys(section, "Style", kStyleSectionKeys);

        std::string theme;
        if (readScalar(section, "Theme", theme))
            data.styleName = theme;
    }

    if (getSection(root, "Accessibility", section))
    {
        warnUnknownKeys(section, "Accessibility", kAccessibilitySectionKeys);

        int scale;
        if (readScalar(section, "Scale", scale))
            data.scale = scale;

        readScalar(section, "TextScale", data.textScale);
    }

    if (getSection(root, "Debug", section))
    {
        warnUnknownKeys(section, "Debug", kDebugSectionKeys);

        bool debugValue = false;
        if (readScalar(section, "ShowEdgeDetection", debugValue))
            data.debugEdgeDetection = debugValue;
    }

    Setting::sanitize(data);
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
