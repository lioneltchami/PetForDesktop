#include "Engine/Settings.hpp"
#include "Engine/Log.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <errno.h>

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

float clampFloat(float value, float minValue, float maxValue)
{
    return std::min(maxValue, std::max(minValue, value));
}

int clampInt(int value, int minValue, int maxValue)
{
    return std::min(maxValue, std::max(minValue, value));
}

bool startsWith(const std::string& text, const std::string& prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool extractSection(const YAML::Node& root, const char* key, YAML::Node& out)
{
    if (root[ key ])
    {
        out = root[ key ];
        return true;
    }

    if (!root.IsSequence())
        return false;

    for (auto item = root.begin(); item != root.end(); ++item)
    {
        if (!item->IsMap())
            continue;

        for (auto it = item->begin(); it != item->end(); ++it)
        {
            if (it->first.as<std::string>() == key)
            {
                out = it->second;
                return true;
            }
        }
    }

    return false;
}

template <typename T>
bool readScalar(const YAML::Node& section, const char* key, T& value)
{
    if (!section[ key ])
        return false;

    try
    {
        value = section[ key ].as<T>();
        return true;
    }
    catch (...) // malformed setting value
    {
        return false;
    }
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
    data.continuousCollisionMaxSqrVelocity = 40.f;
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

void sanitize(GameData& data)
{
    data.FPS        = clampInt(data.FPS, kMinFPS, kMaxFPS);
    data.scale      = clampInt(data.scale, kMinScale, kMaxScale);
    data.textScale  = clampFloat(data.textScale, kMinTextScale, kMaxTextScale);

    data.physicFrameRate = clampInt(data.physicFrameRate, kMinPhysicsFrameRate, kMaxPhysicsFrameRate);

    data.gravity.x = clampFloat(data.gravity.x, kMinGravity, kMaxGravity);
    data.gravity.y = clampFloat(data.gravity.y, kMinGravity, kMaxGravity);
    data.gravityDir = data.gravity.sqrLength() > 0.f ? data.gravity.normalized() : Vec2{0.f, 1.f};

    data.bounciness = clampFloat(data.bounciness, 0.f, 1.f);
    data.friction   = clampFloat(data.friction, 0.f, 1.f);
    data.continuousCollisionMaxSqrVelocity =
        std::max(0.f, std::sqrt(std::pow(clampFloat(data.continuousCollisionMaxSqrVelocity, 0.f, FLT_MAX), 2.f));
    data.footBasementWidth  = std::max(1, data.footBasementWidth);
    data.footBasementHeight = std::max(1, data.footBasementHeight);
    data.collisionPixelRatioStopMovement = clampFloat(data.collisionPixelRatioStopMovement, 0.f, 1.f);
    data.isGroundedDetection           = std::max(0.f, data.isGroundedDetection);
    data.releaseImpulse                = std::max(0.f, data.releaseImpulse);
    data.coyoteTimeCursorPos           = std::max(0.f, data.coyoteTimeCursorPos);
}
} // namespace

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

    YAML::Node gameSection;
    if (extractSection(root, "Game", gameSection))
    {
        readScalar(gameSection["FPS"], "FPS", data.FPS);
        readScalar(gameSection["RandomSeed"], "RandomSeed", data.randomSeed);
    }

    YAML::Node physicSection;
    if (extractSection(root, "Physic", physicSection))
    {
        readScalar(physicSection["PhysicFrameRate"], "PhysicFrameRate", data.physicFrameRate);
        readScalar(physicSection["Bounciness"], "Bounciness", data.bounciness);
        readScalar(physicSection["GravityX"], "GravityX", data.gravity.x);
        readScalar(physicSection["GravityY"], "GravityY", data.gravity.y);
        readScalar(physicSection["Friction"], "Friction", data.friction);
        if (physicSection["ContinuousCollisionMaxVelocity"])
            data.continuousCollisionMaxSqrVelocity = physicSection["ContinuousCollisionMaxVelocity"].as<float>();
        readScalar(physicSection["FootBasementWidth"], "FootBasementWidth", data.footBasementWidth);
        readScalar(physicSection["FootBasementHeight"], "FootBasementHeight", data.footBasementHeight);
        readScalar(physicSection["CollisionPixelRatioStopMovement"], "CollisionPixelRatioStopMovement",
                 data.collisionPixelRatioStopMovement);
        readScalar(physicSection["IsGroundedDetection"], "IsGroundedDetection", data.isGroundedDetection);
        readScalar(physicSection["InputReleaseImpulse"], "InputReleaseImpulse", data.releaseImpulse);
    }

    YAML::Node gamePlaySection;
    if (extractSection(root, "GamePlay", gamePlaySection))
    {
        readScalar(gamePlaySection["CoyoteTimeCursorMovement"], "CoyoteTimeCursorMovement", data.coyoteTimeCursorPos);
    }

    YAML::Node windowSection;
    if (extractSection(root, "Window", windowSection))
    {
        if (windowSection["FullScreenWindow"])
            readScalar(windowSection["FullScreenWindow"], "FullScreenWindow", data.fullScreenWindow);
        if (windowSection["ShowWindow"])
            readScalar(windowSection["ShowWindow"], "ShowWindow", data.showWindow);
        if (windowSection["ShowFrameBufferBackground"])
            readScalar(windowSection["ShowFrameBufferBackground"], "ShowFrameBufferBackground",
                       data.showFrameBufferBackground);
        if (windowSection["UseForwardWindow"])
            readScalar(windowSection["UseForwardWindow"], "UseForwardWindow", data.useForwardWindow);
        if (windowSection["UseMousePassThoughWindow"])
            readScalar(windowSection["UseMousePassThoughWindow"], "UseMousePassThoughWindow",
                       data.useMousePassThoughWindow);
    }

    YAML::Node styleSection;
    if (extractSection(root, "Style", styleSection))
    {
        const auto theme = styleSection["Theme"];
        if (theme)
            data.styleName = theme.as<std::string>();
    }

    YAML::Node accessibility;
    if (extractSection(root, "Accessibility", accessibility))
    {
        readScalar(accessibility["Scale"], "Scale", data.scale);
        readScalar(accessibility["TextScale"], "TextScale", data.textScale);
    }

    YAML::Node debugSection;
    if (extractSection(root, "Debug", debugSection))
    {
        readScalar(debugSection["ShowEdgeDetection"], "ShowEdgeDetection", data.debugEdgeDetection);
    }

    sanitize(data);
}

void Setting::exportFile(const char* dest, GameData& data)
{
    Setting::clampForRuntime(data);

    FILE* file = nullptr;
    if (fopen_s(&file, dest, "wt"))
    {
        logf("The file \"%s\" was not opened to write\n", dest);
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
        out << YAML::Key << "ContinuousCollisionMaxVelocity" << YAML::Value
            << std::sqrt(data.continuousCollisionMaxSqrVelocity);
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
    fwrite(out.c_str(), sizeof(char), out.size(), file);
    fclose(file);
}

void Setting::clampForRuntime(GameData& data)
{
    sanitize(data);
}
