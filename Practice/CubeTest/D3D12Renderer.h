#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <cstdint>
#include <vector>
#include <string>

class D3D12Renderer
{
public:
    void Init(HWND hwnd, UINT width, UINT height);
    void Render();
    void Resize(UINT width, UINT height);
    void Destroy();

private:
    static constexpr UINT FrameCount = 2;

    // Core
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

    // Depth
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;

    // Pipeline
    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT     m_scissorRect = {};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

public:
    // Mesh
    struct Vertex { float pos[3]; float uv[2]; };

    // === CPU-side mesh container ===
    struct CpuMesh
    {
        std::vector<Vertex>    vertices; // pos + uv
        std::vector<uint32_t>  indices;  // 32-bit
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW  m_indexBufferView = {};

    // ✅ Texture + SRV heap (최소)
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;     // shader-visible SRV heap
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;           // default heap texture
    Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUpload;     // upload heap for texture copy

    // Constants (per-frame ring)
    struct alignas(256) PerFrameCB
    {
        DirectX::XMFLOAT4X4 mvp; // row-major로 저장할 거라서 전치 안 함
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    uint8_t* m_cbMapped = nullptr;
    UINT m_cbStride = 0; // 256 aligned

    // “유니티처럼 만질 값”
    float m_cubeX = 0.0f;
    float m_cubeZ = 0.0f;

    UINT m_indexCount = 0;

private:
    // === GPU upload (Default heap) ===
    void CreateMeshFromCpu_DefaultHeap(const CpuMesh& mesh);

    void UploadBufferToDefault(
        const void* srcData, UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outDefault,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outUpload,
        D3D12_RESOURCE_STATES finalState);

    // === File I/O helpers ===
    static bool ReadTextFileUTF8(const wchar_t* path, std::string& outText);

    // === OBJ (CPU parse only) ===
    static bool LoadObjToCpuMesh(const wchar_t* path, CpuMesh& outMesh, std::string* outError = nullptr);

    void CreateDeviceAndSwapChain();
    void CreateRtvHeapAndViews();
    void CreateCommands();
    void CreateSyncObjects();

    void CreateDepthBuffer();

    void CreatePipeline();
    void CreateMesh();
    void CreateConstantBuffer();

    // === Demo resource bundle ===
    void CreateDemoResources();     // 파이프라인 + 메쉬 + 상수버퍼를 한 번에 준비
    void RecordAndSubmitFrame();    // 커맨드 기록/제출(+ present, fence 포함)

    void CreateTexture_Checkerboard(); // 임시 텍스쳐 생성+업로드+SRV까지

    // === Helper (중복 제거) ===
    void CreateUploadBufferAndCopy(const void* srcData, UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer);

    void BeginFrame();
    void DrawCube();
    void EndFrame();

    void UpdateInput();       // WASD
    void UpdateConstants();   // 매 프레임 MVP 갱신

    void WaitForGpu();
};
