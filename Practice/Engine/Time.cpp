#include "Time.h"
#include <Windows.h>

double   Time::s_secondsPerCount = 0.0;
int64_t  Time::s_startCount = 0;
int64_t  Time::s_prevCount = 0;
int64_t  Time::s_currCount = 0;

double   Time::s_deltaSeconds = 0.0;
double   Time::s_totalSeconds = 0.0;

double   Time::s_maxDeltaSeconds = 0.1; // 기본: 100ms
uint64_t Time::s_frameCount = 0;

static int64_t QueryCounter()
{
    LARGE_INTEGER li{};
    QueryPerformanceCounter(&li);
    return (int64_t)li.QuadPart;
}

void Time::Initialize()
{
    LARGE_INTEGER freq{};
    QueryPerformanceFrequency(&freq);
    s_secondsPerCount = 1.0 / (double)freq.QuadPart;

    s_startCount = QueryCounter();
    s_prevCount = s_startCount;
    s_currCount = s_startCount;

    s_deltaSeconds = 0.0;
    s_totalSeconds = 0.0;
    s_frameCount = 0;
}

void Time::Tick()
{
    s_currCount = QueryCounter();

    const int64_t deltaCounts = s_currCount - s_prevCount;
    double dt = (double)deltaCounts * s_secondsPerCount;

    // dt 클램프(디버깅/창 드래그 등으로 dt 폭주 방지)
    if (dt < 0.0) dt = 0.0;
    if (dt > s_maxDeltaSeconds) dt = s_maxDeltaSeconds;

    s_deltaSeconds = dt;

    const int64_t totalCounts = s_currCount - s_startCount;
    s_totalSeconds = (double)totalCounts * s_secondsPerCount;

    s_prevCount = s_currCount;
    ++s_frameCount;
}

double Time::DeltaTime() { return s_deltaSeconds; }
double Time::TotalTime() { return s_totalSeconds; }
uint64_t Time::FrameCount() { return s_frameCount; }

void Time::SetMaxDelta(double seconds)
{
    s_maxDeltaSeconds = (seconds > 0.0) ? seconds : 0.0;
}
