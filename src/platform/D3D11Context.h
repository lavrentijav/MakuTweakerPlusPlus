#pragma once
#include <d3d11.h>
#include <wrl/client.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

namespace maku::platform {

class D3D11Context {
public:
    bool Init(HWND hwnd);
    void Resize(UINT w, UINT h);
    void BeginFrame(const float clear[4]);
    void EndFrame();
    void Shutdown();

    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return context_.Get(); }
    bool UsesAlphaSwapChain() const { return false; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    void CreateRenderTarget();
};

} // namespace maku::platform
