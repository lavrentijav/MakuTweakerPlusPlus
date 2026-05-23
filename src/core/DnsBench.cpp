#include "core/DnsBench.h"
#include "core/OsUtil.h"
#include "core/ProcessRunner.h"
#include "core/Registry.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <algorithm>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace maku::dns {
namespace {

int PingMs(const wchar_t* ip) {
    IN_ADDR addr{};
    if (InetPtonW(AF_INET, ip, &addr) != 1) return -1;
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) return -1;
    char reply[sizeof(ICMP_ECHO_REPLY) + 32]{};
    DWORD sent = IcmpSendEcho(h, addr.S_un.S_addr, nullptr, 0, nullptr, reply, sizeof(reply),
                              1000);
    IcmpCloseHandle(h);
    if (sent == 0) return -1;
    const auto* rep = reinterpret_cast<ICMP_ECHO_REPLY*>(reply);
    return rep->Status == IP_SUCCESS ? static_cast<int>(rep->RoundTripTime) : -1;
}

std::wstring FirstUpAdapter() {
    ULONG len = 16 * 1024;
    std::vector<BYTE> buf(len);
    auto* a = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, a, &len) != NO_ERROR)
        return L"Ethernet";
    for (; a; a = a->Next) {
        if (a->OperStatus == IfOperStatusUp && a->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
            a->FriendlyName)
            return a->FriendlyName;
    }
    return L"Ethernet";
}

} // namespace

std::vector<ServerResult> BenchmarkServers() {
    static const struct {
        const wchar_t* name;
        const wchar_t* ip;
    } kServers[] = {
        {L"Cloudflare", L"1.1.1.1"},
        {L"Google", L"8.8.8.8"},
        {L"Quad9", L"9.9.9.9"},
        {L"AdGuard", L"94.140.14.14"},
    };
    std::vector<ServerResult> out;
    for (const auto& s : kServers) {
        ServerResult r;
        r.name = s.name;
        r.address = s.ip;
        r.pingMs = PingMs(s.ip);
        out.push_back(r);
    }
    std::sort(out.begin(), out.end(), [](const ServerResult& a, const ServerResult& b) {
        if (a.pingMs < 0) return false;
        if (b.pingMs < 0) return true;
        return a.pingMs < b.pingMs;
    });
    return out;
}

bool ApplyDns(const std::wstring& address, const std::wstring& adapterName) {
    const std::wstring iface = adapterName.empty() ? FirstUpAdapter() : adapterName;
    wchar_t cmd[512];
    swprintf_s(cmd, L"interface ip set dns \"%s\" static %s", iface.c_str(), address.c_str());
    if (!os::IsElevated()) return false;
    return proc::Run(L"netsh", cmd).exitCode == 0;
}

void ApplyGamingTcpTweaks(const bool enable) {
    const wchar_t* key = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters";
    if (enable) {
        reg::SetDword(HKEY_LOCAL_MACHINE, key, L"TcpAckFrequency", 1);
        reg::SetDword(HKEY_LOCAL_MACHINE, key, L"TCPNoDelay", 1);
        reg::SetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile",
                      L"NetworkThrottlingIndex", 0xffffffff);
    } else {
        reg::SetDword(HKEY_LOCAL_MACHINE, key, L"TcpAckFrequency", 2);
        reg::SetDword(HKEY_LOCAL_MACHINE, key, L"TCPNoDelay", 0);
        reg::SetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile",
                      L"NetworkThrottlingIndex", 10);
    }
}

} // namespace maku::dns
