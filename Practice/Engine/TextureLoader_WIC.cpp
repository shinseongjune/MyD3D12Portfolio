#include "TextureLoader_WIC.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl.h>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
    return out;
}

static void FlipRowsRGBA8(uint8_t* rgba, uint32_t w, uint32_t h)
{
    const size_t rowBytes = (size_t)w * 4;
    std::vector<uint8_t> tmp(rowBytes);

    for (uint32_t y = 0; y < h / 2; ++y)
    {
        uint8_t* rowA = rgba + (size_t)y * rowBytes;
        uint8_t* rowB = rgba + (size_t)(h - 1 - y) * rowBytes;

        memcpy(tmp.data(), rowA, rowBytes);
        memcpy(rowA, rowB, rowBytes);
        memcpy(rowB, tmp.data(), rowBytes);
    }
}

static Result<ComPtr<IWICImagingFactory>> CreateWicFactory()
{
    // COM 초기화: 엔진 전체에서 한 번 해도 되지만, 지금은 로더 내부에서 안전하게 처리
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return Result<ComPtr<IWICImagingFactory>>::Fail("CoInitializeEx failed.");

    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));

    if (FAILED(hr))
    {
        // 일부 환경에서 Factory2가 없을 수 있어 Factory1로 폴백
        hr = CoCreateInstance(
            CLSID_WICImagingFactory1,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
    }

    if (FAILED(hr) || !factory)
        return Result<ComPtr<IWICImagingFactory>>::Fail("Failed to create WIC Imaging Factory.");

    return Result<ComPtr<IWICImagingFactory>>::Ok(factory);
}

Result<TextureCpuData> LoadTextureRGBA8_WIC(
    const std::wstring& path,
    ImageColorSpace colorSpace,
    bool flipY)
{
    if (path.empty())
        return Result<TextureCpuData>::Fail("Texture path is empty.");

    auto facR = CreateWicFactory();
    if (!facR.IsOk())
        return Result<TextureCpuData>::Fail(facR.error->message);

    ComPtr<IWICImagingFactory> factory = facR.value;

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);

    if (FAILED(hr) || !decoder)
        return Result<TextureCpuData>::Fail("WIC CreateDecoderFromFilename failed.");

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame)
        return Result<TextureCpuData>::Fail("WIC decoder GetFrame(0) failed.");

    UINT w = 0, h = 0;
    hr = frame->GetSize(&w, &h);
    if (FAILED(hr) || w == 0 || h == 0)
        return Result<TextureCpuData>::Fail("WIC frame GetSize failed (or size is zero).");

    // RGBA8로 통일 변환
    ComPtr<IWICFormatConverter> conv;
    hr = factory->CreateFormatConverter(&conv);
    if (FAILED(hr) || !conv)
        return Result<TextureCpuData>::Fail("WIC CreateFormatConverter failed.");

    hr = conv->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA, // 결과 포맷: RGBA
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);

    if (FAILED(hr))
        return Result<TextureCpuData>::Fail("WIC FormatConverter Initialize(GUID_WICPixelFormat32bppRGBA) failed.");

    TextureCpuData out{};
    out.width = (uint32_t)w;
    out.height = (uint32_t)h;
    out.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    out.colorSpace = colorSpace;

    const UINT stride = w * 4;
    const UINT imageSize = stride * h;

    out.pixels.resize((size_t)imageSize);

    hr = conv->CopyPixels(
        nullptr,
        stride,
        imageSize,
        out.pixels.data());

    if (FAILED(hr))
        return Result<TextureCpuData>::Fail("WIC CopyPixels failed.");

    if (flipY)
        FlipRowsRGBA8(out.pixels.data(), out.width, out.height);

    return Result<TextureCpuData>::Ok(std::move(out));
}

Result<TextureCpuData> LoadTextureRGBA8_WIC(
    const std::string& utf8Path,
    ImageColorSpace colorSpace,
    bool flipY)
{
    return LoadTextureRGBA8_WIC(Utf8ToWide(utf8Path), colorSpace, flipY);
}
