#include "platform/UpdateChecker.h"
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <thread>
#pragma comment(lib, "winhttp.lib")

namespace maku::platform {

void CheckForUpdateAsync(int currentBuild, std::function<void(UpdateInfo)> onDone) {
    std::thread([currentBuild, onDone = std::move(onDone)] {
        UpdateInfo info;
        HINTERNET session =
            WinHttpOpen(L"MakuTweaker++/5.6", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
        if (!session) {
            onDone(info);
            return;
        }
        HINTERNET connect = WinHttpConnect(
            session, L"raw.githubusercontent.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        HINTERNET request = nullptr;
        if (connect)
            request = WinHttpOpenRequest(
                connect, L"GET",
                L"/AdderlyMark/MakuTweaker/refs/heads/main/ver.json", nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (request && WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr)) {
            std::string body;
            DWORD size{};
            do {
                WinHttpQueryDataAvailable(request, &size);
                if (!size) break;
                std::string chunk(size, '\0');
                DWORD read{};
                WinHttpReadData(request, chunk.data(), size, &read);
                chunk.resize(read);
                body += chunk;
            } while (size > 0);
            try {
                auto j = nlohmann::json::parse(body);
                if (j.contains("build")) {
                    info.latestBuild = std::stoi(j["build"].get<std::string>());
                    info.available = info.latestBuild > currentBuild;
                }
            } catch (...) {
            }
        }
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        onDone(info);
    }).detach();
}

} // namespace maku::platform
