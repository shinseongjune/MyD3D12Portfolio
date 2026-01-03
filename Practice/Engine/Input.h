#pragma once
#include <cstdint>

enum class Key : uint8_t
{
    W, A, S, D,
    Q, E, R, G,
    Up, Down, Left, Right,
    Escape,
    Space,
};

class Input
{
public:
    void Update();

    bool IsKeyDown(Key k) const;
    bool IsKeyPressed(Key k) const; // 이번 프레임에 눌림
    bool IsKeyReleased(Key k) const;

private:
    bool s_curr[256];
    bool s_prev[256];
};
