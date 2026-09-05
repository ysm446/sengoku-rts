#include "sprite_sheet.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>

namespace {
void check(HRESULT result, const char* message) { if (FAILED(result)) throw std::runtime_error(message); }
struct ComScope {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComScope() { if (result != RPC_E_CHANGED_MODE) check(result, "Cannot initialize COM for sprite decoding"); }
    ~ComScope() { if (SUCCEEDED(result)) CoUninitialize(); }
};
}

std::vector<std::uint32_t> loadSpriteSheet(const std::filesystem::path& path, unsigned width, unsigned height) {
    ComScope scope;
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)), "Cannot create WIC factory");
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder), "Cannot decode soldier sprite sheet");
    ComPtr<IWICBitmapFrameDecode> frame;
    check(decoder->GetFrame(0, &frame), "Cannot read sprite frame");
    UINT actualWidth = 0, actualHeight = 0;
    check(frame->GetSize(&actualWidth, &actualHeight), "Cannot read sprite size");
    if (actualWidth != width || actualHeight != height)
        throw std::runtime_error("Sprite sheet must be " + std::to_string(width) + "x" + std::to_string(height));
    ComPtr<IWICFormatConverter> converter;
    check(factory->CreateFormatConverter(&converter), "Cannot create sprite converter");
    check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
        nullptr, 0, WICBitmapPaletteTypeCustom), "Cannot convert sprite to RGBA");
    std::vector<std::uint32_t> pixels(width * height);
    check(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size() * 4),
        reinterpret_cast<BYTE*>(pixels.data())), "Cannot copy sprite pixels");
    return pixels;
}
