#include "platform/ImGuiDx11Glass.h"

#include <imgui.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace maku::platform {

// Must match imgui v1.91.8 ImGui_ImplDX11_Data layout (see backends/imgui_impl_dx11.cpp).
struct ImGui_ImplDX11_Data_Glass {
    ID3D11Device* pd3dDevice;
    ID3D11DeviceContext* pd3dDeviceContext;
    IDXGIFactory* pFactory;
    ID3D11Buffer* pVB;
    ID3D11Buffer* pIB;
    ID3D11VertexShader* pVertexShader;
    ID3D11InputLayout* pInputLayout;
    ID3D11Buffer* pVertexConstantBuffer;
    ID3D11PixelShader* pPixelShader;
    ID3D11SamplerState* pFontSampler;
    ID3D11ShaderResourceView* pFontTextureView;
    ID3D11RasterizerState* pRasterizerState;
    ID3D11BlendState* pBlendState;
    ID3D11DepthStencilState* pDepthStencilState;
    int VertexBufferSize;
    int IndexBufferSize;
};

static ImGui_ImplDX11_Data_Glass* GetBackend() {
    if (!ImGui::GetCurrentContext()) return nullptr;
    return reinterpret_cast<ImGui_ImplDX11_Data_Glass*>(ImGui::GetIO().BackendRendererUserData);
}

void PatchImGuiDx11ForDwmGlass(ID3D11Device* device) {
    if (!device) return;
    ImGui_ImplDX11_Data_Glass* bd = GetBackend();
    if (!bd || !bd->pd3dDevice) return;

    // Premultiplied output for DXGI_ALPHA_MODE_PREMULTIPLIED swap chains + DwmExtendFrameIntoClientArea.
    if (bd->pPixelShader) {
        bd->pPixelShader->Release();
        bd->pPixelShader = nullptr;
    }
    static const char* kPsPremul = R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
sampler sampler0;
Texture2D texture0;
float4 main(PS_INPUT input) : SV_Target {
    float4 c = input.col * texture0.Sample(sampler0, input.uv);
    c.rgb *= c.a;
    return c;
}
)";
    ID3DBlob* blob = nullptr;
    if (FAILED(D3DCompile(kPsPremul, strlen(kPsPremul), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0,
                          &blob, nullptr)))
        return;
    device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &bd->pPixelShader);
    blob->Release();

    if (bd->pBlendState) {
        bd->pBlendState->Release();
        bd->pBlendState = nullptr;
    }
    D3D11_BLEND_DESC desc{};
    desc.RenderTarget[0].BlendEnable = true;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&desc, &bd->pBlendState);
}

} // namespace maku::platform
