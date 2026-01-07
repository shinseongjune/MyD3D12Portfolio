#include "DWriteFontFace.h"
#include <vector>
#include <cassert>
#include <cmath>

static int RoundToInt(float v) { return (int)std::lround(v); }

bool DWriteFontFace::Initialize(TextureManager& tm, const wchar_t* familyName, float emSizePx)
{
    m_emSize = emSizePx;

    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_factory.GetAddressOf())
    );
    if (FAILED(hr)) return false;

    if (!CreateFontFace(familyName)) return false;

    ComputeFontMetrics();
    return BuildAsciiAtlas(tm);
}

bool DWriteFontFace::CreateFontFace(const wchar_t* familyName)
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    if (FAILED(m_factory->GetSystemFontCollection(&fonts))) return false;

    UINT32 index = 0;
    BOOL exists = FALSE;
    if (FAILED(fonts->FindFamilyName(familyName, &index, &exists)) || !exists)
        return false;

    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    if (FAILED(fonts->GetFontFamily(index, &family))) return false;

    Microsoft::WRL::ComPtr<IDWriteFont> font;
    if (FAILED(family->GetFirstMatchingFont(
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font)))
        return false;

    Microsoft::WRL::ComPtr<IDWriteFontFace> face;
    if (FAILED(font->CreateFontFace(&face))) return false;

    m_face = face;
    return true;
}

void DWriteFontFace::ComputeFontMetrics()
{
    DWRITE_FONT_METRICS fm{};
    m_face->GetMetrics(&fm);

    const float scale = m_emSize / float(fm.designUnitsPerEm);

    const float ascent = fm.ascent * scale;
    const float descent = fm.descent * scale;
    const float gap = fm.lineGap * scale;

    m_baseline = (int)std::ceil(ascent);
    m_lineHeight = (int)std::ceil(ascent + descent + gap);
}

bool DWriteFontFace::BuildAsciiAtlas(TextureManager& tm)
{
    // 1차: 고정 크기 아틀라스(충분히 크게) 잡고, 안 들어가면 키우는 식으로.
    // ASCII 95개(32~126)라서 512x512면 거의 대부분 OK.
    int atlasW = 512, atlasH = 512;

    TextureCpuData cpu{};
    cpu.width = atlasW;
    cpu.height = atlasH;
    cpu.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    cpu.colorSpace = ImageColorSpace::Linear; // UI 텍스트는 보통 linear가 낫지만, SRGB여도 큰 문제는 아님.
    cpu.pixels.assign((size_t)atlasW * atlasH * 4, 0);

    AtlasPacker pack{};
    pack.W = atlasW; pack.H = atlasH; pack.padding = 1;

    // 기본: 안 쓰는 글리프는 0으로 초기화(공백 포함)
    for (auto& g : m_glyphs)
        g = { 0,0,0,0, 0,0, 0,0, 0 };

    // 공백 advance는 따로 설정(DirectWrite로도 얻을 수 있지만 간단히 metrics로)
    {
        uint16_t gi = 0;
        UINT32 cp = (UINT32)' ';
        m_face->GetGlyphIndicesW(&cp, 1, &gi);

        DWRITE_GLYPH_METRICS gm{};
        m_face->GetDesignGlyphMetrics(&gi, 1, &gm, FALSE);

        DWRITE_FONT_METRICS fm{};
        m_face->GetMetrics(&fm);
        float scale = m_emSize / float(fm.designUnitsPerEm);

        Glyph space{};
        space.advance = (int)std::ceil(gm.advanceWidth * scale);
        m_glyphs[(uint8_t)' '] = space;
    }

    // 2) ASCII(32~126) 글리프를 하나씩 래스터해서 아틀라스에 복사
    for (uint32_t c = 32; c <= 126; ++c)
    {
        UINT32 cp = (UINT32)(uint8_t)c;
        uint16_t glyphIndex = 0;
        m_face->GetGlyphIndicesW(&cp, 1, &glyphIndex);

        if (glyphIndex == 0)
            continue; // 폰트에 없으면 스킵(대부분은 있음)

        // --- GlyphRun 구성(베이스라인 원점 = (0,0)) ---
        DWRITE_GLYPH_RUN run{};
        run.fontFace = m_face.Get();
        run.fontEmSize = m_emSize;
        run.glyphCount = 1;
        run.glyphIndices = &glyphIndex;

        // advance는 레이아웃에 필요 -> metrics에서 구해서 px로 저장
        DWRITE_GLYPH_METRICS gm{};
        m_face->GetDesignGlyphMetrics(&glyphIndex, 1, &gm, FALSE);

        DWRITE_FONT_METRICS fm{};
        m_face->GetMetrics(&fm);
        float scale = m_emSize / float(fm.designUnitsPerEm);

        float advanceF = gm.advanceWidth * scale;
        int advancePx = (int)std::ceil(advanceF);

        // --- GlyphRunAnalysis로 알파 텍스처 생성 ---
        Microsoft::WRL::ComPtr<IDWriteGlyphRunAnalysis> analysis;
        HRESULT hr = m_factory->CreateGlyphRunAnalysis(
            &run,
            1.0f,                         // pixelsPerDip
            nullptr,                      // transform
            DWRITE_RENDERING_MODE_ALIASED, // 1차는 알리아스(또렷). 나중에 NATURAL/GRAY로 바꿀 수 있음.
            DWRITE_MEASURING_MODE_NATURAL,
            0.0f, 0.0f,                   // baseline origin
            &analysis
        );
        if (FAILED(hr) || !analysis)
            continue;

        RECT bounds{};
        hr = analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &bounds);
        if (FAILED(hr))
            continue;

        const int gw = bounds.right - bounds.left;
        const int gh = bounds.bottom - bounds.top;

        // 비트맵이 없는 글리프(예: 공백)는 UV 없이 advance만
        if (gw <= 0 || gh <= 0)
        {
            Glyph g{};
            g.advance = advancePx;
            m_glyphs[(uint8_t)c] = g;
            continue;
        }

        std::vector<uint8_t> alpha((size_t)gw * gh);
        hr = analysis->CreateAlphaTexture(
            DWRITE_TEXTURE_ALIASED_1x1,
            &bounds,
            alpha.data(),
            (UINT32)alpha.size()
        );
        if (FAILED(hr))
            continue;

        // --- 아틀라스 배치 ---
        int ax, ay;
        if (!pack.Place(gw, gh, ax, ay))
        {
            // 1차 완성: 여기 오면 atlasW/H 키워서 재시도 로직을 넣으면 되는데
            // 대부분 512로 충분하니 일단 실패 처리.
            assert(false && "Font atlas too small");
            return false;
        }

        // --- 알파를 RGBA로 아틀라스에 복사 (RGB=255, A=alpha) ---
        for (int y = 0; y < gh; ++y)
        {
            for (int x = 0; x < gw; ++x)
            {
                const uint8_t a = alpha[(size_t)y * gw + x];

                const int dstX = ax + x;
                const int dstY = ay + y;
                size_t di = ((size_t)dstY * atlasW + dstX) * 4;

                cpu.pixels[di + 0] = 255;
                cpu.pixels[di + 1] = 255;
                cpu.pixels[di + 2] = 255;
                cpu.pixels[di + 3] = a;
            }
        }

        Glyph g{};
        g.w = gw;
        g.h = gh;
        g.offX = bounds.left;
        g.offY = bounds.top;
        g.advance = advancePx;

        g.u0 = (float)ax / (float)atlasW;
        g.v0 = (float)ay / (float)atlasH;
        g.u1 = (float)(ax + gw) / (float)atlasW;
        g.v1 = (float)(ay + gh) / (float)atlasH;

        m_glyphs[(uint8_t)c] = g;
    }

    // 3) TextureManager에 등록(= TextureHandle 생성)
    m_atlas = tm.Create(cpu);
    return m_atlas.IsValid();
}
