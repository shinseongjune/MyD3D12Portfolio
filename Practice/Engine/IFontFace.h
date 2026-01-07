#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include "TextureHandle.h"

struct Glyph
{
    // 아틀라스 UV
    float u0, v0, u1, v1;

    // 글리프 비트맵 크기(px)
    int w, h;

    // 베이스라인 기준 오프셋(px)
    int offX, offY;  // (penX, penYBaseline)에 더할 값. DWrite bounds.left/top 그대로 쓰면 편함.

    // 다음 글자 이동(px)
    int advance;
};

class IFontFace
{
public:
    virtual ~IFontFace() = default;

    virtual TextureHandle Atlas() const = 0;
    virtual int LineHeightPx() const = 0;
    virtual int BaselinePx() const = 0;

    // 일단 ASCII만. (나중에 uint32_t codepoint로 확장)
    virtual const Glyph& GetGlyph(uint8_t ascii) const = 0;
};