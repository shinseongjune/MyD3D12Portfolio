#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d11on12.h>
#include <d3d11_4.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <string>
#include <DirectXMath.h>
#include "IRenderer.h"
#include "TextureCubeCpuData.h"

class MeshManager;
struct MeshCPUData;

class TextureManager;
struct TextureCpuData;
struct TextureHandle;

// ---------------------------
// GPU Mesh Data
// ---------------------------
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

// ---------------------------
// GPU Texture Data
// ---------------------------
struct TextureGPUData
{
    Microsoft::WRL::ComPtr<ID3D12Resource> tex;
    Microsoft::WRL::ComPtr<ID3D12Resource> upload; // fence 완료 후 release
    uint32_t srvIndex = 0;

    uint32_t width = 0;
    uint32_t height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    bool isCubemap = false;
};

struct PendingTextureRelease
{
    uint32_t texId = 0;              // TextureHandle.id
    uint64_t retireFenceValue = 0;
};

struct PendingTextureUploadRelease
{
    uint32_t texId = 0;              // TextureHandle.id
    uint64_t retireFenceValue = 0;
};

// ---------------------------
// Renderer
// ---------------------------
class D3D12Renderer final : public IRenderer
{
public:
    void Initialize(HWND hwnd, uint32_t width, uint32_t height) override;
    void Resize(uint32_t width, uint32_t height) override;
    void Render(const std::vector<RenderItem>& items, const RenderCamera& cam, TextureHandle skybox, const std::vector<UIDrawItem>& ui, const std::vector<UITextDraw>& text) override;
    void RenderUI(const std::vector<UIDrawItem>& ui) override;
    void Shutdown() override;

public:
    void SetMeshManager(MeshManager* mm);
    void SetTextureManager(TextureManager* tm);

    // slot 0에 예약된 기본 텍스처(SRV) 인덱스
    uint32_t GetDefaultSrvIndex() const { return 0; }

private:
    static void ThrowIfFailed(HRESULT hr);
    static uint32_t Align256(uint32_t size) { return (size + 255u) & ~255u; }

private:
    // ---- Core init ----
    void CreateDeviceAndSwapChain(HWND hwnd);
    void CreateCommandObjects();
    void CreateDescriptorHeaps();
    void CreateRenderTargets();
    void CreateDepthStencil(uint32_t width, uint32_t height);

    void CreatePipeline();
    void CreateDebugLinePipeline();
    void CreateUIPipeline();

    void CreateConstantBuffer();
    void CreateDebugVertexBuffer();
    void CreateUIVertexBuffer();

    // ---- DirectWrite text overlay ----
    void CreateTextOverlay();
    void RecreateTextOverlayTargets();
    void DrawTextOverlay(const std::vector<UITextDraw>& text);

    void WaitForGPU();
    void MoveToNextFrame();

private:
    // ---- Mesh GPU cache ----
    MeshGPUData& GetOrCreateGPUMesh(uint32_t meshId);
    void CreateGPUMeshFromCPU(const MeshCPUData& cpu, MeshGPUData& out);
    void CreateGPUCubeTextureFromCPU(const TextureCubeCpuData& cpu, TextureGPUData& out);

    void RetireMesh(uint32_t meshId);
    void ProcessPendingMeshReleases();

private:
    // ---- Texture GPU cache ----
    // (1) TextureHandle -> srvIndex (필요하면 업로드하고 생성)
    uint32_t GetOrCreateSrvIndex(const TextureHandle& h);

    // (2) CPU 텍스처 -> GPU 텍스처 + SRV 생성 (커맨드리스트에 copy/transition 기록)
    void CreateGPUTextureFromCPU(const TextureCpuData& cpu, TextureGPUData& out);

    // (3) TextureManager::Destroy 콜백을 통해 지연 해제
    void RetireTexture(uint32_t texId);
    void ProcessPendingTextureReleases();
    void ProcessPendingTextureUploadReleases();

    // 기본 텍스처(slot0) 생성
    void CreateDefaultTexture_Checkerboard();

	// ---- Skybox ----
    void CreateSkyboxPipeline();
    void CreateSkyboxMesh();

private:
    static constexpr uint32_t FrameCount = 2;

    HWND m_hwnd = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Managers (CPU side)
    MeshManager* m_meshManager = nullptr;
    TextureManager* m_textureManager = nullptr;

    // DXGI/D3D12 core
    Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];

    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    uint32_t m_frameIndex = 0;

    // Heaps
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    uint32_t m_rtvDescriptorSize = 0;
    uint32_t m_srvDescriptorSize = 0;

    // Render targets / depth
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;

    // Pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    // Debug line pipeline
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoDebugLine;

    // Fence
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValues[FrameCount] = {};
    HANDLE m_fenceEvent = nullptr;

    // Viewport / Scissor
    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};

    // ---------------------------
    // Constant Buffer (persist-mapped upload)
    // ---------------------------
    struct DrawCB
    {
        DirectX::XMFLOAT4X4 mvp;
        DirectX::XMFLOAT4   color;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_cb;
    uint8_t* m_cbMapped = nullptr;
    uint32_t  m_cbStride = 0;

    static constexpr uint32_t MaxDrawsPerFrame = 2048;

    // ---------------------------
    // Debug VB (persist-mapped upload)
    // ---------------------------
    struct DebugVertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_debugVB;
    uint8_t* m_debugVBMapped = nullptr;
    uint32_t  m_debugVBStride = 0;

    static constexpr uint32_t MaxDebugLinesPerFrame = 16384;
    static constexpr uint32_t MaxDebugVerticesPerFrame = MaxDebugLinesPerFrame * 2;

    // ---------------------------
    // GPU caches
    // ---------------------------
    std::unordered_map<uint32_t, MeshGPUData> m_gpuMeshes;
    std::vector<PendingMeshRelease> m_pendingMeshReleases;

    std::unordered_map<uint32_t, TextureGPUData> m_gpuTextures; // key = TextureHandle.id
    std::vector<PendingTextureRelease> m_pendingTextureReleases;
    std::vector<PendingTextureUploadRelease> m_pendingTextureUploadReleases;

    // SRV slot allocator (slot 0 is reserved for default texture)
    uint32_t m_nextSrvIndex = 0;

    // 런타임 텍스처 생성 시, "이번 프레임 커맨드에 의해 업로드된 텍스처" 목록
    std::vector<uint32_t> m_texturesCreatedThisFrame;

    // ---------------------------
    // UI VB (persist-mapped upload)
    // ---------------------------
    struct UIVertex
    {
        DirectX::XMFLOAT2 pos;   // NDC (-1~1)
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT4 color;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_uiVB;
    uint8_t* m_uiVBMapped = nullptr;
    uint32_t m_uiVBStride = 0;

    static constexpr uint32_t MaxUIQuadsPerFrame = 4096;
    static constexpr uint32_t MaxUIVertsPerFrame = MaxUIQuadsPerFrame * 6;

    // UI pipeline
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoUI;

    // ---------------------------
    // DirectWrite/Direct2D text overlay
    // ---------------------------
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11Context;
    Microsoft::WRL::ComPtr<ID3D11On12Device> m_d3d11On12;

    Microsoft::WRL::ComPtr<ID2D1Factory3> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device2> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext2> m_d2dContext;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_d2dBrush;

    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<IDWriteTextFormat>> m_textFormats;

    Microsoft::WRL::ComPtr<ID3D11Resource> m_wrappedBackBuffers[FrameCount];
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_d2dTargets[FrameCount];

	// ---------------------------
	// Skybox
	// ---------------------------
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_skyPso;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_skyVB;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_skyIB;
    D3D12_VERTEX_BUFFER_VIEW m_skyVBView{};
    D3D12_INDEX_BUFFER_VIEW  m_skyIBView{};
    uint32_t m_skyIndexCount = 0;

};
