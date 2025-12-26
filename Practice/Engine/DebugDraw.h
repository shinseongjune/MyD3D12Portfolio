#pragma once
#include <DirectXMath.h>
#include <vector>

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
