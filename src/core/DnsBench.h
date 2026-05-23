#pragma once
#include <string>
#include <vector>

namespace maku::dns {

struct ServerResult {
    std::wstring name;
    std::wstring address;
    int pingMs = -1;
};

std::vector<ServerResult> BenchmarkServers();
bool ApplyDns(const std::wstring& address, const std::wstring& adapterName = L"");
void ApplyGamingTcpTweaks(bool enable);

} // namespace maku::dns
