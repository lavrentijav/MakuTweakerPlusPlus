#include "platform/CrashHandler.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>

namespace maku::platform {

static LONG WINAPI Filter(EXCEPTION_POINTERS*) {
    wchar_t desktop[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_DESKTOP, nullptr, 0, desktop);
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t file[MAX_PATH]{};
    swprintf_s(file, L"%s\\makutw-crash_%04d-%02d-%02d_%02d-%02d-%02d.txt", desktop,
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::wofstream out(file);
    out << L"MakuTweaker crash log\n";
    out.close();
    MessageBoxW(nullptr,
                L"Unfortunately, MakuTweaker has crashed.\nCrash log saved to Desktop.",
                L"MakuTweaker Crash", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

void InstallCrashHandler() { SetUnhandledExceptionFilter(Filter); }

} // namespace maku::platform
