#include "D3D12Renderer.h"

#include <stdexcept>
#include <algorithm>
#include <d3dcompiler.h>
#include "MeshManager.h"
#include "DebugDraw.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

static void ThrowIfFailed(HRESULT hr)
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

    // viewport/scissor
    m_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    m_scissor = { 0, 0, (LONG)width, (LONG)height };

    // fence
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValues[0] = 1;
    m_fenceValues[1] = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) throw std::runtime_error("CreateEvent failed.");

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12Renderer::Shutdown()
{
    WaitForGPU();

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

    // ComPtr들은 자동 해제
}

void D3D12Renderer::SetMeshManager(const MeshManager* mm)
{
    m_meshManager = mm;
}

void D3D12Renderer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return; // 최소화 등
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

    // View/Proj 행렬
    XMMATRIX V = XMLoadFloat4x4(&cam.view);
    XMMATRIX P = XMLoadFloat4x4(&cam.proj);

    // CB offset (frame-local)
    const uint32_t drawCount = std::min<uint32_t>((uint32_t)items.size(), MaxDrawsPerFrame);
    const uint32_t frameBase = m_frameIndex * MaxDrawsPerFrame;

    for (uint32_t i = 0; i < drawCount; ++i)
    {
        const RenderItem& it = items[i];

        // 1) GPU 메쉬 확보
        GPUMesh& mesh = GetOrCreateGPUMesh(it.mesh.id);

        // 2) IA 설정
        m_commandList->IASetVertexBuffers(0, 1, &mesh.vbView);
        m_commandList->IASetIndexBuffer(&mesh.ibView);

        // 3) 상수버퍼 설정 (MVP, color)
        XMMATRIX W = XMLoadFloat4x4(&it.world);
        XMMATRIX MVP = W * V * P;

        DrawCB cb{};
        XMStoreFloat4x4(&cb.mvp, MVP);
        cb.color = it.color;

        const uint32_t slot = frameBase + i;
        memcpy(m_cbMapped + slot * m_cbStride, &cb, sizeof(DrawCB));

        D3D12_GPU_VIRTUAL_ADDRESS cbAddr =
            m_cb->GetGPUVirtualAddress() + (UINT64)slot * m_cbStride;

        m_commandList->SetGraphicsRootConstantBufferView(0, cbAddr);

        // 4) Draw
        m_commandList->DrawIndexedInstanced(
            mesh.indexCount, 1, 0, 0, 0);
    }


    // --- Debug Lines (world-space) ---
    {
        const auto& lines = DebugDraw::GetLines();
        if (!lines.empty() && drawCount < MaxDrawsPerFrame)
        {
            const uint32_t lineCount = std::min<uint32_t>((uint32_t)lines.size(), MaxDebugLinesPerFrame);
            const uint32_t vertexCount = lineCount * 2;

            // Fill frame-sliced VB
            const uint32_t baseVertex = m_frameIndex * MaxDebugVerticesPerFrame;
            DebugVertex* dst = reinterpret_cast<DebugVertex*>(m_debugVBMapped) + baseVertex;

            for (uint32_t i = 0; i < lineCount; ++i)
            {
                const DebugLine& L = lines[i];
                dst[i * 2 + 0] = { L.a, L.color };
                dst[i * 2 + 1] = { L.b, L.color };
            }

            // Set pipeline
            m_commandList->SetPipelineState(m_psoDebugLine.Get());
            m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
            m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

            // Set VB view (subrange for this frame)
            D3D12_VERTEX_BUFFER_VIEW vbv{};
            vbv.BufferLocation = m_debugVB->GetGPUVirtualAddress() + (UINT64)baseVertex * (UINT64)m_debugVBStride;
            vbv.SizeInBytes = vertexCount * m_debugVBStride;
            vbv.StrideInBytes = m_debugVBStride;
            m_commandList->IASetVertexBuffers(0, 1, &vbv);

            // Set CB (use next slot after mesh draws in this frame)
            XMMATRIX VP = V * P;

            DrawCB cb{};
            XMStoreFloat4x4(&cb.mvp, VP);
            cb.color = { 1,1,1,1 }; // unused in debug PS

            const uint32_t debugSlot = frameBase + drawCount; // safe: drawCount <= MaxDrawsPerFrame
            memcpy(m_cbMapped + debugSlot * m_cbStride, &cb, sizeof(DrawCB));

            D3D12_GPU_VIRTUAL_ADDRESS cbAddr =
                m_cb->GetGPUVirtualAddress() + (UINT64)debugSlot * (UINT64)m_cbStride;

            m_commandList->SetGraphicsRootConstantBufferView(0, cbAddr);

            // Draw
            m_commandList->DrawInstanced(vertexCount, 1, 0, 0);

            // Restore triangle PSO for future (optional)
            m_commandList->SetPipelineState(m_pso.Get());
            m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }


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

    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    // Present (vsync on)
    ThrowIfFailed(m_swapChain->Present(1, 0));

    MoveToNextFrame();
}

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

    // Adapter 선택 (첫 하드웨어 어댑터)
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
        // fallback: WARP
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

    // 처음엔 close 상태여야 reset이 쉬움
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
    dsv.Flags = D3D12_DSV_FLAG_NONE;

    m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsv, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Renderer::CreatePipeline()
{
    // Root signature: CBV(b0)
    D3D12_ROOT_PARAMETER rp{};
    rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp.Descriptor.ShaderRegister = 0;
    rp.Descriptor.RegisterSpace = 0;
    rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 1;
    rs.pParameters = &rp;
    rs.NumStaticSamplers = 0;
    rs.pStaticSamplers = nullptr;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
    ThrowIfFailed(m_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    // Shaders (inline)
    const char* vsCode = R"(
    cbuffer DrawCB : register(b0)
    {
        row_major float4x4 mvp;
        float4 color;
    };

    struct VSIn { float3 pos : POSITION; };
    struct VSOut { float4 pos : SV_POSITION; };

    VSOut main(VSIn i)
    {
        VSOut o;
        o.pos = mul(float4(i.pos, 1.0), mvp);
        return o;
    }
    )";

    const char* psCode = R"(
    cbuffer DrawCB : register(b0)
    {
        row_major float4x4 mvp;
        float4 color;
    };
    float4 main() : SV_TARGET { return color; }
    )";

    ComPtr<ID3DBlob> vs, ps;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ThrowIfFailed(D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "main", "vs_5_0", flags, 0, &vs, &err));
    ThrowIfFailed(D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "main", "ps_5_0", flags, 0, &ps, &err));

    // Input layout
    D3D12_INPUT_ELEMENT_DESC il[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.InputLayout = { il, _countof(il) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    //pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // d3dx12 없으면 아래로 교체
    //pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    //pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    // RasterizerState (default)
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

    // BlendState (default)
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

    // DepthStencilState (default depth on)
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    pso.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    pso.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    pso.DepthStencilState.BackFace = pso.DepthStencilState.FrontFace;

    pso.SampleMask = UINT_MAX;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso)));
}

void D3D12Renderer::CreateDebugLinePipeline()
{
    // Shaders (inline)
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

    ComPtr<ID3DBlob> vs, ps, err;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ThrowIfFailed(D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "main", "vs_5_0", flags, 0, &vs, &err));
    ThrowIfFailed(D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "main", "ps_5_0", flags, 0, &ps, &err));

    // Input layout
    D3D12_INPUT_ELEMENT_DESC il[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.InputLayout = { il, _countof(il) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    // RasterizerState (default-ish, line AA enable)
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    pso.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    pso.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.RasterizerState.MultisampleEnable = FALSE;
    pso.RasterizerState.AntialiasedLineEnable = TRUE;
    pso.RasterizerState.ForcedSampleCount = 0;
    pso.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // BlendState (default)
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

    // Depth: on
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    pso.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    pso.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    pso.DepthStencilState.BackFace = pso.DepthStencilState.FrontFace;

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
    const uint64_t totalSize = (uint64_t)m_cbStride * (uint64_t)MaxDrawsPerFrame * (uint64_t)FrameCount;

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

GPUMesh& D3D12Renderer::GetOrCreateGPUMesh(uint32_t meshId)
{
    auto it = m_gpuMeshes.find(meshId);
    if (it != m_gpuMeshes.end())
        return it->second;

    // 아직 GPU 메쉬 없음 → 생성
    assert(m_meshManager && "MeshManager not set");

    const MeshResource& cpu = m_meshManager->Get({ meshId });

    GPUMesh gpu{};
    CreateGPUMeshFromCPU(cpu, gpu);

    auto [iter, _] = m_gpuMeshes.emplace(meshId, std::move(gpu));
    return iter->second;
}

void D3D12Renderer::CreateGPUMeshFromCPU(const MeshResource& cpu, GPUMesh& out)
{
    const UINT vbSize =
        (UINT)(cpu.positions.size() * sizeof(DirectX::XMFLOAT3));
    const UINT ibSize =
        (UINT)(cpu.indices.size() * sizeof(uint16_t));

    out.indexCount = (uint32_t)cpu.indices.size();

    // Upload heap
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    // --- Vertex Buffer ---
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
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&out.vb)));

    void* p = nullptr;
    out.vb->Map(0, nullptr, &p);
    memcpy(p, cpu.positions.data(), vbSize);
    out.vb->Unmap(0, nullptr);

    out.vbView.BufferLocation = out.vb->GetGPUVirtualAddress();
    out.vbView.SizeInBytes = vbSize;
    out.vbView.StrideInBytes = sizeof(DirectX::XMFLOAT3);

    // --- Index Buffer ---
    D3D12_RESOURCE_DESC ibDesc = vbDesc;
    ibDesc.Width = ibSize;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&out.ib)));

    out.ib->Map(0, nullptr, &p);
    memcpy(p, cpu.indices.data(), ibSize);
    out.ib->Unmap(0, nullptr);

    out.ibView.BufferLocation = out.ib->GetGPUVirtualAddress();
    out.ibView.SizeInBytes = ibSize;
    out.ibView.Format = DXGI_FORMAT_R16_UINT;
}
