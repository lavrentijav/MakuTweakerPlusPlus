#pragma once

#include <d3d11.h>

namespace maku::platform {

/// Patches ImGui DX11 backend for DWM glass: premultiplied pixel shader + blend (ONE / INV_SRC_ALPHA).
/// Call once after ImGui_ImplDX11_CreateDeviceObjects().
void PatchImGuiDx11ForDwmGlass(ID3D11Device* device);

} // namespace maku::platform
