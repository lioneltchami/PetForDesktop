#include "Engine/WindowGLFW.hpp"

#include "Game/GameData.hpp"
#include "Engine/Log.hpp"
#include "Engine/Graphics/WindowOGL.hpp"
#include "Game/Pet.hpp"
#include "Engine/Monitors.hpp"

#include <atomic>
#include <cmath>

namespace
{
std::atomic<Monitors*> g_windowMonitors{nullptr};

Vec2 normalizeWindowCursorToLogical(GLFWwindow* window, const double x, const double y)
{
    Vec2 cursorPosition{static_cast<float>(x), static_cast<float>(y)};

    int logicalWidth  = 0;
    int logicalHeight = 0;
    int pixelWidth    = 0;
    int pixelHeight   = 0;
    glfwGetWindowSize(window, &logicalWidth, &logicalHeight);
    glfwGetFramebufferSize(window, &pixelWidth, &pixelHeight);

    const auto isInsideRect = [](const Vec2& point, const float width, const float height) {
        return point.x >= 0.f && point.y >= 0.f && point.x <= width && point.y <= height;
    };

    const bool logicalRange = isInsideRect(cursorPosition,
                                          static_cast<float>(logicalWidth),
                                          static_cast<float>(logicalHeight));
    if (logicalRange)
        return cursorPosition;

    if (logicalWidth <= 0 || logicalHeight <= 0)
        return cursorPosition;

    const float scaleX = static_cast<float>(pixelWidth) / static_cast<float>(logicalWidth);
    const float scaleY = static_cast<float>(pixelHeight) / static_cast<float>(logicalHeight);
    if (std::isfinite(scaleX) && std::isfinite(scaleY) && scaleX > 0.f && scaleY > 0.f)
    {
        const float safeScaleX = std::max(scaleX, 0.0001f);
        const float safeScaleY = std::max(scaleY, 0.0001f);
        const Vec2 physicalCandidate = {cursorPosition.x / safeScaleX, cursorPosition.y / safeScaleY};
        if (isInsideRect(physicalCandidate,
                         static_cast<float>(logicalWidth),
                         static_cast<float>(logicalHeight)))
        {
            return physicalCandidate;
        }
    }

    const bool physicalRange = isInsideRect(cursorPosition, static_cast<float>(pixelWidth), static_cast<float>(pixelHeight));
    if (physicalRange)
    {
        const float safeScaleX = std::max(std::abs(scaleX), 0.0001f);
        const float safeScaleY = std::max(std::abs(scaleY), 0.0001f);
        return {cursorPosition.x / safeScaleX, cursorPosition.y / safeScaleY};
    }

    const float safeScaleX = std::max(std::abs(scaleX), 0.0001f);
    const float safeScaleY = std::max(std::abs(scaleY), 0.0001f);
    return {cursorPosition.x / safeScaleX, cursorPosition.y / safeScaleY};
}

void glfwMonitorConnectionCallback(GLFWmonitor* monitor, int event)
{
    Monitors* currentMonitors = g_windowMonitors.load(std::memory_order_acquire);
    if (currentMonitors != nullptr)
        currentMonitors->onMonitorConnectionChanged(monitor, event);
}
}

void WindowGLFW::initGLFW()
{
    // initialize the library
    if (!glfwInit())
        errorAndExit("glfw initialization error");
}

void WindowGLFW::preSetupWindow(const GameData& datas)
{
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, !datas.showFrameBufferBackground);
    glfwWindowHint(GLFW_VISIBLE, datas.showFrameBufferBackground);
    glfwWindowHint(GLFW_FLOATING, datas.useForwardWindow);

    // Disable depth and stencil buffers
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
}

void WindowGLFW::postSetupWindow(GameData& datas)
{
    useMousePassThrough = datas.useMousePassThoughWindow;
    isMousePassThrough = true;
    glfwSetWindowAttrib(datas.window->getWindow(), GLFW_MOUSE_PASSTHROUGH, isMousePassThrough);
    glfwSetWindowAttrib(datas.window->getWindow(), GLFW_TRANSPARENT_FRAMEBUFFER, true);
    glfwSetWindowAttrib(window, GLFW_DECORATED, datas.showWindow);
    glfwSetWindowAttrib(window, GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    glfwSetWindowUserPointer(window, &datas);
    glfwSetMouseButtonCallback(window, mousButtonCallBack);
    glfwSetCursorPosCallback(window, cursorPositionCallback);

    glfwDefaultWindowHints();
}

void WindowGLFW::initWindow(GameData& datas)
{
    preSetupWindow(datas);

    m_size = {1.f, 1.f};
    window = glfwCreateWindow(m_size.x, m_size.y, PROJECT_NAME, NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        errorAndExit("Create Window error");
    }

    glfwMakeContextCurrent(window);
    postSetupWindow(datas);

    glfwShowWindow(window);

    glfwSetWindowPos(window, m_position.x, m_position.y);
}

void WindowGLFW::setMonitorCallback(Monitors* monitors)
{
    g_windowMonitors.store(monitors, std::memory_order_release);
    glfwSetMonitorCallback(monitors ? glfwMonitorConnectionCallback : nullptr);
}

void cursorPositionCallback(GLFWwindow* window, double x, double y)
{
    GameData& datas = *static_cast<GameData*>(glfwGetWindowUserPointer(window));
    if (datas.leftButtonEvent == GLFW_PRESS)
    {
        const Vec2  windowLogicalPos = datas.window->getPosition();
        const Vec2  localCursorPos   = normalizeWindowCursorToLogical(window, x, y);
        const float globalScreenPosX  = static_cast<float>(windowLogicalPos.x) + localCursorPos.x;
        const float globalScreenPosY  = static_cast<float>(windowLogicalPos.y) + localCursorPos.y;
        datas.deltaCursorPosX += globalScreenPosX - datas.prevCursorPosX;
        datas.deltaCursorPosY += globalScreenPosY - datas.prevCursorPosY;
        datas.prevCursorPosX = globalScreenPosX;
        datas.prevCursorPosY = globalScreenPosY;
        Vec2 delta(datas.deltaCursorPosX, datas.deltaCursorPosY);
        datas.deltasCursorPosBuffer.emplace(datas.timeAcc, delta);
        datas.deltaCursorAcc += delta;
    }
}

void mousButtonCallBack(GLFWwindow* window, int button, int action, int mods)
{
    GameData& datas = *static_cast<GameData*>(glfwGetWindowUserPointer(window));
   
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        datas.leftButtonEvent = action;
   
        switch (action)
        {
        case GLFW_PRESS: {
            const Vec2 windowLogicalPos = datas.window->getPosition();
            const Vec2 windowCursorPosLogical = normalizeWindowCursorToLogical(window, datas.cursorPos.x, datas.cursorPos.y);
            datas.prevCursorPosX         = windowLogicalPos.x + windowCursorPosLogical.x;
            datas.prevCursorPosY         = windowLogicalPos.y + windowCursorPosLogical.y;
            datas.deltaCursorPosX = 0.f;
            datas.deltaCursorPosY = 0.f;
            break;
        }
        case GLFW_RELEASE:
            break;
        default:
            break;
        }
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        datas.rightButtonEvent = action;
        break;
    default:
        break;
    }
}

void processInput(GLFWwindow* window)
{
    GameData& datas = *static_cast<GameData*>(glfwGetWindowUserPointer(window));

    // Need always capture the mouse position to trigger the pass through
    double cursPosX, cursPosY;
    glfwGetCursorPos(window, &cursPosX, &cursPosY);
    const Vec2 cursorLogicalPos = normalizeWindowCursorToLogical(window, cursPosX, cursPosY);
    datas.cursorPos = {static_cast<int>(std::lround(cursorLogicalPos.x)), static_cast<int>(std::lround(cursorLogicalPos.y))};
    
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
