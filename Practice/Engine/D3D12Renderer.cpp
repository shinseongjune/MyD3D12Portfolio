#include "D3D12Renderer.h"
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <d3dcompiler.h>
#include "MeshManager.h"
#include "DebugDraw.h"
#include "TextureManager.h"
#include "TextureHandle.h"
#include "TextureCpuData.h"
#include "Utilities.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

void D3D12Renderer::ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) throw std::runtime_error("D3D12 call failed.");
}

void D3D12Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height)
{
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    CreateDeviceAndSwapChain(hwnd);
    CreateCommandObjects();
    CreateDescriptorHeaps();
    CreateRenderTargets();
    CreateDepthStencil(width, height);

    CreatePipeline();
    CreateConstantBuffer();

    CreateDebugLinePipeline();
    CreateDebugVertexBuffer();

    // fence
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValues[0] = 1;
    m_fenceValues[1] = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) throw std::runtime_error("CreateEvent failed.");

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // 초기 커맨드 리스트 녹화 (기본 텍스처 업로드 포함)
    ThrowIfFailed(m_commandAllocators[0]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(), nullptr));

    // slot 0 기본 텍스처 생성 (checkerboard)
    CreateDefaultTexture_Checkerboard();

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    // viewport/scissor
    m_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    m_scissor = { 0, 0, (LONG)width, (LONG)height };

    WaitForGPU();

    // 초기화에서 만든 텍스처 upload는 fence 완료됐으니 정리 가능
    for (auto& [id, t] : m_gpuTextures)
        t.upload.Reset();
}

void D3D12Renderer::Shutdown()
{
    WaitForGPU();

    m_pendingMeshReleases.clear();
    m_gpuMeshes.clear();

    m_pendingTextureReleases.clear();
    m_pendingTextureUploadReleases.clear();
    m_gpuTextures.clear();

    if (m_cbMapped)
    {
        m_cb->Unmap(0, nullptr);
        m_cbMapped = nullptr;
    }

    if (m_debugVBMapped)
    {
        m_debugVB->Unmap(0, nullptr);
        m_debugVBMapped = nullptr;
    }

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    // ComPtr 자동 해제
}

void D3D12Renderer::SetMeshManager(MeshManager* mm)
{
    m_meshManager = mm;

    if (m_meshManager)
    {
        m_meshManager->SetOnDestroy([this](uint32_t meshId) {
            this->RetireMesh(meshId);
            });
    }
}

void D3D12Renderer::SetTextureManager(TextureManager* tm)
{
    m_textureManager = tm;

    if (m_textureManager)
    {
        m_textureManager->SetOnDestroy([this](uint32_t texId) {
            this->RetireTexture(texId);
            });
    }
}

void D3D12Renderer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return;
    if (width == m_width && height == m_height) return;

    WaitForGPU();

    m_width = width;
    m_height = height;

    for (uint32_t i = 0; i < FrameCount; ++i)
        m_renderTargets[i].Reset();
    m_depthStencil.Reset();

    DXGI_SWAP_CHAIN_DESC desc{};
    ThrowIfFailed(m_swapChain->GetDesc(&desc));
    ThrowIfFailed(m_swapChain->ResizeBuffers(
        FrameCount,
        width, height,
        desc.BufferDesc.Format,
        desc.Flags));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    CreateRenderTargets();
    CreateDepthStencil(width, height);

    m_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    m_scissor = { 0, 0, (LONG)width, (LONG)height };
}

void D3D12Renderer::Render(const std::vector<RenderItem>& items, const RenderCamera& cam)
{
    ProcessPendingMeshReleases();
    ProcessPendingTextureUploadReleases();
    ProcessPendingTextureReleases();

    m_texturesCreatedThisFrame.clear();

    // Reset allocator/list
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pso.Get()));

    // Transition: Present -> RenderTarget
    {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = m_renderTargets[m_frameIndex].Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &b);
    }

    // RTV/DSV handles
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += (SIZE_T)m_frameIndex * m_rtvDescriptorSize;

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    // Clear
    const float clearColor[4] = { 0.08f, 0.09f, 0.12f, 1.0f };
    m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // State
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissor);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // SRV heap
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);

    // View/Proj
    XMMATRIX V = XMLoadFloat4x4(&cam.view);
    XMMATRIX P = XMLoadFloat4x4(&cam.proj);

    const uint32_t drawCount = std::min<uint32_t>((uint32_t)items.size(), MaxDrawsPerFrame);
    const uint32_t frameBase = m_frameIndex * MaxDrawsPerFrame;

    struct DrawKey
    {
        uint32_t itemIndex = 0;
        uint32_t srvIndex = 0;
        uint32_t meshId = 0;
        uint64_t key = 0;
    };

    std::vector<DrawKey> order;
    order.resize(drawCount);

    for (uint32_t i = 0; i < drawCount; ++i)
    {
        const RenderItem& it = items[i];

        // TextureHandle -> srvIndex 를 renderer가 해결
        uint32_t srvIndex = 0;
        srvIndex = GetOrCreateSrvIndex(it.albedo); // TextureHandle이 없으면 0(기본)

        order[i].itemIndex = i;
        order[i].srvIndex = srvIndex;
        order[i].meshId = it.mesh.id;
        order[i].key = (uint64_t(srvIndex) << 32) | uint64_t(it.mesh.id);
    }

    std::sort(order.begin(), order.end(),
        [&](const DrawKey& a, const DrawKey& b) { return a.key < b.key; });

    // (2) Cached state
    uint32_t lastSrvIndex = 0xFFFFFFFFu;
    uint32_t lastMeshId = 0xFFFFFFFFu;

    D3D12_GPU_DESCRIPTOR_HANDLE srvBase = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

    for (uint32_t k = 0; k < drawCount; ++k)
    {
        const RenderItem& it = items[order[k].itemIndex];
        const uint32_t srvIndex = order[k].srvIndex;

        // (A) SRV 바뀔 때만 DescriptorTable 세팅
        if (srvIndex != lastSrvIndex)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE h = srvBase;
            h.ptr += (UINT64)srvIndex * (UINT64)m_srvDescriptorSize;
            m_commandList->SetGraphicsRootDescriptorTable(1, h);
            lastSrvIndex = srvIndex;
        }

        // (B) Mesh 바뀔 때만 IA 설정
        if (it.mesh.id != lastMeshId)
        {
            MeshGPUData& mesh = GetOrCreateGPUMesh(it.mesh.id);
            m_commandList->IASetVertexBuffers(0, 1, &mesh.vbView);
            m_commandList->IASetIndexBuffer(&mesh.ibView);
            lastMeshId = it.mesh.id;
        }

        // (C) Per-draw CB
        XMMATRIX W = XMLoadFloat4x4(&it.world);
        XMMATRIX MVP = W * V * P;

        DrawCB cb{};
        XMStoreFloat4x4(&cb.mvp, MVP);
        cb.color = it.color;

        const uint32_t slot = frameBase + k;
        std::memcpy(m_cbMapped + slot * m_cbStride, &cb, sizeof(DrawCB));

        D3D12_GPU_VIRTUAL_ADDRESS cbAddr =
            m_cb->GetGPUVirtualAddress() + (UINT64)slot * (UINT64)m_cbStride;

        m_commandList->SetGraphicsRootConstantBufferView(0, cbAddr);

        // Draw
        MeshGPUData& mesh = GetOrCreateGPUMesh(it.mesh.id);

        // count 결정: it.indexCount==0이면 mesh 전체
        const uint32_t count = (it.indexCount != 0) ? it.indexCount : mesh.indexCount;
        const uint32_t start = it.startIndex;

        // Draw
        m_commandList->DrawIndexedInstanced(count, 1, start, 0, 0);
    }

#if defined(_DEBUG)
    // --- Debug Lines ---
    {
        const auto& lines = DebugDraw::GetLines();
        if (!lines.empty() && drawCount < MaxDrawsPerFrame)
        {
            const uint32_t lineCount = std::min<uint32_t>((uint32_t)lines.size(), MaxDebugLinesPerFrame);
            const uint32_t vertexCount = lineCount * 2;

            const uint32_t baseVertex = m_frameIndex * MaxDebugVerticesPerFrame;
            DebugVertex* dst = reinterpret_cast<DebugVertex*>(m_debugVBMapped) + baseVertex;

            for (uint32_t i = 0; i < lineCount; ++i)
            {
                const DebugLine& L = lines[i];
                dst[i * 2 + 0] = { L.a, L.color };
                dst[i * 2 + 1] = { L.b, L.color };
            }

            m_commandList->SetPipelineState(m_psoDebugLine.Get());
            m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
            m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

            D3D12_VERTEX_BUFFER_VIEW vbv{};
            vbv.BufferLocation = m_debugVB->GetGPUVirtualAddress() + (UINT64)baseVertex * (UINT64)m_debugVBStride;
            vbv.SizeInBytes = vertexCount * m_debugVBStride;
            vbv.StrideInBytes = m_debugVBStride;
            m_commandList->IASetVertexBuffers(0, 1, &vbv);

            XMMATRIX VP = V * P;

            DrawCB cb{};
            XMStoreFloat4x4(&cb.mvp, VP);
            cb.color = { 1,1,1,1 };

            const uint32_t debugSlot = frameBase + drawCount;
            std::memcpy(m_cbMapped + debugSlot * m_cbStride, &cb, sizeof(DrawCB));

            D3D12_GPU_VIRTUAL_ADDRESS cbAddr =
                m_cb->GetGPUVirtualAddress() + (UINT64)debugSlot * (UINT64)m_cbStride;

            m_commandList->SetGraphicsRootConstantBufferView(0, cbAddr);

            m_commandList->DrawInstanced(vertexCount, 1, 0, 0);

            // restore
            m_commandList->SetPipelineState(m_pso.Get());
            m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }
#endif

    // Transition: RenderTarget -> Present
    {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = m_renderTargets[m_frameIndex].Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &b);
    }

    ThrowIfFailed(m_commandList->Close());

    // 이 프레임 커맨드 제출에 해당하는 fence 값(업로드 release 기준)
    const uint64_t submitFenceValue = m_fenceValues[m_frameIndex];

    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    ThrowIfFailed(m_swapChain->Present(1, 0));

    // 이번 프레임에서 새로 만든 텍스처들의 upload는 fence 완료 후 release 예약
    for (uint32_t texId : m_texturesCreatedThisFrame)
    {
        m_pendingTextureUploadReleases.push_back(PendingTextureUploadRelease{ texId, submitFenceValue });
    }

    MoveToNextFrame();
}

// ---------------------------
// Core Init
// ---------------------------
void D3D12Renderer::CreateDeviceAndSwapChain(HWND hwnd)
{
    UINT dxgiFlags = 0;
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer();
        dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&m_factory)));

    // Adapter 선택
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
            break;
    }

    if (!m_device)
    {
        ComPtr<IDXGIAdapter> warp;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }

    // Command queue
    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_commandQueue)));

    // Swap chain
    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = FrameCount;
    sc.Width = m_width;
    sc.Height = m_height;
    sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swap1;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(), hwnd, &sc, nullptr, nullptr, &swap1));

    ThrowIfFailed(m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swap1.As(&m_swapChain));
}

void D3D12Renderer::CreateCommandObjects()
{
    for (uint32_t i = 0; i < FrameCount; ++i)
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));

    ThrowIfFailed(m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    ThrowIfFailed(m_commandList->Close());
}

void D3D12Renderer::CreateDescriptorHeaps()
{
    // RTV
    D3D12_DESCRIPTOR_HEAP_DESC rtv{};
    rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv.NumDescriptors = FrameCount;
    rtv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // DSV
    D3D12_DESCRIPTOR_HEAP_DESC dsv{};
    dsv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv.NumDescriptors = 1;
    dsv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsv, IID_PPV_ARGS(&m_dsvHeap)));

    // SRV heap (shader-visible)
    D3D12_DESCRIPTOR_HEAP_DESC sh{};
    sh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    sh.NumDescriptors = 256;
    sh.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&sh, IID_PPV_ARGS(&m_srvHeap)));
    m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    m_nextSrvIndex = 0; // slot0은 기본 텍스처가 쓸 예정
}

void D3D12Renderer::CreateRenderTargets()
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtv);
        rtv.ptr += m_rtvDescriptorSize;
    }
}

void D3D12Renderer::CreateDepthStencil(uint32_t width, uint32_t height)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clear,
        IID_PPV_ARGS(&m_depthStencil)));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    m_device->CreateDepthStencilView(
        m_depthStencil.Get(),
        &dsv,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Renderer::CreatePipeline()
{
    // Root Signature:
    // [0] CBV(b0)
    // [1] DescriptorTable(SRV t0) 1개
    // StaticSampler(s0)
    D3D12_ROOT_PARAMETER rp[2]{};

    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp[0].Descriptor.ShaderRegister = 0;
    rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[1].DescriptorTable.NumDescriptorRanges = 1;
    rp[1].DescriptorTable.pDescriptorRanges = &range;
    rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC ss{};
    ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    ss.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    ss.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    ss.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    ss.MaxLOD = D3D12_FLOAT32_MAX;
    ss.ShaderRegister = 0;
    ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = _countof(rp);
    rs.pParameters = rp;
    rs.NumStaticSamplers = 1;
    rs.pStaticSamplers = &ss;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr))
    {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    ThrowIfFailed(m_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));

    const char* vsCode = R"(
    cbuffer DrawCB : register(b0)
    {
        row_major float4x4 mvp;
        float4 color;
    };

    struct VSIn
    {
        float3 pos : POSITION;
        float2 uv  : TEXCOORD0;
    };

    struct VSOut
    {
        float4 pos : SV_POSITION;
        float2 uv  : TEXCOORD0;
    };

    VSOut main(VSIn i)
    {
        VSOut o;
        o.pos = mul(float4(i.pos, 1.0), mvp);
        o.uv = i.uv;
        return o;
    }
    )";

    const char* psCode = R"(
    cbuffer DrawCB : register(b0)
    {
        row_major float4x4 mvp;
        float4 color;
    };

    Texture2D    gTex  : register(t0);
    SamplerState gSamp : register(s0);

    struct PSIn
    {
        float4 pos : SV_POSITION;
        float2 uv  : TEXCOORD0;
    };

    float4 main(PSIn i) : SV_TARGET
    {
        float4 tex = gTex.Sample(gSamp, i.uv);
        return tex * color;
    }
    )";

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vs, ps, e;
    hr = D3DCompile(vsCode, std::strlen(vsCode), nullptr, nullptr, nullptr, "main", "vs_5_0", flags, 0, &vs, &e);
    if (FAILED(hr))
    {
        if (e) OutputDebugStringA((const char*)e->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    e.Reset();
    hr = D3DCompile(psCode, std::strlen(psCode), nullptr, nullptr, nullptr, "main", "ps_5_0", flags, 0, &ps, &e);
    if (FAILED(hr))
    {
        if (e) OutputDebugStringA((const char*)e->GetBufferPointer());
        ThrowIfFailed(hr);
    }

    D3D12_INPUT_ELEMENT_DESC il[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.InputLayout = { il, _countof(il) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthClipEnable = TRUE;

    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC rtBlend =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };
    for (int i = 0; i < 8; ++i) pso.BlendState.RenderTarget[i] = rtBlend;

    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    pso.SampleMask = UINT_MAX;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso)));
}

void D3D12Renderer::CreateDebugLinePipeline()
{
    const char* vsCode = R"(
    cbuffer DrawCB : register(b0)
    {
        row_major float4x4 mvp;
        float4 color;
    };

    struct VSIn { float3 pos : POSITION; float4 col : COLOR; };
    struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };

    VSOut main(VSIn i)
    {
        VSOut o;
        o.pos = mul(float4(i.pos, 1.0), mvp);
        o.col = i.col;
        return o;
    }
    )";

    const char* psCode = R"(
    struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; };
    float4 main(PSIn i) : SV_TARGET { return i.col; }
    )";

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vs, ps, err;
    ThrowIfFailed(D3DCompile(vsCode, std::strlen(vsCode), nullptr, nullptr, nullptr, "main", "vs_5_0", flags, 0, &vs, &err));
    err.Reset();
    ThrowIfFailed(D3DCompile(psCode, std::strlen(psCode), nullptr, nullptr, nullptr, "main", "ps_5_0", flags, 0, &ps, &err));

    D3D12_INPUT_ELEMENT_DESC il[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.InputLayout = { il, _countof(il) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.RasterizerState.AntialiasedLineEnable = TRUE;

    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC rtBlend =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };
    for (int i = 0; i < 8; ++i) pso.BlendState.RenderTarget[i] = rtBlend;

    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    pso.SampleMask = UINT_MAX;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_psoDebugLine)));
}

void D3D12Renderer::CreateDebugVertexBuffer()
{
    m_debugVBStride = (uint32_t)sizeof(DebugVertex);
    const uint64_t totalVerts = (uint64_t)MaxDebugVerticesPerFrame * (uint64_t)FrameCount;
    const uint64_t totalSize = totalVerts * (uint64_t)m_debugVBStride;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = totalSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_debugVB)));

    ThrowIfFailed(m_debugVB->Map(0, nullptr, (void**)&m_debugVBMapped));
}

void D3D12Renderer::CreateConstantBuffer()
{
    m_cbStride = Align256((uint32_t)sizeof(DrawCB));
    const uint64_t totalSize =
        (uint64_t)m_cbStride * (uint64_t)MaxDrawsPerFrame * (uint64_t)FrameCount;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = totalSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_cb)));

    ThrowIfFailed(m_cb->Map(0, nullptr, (void**)&m_cbMapped));
}

void D3D12Renderer::WaitForGPU()
{
    const uint64_t fenceValue = m_fenceValues[m_frameIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceValue));
    ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
    m_fenceValues[m_frameIndex]++;
}

void D3D12Renderer::MoveToNextFrame()
{
    const uint64_t currentFence = m_fenceValues[m_frameIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFence));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_fenceValues[m_frameIndex] = currentFence + 1;
}

// ---------------------------
// Mesh cache
// ---------------------------
MeshGPUData& D3D12Renderer::GetOrCreateGPUMesh(uint32_t meshId)
{
    auto it = m_gpuMeshes.find(meshId);
    if (it != m_gpuMeshes.end())
        return it->second;

    assert(m_meshManager && "MeshManager not set");
    const MeshCPUData& cpu = m_meshManager->Get({ meshId });

    MeshGPUData gpu{};
    CreateGPUMeshFromCPU(cpu, gpu);

    auto [iter, _] = m_gpuMeshes.emplace(meshId, std::move(gpu));
    return iter->second;
}

void D3D12Renderer::CreateGPUMeshFromCPU(const MeshCPUData& cpu, MeshGPUData& out)
{
    struct VertexPU
    {
        XMFLOAT3 pos;
        XMFLOAT2 uv;
    };

    std::vector<VertexPU> verts;
    verts.resize(cpu.positions.size());

    for (size_t i = 0; i < verts.size(); ++i)
    {
        verts[i].pos = cpu.positions[i];
        verts[i].uv = (i < cpu.uvs.size()) ? cpu.uvs[i] : XMFLOAT2{ 0,0 };
    }

    const UINT vbSize = (UINT)(verts.size() * sizeof(VertexPU));
    const UINT ibSize = (UINT)(cpu.indices.size() * sizeof(uint16_t));

    out.indexCount = (uint32_t)cpu.indices.size();

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    // VB
    D3D12_RESOURCE_DESC vbDesc{};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = vbSize;
    vbDesc.Height = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels = 1;
    vbDesc.SampleDesc.Count = 1;
    vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out.vb)));

    void* p = nullptr;
    out.vb->Map(0, nullptr, &p);
    std::memcpy(p, verts.data(), vbSize);
    out.vb->Unmap(0, nullptr);

    out.vbView.BufferLocation = out.vb->GetGPUVirtualAddress();
    out.vbView.SizeInBytes = vbSize;
    out.vbView.StrideInBytes = sizeof(VertexPU);

    // IB
    D3D12_RESOURCE_DESC ibDesc = vbDesc;
    ibDesc.Width = ibSize;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out.ib)));

    out.ib->Map(0, nullptr, &p);
    std::memcpy(p, cpu.indices.data(), ibSize);
    out.ib->Unmap(0, nullptr);

    out.ibView.BufferLocation = out.ib->GetGPUVirtualAddress();
    out.ibView.SizeInBytes = ibSize;
    out.ibView.Format = DXGI_FORMAT_R16_UINT;
}

void D3D12Renderer::RetireMesh(uint32_t meshId)
{
    if (m_gpuMeshes.find(meshId) == m_gpuMeshes.end())
        return;

    const uint64_t retireFence = m_fenceValues[m_frameIndex];
    m_pendingMeshReleases.push_back(PendingMeshRelease{ meshId, retireFence });
}

void D3D12Renderer::ProcessPendingMeshReleases()
{
    if (!m_fence) return;

    const uint64_t completed = m_fence->GetCompletedValue();

    size_t write = 0;
    for (size_t i = 0; i < m_pendingMeshReleases.size(); ++i)
    {
        const auto& r = m_pendingMeshReleases[i];
        if (completed >= r.retireFenceValue)
        {
            m_gpuMeshes.erase(r.meshId);
        }
        else
        {
            m_pendingMeshReleases[write++] = r;
        }
    }
    m_pendingMeshReleases.resize(write);
}

// ---------------------------
// Texture cache
// ---------------------------
uint32_t D3D12Renderer::GetOrCreateSrvIndex(const TextureHandle& h)
{
    // invalid -> default slot 0
    if (!h.IsValid())
        return 0;

    auto it = m_gpuTextures.find(h.id);
    if (it != m_gpuTextures.end())
        return it->second.srvIndex;

    assert(m_textureManager && "TextureManager not set");
    const TextureCpuData& cpu = m_textureManager->Get(h);

    TextureGPUData gpu{};
    CreateGPUTextureFromCPU(cpu, gpu);

    auto [iter, inserted] = m_gpuTextures.emplace(h.id, std::move(gpu));
    m_texturesCreatedThisFrame.push_back(h.id);
    return iter->second.srvIndex;
}

void D3D12Renderer::CreateGPUTextureFromCPU(const TextureCpuData& cpu, TextureGPUData& out)
{
    if (cpu.width == 0 || cpu.height == 0 || cpu.pixels.empty())
        throw std::runtime_error("CreateGPUTextureFromCPU: invalid cpu texture data.");

    // SRV 슬롯 할당
    const uint32_t srvIndex = m_nextSrvIndex++;
    if (srvIndex >= 256)
        throw std::runtime_error("SRV heap is full (>=256).");

    const DXGI_FORMAT fmt =
        (cpu.colorSpace == ImageColorSpace::SRGB)
        ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
        : DXGI_FORMAT_R8G8B8A8_UNORM;

    // Texture (Default heap)
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = cpu.width;
    td.Height = cpu.height;
    td.DepthOrArraySize = 1;
    td.MipLevels = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES hpDefault{};
    hpDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

    ComPtr<ID3D12Resource> tex;
    ThrowIfFailed(m_device->CreateCommittedResource(
        &hpDefault,
        D3D12_HEAP_FLAG_NONE,
        &td,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&tex)));

    // Upload buffer
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;
    m_device->GetCopyableFootprints(&td, 0, 1, 0, &fp, &numRows, &rowSizeInBytes, &totalBytes);

    D3D12_RESOURCE_DESC bd{};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = totalBytes;
    bd.Height = 1;
    bd.DepthOrArraySize = 1;
    bd.MipLevels = 1;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES hpUpload{};
    hpUpload.Type = D3D12_HEAP_TYPE_UPLOAD;

    ComPtr<ID3D12Resource> upload;
    ThrowIfFailed(m_device->CreateCommittedResource(
        &hpUpload,
        D3D12_HEAP_FLAG_NONE,
        &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&upload)));

    // Map + copy rowPitch
    uint8_t* dst = nullptr;
    D3D12_RANGE r{ 0,0 };
    ThrowIfFailed(upload->Map(0, &r, (void**)&dst));

    const uint8_t* src = cpu.pixels.data();
    const uint32_t srcRowBytes = cpu.width * 4;

    for (uint32_t y = 0; y < cpu.height; ++y)
    {
        std::memcpy(dst + (size_t)y * fp.Footprint.RowPitch,
            src + (size_t)y * srcRowBytes,
            srcRowBytes);
    }
    upload->Unmap(0, nullptr);

    // CopyTextureRegion
    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = tex.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = upload.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = fp;

    m_commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    // barrier: COPY_DEST -> PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = tex.Get();
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    m_commandList->ResourceBarrier(1, &b);

    // SRV 생성
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = fmt;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += (UINT64)srvIndex * (UINT64)m_srvDescriptorSize;
    m_device->CreateShaderResourceView(tex.Get(), &srv, cpuHandle);

    // out
    out.tex = tex;
    out.upload = upload;
    out.srvIndex = srvIndex;
    out.width = cpu.width;
    out.height = cpu.height;
    out.format = fmt;
}

void D3D12Renderer::RetireTexture(uint32_t texId)
{
    auto it = m_gpuTextures.find(texId);
    if (it == m_gpuTextures.end())
        return;

    const uint64_t retireFence = m_fenceValues[m_frameIndex];
    m_pendingTextureReleases.push_back(PendingTextureRelease{ texId, retireFence });
}

void D3D12Renderer::ProcessPendingTextureReleases()
{
    if (!m_fence) return;

    const uint64_t completed = m_fence->GetCompletedValue();

    size_t write = 0;
    for (size_t i = 0; i < m_pendingTextureReleases.size(); ++i)
    {
        const auto& r = m_pendingTextureReleases[i];
        if (completed >= r.retireFenceValue)
        {
            m_gpuTextures.erase(r.texId);
        }
        else
        {
            m_pendingTextureReleases[write++] = r;
        }
    }
    m_pendingTextureReleases.resize(write);
}

void D3D12Renderer::ProcessPendingTextureUploadReleases()
{
    if (!m_fence) return;

    const uint64_t completed = m_fence->GetCompletedValue();

    size_t write = 0;
    for (size_t i = 0; i < m_pendingTextureUploadReleases.size(); ++i)
    {
        const auto& r = m_pendingTextureUploadReleases[i];
        if (completed >= r.retireFenceValue)
        {
            auto it = m_gpuTextures.find(r.texId);
            if (it != m_gpuTextures.end())
                it->second.upload.Reset();
        }
        else
        {
            m_pendingTextureUploadReleases[write++] = r;
        }
    }
    m_pendingTextureUploadReleases.resize(write);
}

void D3D12Renderer::CreateDefaultTexture_Checkerboard()
{
    // slot0 예약: 첫 텍스처 생성이 slot0을 먹도록
    // (CreateDescriptorHeaps에서 m_nextSrvIndex=0)
    const uint32_t texW = 256, texH = 256;
    std::vector<uint8_t> rgba(texW * texH * 4);

    for (uint32_t y = 0; y < texH; ++y)
    {
        for (uint32_t x = 0; x < texW; ++x)
        {
            bool c = ((x / 32) ^ (y / 32)) & 1;
            uint8_t v = c ? 230 : 30;

            const size_t i = (size_t)(y * texW + x) * 4;
            rgba[i + 0] = v;
            rgba[i + 1] = v;
            rgba[i + 2] = v;
            rgba[i + 3] = 255;
        }
    }

    TextureCpuData cpu{};
    cpu.width = texW;
    cpu.height = texH;
    cpu.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    cpu.colorSpace = ImageColorSpace::SRGB;
    cpu.pixels = std::move(rgba);

    // 기본 텍스처는 TextureHandle 없이도 slot0만 쓰면 되므로,
    // id=0 캐시에 넣어둔다.
    TextureGPUData gpu{};
    CreateGPUTextureFromCPU(cpu, gpu);

    // slot0인지 확인 (디버그)
#if defined(_DEBUG)
    assert(gpu.srvIndex == 0 && "Default texture must occupy SRV slot 0.");
#endif

    m_gpuTextures.emplace(0u, std::move(gpu));

    m_nextSrvIndex = 1; // 이제부터는 slot1부터
}
