#pragma once
#include <random>

static float RandRange(float a, float b)
{
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> dist(a, b);
    return dist(rng);
}