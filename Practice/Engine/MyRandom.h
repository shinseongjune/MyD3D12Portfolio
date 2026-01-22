#pragma once
#include <random>

inline std::mt19937& GlobalRNG()
{
    static std::mt19937 rng{ std::random_device{}() };
    return rng;
}

static float RandRange(float a, float b)
{
    std::uniform_real_distribution<float> dist(a, b);
    return dist(GlobalRNG());
}

static int RandRangeInt(int a, int b)
{
    std::uniform_int_distribution<int> dist(a, b);
    return dist(GlobalRNG());
}