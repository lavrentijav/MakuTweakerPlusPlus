#include "platform/D3D11Context.h"
#include <dxgi.h>

namespace maku::platform {

bool D3D11Context::Init(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 1, D3D11_SDK_VERSION, &sd,
            swapChain_.GetAddressOf(), device_.GetAddressOf(), &level, context_.GetAddressOf())))
        return false;
    CreateRenderTarget();
    return true;
}

void D3D11Context::CreateRenderTarget() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf()));
    device_->CreateRenderTargetView(back.Get(), nullptr, rtv_.GetAddressOf());
}

void D3D11Context::Resize(UINT w, UINT h) {
    if (!swapChain_) return;
    rtv_.Reset();
    swapChain_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();
}

void D3D11Context::BeginFrame(const float clear[4]) {
    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(rtv_.Get(), clear);
}

void D3D11Context::EndFrame() {
    // Sync interval 0: do not block on vsync; keeps UI responsive when a frame is heavy.
    swapChain_->Present(0, 0);
}

void D3D11Context::Shutdown() {
    rtv_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
}

} // namespace maku::platform
