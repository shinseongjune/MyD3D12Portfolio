#pragma once
#include <cstdint>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

#include "IRenderer.h"

class D3D12Renderer final : public IRenderer
{
public:
    D3D12Renderer() = default;
    ~D3D12Renderer() override = default;

    void Initialize(HWND hwnd, uint32_t width, uint32_t height) override;
    void Resize(uint32_t width, uint32_t height) override;
    void Render(const std::vector<RenderItem>& items, const RenderCamera& cam) override;
    void Shutdown() override;

private:
    static constexpr uint32_t FrameCount = 2;
    static constexpr uint32_t MaxDrawsPerFrame = 2048; // RenderItem 최대치

    struct alignas(256) DrawCB
    {
        DirectX::XMFLOAT4X4 mvp;
        DirectX::XMFLOAT4   color;
    };

private:
    void CreateDeviceAndSwapChain(HWND hwnd);
    void CreateCommandObjects();
    void CreateDescriptorHeaps();
    void CreateRenderTargets();
    void CreateDepthStencil(uint32_t width, uint32_t height);

    void CreatePipeline();
    void CreateCubeGeometry();
    void CreateConstantBuffer();

    void WaitForGPU();
    void MoveToNextFrame();

    uint32_t Align256(uint32_t size) const { return (size + 255u) & ~255u; }

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

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};

    // Geometry (Cube)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vb;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_ib;
    D3D12_VERTEX_BUFFER_VIEW m_vbView{};
    D3D12_INDEX_BUFFER_VIEW  m_ibView{};
    uint32_t m_indexCount = 0;

    // Constant Buffer (Upload, persist-mapped)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cb;
    uint8_t* m_cbMapped = nullptr;
    uint32_t m_cbStride = 0; // 256 align된 DrawCB size
};
