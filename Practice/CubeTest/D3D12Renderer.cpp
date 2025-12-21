#include "D3D12Renderer.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <Windows.h>
#include <cstring>
#include <ios>
#include <sstream>
#include <fstream>
#include <cctype>
#include <cstdio>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

static inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) throw std::runtime_error("HRESULT failed");
}

static ComPtr<ID3DBlob> CompileShader(const char* entry, const char* target, const char* src)
{
    UINT flags = 0;
#if defined(_DEBUG)
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompile(src, std::strlen(src), nullptr, nullptr, nullptr, entry, target, flags, 0, &blob, &err);
    if (FAILED(hr))
    {
        if (err) OutputDebugStringA((char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    return blob;
}

static const D3D12Renderer::Vertex kQuadVerts[] =
{
    {{-1.0f,-1.0f, 0.0f}, {0.0f, 1.0f}}, // left-bottom
    {{-1.0f,+1.0f, 0.0f}, {0.0f, 0.0f}}, // left-top
    {{+1.0f,+1.0f, 0.0f}, {1.0f, 0.0f}}, // right-top
    {{+1.0f,-1.0f, 0.0f}, {1.0f, 1.0f}}, // right-bottom
};

static const uint16_t kQuadIndices[] =
{
    0,1,2,
    0,2,3
};


void D3D12Renderer::Init(HWND hwnd, UINT width, UINT height)
{
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> dbg;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
        dbg->EnableDebugLayer();
#endif

    CreateDeviceAndSwapChain();
    CreateRtvHeapAndViews();
    CreateDepthBuffer();
    CreateCommands();
    CreateSyncObjects();

    CreateDemoResources();

    CreateTexture_Checkerboard();

    // === TEMP: OBJ CPU parse test ===
    {
        CpuMesh cpu;
        std::string err;
        if (LoadObjToCpuMesh(L"assets\\cube.obj", cpu, &err))
        {
            char buf[256];
            sprintf_s(buf, "OBJ OK: verts=%zu, indices=%zu\n", cpu.vertices.size(), cpu.indices.size());
            OutputDebugStringA(buf);
        }
        else
        {
            OutputDebugStringA(("OBJ FAIL: " + err + "\n").c_str());
        }
    }
}

static inline void TrimLeft(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
}

static inline std::vector<std::string_view> SplitWS(std::string_view line)
{
    std::vector<std::string_view> out;
    size_t i = 0;
    while (i < line.size())
    {
        while (i < line.size() && std::isspace((unsigned char)line[i])) ++i;
        size_t start = i;
        while (i < line.size() && !std::isspace((unsigned char)line[i])) ++i;
        if (start < i) out.push_back(line.substr(start, i - start));
    }
    return out;
}

// OBJ 인덱스는 1-based, 음수도 가능(뒤에서부터). 여기선 최소 지원: 양수만.
static inline bool ParseInt(std::string_view sv, int& out)
{
    try
    {
        std::string tmp(sv);
        out = std::stoi(tmp);
        return true;
    }
    catch (...) { return false; }
}

static inline bool ParseFloat(std::string_view sv, float& out)
{
    try
    {
        std::string tmp(sv);
        out = std::stof(tmp);
        return true;
    }
    catch (...) { return false; }
}

// token이 "v/vt" 형태인지 파싱 (최소 지원)
// outV, outVT 는 OBJ의 "1-based index"
static inline bool ParseFaceToken_V_VT(std::string_view tok, int& outV, int& outVT)
{
    auto slash = tok.find('/');
    if (slash == std::string_view::npos) return false;

    auto a = tok.substr(0, slash);
    auto b = tok.substr(slash + 1);

    // "v//vn" 같은 건 여기선 거절
    if (b.empty()) return false;
    if (b.find('/') != std::string_view::npos) return false;

    if (!ParseInt(a, outV)) return false;
    if (!ParseInt(b, outVT)) return false;
    return true;
}

bool D3D12Renderer::ReadTextFileUTF8(const wchar_t* path, std::string& outText)
{
    outText.clear();

    // Windows에서 wide path를 받으니 wifstream이 편하지만,
    // 여기선 "바이너리로 읽고 string에 담기"로 단순화.
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::ostringstream ss;
    ss << f.rdbuf();
    outText = ss.str();
    return true;
}

bool D3D12Renderer::LoadObjToCpuMesh(const wchar_t* path, CpuMesh& outMesh, std::string* outError)
{
    outMesh.vertices.clear();
    outMesh.indices.clear();
    if (outError) outError->clear();

    std::string text;
    if (!ReadTextFileUTF8(path, text))
    {
        if (outError) *outError = "Failed to open OBJ file.";
        return false;
    }

    // OBJ pools
    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT2> uvs;

    std::istringstream iss(text);
    std::string line;
    uint32_t runningIndex = 0;

    while (std::getline(iss, line))
    {
        TrimLeft(line);
        if (line.empty() || line[0] == '#') continue;

        std::string_view sv(line);
        auto parts = SplitWS(sv);
        if (parts.empty()) continue;

        if (parts[0] == "v")
        {
            if (parts.size() < 4) continue;
            float x, y, z;
            if (!ParseFloat(parts[1], x) || !ParseFloat(parts[2], y) || !ParseFloat(parts[3], z))
                continue;
            positions.push_back({ x, y, z });
        }
        else if (parts[0] == "vt")
        {
            if (parts.size() < 3) continue;
            float u, v;
            if (!ParseFloat(parts[1], u) || !ParseFloat(parts[2], v))
                continue;

            // D3D 텍스처 좌표가 보통 위아래가 뒤집혀 보이는 경우가 많아서:
            // OBJ의 vt는 (0,0)이 아래인 경우가 많음 → 일단 v를 뒤집어준다(필요 없으면 나중에 제거)
            v = 1.0f - v;

            uvs.push_back({ u, v });
        }
        else if (parts[0] == "f")
        {
            // 최소: f v/vt v/vt v/vt ... (폴리곤도 허용 -> fan triangulation)
            if (parts.size() < 4) continue; // 최소 삼각형

            // face 코너들(각 코너는 v/vt)
            struct Corner { int v; int vt; };
            std::vector<Corner> corners;
            corners.reserve(parts.size() - 1);

            bool ok = true;
            for (size_t i = 1; i < parts.size(); ++i)
            {
                int vi = 0, vti = 0;
                if (!ParseFaceToken_V_VT(parts[i], vi, vti))
                {
                    ok = false;
                    break;
                }
                if (vi <= 0 || vti <= 0) { ok = false; break; } // 음수/0 미지원
                corners.push_back({ vi, vti });
            }
            if (!ok)
            {
                if (outError) *outError = "Unsupported face format. Only 'f v/vt ...' with positive indices is supported for now.";
                return false;
            }

            auto emitCorner = [&](const Corner& c)
                {
                    const int vIndex = c.v - 1;   // to 0-based
                    const int vtIndex = c.vt - 1;

                    if (vIndex < 0 || vIndex >= (int)positions.size() ||
                        vtIndex < 0 || vtIndex >= (int)uvs.size())
                    {
                        if (outError) *outError = "OBJ index out of range.";
                        return false;
                    }

                    Vertex vx{};
                    vx.pos[0] = positions[vIndex].x;
                    vx.pos[1] = positions[vIndex].y;
                    vx.pos[2] = positions[vIndex].z;
                    vx.uv[0] = uvs[vtIndex].x;
                    vx.uv[1] = uvs[vtIndex].y;

                    outMesh.vertices.push_back(vx);
                    outMesh.indices.push_back(runningIndex++);
                    return true;
                };

            // fan triangulation: (0, i, i+1)
            // corners[0] 고정, i=1..n-2
            for (size_t i = 1; i + 1 < corners.size(); ++i)
            {
                if (!emitCorner(corners[0])) return false;
                if (!emitCorner(corners[i])) return false;
                if (!emitCorner(corners[i + 1])) return false;
            }
        }
        else
        {
            // vn, mtllib, usemtl, o, g ... 는 나중 단계
            continue;
        }
    }

    if (outMesh.vertices.empty() || outMesh.indices.empty())
    {
        if (outError) *outError = "OBJ loaded but produced empty mesh (no supported faces).";
        return false;
    }

    return true;
}

void D3D12Renderer::CreateDeviceAndSwapChain()
{
    UINT dxgiFlags = 0;
#if defined(_DEBUG)
    dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
            break;
    }

    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = FrameCount;
    sc.Width = m_width;
    sc.Height = m_height;
    sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> sc1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(), m_hwnd, &sc, nullptr, nullptr, &sc1));

    ThrowIfFailed(factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(sc1.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12Renderer::CreateRtvHeapAndViews()
{
    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = FrameCount;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT n = 0; n < FrameCount; ++n)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
        m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, h);
        h.ptr += (SIZE_T)m_rtvDescriptorSize;
    }

    m_viewport = { 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f };
    m_scissorRect = { 0, 0, (LONG)m_width, (LONG)m_height };
}

void D3D12Renderer::CreateDepthBuffer()
{
    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    hd.NumDescriptors = 1;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_dsvHeap)));

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = m_width;
    rd.Height = m_height;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_D32_FLOAT;
    rd.SampleDesc.Count = 1;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE cv{};
    cv.Format = DXGI_FORMAT_D32_FLOAT;
    cv.DepthStencil.Depth = 1.0f;
    cv.DepthStencil.Stencil = 0;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv,
        IID_PPV_ARGS(&m_depthBuffer)));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsv, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Renderer::CreateCommands()
{
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    ThrowIfFailed(m_commandList->Close());
}

void D3D12Renderer::CreateSyncObjects()
{
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
}

void D3D12Renderer::CreatePipeline()
{
    // Root Parameters:
    // 0) CBV(b0)
    D3D12_ROOT_PARAMETER params[2]{};

    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // 1) DescriptorTable(SRV t0)
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0; // t0
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = 0;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &range;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static Sampler(s0)
    D3D12_STATIC_SAMPLER_DESC samp{};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.ShaderRegister = 0; // s0
    samp.RegisterSpace = 0;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samp.MaxLOD = D3D12_FLOAT32_MAX;

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = _countof(params);
    rs.pParameters = params;
    rs.NumStaticSamplers = 1;
    rs.pStaticSamplers = &samp;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
    ThrowIfFailed(m_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    // HLSL: pos+uv, sample texture
    const char* hlsl = R"(
    cbuffer PerFrame : register(b0)
    {
        row_major float4x4 mvp;
    };

    Texture2D    gTex  : register(t0);
    SamplerState gSamp : register(s0);

    struct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD0; };
    struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

    PSIn VSMain(VSIn i)
    {
        PSIn o;
        o.pos = mul(float4(i.pos, 1.0), mvp);
        o.uv  = i.uv;
        return o;
    }

    float4 PSMain(PSIn i) : SV_TARGET
    {
        return gTex.Sample(gSamp, i.uv);
    }
    )";

    auto vs = CompileShader("VSMain", "vs_5_0", hlsl);
    auto ps = CompileShader("PSMain", "ps_5_0", hlsl);

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.InputLayout = { layout, _countof(layout) };
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };

    // Rasterizer
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthClipEnable = TRUE;

    // Blend
    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    for (int i = 0; i < 8; ++i)
    {
        auto& rt = pso.BlendState.RenderTarget[i];
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        rt.BlendEnable = FALSE;
        rt.LogicOpEnable = FALSE;
    }

    // Depth ON 유지 (그대로)
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pipelineState)));
}

void D3D12Renderer::CreateMesh()
{
    const UINT vbSize = (UINT)sizeof(kQuadVerts);
    const UINT ibSize = (UINT)sizeof(kQuadIndices);

    // VB (upload)
    {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = vbSize; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
        rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer)));

        void* p = nullptr;
        D3D12_RANGE r{ 0,0 };
        ThrowIfFailed(m_vertexBuffer->Map(0, &r, &p));
        std::memcpy(p, kQuadVerts, vbSize);
        m_vertexBuffer->Unmap(0, nullptr);

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.SizeInBytes = vbSize;
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    }

    // IB (upload)
    {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = ibSize; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
        rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_indexBuffer)));

        void* p = nullptr;
        D3D12_RANGE r{ 0,0 };
        ThrowIfFailed(m_indexBuffer->Map(0, &r, &p));
        std::memcpy(p, kQuadIndices, ibSize);
        m_indexBuffer->Unmap(0, nullptr);

        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.SizeInBytes = ibSize;
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    }
}

void D3D12Renderer::CreateConstantBuffer()
{
    m_cbStride = (UINT)((sizeof(PerFrameCB) + 255) & ~255u);
    UINT totalSize = m_cbStride * FrameCount;

    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = totalSize; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffer)));

    D3D12_RANGE r{ 0,0 };
    ThrowIfFailed(m_constantBuffer->Map(0, &r, (void**)&m_cbMapped));
}

void D3D12Renderer::CreateDemoResources()
{
    // “화면에 그리기 위해 필요한 것들”을 한 곳에 모아둠
    // 1) 파이프라인(루트시그니처/PSO/셰이더)
    CreatePipeline();

    // 2) 메쉬(버텍스/인덱스 버퍼)
    CreateMesh();

    // 3) 상수버퍼(프레임마다 MVP 쓸 공간)
    CreateConstantBuffer();
}

void D3D12Renderer::RecordAndSubmitFrame()
{
    BeginFrame();
    DrawCube();
    EndFrame();
}

void D3D12Renderer::CreateTexture_Checkerboard()
{
    // 1) SRV heap (shader-visible) 생성
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = 1;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_srvHeap)));
    }

    // 2) CPU에서 임시 RGBA 체커보드 생성
    const UINT texW = 256;
    const UINT texH = 256;
    std::vector<uint32_t> pixels(texW * texH);

    for (UINT y = 0; y < texH; ++y)
    {
        for (UINT x = 0; x < texW; ++x)
        {
            bool checker = ((x / 32) % 2) ^ ((y / 32) % 2);
            uint8_t c = checker ? 230 : 30;
            uint32_t rgba =
                (uint32_t)c |
                ((uint32_t)c << 8) |
                ((uint32_t)c << 16) |
                (0xFFu << 24); // A=255
            pixels[y * texW + x] = rgba;
        }
    }

    // 3) GPU Texture2D(default heap) 생성 (초기 state: COPY_DEST)
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = texW;
    td.Height = texH;
    td.DepthOrArraySize = 1;
    td.MipLevels = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    td.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES hpDef{};
    hpDef.Type = D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &hpDef, D3D12_HEAP_FLAG_NONE, &td,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&m_texture)));

    // 4) 업로드 버퍼 생성 + rowPitch 맞춰 복사
    UINT64 uploadSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;

    m_device->GetCopyableFootprints(&td, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);
    uploadSize = totalBytes;

    D3D12_HEAP_PROPERTIES hpUp{};
    hpUp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bd{};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = uploadSize;
    bd.Height = 1;
    bd.DepthOrArraySize = 1;
    bd.MipLevels = 1;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &hpUp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_textureUpload)));

    // Upload buffer에 tex 데이터를 Row 단위로 복사(footprint.RowPitch 사용)
    uint8_t* mapped = nullptr;
    D3D12_RANGE r{ 0,0 };
    ThrowIfFailed(m_textureUpload->Map(0, &r, (void**)&mapped));

    const uint8_t* src = (const uint8_t*)pixels.data();
    uint8_t* dst = mapped + footprint.Offset;

    for (UINT y = 0; y < texH; ++y)
    {
        std::memcpy(dst + (size_t)footprint.Footprint.RowPitch * y,
            src + (size_t)texW * 4 * y,
            (size_t)texW * 4);
    }

    m_textureUpload->Unmap(0, nullptr);

    // 5) copy 커맨드 기록/실행/대기 (init 시점 1회)
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = m_texture.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = m_textureUpload.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = footprint;

    m_commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    // COPY_DEST -> PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = m_texture.Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &b);

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    WaitForGpu(); // ✅ 업로드 끝날 때까지 기다림(연습용으로 가장 단순)

    // 6) SRV 생성
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;

    m_device->CreateShaderResourceView(m_texture.Get(), &srv,
        m_srvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Renderer::UpdateInput()
{
    float s = 0.05f;
    if (GetAsyncKeyState('W') & 0x8000) m_cubeZ += s;
    if (GetAsyncKeyState('S') & 0x8000) m_cubeZ -= s;
    if (GetAsyncKeyState('A') & 0x8000) m_cubeX -= s;
    if (GetAsyncKeyState('D') & 0x8000) m_cubeX += s;
}

void D3D12Renderer::UpdateConstants()
{
    // 텍스쳐 표시 확인용: 그냥 identity MVP
    XMMATRIX mvp = XMMatrixIdentity();

    PerFrameCB cb{};
    XMStoreFloat4x4(&cb.mvp, mvp);

    uint8_t* dst = m_cbMapped + (size_t)m_cbStride * m_frameIndex;
    std::memcpy(dst, &cb, sizeof(cb));
}

void D3D12Renderer::CreateUploadBufferAndCopy(const void* srcData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer)
{
    // Upload heap = CPU에서 Map해서 GPU가 읽는 메모리(연습용으로 편함)
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = byteSize;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&outBuffer)));

    void* mapped = nullptr;
    D3D12_RANGE range{ 0,0 }; // CPU가 읽지 않음(쓰기만 함)
    ThrowIfFailed(outBuffer->Map(0, &range, &mapped));
    std::memcpy(mapped, srcData, (size_t)byteSize);
    outBuffer->Unmap(0, nullptr);
}

void D3D12Renderer::BeginFrame()
{
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Present -> RT
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &b);

    // RTV/DSV
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += (SIZE_T)m_frameIndex * (SIZE_T)m_rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    const float clearColor[4] = { 0.05f, 0.1f, 0.2f, 1.0f };
    m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
}

void D3D12Renderer::DrawCube()
{
    // ✅ SRV heap 바인딩 (shader-visible heap)
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);

    // Root CBV = 현재 프레임 CB 주소
    D3D12_GPU_VIRTUAL_ADDRESS cbAddr =
        m_constantBuffer->GetGPUVirtualAddress() + (UINT64)m_cbStride * m_frameIndex;
    m_commandList->SetGraphicsRootConstantBufferView(0, cbAddr);

    // ✅ Root SRV table = t0
    m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    // 쿼드 인덱스: 6
    m_commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void D3D12Renderer::EndFrame()
{
    // RT -> Present
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &b);

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    ThrowIfFailed(m_swapChain->Present(1, 0));

    // fence
    const UINT64 fenceToWait = m_fenceValue++;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceToWait));
    if (m_fence->GetCompletedValue() < fenceToWait)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12Renderer::Render()
{
    UpdateInput();
    UpdateConstants();

    RecordAndSubmitFrame();
}

void D3D12Renderer::WaitForGpu()
{
    if (!m_commandQueue || !m_fence) return;
    const UINT64 fenceToWait = m_fenceValue++;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceToWait));
    ThrowIfFailed(m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
}

void D3D12Renderer::Resize(UINT width, UINT height)
{
    if (width == 0 || height == 0) return;
    if (!m_swapChain) return;

    WaitForGpu();

    for (UINT i = 0; i < FrameCount; ++i) m_renderTargets[i].Reset();
    m_depthBuffer.Reset();

    m_width = width;
    m_height = height;

    DXGI_SWAP_CHAIN_DESC desc{};
    ThrowIfFailed(m_swapChain->GetDesc(&desc));
    ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, m_width, m_height, desc.BufferDesc.Format, desc.Flags));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    CreateRtvHeapAndViews();
    CreateDepthBuffer();

    m_viewport = { 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f };
    m_scissorRect = { 0, 0, (LONG)m_width, (LONG)m_height };
}

void D3D12Renderer::Destroy()
{
    WaitForGpu();
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    m_cbMapped = nullptr;
}
