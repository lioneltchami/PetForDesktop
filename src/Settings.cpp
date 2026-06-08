#include "Engine/Settings.hpp"
#include "Engine/Log.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
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
constexpr float kMinBounciness = 0.f;
constexpr float kMaxBounciness = 1.f;
constexpr float kMinFriction = 0.f;
constexpr float kMaxFriction = 1.f;
constexpr float kMinCollisionRatio = 0.f;
constexpr float kMaxCollisionRatio = 1.f;
constexpr float kMinPositiveFloat = 0.0001f;
constexpr int kMinFootBasement = 1;

float clampFloat(float value, float minValue, float maxValue)
{
    return std::clamp(value, minValue, maxValue);
}

int clampInt(int value, int minValue, int maxValue)
{
    return std::clamp(value, minValue, maxValue);
}

bool isFiniteFloat(float value)
{
    return std::isfinite(value);
}

bool extractSection(const YAML::Node& root, const char* key, YAML::Node& out)
{
    if (root[key])
    {
        out = root[key];
        return true;
    }

    if (!root.IsSequence())
        return false;

    for (const auto& roleItem : root)
    {
        if (roleItem[key])
        {
            out = roleItem[key];
            return true;
        }
    }

    return false;
}

template <typename T>
bool readScalar(const YAML::Node& section, const char* key, T& value)
{
    const YAML::Node entry = section[key];
    if (!entry)
        return false;

    try
    {
        value = entry.as<T>();
        return true;
    }
    catch (...)
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

void clampAndNormalize(GameData& data)
{
    data.FPS        = clampInt(data.FPS, kMinFPS, kMaxFPS);
    data.scale      = clampInt(data.scale, kMinScale, kMaxScale);
    data.textScale  = std::roundf(clampFloat(data.textScale, kMinTextScale, kMaxTextScale) * 10.f) / 10.f;

    data.physicFrameRate = clampInt(data.physicFrameRate, kMinPhysicsFrameRate, kMaxPhysicsFrameRate);

    data.gravity.x = clampFloat(data.gravity.x, kMinGravity, kMaxGravity);
    data.gravity.y = clampFloat(data.gravity.y, kMinGravity, kMaxGravity);
    data.gravityDir = (data.gravity.sqrLength() > 0.f && isFiniteFloat(data.gravity.x) && isFiniteFloat(data.gravity.y))
                         ? data.gravity.normalized()
                         : Vec2{0.f, 1.f};

    data.bounciness = clampFloat(data.bounciness, kMinBounciness, kMaxBounciness);
    data.friction   = clampFloat(data.friction, kMinFriction, kMaxFriction);
    data.continuousCollisionMaxSqrVelocity =
        std::max(kMinPositiveFloat,
                 data.continuousCollisionMaxSqrVelocity * data.continuousCollisionMaxSqrVelocity);

    data.footBasementWidth  = std::max(kMinFootBasement, data.footBasementWidth);
    data.footBasementHeight = std::max(kMinFootBasement, data.footBasementHeight);
    data.collisionPixelRatioStopMovement = clampFloat(data.collisionPixelRatioStopMovement, kMinCollisionRatio, kMaxCollisionRatio);
    data.isGroundedDetection           = std::max(kMinPositiveFloat, data.isGroundedDetection);
    data.releaseImpulse                = std::max(kMinPositiveFloat, data.releaseImpulse);
    data.coyoteTimeCursorPos           = std::max(kMinPositiveFloat, data.coyoteTimeCursorPos);

    if (data.styleName.empty())
        data.styleName = "PetForDesktop";
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

    YAML::Node section;

    if (extractSection(root, "Game", section))
    {
        readScalar(section, "FPS", data.FPS);
        readScalar(section, "RandomSeed", data.randomSeed);
    }

    if (extractSection(root, "Physic", section))
    {
        readScalar(section, "PhysicFrameRate", data.physicFrameRate);
        readScalar(section, "Bounciness", data.bounciness);
        readScalar(section, "GravityX", data.gravity.x);
        readScalar(section, "GravityY", data.gravity.y);
        readScalar(section, "Friction", data.friction);

        float maxVelocity = 0.f;
        if (readScalar(section, "ContinuousCollisionMaxVelocity", maxVelocity))
            data.continuousCollisionMaxSqrVelocity = maxVelocity;

        readScalar(section, "FootBasementWidth", data.footBasementWidth);
        readScalar(section, "FootBasementHeight", data.footBasementHeight);
        readScalar(section, "CollisionPixelRatioStopMovement", data.collisionPixelRatioStopMovement);
        readScalar(section, "IsGroundedDetection", data.isGroundedDetection);
        readScalar(section, "InputReleaseImpulse", data.releaseImpulse);
    }

    if (extractSection(root, "GamePlay", section))
    {
        readScalar(section, "CoyoteTimeCursorMovement", data.coyoteTimeCursorPos);
    }

    if (extractSection(root, "Window", section))
    {
        readScalar(section, "FullScreenWindow", data.fullScreenWindow);
        readScalar(section, "ShowWindow", data.showWindow);
        readScalar(section, "ShowFrameBufferBackground", data.showFrameBufferBackground);
        readScalar(section, "UseForwardWindow", data.useForwardWindow);
        readScalar(section, "UseMousePassThoughWindow", data.useMousePassThoughWindow);
    }

    if (extractSection(root, "Style", section))
    {
        std::string theme;
        if (readScalar(section, "Theme", theme))
            data.styleName = theme;
    }

    if (extractSection(root, "Accessibility", section))
    {
        readScalar(section, "Scale", data.scale);
        readScalar(section, "TextScale", data.textScale);
    }

    if (extractSection(root, "Debug", section))
    {
        readScalar(section, "ShowEdgeDetection", data.debugEdgeDetection);
    }

    clampAndNormalize(data);
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
    fwrite(out.c_str(), sizeof(char), out.size(), file);
    fclose(file);
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
