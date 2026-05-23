#pragma once

#include <d3d11.h>

#include <dxgi1_2.h>

#include <wrl/client.h>



namespace maku::platform {



enum class GlassSwapMode {
    None,
    LegacyDiscard,
    FlipPremulForeground,  // FLIP + PREMULTIPLIED + FOREGROUND_LAYER (DPG-style + DWM)
    FlipDwmAlpha,
};



class D3D11Context {

public:

    bool Init(HWND hwnd);

    void Resize(UINT w, UINT h);

    void BeginFrame(const float clear[4]);

    void EndFrame();

    void Shutdown();



    ID3D11Device* Device() const { return device_.Get(); }

    ID3D11DeviceContext* Context() const { return context_.Get(); }

    GlassSwapMode GetGlassSwapMode() const { return glassMode_; }

    const char* GlassSwapModeName() const;



    bool UsesAlphaSwapChain() const {
        return glassMode_ == GlassSwapMode::FlipDwmAlpha ||
               glassMode_ == GlassSwapMode::FlipPremulForeground;
    }

    bool UsesPremultipliedAlpha() const { return glassMode_ == GlassSwapMode::FlipPremulForeground; }



private:

    Microsoft::WRL::ComPtr<ID3D11Device> device_;

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;

    HWND hwnd_ = nullptr;

    GlassSwapMode glassMode_ = GlassSwapMode::None;

    UINT bufferWidth_ = 1;

    UINT bufferHeight_ = 1;



    void CreateRenderTarget();

    bool CreateDevice(D3D_DRIVER_TYPE driver);

    bool TryCreateFlipSwapChain(DXGI_ALPHA_MODE alpha, UINT flags);

    bool InitGlassSwapChain();

    bool InitLegacySwapChain();

    void LogGlassMode() const;

};



} // namespace maku::platform
