#include "platform/D3D11Context.h"

#include <algorithm>
#include <cstdio>

#include <dxgi.h>

#ifndef DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER
#define DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER 0x00000010
#endif

namespace maku::platform {

namespace {

void DebugLog(const char* msg) { OutputDebugStringA(msg); }

void ClientSize(HWND hwnd, UINT& w, UINT& h) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    w = static_cast<UINT>(std::max<LONG>(rc.right - rc.left, 1));
    h = static_cast<UINT>(std::max<LONG>(rc.bottom - rc.top, 1));
}

} // namespace

const char* D3D11Context::GlassSwapModeName() const {
    switch (glassMode_) {
    case GlassSwapMode::FlipPremulForeground:
        return "FlipPremulForeground";
    case GlassSwapMode::FlipDwmAlpha:
        return "FlipDwmAlpha";
    case GlassSwapMode::LegacyDiscard:
        return "LegacyDiscard (opaque)";
    default:
        return "None";
    }
}

void D3D11Context::LogGlassMode() const {
    char buf[128]{};
    snprintf(buf, sizeof(buf), "MakuTweaker++ D3D11 glass mode: %s\n", GlassSwapModeName());
    DebugLog(buf);
}

bool D3D11Context::CreateDevice(D3D_DRIVER_TYPE driver) {
    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    return SUCCEEDED(D3D11CreateDevice(nullptr, driver, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                       levels, 1, D3D11_SDK_VERSION, device_.GetAddressOf(), &level,
                                       context_.GetAddressOf()));
}

bool D3D11Context::TryCreateFlipSwapChain(DXGI_ALPHA_MODE alpha, UINT flags) {
    if (!device_ || !hwnd_) return false;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device_.As(&dxgiDevice))) return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))) return false;

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())))) return false;

    ClientSize(hwnd_, bufferWidth_, bufferHeight_);

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = bufferWidth_;
    desc.Height = bufferHeight_;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.AlphaMode = alpha;
    desc.Flags = flags;

    swapChain_.Reset();
    const HRESULT hr = factory->CreateSwapChainForHwnd(device_.Get(), hwnd_, &desc, nullptr, nullptr,
                                                     swapChain_.GetAddressOf());
    if (FAILED(hr)) {
        char msg[96]{};
        snprintf(msg, sizeof(msg), "MakuTweaker++: CreateSwapChainForHwnd failed hr=0x%08lX\n",
                 static_cast<unsigned long>(hr));
        DebugLog(msg);
        return false;
    }

    factory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
    CreateRenderTarget();
    return true;
}

bool D3D11Context::InitGlassSwapChain() {
    glassMode_ = GlassSwapMode::None;

    if (!CreateDevice(D3D_DRIVER_TYPE_HARDWARE)) return false;

    // HWND + premultiplied alpha (requires FOREGROUND_LAYER). Primary glass path.
    if (TryCreateFlipSwapChain(DXGI_ALPHA_MODE_PREMULTIPLIED, DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER)) {
        glassMode_ = GlassSwapMode::FlipPremulForeground;
        LogGlassMode();
        return true;
    }

    device_.Reset();
    context_.Reset();
    if (!CreateDevice(D3D_DRIVER_TYPE_HARDWARE)) return false;

    if (TryCreateFlipSwapChain(DXGI_ALPHA_MODE_IGNORE, 0)) {
        glassMode_ = GlassSwapMode::FlipDwmAlpha;
        LogGlassMode();
        return true;
    }

    device_.Reset();
    context_.Reset();
    if (!CreateDevice(D3D_DRIVER_TYPE_WARP)) return false;

    if (TryCreateFlipSwapChain(DXGI_ALPHA_MODE_PREMULTIPLIED, DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER)) {
        glassMode_ = GlassSwapMode::FlipPremulForeground;
        LogGlassMode();
        return true;
    }

    device_.Reset();
    context_.Reset();
    return false;
}

bool D3D11Context::InitLegacySwapChain() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd_;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    Microsoft::WRL::ComPtr<IDXGISwapChain> legacy;
    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1,
            D3D11_SDK_VERSION, &sd, legacy.GetAddressOf(), device_.GetAddressOf(), &level,
            context_.GetAddressOf())))
        return false;

    if (FAILED(legacy.As(&swapChain_))) return false;

    glassMode_ = GlassSwapMode::LegacyDiscard;
    ClientSize(hwnd_, bufferWidth_, bufferHeight_);
    CreateRenderTarget();
    LogGlassMode();
    return true;
}

bool D3D11Context::Init(HWND hwnd) {
    hwnd_ = hwnd;
    device_.Reset();
    context_.Reset();
    swapChain_.Reset();
    rtv_.Reset();
    glassMode_ = GlassSwapMode::None;

    if (InitGlassSwapChain()) return true;

    return InitLegacySwapChain();
}

void D3D11Context::CreateRenderTarget() {
    rtv_.Reset();
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf()));
    device_->CreateRenderTargetView(back.Get(), nullptr, rtv_.GetAddressOf());
}

void D3D11Context::Resize(UINT w, UINT h) {
    if (!swapChain_) return;
    bufferWidth_ = std::max<UINT>(w, 1u);
    bufferHeight_ = std::max<UINT>(h, 1u);
    rtv_.Reset();
    swapChain_->ResizeBuffers(0, bufferWidth_, bufferHeight_, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();
}

void D3D11Context::BeginFrame(const float clear[4]) {
    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(rtv_.Get(), clear);
}

void D3D11Context::EndFrame() { swapChain_->Present(1, 0); }

void D3D11Context::Shutdown() {
    rtv_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
    hwnd_ = nullptr;
    glassMode_ = GlassSwapMode::None;
}

} // namespace maku::platform
