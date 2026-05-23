#include "platform/MicaBackdrop.h"

#include "platform/WinAppSdkRuntime.h"

#include <cstdio>

#ifdef MAKU_WINAPPSDK

#include <windows.ui.composition.interop.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.h>

namespace maku::platform {

namespace winrt_sys = winrt::Microsoft::UI::Composition::SystemBackdrops;

struct MicaBackdrop::State {
    HWND hwnd{nullptr};
    winrt::Windows::UI::Composition::Compositor compositor{nullptr};
    winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target{nullptr};
    winrt_sys::MicaController controller{nullptr};
    bool supported{false};
    char status[96]{"WASDK:Mica idle"};
};

MicaBackdrop& MicaBackdrop::Instance() {
    static MicaBackdrop inst;
    return inst;
}

bool MicaBackdrop::Attach(HWND hwnd) {
    if (!hwnd || !WinAppSdkRuntime::IsReady()) return false;
    if (state_ && state_->hwnd == hwnd && state_->supported) return true;
    Detach();

    try {
        if (!state_) state_ = new State();
        state_->hwnd = hwnd;

        LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
        SetWindowLongW(hwnd, GWL_EXSTYLE, ex | WS_EX_COMPOSITED | WS_EX_NOREDIRECTIONBITMAP);

        state_->compositor = winrt::Windows::UI::Composition::Compositor();

        auto interop = state_->compositor.as<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop>();
        winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target{nullptr};
        winrt::check_hresult(interop->CreateDesktopWindowTarget(
            hwnd, true, reinterpret_cast<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget**>(
                            winrt::put_abi(target))));
        state_->target = target;
        state_->target.Root(state_->compositor.CreateContainerVisual());

        state_->controller = winrt_sys::MicaController();
        const winrt::Microsoft::UI::WindowId windowId{
            reinterpret_cast<uint64_t>(hwnd)};
        state_->supported = state_->controller.SetTarget(windowId, state_->target);

        snprintf(state_->status, sizeof(state_->status), "WASDK:Mica %s",
                 state_->supported ? "OK" : "unsupported");
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        return state_->supported;
    } catch (const winrt::hresult_error& e) {
        if (state_) snprintf(state_->status, sizeof(state_->status), "WASDK:Mica error 0x%08X",
                             static_cast<unsigned>(e.code().value));
        Detach();
        return false;
    }
}

void MicaBackdrop::Detach() {
    if (!state_) return;
    state_->controller = nullptr;
    state_->target = nullptr;
    state_->compositor = nullptr;
    state_->supported = false;
    state_->hwnd = nullptr;
    snprintf(state_->status, sizeof(state_->status), "WASDK:Mica detached");
}

bool MicaBackdrop::IsActive() const { return state_ && state_->supported; }

bool MicaBackdrop::IsSupported() const { return state_ && state_->supported; }

const char* MicaBackdrop::StatusText() const {
    static const char kOff[] = "WASDK:off";
    return state_ ? state_->status : kOff;
}

} // namespace maku::platform

#else

namespace maku::platform {

struct MicaBackdrop::State {
    char status[32]{"WASDK:not built"};
};

MicaBackdrop& MicaBackdrop::Instance() {
    static MicaBackdrop inst;
    return inst;
}

bool MicaBackdrop::Attach(HWND) { return false; }
void MicaBackdrop::Detach() {}
bool MicaBackdrop::IsActive() const { return false; }
bool MicaBackdrop::IsSupported() const { return false; }

const char* MicaBackdrop::StatusText() const {
    static const char k[] = "WASDK:not built";
    return k;
}

} // namespace maku::platform

#endif
