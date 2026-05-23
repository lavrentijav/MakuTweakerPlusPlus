#include "platform/WinAppSdkRuntime.h"

#ifdef MAKU_WINAPPSDK

#include "app/Branding.h"

#include <ShlObj.h>
#include <appmodel.h>
#include <DispatcherQueue.h>
#include <WindowsAppSDK-VersionInfo.h>

#include <winrt/Windows.System.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace maku::platform {
namespace {

#ifndef IDR_WASDK_BOOTSTRAP_DLL
#define IDR_WASDK_BOOTSTRAP_DLL 101
#endif

std::wstring LocalRuntimeDir() {
    wchar_t base[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, base)))
        return L"";
    std::wstring dir = base;
    dir += L"\\";
    dir += brand::kAppDataFolder;
    dir += L"\\wasdk";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

bool WriteFileIfChanged(const std::wstring& path, const void* data, DWORD size) {
    WIN32_FILE_ATTRIBUTE_DATA existing{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &existing)) {
        if (existing.nFileSizeLow == size && existing.nFileSizeHigh == 0) {
            HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                std::vector<std::uint8_t> buf(size);
                DWORD read = 0;
                const BOOL ok =
                    ReadFile(h, buf.data(), size, &read, nullptr) && read == size;
                CloseHandle(h);
                if (ok && memcmp(buf.data(), data, size) == 0) return true;
            }
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(static_cast<const char*>(data), size);
    return out.good();
}

bool ExtractResourceToFile(UINT id, const wchar_t* fileName, std::wstring& outPath) {
    const HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(mod, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return false;
    HGLOBAL loaded = LoadResource(mod, res);
    if (!loaded) return false;
    const void* data = LockResource(loaded);
    const DWORD size = SizeofResource(mod, res);
    if (!data || size == 0) return false;

    const std::wstring dir = LocalRuntimeDir();
    if (dir.empty()) return false;
    outPath = dir + L"\\" + fileName;
    return WriteFileIfChanged(outPath, data, size);
}

using MddBootstrapInitializeFn = HRESULT(WINAPI*)(UINT32, PCWSTR, PACKAGE_VERSION);
using MddBootstrapShutdownFn = HRESULT(WINAPI*)();

HMODULE g_bootstrapModule = nullptr;
MddBootstrapInitializeFn g_MddBootstrapInitialize = nullptr;
MddBootstrapShutdownFn g_MddBootstrapShutdown = nullptr;
bool g_bootstrapActive = false;

bool PrepareEmbeddedWinAppSdkRuntime() {
    std::wstring bootstrapPath;
    if (!ExtractResourceToFile(IDR_WASDK_BOOTSTRAP_DLL, L"Microsoft.WindowsAppRuntime.Bootstrap.dll",
                               bootstrapPath))
        return false;

    if (g_bootstrapModule) return g_MddBootstrapInitialize != nullptr;

    g_bootstrapModule = LoadLibraryW(bootstrapPath.c_str());
    if (!g_bootstrapModule) return false;

    g_MddBootstrapInitialize = reinterpret_cast<MddBootstrapInitializeFn>(
        GetProcAddress(g_bootstrapModule, "MddBootstrapInitialize"));
    g_MddBootstrapShutdown = reinterpret_cast<MddBootstrapShutdownFn>(
        GetProcAddress(g_bootstrapModule, "MddBootstrapShutdown"));
    return g_MddBootstrapInitialize != nullptr;
}

bool InitializeEmbeddedMddBootstrap() {
    if (!g_MddBootstrapInitialize) return false;
    const PACKAGE_VERSION minVersion{};
    const HRESULT hr = g_MddBootstrapInitialize(WINDOWSAPPSDK_RELEASE_MAJORMINOR,
                                                WINDOWSAPPSDK_RELEASE_VERSION_TAG_W, minVersion);
    g_bootstrapActive = SUCCEEDED(hr);
    return g_bootstrapActive;
}

void ShutdownEmbeddedMddBootstrap() {
    if (g_bootstrapActive && g_MddBootstrapShutdown) g_MddBootstrapShutdown();
    g_bootstrapActive = false;
}

struct Bootstrap {
    bool ok{false};
    Bootstrap() { ok = InitializeEmbeddedMddBootstrap(); }
    ~Bootstrap() { ShutdownEmbeddedMddBootstrap(); }
};

Bootstrap* g_bootstrap = nullptr;
winrt::Windows::System::DispatcherQueueController g_dispatcher{nullptr};
bool g_ready = false;

winrt::Windows::System::DispatcherQueueController CreateDispatcherQueue() {
    DispatcherQueueOptions options{sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT,
                                   DQTAT_COM_NONE};
    winrt::Windows::System::DispatcherQueueController controller{nullptr};
    winrt::check_hresult(
        CreateDispatcherQueueController(options, reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(
                                                    winrt::put_abi(controller))));
    return controller;
}

} // namespace

bool WinAppSdkRuntime::Initialize() {
    if (g_ready) return true;
    if (!PrepareEmbeddedWinAppSdkRuntime()) return false;
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        g_bootstrap = new Bootstrap();
        if (!g_bootstrap->ok) {
            delete g_bootstrap;
            g_bootstrap = nullptr;
            return false;
        }
        g_dispatcher = CreateDispatcherQueue();
        g_ready = true;
        return true;
    } catch (const winrt::hresult_error& e) {
        char buf[128]{};
        snprintf(buf, sizeof(buf), "MakuTweaker++ WinAppSDK init failed: 0x%08X\n",
                 static_cast<unsigned>(e.code().value));
        OutputDebugStringA(buf);
        return false;
    }
}

void WinAppSdkRuntime::Shutdown() {
    g_dispatcher = nullptr;
    delete g_bootstrap;
    g_bootstrap = nullptr;
    g_ready = false;
}

bool WinAppSdkRuntime::IsReady() { return g_ready; }

} // namespace maku::platform

#else

namespace maku::platform {

bool WinAppSdkRuntime::Initialize() { return false; }
void WinAppSdkRuntime::Shutdown() {}
bool WinAppSdkRuntime::IsReady() { return false; }

} // namespace maku::platform

#endif
