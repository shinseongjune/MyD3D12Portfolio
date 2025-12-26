#include "DebugDraw.h"

std::vector<DebugLine> DebugDraw::s_lines;

void DebugDraw::BeginFrame()
{
    s_lines.clear();
}

void DebugDraw::Line(const DirectX::XMFLOAT3& a,
    const DirectX::XMFLOAT3& b,
    const DirectX::XMFLOAT4& color)
{
    s_lines.push_back({ a, b, color });
}

const std::vector<DebugLine>& DebugDraw::GetLines()
{
    return s_lines;
}
