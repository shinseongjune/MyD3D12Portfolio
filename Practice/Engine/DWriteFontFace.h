#pragma once
#include "IFontFace.h"
#include "TextureManager.h"

#include <wrl.h>
#include <dwrite.h>
#include <array>
#include <string>

class DWriteFontFace final : public IFontFace
{
public:
    // ¿¹: familyName = L"Consolas", emSizePx = 16.0f
    bool Initialize(TextureManager& tm, const wchar_t* familyName, float emSizePx);

    TextureHandle Atlas() const override { return m_atlas; }
    int LineHeightPx() const override { return m_lineHeight; }
    int BaselinePx() const override { return m_baseline; }
    const Glyph& GetGlyph(uint8_t ascii) const override { return m_glyphs[ascii]; }

private:
    using ComPtr = Microsoft::WRL::ComPtr<IUnknown>;
    Microsoft::WRL::ComPtr<IDWriteFactory>  m_factory;
    Microsoft::WRL::ComPtr<IDWriteFontFace> m_face;

    float m_emSize = 16.0f;
    int m_lineHeight = 16;
    int m_baseline = 12;

    TextureHandle m_atlas{ 0 };
    std::array<Glyph, 256> m_glyphs{};

    struct AtlasPacker
    {
        int W = 0, H = 0;
        int x = 1, y = 1, rowH = 0;
        int padding = 1;

        bool Place(int gw, int gh, int& outX, int& outY)
        {
            if (x + gw + padding > W) { x = padding; y += rowH + padding; rowH = 0; }
            if (y + gh + padding > H) return false;
            outX = x; outY = y;
            x += gw + padding;
            rowH = (std::max)(rowH, gh);
            return true;
        }
    };

private:
    bool CreateFontFace(const wchar_t* familyName);
    void ComputeFontMetrics();
    bool BuildAsciiAtlas(TextureManager& tm);
};
