#pragma once
#include <cstdint>

class Time
{
public:
    // 프로그램 시작 시 1회 호출
    static void Initialize();

    // 매 프레임 호출 (deltaTime 계산)
    static void Tick();

    // 프레임 deltaTime (초 단위)
    static double DeltaTime();

    // 누적 시간 (초 단위)
    static double TotalTime();

    // 프레임 번호
    static uint64_t FrameCount();

    // 너무 큰 dt(예: 창 드래그/디버거 정지) 방지용 클램프(초)
    static void SetMaxDelta(double seconds);

private:
    static double s_secondsPerCount;
    static int64_t s_startCount;
    static int64_t s_prevCount;
    static int64_t s_currCount;

    static double s_deltaSeconds;
    static double s_totalSeconds;

    static double s_maxDeltaSeconds;
    static uint64_t s_frameCount;
};
