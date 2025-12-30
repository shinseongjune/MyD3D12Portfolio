#pragma once
#include <cstdint>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <unordered_map>
#include "IRenderer.h"

class MeshManager;
struct MeshCPUData;

struct MeshGPUData
{
    Microsoft::WRL::ComPtr<ID3D12Resource> vb;
    Microsoft::WRL::ComPtr<ID3D12Resource> ib;

    D3D12_VERTEX_BUFFER_VIEW vbView{};
    D3D12_INDEX_BUFFER_VIEW  ibView{};
    uint32_t indexCount = 0;
};

struct PendingMeshRelease
{
    uint32_t meshId = 0;
    uint64_t retireFenceValue = 0;
};

class D3D12Renderer final : public IRenderer
{
public:
    D3D12Renderer() = default;
    ~D3D12Renderer() override = default;

    void Initialize(HWND hwnd, uint32_t width, uint32_t height) override;
    void Resize(uint32_t width, uint32_t height) override;
    void Render(const std::vector<RenderItem>& items, const RenderCamera& cam) override;
    void Shutdown() override;

    void SetMeshManager(MeshManager* mm);

private:
    std::unordered_map<uint32_t, MeshGPUData> m_gpuMeshes;
    MeshManager* m_meshManager = nullptr;

    static constexpr uint32_t FrameCount = 2;
    static constexpr uint32_t MaxDrawsPerFrame = 2048; // RenderItem 최대치

    std::vector<PendingMeshRelease> m_pendingMeshReleases;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUpload;
    uint32_t m_srvDescriptorSize = 0;

    struct alignas(256) DrawCB
    {
        DirectX::XMFLOAT4X4 mvp;
        DirectX::XMFLOAT4   color;
    };

    struct DebugVertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
    };

private:
    void CreateDeviceAndSwapChain(HWND hwnd);
    void CreateCommandObjects();
    void CreateDescriptorHeaps();
    void CreateRenderTargets();
    void CreateDepthStencil(uint32_t width, uint32_t height);

    void CreatePipeline();
    void CreateConstantBuffer();

    void CreateDebugLinePipeline();
    void CreateDebugVertexBuffer();

    void WaitForGPU();
    void MoveToNextFrame();

    uint32_t Align256(uint32_t size) const { return (size + 255u) & ~255u; }

    MeshGPUData& GetOrCreateGPUMesh(uint32_t meshId);
    void CreateGPUMeshFromCPU(const MeshCPUData& cpu, MeshGPUData& out);

    void RetireMesh(uint32_t meshId);
    void ProcessPendingMeshReleases();

    void CreateTextureCheckerboard();

private:
    // Window
    HWND m_hwnd = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // DXGI / Device
    Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

    // Frame resources
    uint32_t m_frameIndex = 0;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // RTV/DSV
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    uint32_t m_rtvDescriptorSize = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;

    // Fence
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValues[FrameCount]{};
    HANDLE m_fenceEvent = nullptr;

    // Pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    // Debug Line Pipeline
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoDebugLine;

    // Debug Vertex Buffer (Upload, persist-mapped, frame-sliced)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_debugVB;
    uint8_t* m_debugVBMapped = nullptr;

    static constexpr uint32_t MaxDebugLinesPerFrame = 16384;
    static constexpr uint32_t MaxDebugVerticesPerFrame = MaxDebugLinesPerFrame * 2;

    uint32_t m_debugVBStride = 0; // sizeof(DebugVertex)

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};

    // Constant Buffer (Upload, persist-mapped)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cb;
    uint8_t* m_cbMapped = nullptr;
    uint32_t m_cbStride = 0; // 256 align된 DrawCB size
};
