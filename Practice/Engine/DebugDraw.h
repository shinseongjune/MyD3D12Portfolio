#pragma once
#include <cstdio>
#include <DirectXMath.h>
#include <vector>
#include <Windows.h>

#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) do { \
    char _buf[1024]; \
    std::snprintf(_buf, sizeof(_buf), "[ERROR] " fmt "\n", ##__VA_ARGS__); \
    std::fputs(_buf, stderr); \
    OutputDebugStringA(_buf); \
} while(0)
#endif

struct DebugLine
{
    DirectX::XMFLOAT3 a;
    DirectX::XMFLOAT3 b;
    DirectX::XMFLOAT4 color;
};

class DebugDraw
{
public:
    static void BeginFrame();
    static void Line(const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b,
        const DirectX::XMFLOAT4& color);
    static const std::vector<DebugLine>& GetLines();

private:
    static std::vector<DebugLine> s_lines;
};
