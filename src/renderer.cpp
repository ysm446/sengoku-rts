#include "renderer.h"
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
void check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << operation << " failed (HRESULT 0x" << std::hex << static_cast<unsigned>(result) << ")";
        throw std::runtime_error(message.str());
    }
}
D3D12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES result{};
    result.Type = type; result.CreationNodeMask = 1; result.VisibleNodeMask = 1;
    return result;
}
D3D12_RESOURCE_DESC textureDesc(unsigned width, unsigned height, DXGI_FORMAT format) {
    D3D12_RESOURCE_DESC result{};
    result.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    result.Width = width; result.Height = height; result.DepthOrArraySize = 1;
    result.MipLevels = 1; result.Format = format; result.SampleDesc.Count = 1;
    return result;
}
void transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
                D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
    list->ResourceBarrier(1, &barrier);
}
Microsoft::WRL::ComPtr<ID3DBlob> compile(const std::filesystem::path& path, const char* entry, const char* target) {
    Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
    const HRESULT result = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &code, &errors);
    if (FAILED(result) && errors) throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));
    check(result, "D3DCompileFromFile");
    return code;
}
}

Renderer::Renderer(HWND window, unsigned width, unsigned height, bool warp, const std::filesystem::path& shader)
    : width_(width), height_(height) {
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer(); debugEnabled_ = true;
    }
    check(CreateDXGIFactory2(debugEnabled_ ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&factory_)), "CreateDXGIFactory2");
    ComPtr<IDXGIAdapter1> adapter;
    if (warp) {
        check(factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "EnumWarpAdapter");
        check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)), "D3D12CreateDevice WARP");
    } else {
        for (UINT index = 0;; ++index) {
            const auto result = factory_->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
            if (result == DXGI_ERROR_NOT_FOUND) break;
            check(result, "EnumAdapterByGpuPreference");
            DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
                SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) break;
            adapter.Reset();
        }
        if (!device_) throw std::runtime_error("No DirectX 12 GPU found. Try --warp for software rendering.");
    }
    DXGI_ADAPTER_DESC1 adapterDesc{};
    check(adapter->GetDesc1(&adapterDesc), "GetDesc1"); adapterName_ = adapterDesc.Description;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");
    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.Width = width_; swapDesc.Height = height_; swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count = 1; swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2; swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swap;
    check(factory_->CreateSwapChainForHwnd(queue_.Get(), window, &swapDesc, nullptr, nullptr, &swap), "CreateSwapChainForHwnd");
    check(swap.As(&swapChain_), "Query swap chain");
    check(factory_->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "MakeWindowAssociation");
    auto descriptorHeap = [&](D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool visible, ComPtr<ID3D12DescriptorHeap>& out) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = type; desc.NumDescriptors = count;
        desc.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        check(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&out)), "CreateDescriptorHeap");
    };
    descriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false, rtvHeap_);
    descriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false, dsvHeap_);
    descriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, true, srvHeap_);
    rtvStride_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)), "CreateCommandAllocator");
    check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&list_)), "CreateCommandList");
    check(list_->Close(), "Close command list");
    check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence");
    createTargets();
    createPipelines(shader);
    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) throw std::runtime_error("CreateEvent failed");
}

Renderer::~Renderer() {
    try { if (fenceEvent_) waitGpu(); } catch (...) { /* 終了時に例外を送出しない。 */ }
    if (fenceEvent_) CloseHandle(fenceEvent_);
}

void Renderer::waitGpu() {
    check(queue_->Signal(fence_.Get(), ++fenceValue_), "Signal fence");
    if (fence_->GetCompletedValue() < fenceValue_) {
        check(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_), "SetEventOnCompletion");
        if (WaitForSingleObject(fenceEvent_, 30000) != WAIT_OBJECT_0) throw std::runtime_error("GPU fence wait timed out");
    }
    check(device_->GetDeviceRemovedReason(), "GPU device status");
}

void Renderer::createTargets() {
    auto handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 2; ++i) {
        check(swapChain_->GetBuffer(i, IID_PPV_ARGS(&targets_[i])), "Get swap buffer");
        device_->CreateRenderTargetView(targets_[i].Get(), nullptr, handle);
        handle.ptr += rtvStride_;
    }
    auto desc = textureDesc(width_, height_, DXGI_FORMAT_D32_FLOAT);
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    const auto heap = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clear{}; clear.Format = desc.Format; clear.DepthStencil.Depth = 1;
    check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clear, IID_PPV_ARGS(&depth_)), "Create depth buffer");
    device_->CreateDepthStencilView(depth_.Get(), nullptr, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}

void Renderer::resize(unsigned width, unsigned height) {
    if (!width || !height || (width == width_ && height == height_)) return;
    waitGpu();
    for (auto& target : targets_) target.Reset();
    depth_.Reset();
    check(swapChain_->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0), "ResizeBuffers");
    width_ = width; height_ = height; createTargets();
}

void Renderer::createPipelines(const std::filesystem::path& shader) {
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants = {0, 0, 24}; params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable = {1, &range}; params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS; sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{2, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    ComPtr<ID3DBlob> blob, errors;
    check(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors), "SerializeRootSignature");
    check(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_)), "CreateRootSignature");
    const auto tv = compile(shader, "terrainVS", "vs_5_1"), tp = compile(shader, "terrainPS", "ps_5_1");
    const auto sv = compile(shader, "spriteVS", "vs_5_1"), sp = compile(shader, "spritePS", "ps_5_1");
    const D3D12_INPUT_ELEMENT_DESC terrainLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    const D3D12_INPUT_ELEMENT_DESC spriteLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"TILE", 0, DXGI_FORMAT_R32_UINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"RIGHTAXIS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"UPAXIS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root_.Get();
    desc.VS = {tv->GetBufferPointer(), tv->GetBufferSize()}; desc.PS = {tp->GetBufferPointer(), tp->GetBufferSize()};
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = TRUE; desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.InputLayout = {terrainLayout, 2}; desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1; desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT; desc.SampleDesc.Count = 1;
    check(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&terrainPipeline_)), "Create terrain pipeline");
    desc.VS = {sv->GetBufferPointer(), sv->GetBufferSize()}; desc.PS = {sp->GetBufferPointer(), sp->GetBufferSize()};
    desc.InputLayout = {spriteLayout, 6};
    check(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&spritePipeline_)), "Create sprite pipeline");
}

Renderer::ComPtr<ID3D12Resource> Renderer::buffer(UINT64 bytes, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width = bytes; desc.Height = 1;
    desc.DepthOrArraySize = 1; desc.MipLevels = 1; desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    const auto heap = heapProperties(heapType);
    ComPtr<ID3D12Resource> resource;
    check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)), "Create buffer");
    return resource;
}

void Renderer::setScene(const Scene& scene) {
    waitGpu();
    auto upload = [&](const auto& data, auto& resource, auto& view) {
        const UINT bytes = static_cast<UINT>(data.size() * sizeof(data[0]));
        resource = buffer(bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped = nullptr; const D3D12_RANGE noRead{0, 0};
        check(resource->Map(0, &noRead, &mapped), "Map vertices");
        std::memcpy(mapped, data.data(), bytes); resource->Unmap(0, nullptr);
        view = {resource->GetGPUVirtualAddress(), bytes, static_cast<UINT>(sizeof(data[0]))};
    };
    upload(scene.terrain, terrain_, terrainView_); upload(scene.sprites, sprites_, spriteView_);
    terrainCount_ = static_cast<UINT>(scene.terrain.size()); spriteCount_ = static_cast<UINT>(scene.sprites.size());
    atlas_.Reset();
    const auto desc = textureDesc(Scene::atlasWidth, Scene::atlasHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
    const auto heap = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&atlas_)), "Create atlas");
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 bytes = 0;
    device_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
    auto staging = buffer(bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped = nullptr; const D3D12_RANGE noRead{0, 0};
    check(staging->Map(0, &noRead, &mapped), "Map atlas");
    for (UINT y = 0; y < Scene::atlasHeight; ++y)
        std::memcpy(static_cast<std::byte*>(mapped) + footprint.Offset + y * footprint.Footprint.RowPitch,
                    scene.atlas.data() + y * Scene::atlasWidth, Scene::atlasWidth * 4);
    staging->Unmap(0, nullptr);
    check(allocator_->Reset(), "Reset allocator"); check(list_->Reset(allocator_.Get(), nullptr), "Reset list");
    D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource = staging.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; source.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION dest{}; dest.pResource = atlas_.Get(); dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list_->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);
    transition(list_.Get(), atlas_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    check(list_->Close(), "Close upload list");
    ID3D12CommandList* commands[] = {list_.Get()}; queue_->ExecuteCommandLists(1, commands); waitGpu();
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = desc.Format; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(atlas_.Get(), &srv, srvHeap_->GetCPUDescriptorHandleForHeapStart());
}

void Renderer::updateSprites(const std::vector<SpriteInstance>& sprites) {
    if (sprites.size() != spriteCount_) throw std::runtime_error("Sprite count changed without setScene");
    // render()が毎回Fence完了まで待つため、前フレームとの書き込み競合はない。
    void* mapped = nullptr; const D3D12_RANGE noRead{0, 0};
    check(sprites_->Map(0, &noRead, &mapped), "Map dynamic sprites");
    std::memcpy(mapped, sprites.data(), sprites.size() * sizeof(SpriteInstance));
    sprites_->Unmap(0, nullptr);
}

void Renderer::render(const Camera& camera, const std::filesystem::path& capture) {
    check(allocator_->Reset(), "Reset allocator"); check(list_->Reset(allocator_.Get(), terrainPipeline_.Get()), "Reset render list");
    const UINT index = swapChain_->GetCurrentBackBufferIndex();
    auto* target = targets_[index].Get();
    transition(list_.Get(), target, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart(); rtv.ptr += index * rtvStride_;
    const auto dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    const float clear[] = {0.24f, 0.25f, 0.18f, 1};
    list_->ClearRenderTargetView(rtv, clear, 0, nullptr);
    list_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
    list_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(width_), static_cast<float>(height_), 0, 1};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
    list_->RSSetViewports(1, &viewport); list_->RSSetScissorRects(1, &scissor);
    list_->SetGraphicsRootSignature(root_.Get());
    DirectX::XMFLOAT4X4 matrix;
    DirectX::XMStoreFloat4x4(&matrix, camera.matrix(static_cast<float>(width_) / static_cast<float>(height_)));
    list_->SetGraphicsRoot32BitConstants(0, 16, &matrix, 0);
    const auto right = camera.right(), up = camera.up();
    const float basis[] = {right.x, right.y, right.z, 0, up.x, up.y, up.z, 0};
    list_->SetGraphicsRoot32BitConstants(0, 8, basis, 16);
    ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()}; list_->SetDescriptorHeaps(1, heaps);
    list_->SetGraphicsRootDescriptorTable(1, srvHeap_->GetGPUDescriptorHandleForHeapStart());
    list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list_->IASetVertexBuffers(0, 1, &terrainView_); list_->DrawInstanced(terrainCount_, 1, 0, 0);
    list_->SetPipelineState(spritePipeline_.Get());
    list_->IASetVertexBuffers(0, 1, &spriteView_); list_->DrawInstanced(6, spriteCount_, 0, 0);
    ComPtr<ID3D12Resource> readback; D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    if (!capture.empty()) {
        transition(list_.Get(), target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        const auto desc = target->GetDesc(); UINT64 bytes = 0;
        device_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        readback = buffer(bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource = target; source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION dest{}; dest.pResource = readback.Get();
        dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dest.PlacedFootprint = footprint;
        list_->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);
        transition(list_.Get(), target, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
    } else transition(list_.Get(), target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    check(list_->Close(), "Close render list");
    ID3D12CommandList* commands[] = {list_.Get()}; queue_->ExecuteCommandLists(1, commands);
    check(swapChain_->Present(1, 0), "Present");
    // 最小構成では毎フレーム完了を待つ。複数フレームの並列化は計測後に行う。
    waitGpu();
    if (readback) writeCapture(readback.Get(), footprint, capture);
}

void Renderer::writeCapture(ID3D12Resource* readback, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint,
                           const std::filesystem::path& path) {
    const UINT size = width_ * height_ * 4;
    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = 0x4d42; fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + size;
    BITMAPINFOHEADER info{}; info.biSize = sizeof(info); info.biWidth = static_cast<LONG>(width_);
    info.biHeight = -static_cast<LONG>(height_); info.biPlanes = 1; info.biBitCount = 32; info.biSizeImage = size;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open capture path");
    file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    file.write(reinterpret_cast<const char*>(&info), sizeof(info));
    void* mapped = nullptr;
    const D3D12_RANGE range{0, static_cast<SIZE_T>(readback->GetDesc().Width)};
    check(readback->Map(0, &range, &mapped), "Map capture");
    std::vector<std::uint8_t> row(width_ * 4);
    for (UINT y = 0; y < height_; ++y) {
        const auto* source = static_cast<const std::uint8_t*>(mapped) + footprint.Offset + y * footprint.Footprint.RowPitch;
        for (UINT x = 0; x < width_; ++x) {
            row[x * 4] = source[x * 4 + 2]; row[x * 4 + 1] = source[x * 4 + 1];
            row[x * 4 + 2] = source[x * 4]; row[x * 4 + 3] = 255;
        }
        file.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    const D3D12_RANGE noWrite{0, 0}; readback->Unmap(0, &noWrite);
    if (!file) throw std::runtime_error("Cannot write capture");
}

void Renderer::checkDebugMessages() {
    ComPtr<ID3D12InfoQueue> info;
    if (FAILED(device_.As(&info))) return;
    std::string failures;
    for (UINT64 i = 0; i < info->GetNumStoredMessages(); ++i) {
        SIZE_T size = 0; check(info->GetMessage(i, nullptr, &size), "Get debug message size");
        std::vector<std::byte> storage(size);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        check(info->GetMessage(i, message, &size), "Get debug message");
        if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
            failures += message->pDescription; failures += '\n';
        }
    }
    info->ClearStoredMessages();
    if (!failures.empty()) throw std::runtime_error(failures);
}
