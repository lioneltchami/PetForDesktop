#pragma once

#include "Engine/Singleton.hpp"
#include "Game/GameData.hpp"

#include <GLFW/glfw3.h>

#include <functional>
#include <algorithm>
#include <queue>
#include <vector>
#include <cmath>

namespace TimeManagerLogic
{
inline void pruneExpiredCursorDeltas(GameData& datas, double nowSeconds)
{
    while (!datas.deltasCursorPosBuffer.empty() &&
           datas.deltasCursorPosBuffer.top().timer + datas.coyoteTimeCursorPos <= nowSeconds)
    {
        const GameData::DeltaCursosPosElem& elem = datas.deltasCursorPosBuffer.top();
        datas.deltaCursorAcc -= elem.pos;
        datas.deltasCursorPosBuffer.pop();
    }
}
} // namespace TimeManagerLogic

struct TimerTask
{
    std::function<void()> task        = nullptr;
    double                localTimer  = 0.; // if current time egal 1s and local timer egal 0.5 global time egal 1.5
    double                globalTimer = 0.;
    bool                  isLooping   = false;

    TimerTask(const std::function<void()>& task = nullptr, double localTimer = .0, double globalTimer = .0,
              bool isLooping = false)
        : task{task}, localTimer{localTimer}, globalTimer{globalTimer}, isLooping{isLooping}
    {
    }

    bool operator>(const TimerTask& other) const noexcept
    {
        return globalTimer > other.globalTimer;
    }
};

class TimeManager : public Singleton<TimeManager>
{
protected:
    double m_time     = glfwGetTime();
    double m_tempTime = m_time;

    double    m_timeAccLoop       = 0.;
    double    m_deltaTime         = 0.;
    double    m_fixedDeltaTime    = 1. / 60.;
    double    m_interpolation     = 0.;
    int       m_maxSubSteps       = 8;
    double    m_clampFrameTime    = 0.25;
    GameData* datas;

    std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> m_timerQueue;

public:
    void Init(GameData& data)
    {
        setFrameRate(data.FPS);
        datas = &data;
    }

    // improve first frame accurancy
    void start()
    {
        m_time     = glfwGetTime();
        m_tempTime = m_time;
    }

    inline void emplaceTimer(std::function<void()> functionToExecute, double delay, bool isLooping = false) noexcept
    {
        m_timerQueue.emplace(functionToExecute, delay, delay + datas->timeAcc, isLooping);
    }

    void setFrameRate(int FPS)
    {
        m_fixedDeltaTime = 1. / FPS;
        m_maxSubSteps   = std::max(1, static_cast<int>(std::ceil(m_clampFrameTime / m_fixedDeltaTime)));
    }

    void update(std::function<void(double deltaTime)> unlimitedUpdateFunction,
                std::function<void(double deltaTime)> fixedUpdateFunction,
                std::function<void(double interpolation)> renderUpdateFunction)
    {
        /*Prepar the next frame*/
        m_tempTime  = glfwGetTime();
        m_deltaTime = m_tempTime - m_time;
        m_time      = m_tempTime;

        if (m_deltaTime < 0.)
            m_deltaTime = 0.;
        m_deltaTime = std::min(m_deltaTime, m_clampFrameTime);

        // prevent simulation burst after long stalls (pause, alt-tab, debugger break)
        if (!std::isfinite(m_deltaTime))
            m_deltaTime = m_fixedDeltaTime;

        unlimitedUpdateFunction(m_deltaTime);

        /*Add accumulator*/
        datas->timeAcc += m_deltaTime;
        if (!std::isfinite(datas->timeAcc))
            datas->timeAcc = 0.;

        m_timeAccLoop += m_deltaTime;

        int subStepCount = 0;
        while (m_timeAccLoop >= m_fixedDeltaTime)
        {
            fixedUpdateFunction(m_fixedDeltaTime);
            m_timeAccLoop -= m_fixedDeltaTime;
            ++subStepCount;

            if (subStepCount >= m_maxSubSteps)
            {
                m_timeAccLoop = 0.;
                break;
            }
        }

        m_interpolation = m_fixedDeltaTime != 0 ? std::min(1., std::max(0., m_timeAccLoop / m_fixedDeltaTime)) : 0;
        renderUpdateFunction(m_interpolation);

        while (!m_timerQueue.empty() && m_timerQueue.top().globalTimer <= datas->timeAcc)
        {
            const TimerTask& timerTask = m_timerQueue.top();
            timerTask.task();

            if (timerTask.isLooping)
            {
                emplaceTimer(timerTask.task, timerTask.localTimer, timerTask.isLooping);
            }
            m_timerQueue.pop();
        }

        TimeManagerLogic::pruneExpiredCursorDeltas(*datas, datas->timeAcc);
    }

    double getInterpolation() const
    {
        return m_interpolation;
    }

    double getDeltaTime() const
    {
        return m_deltaTime;
    }

    double getFixedDeltaTime() const
    {
        return m_fixedDeltaTime;
    }
};
