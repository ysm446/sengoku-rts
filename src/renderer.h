#pragma once
#include "camera.h"
#include "scene.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
#include <string>

class Renderer {
public:
    Renderer(HWND window, unsigned width, unsigned height, bool warp, const std::filesystem::path& shader);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void setScene(const Scene& scene);
    void resize(unsigned width, unsigned height);
    void render(const Camera& camera, const std::filesystem::path& capture = {});
    void checkDebugMessages();
    std::wstring adapterName() const { return adapterName_; }
    bool debugEnabled() const { return debugEnabled_; }

private:
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    void waitGpu();
    void createTargets();
    void createPipelines(const std::filesystem::path& shader);
    ComPtr<ID3D12Resource> buffer(UINT64 bytes, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state);
    void writeCapture(ID3D12Resource* readback, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint,
                      const std::filesystem::path& path);
    ComPtr<IDXGIFactory6> factory_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_, dsvHeap_, srvHeap_;
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> terrainPipeline_, spritePipeline_;
    std::array<ComPtr<ID3D12Resource>, 2> targets_;
    ComPtr<ID3D12Resource> depth_, terrain_, sprites_, atlas_;
    D3D12_VERTEX_BUFFER_VIEW terrainView_{}, spriteView_{};
    HANDLE fenceEvent_ = nullptr;
    UINT64 fenceValue_ = 0;
    unsigned width_, height_, rtvStride_ = 0, terrainCount_ = 0, spriteCount_ = 0;
    bool debugEnabled_ = false;
    std::wstring adapterName_;
};
