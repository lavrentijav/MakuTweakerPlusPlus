#pragma once

#include <Windows.h>

namespace maku::platform {

/// Windows App SDK MicaController (official Win32 path).
class MicaBackdrop {
public:
    static MicaBackdrop& Instance();

    bool Attach(HWND hwnd);
    void Detach();
    bool IsActive() const;
    bool IsSupported() const;

    /// Status for Settings / About (e.g. "WASDK:Mica OK").
    const char* StatusText() const;

private:
    MicaBackdrop() = default;
    struct State;
    State* state_{nullptr};
};

} // namespace maku::platform
