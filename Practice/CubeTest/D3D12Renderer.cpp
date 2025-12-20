#include "D3D12Renderer.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <Windows.h>
#include <cstring>

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

// ---- Cube data (정상) ----
static const D3D12Renderer::Vertex kCubeVerts[] =
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
    0,1,2, 0,2,3,
    4,6,5, 4,7,6,
    4,5,1, 4,1,0,
    3,2,6, 3,6,7,
    1,5,6, 1,6,2,
    4,0,3, 4,3,7
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
    // Root: CBV(b0)
    D3D12_ROOT_PARAMETER rp{};
    rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp.Descriptor.ShaderRegister = 0;
    rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 1;
    rs.pParameters = &rp;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
    ThrowIfFailed(m_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    // ★ 행렬 꼬임 방지 핵심: row_major + mul(v, m)
    const char* hlsl = R"(
    cbuffer PerFrame : register(b0)
    {
        row_major float4x4 mvp;
    };

    struct VSIn { float3 pos : POSITION; float3 col : COLOR; };
    struct PSIn { float4 pos : SV_POSITION; float3 col : COLOR; };

    PSIn VSMain(VSIn i)
    {
        PSIn o;
        o.pos = mul(float4(i.pos, 1.0), mvp);
        o.col = i.col;
        return o;
    }

    float4 PSMain(PSIn i) : SV_TARGET
    {
        return float4(i.col, 1.0);
    }
    )";

    auto vs = CompileShader("VSMain", "vs_5_0", hlsl);
    auto ps = CompileShader("PSMain", "ps_5_0", hlsl);

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT,0,0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
        { "COLOR",   0, DXGI_FORMAT_R32G32B32_FLOAT,0,12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
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

    // Blend (default-ish)
    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    for (int i = 0; i < 8; ++i)
    {
        auto& rt = pso.BlendState.RenderTarget[i];
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        rt.BlendEnable = FALSE;
        rt.LogicOpEnable = FALSE;
    }

    // Depth ON (★ 큐브 찌그러짐 방지 핵심)
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
    const UINT vbSize = (UINT)sizeof(kCubeVerts);
    const UINT ibSize = (UINT)sizeof(kCubeIndices);

    // VB
    CreateUploadBufferAndCopy(kCubeVerts, vbSize, m_vertexBuffer);
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes = vbSize;
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);

    // IB
    CreateUploadBufferAndCopy(kCubeIndices, ibSize, m_indexBuffer);
    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.SizeInBytes = ibSize;
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
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
    // ★ 쿼터뷰: 위에서 내려다보는 카메라
    XMMATRIX world = XMMatrixRotationY(0.6f) * XMMatrixTranslation(m_cubeX, 0.0f, m_cubeZ);

    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(3.0f, 3.0f, -3.0f, 1.0f),
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );

    // 튜토리얼 느낌(크기 변화 싫으면) → Ortho 추천
    XMMATRIX proj = XMMatrixOrthographicLH(12.0f, 12.0f, 0.1f, 100.0f);

    XMMATRIX mvp = world * view * proj;

    PerFrameCB cb{};
    XMStoreFloat4x4(&cb.mvp, mvp); // row_major + mul(v,m) 이라 전치 없음

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
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);

    // Root CBV = 현재 프레임 CB 주소
    D3D12_GPU_VIRTUAL_ADDRESS cbAddr =
        m_constantBuffer->GetGPUVirtualAddress() + (UINT64)m_cbStride * m_frameIndex;

    m_commandList->SetGraphicsRootConstantBufferView(0, cbAddr);

    m_commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
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
