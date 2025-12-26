#pragma once
#include <cstdint>

enum class Key : uint8_t
{
    W, A, S, D,
    Q, E,
    Up, Down, Left, Right,
    Escape,
};

class Input
{
public:
    static void Update();

    static bool IsKeyDown(Key k);
    static bool IsKeyPressed(Key k); // 이번 프레임에 눌림
    static bool IsKeyReleased(Key k);

private:
    static bool s_curr[256];
    static bool s_prev[256];
};
