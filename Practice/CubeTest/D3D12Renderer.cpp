#include "D3D12Renderer.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <DirectXMath.h>
using namespace DirectX;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

struct Vertex { float pos[3]; float color[3]; };

static const Vertex kCubeVerts[] =
{
    {{-0.5f,-0.5f,-0.5f},{1,0,0}},
    {{-0.5f,+0.5f,-0.5f},{0,1,0}},
    {{+0.5f,+0.5f,-0.5f},{0,0,1}},
    {{+0.5f,-0.5f,-0.5f},{1,1,0}},
    {{-0.5f,-0.5f,+0.5f},{1,0,1}},
    {{-0.5f,+0.5f,+0.5f},{0,1,1}},
    {{+0.5f,+0.5f,+0.5f},{1,1,1}},
    {{+0.5f,-0.5f,+0.5f},{0,0,0}},
};

static const uint16_t kCubeIndices[] =
{
    // front (-z)
    0,1,2, 0,2,3,
    // back (+z)
    4,6,5, 4,7,6,
    // left (-x)
    4,5,1, 4,1,0,
    // right (+x)
    3,2,6, 3,6,7,
    // top (+y)
    1,5,6, 1,6,2,
    // bottom (-y)
    4,0,3, 4,3,7
};

static inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) throw std::runtime_error("HRESULT failed");
}

void D3D12Renderer::Init(HWND hwnd, UINT width, UINT height)
{
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

#if defined(_DEBUG)
    // 디버그 레이어 (가능하면 켜두는 게 좋음)
    ComPtr<ID3D12Debug> dbg;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
        dbg->EnableDebugLayer();
#endif

    CreateDeviceAndSwapChain();
    CreateRtvHeapAndViews();
    CreateCommands();
    CreateSyncObjects();
    LoadAssets();
}

void D3D12Renderer::CreateDeviceAndSwapChain()
{
    UINT dxgiFlags = 0;
#if defined(_DEBUG)
    dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory)));

    // 하드웨어 어댑터 선택 (가장 단순 버전)
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            break;
    }

    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

    // CommandQueue
    D3D12_COMMAND_QUEUE_DESC q = {};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_commandQueue)));

    // SwapChain
    DXGI_SWAP_CHAIN_DESC1 sc = {};
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
    D3D12_DESCRIPTOR_HEAP_DESC hd = {};
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
}

void D3D12Renderer::CreateCommands()
{
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
    ThrowIfFailed(m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    ThrowIfFailed(m_commandList->Close());
}

void D3D12Renderer::CreateSyncObjects()
{
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
}

static ComPtr<ID3DBlob> CompileShader(const wchar_t* name, const char* entry, const char* target, const char* src)
{
    UINT flags = 0;
#if defined(_DEBUG)
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, flags, 0, &blob, &err);
    if (FAILED(hr))
    {
        if (err) OutputDebugStringA((char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    return blob;
}

void D3D12Renderer::LoadAssets()
{
    // Viewport / Scissor
    m_viewport = {};
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = (float)m_width;
    m_viewport.Height = (float)m_height;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
    m_scissorRect = { 0, 0, (LONG)m_width, (LONG)m_height };

    // ----- Root Signature: Root CBV 하나(b0)만 사용 -----
    D3D12_ROOT_PARAMETER rp = {};
    rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp.Descriptor.ShaderRegister = 0;   // b0
    rp.Descriptor.RegisterSpace = 0;
    rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 1;
    rsDesc.pParameters = &rp;
    rsDesc.NumStaticSamplers = 0;
    rsDesc.pStaticSamplers = nullptr;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob));
    ThrowIfFailed(m_device->CreateRootSignature(
        0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    // ----- HLSL (문자열로 박아두기: 파일 관리 생략, 큐브 뜨는 게 목표) -----
    const char* shaderSrc = R"(
cbuffer MVP : register(b0)
{
    float4x4 mvp;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 col : COLOR;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

PSIn VSMain(VSIn vin)
{
    PSIn o;
    o.pos = mul(mvp, float4(vin.pos, 1.0));
    o.col = vin.col;
    return o;
}

float4 PSMain(PSIn pin) : SV_TARGET
{
    return float4(pin.col, 1.0);
}
)";

    auto vs = CompileShader(L"vs", "VSMain", "vs_5_0", shaderSrc);
    auto ps = CompileShader(L"ps", "PSMain", "ps_5_0", shaderSrc);

    // ----- PSO -----
    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT,0,0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
        { "COLOR",0, DXGI_FORMAT_R32G32B32_FLOAT,0,12,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.InputLayout = { layout, _countof(layout) };
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    
    // RasterizerState (기본값에 해당)
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    pso.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    pso.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.RasterizerState.MultisampleEnable = FALSE;
    pso.RasterizerState.AntialiasedLineEnable = FALSE;
    pso.RasterizerState.ForcedSampleCount = 0;
    pso.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // BlendState (기본값에 해당)
    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    {
        D3D12_RENDER_TARGET_BLEND_DESC rt = {};
        rt.BlendEnable = FALSE;
        rt.LogicOpEnable = FALSE;
        rt.SrcBlend = D3D12_BLEND_ONE;
        rt.DestBlend = D3D12_BLEND_ZERO;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ZERO;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.LogicOp = D3D12_LOGIC_OP_NOOP;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        for (int i = 0; i < 8; ++i)
            pso.BlendState.RenderTarget[i] = rt;
    }

    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pipelineState)));

    // ----- Vertex/Index Buffer: 업로드 힙(단순화) -----
    const UINT vbSize = (UINT)sizeof(kCubeVerts);
    const UINT ibSize = (UINT)sizeof(kCubeIndices);

    // VertexBuffer (UPLOAD)
    {
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = vbSize;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        void* p = nullptr;
        D3D12_RANGE r = { 0,0 };
        ThrowIfFailed(m_vertexBuffer->Map(0, &r, &p));
        memcpy(p, kCubeVerts, vbSize);
        m_vertexBuffer->Unmap(0, nullptr);

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.SizeInBytes = vbSize;
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    }

    // IndexBuffer (UPLOAD)
    {
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = ibSize;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_indexBuffer)));

        void* p = nullptr;
        D3D12_RANGE r = { 0,0 };
        ThrowIfFailed(m_indexBuffer->Map(0, &r, &p));
        memcpy(p, kCubeIndices, ibSize);
        m_indexBuffer->Unmap(0, nullptr);

        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.SizeInBytes = ibSize;
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    }

    // ----- ConstantBuffer (UPLOAD, 256바이트 정렬) -----
    {
        UINT cbSize = (UINT)sizeof(XMFLOAT4X4);
        cbSize = (cbSize + 255) & ~255u;

        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = cbSize;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_constantBuffer)));

        // WVP 작성 (간단 카메라)
        XMMATRIX world = XMMatrixIdentity();
        XMMATRIX view = XMMatrixLookAtLH({ 0,0,-2.0f }, { 0,0,0 }, { 0,1,0 });
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)m_width / (float)m_height, 0.1f, 100.0f);
        XMMATRIX mvp = XMMatrixTranspose(world * view * proj);

        XMFLOAT4X4 mvpMat;
        XMStoreFloat4x4(&mvpMat, mvp);

        D3D12_RANGE r = { 0,0 };
        ThrowIfFailed(m_constantBuffer->Map(0, &r, (void**)&m_cbvMappedData));
        memcpy(m_cbvMappedData, &mvpMat, sizeof(mvpMat));
        // Unmap 안 함: 상수버퍼는 보통 계속 매핑해둠
    }
}

void D3D12Renderer::Render()
{
    // Reset
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Present -> RenderTarget
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &b);

    // RTV handle for current frame
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += (SIZE_T)m_frameIndex * (SIZE_T)m_rtvDescriptorSize;

    // Clear
    const float clearColor[4] = { 0.05f, 0.1f, 0.2f, 1.0f };
    m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    // 1) 뷰포트/가위 설정
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // 2) 파이프라인 지정
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetPipelineState(m_pipelineState.Get());

    m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());

    // 5) IA 단계: 버텍스/인덱스 바인딩
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);

    // 6) 드로우
    m_commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);

    // RenderTarget -> Present
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    m_commandList->ResourceBarrier(1, &b);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    ThrowIfFailed(m_swapChain->Present(1, 0));

    // Fence sync (간단 버전)
    const UINT64 fenceToWait = m_fenceValue++;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceToWait));
    if (m_fence->GetCompletedValue() < fenceToWait)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
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
}

void D3D12Renderer::Destroy()
{
    WaitForGpu();
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
}