#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

class D3D12Renderer
{
public:
    void Init(HWND hwnd, UINT width, UINT height);
    void Render();
    void WaitForGpu();
    void Resize(UINT width, UINT height);
    void Destroy();

private:
    static constexpr UINT FrameCount = 2;

    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    UINT m_rtvDescriptorSize = 0;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    UINT m_frameIndex = 0;
    HWND m_hwnd = nullptr;
    UINT m_width = 0, m_height = 0;

    // --- Draw resources (cube) ---
    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT     m_scissorRect = {};

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

    // Geometry
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW  m_indexBufferView = {};

    // Constant buffer (WVP)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_cbvMappedData = nullptr;

    // (단순화를 위해) Root CBV로 바인딩: GPU handle heap 불필요
    // 즉 m_cbvSrvHeap, m_cbvGpuHandle도 안 씀

private:
    void CreateDeviceAndSwapChain();
    void CreateRtvHeapAndViews();
    void CreateCommands();
    void CreateSyncObjects();

    void LoadAssets();
};