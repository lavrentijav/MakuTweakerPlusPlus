#include "core/SystemReport.h"

#include "core/StringUtil.h"

#include <sstream>

namespace maku::report {
namespace {

std::wstring MapField(const std::wstring& value, const StatusMapper& map) {
    if (!map) return value;
    return map(value);
}

} // namespace

std::wstring BuildCompactSummaryFromSnapshot(const pci::Snapshot& s, StatusMapper mapStatus) {
    std::wstringstream out;
    out << s.osCaption << L" " << s.displayVersion << L" " << s.edition << L" (build " << s.build
        << L")\n";
    if (!s.computerName.empty())
        out << L"PC: " << s.computerName;
    if (!s.username.empty()) out << L"  User: " << s.username;
    if (!s.computerName.empty() || !s.username.empty()) out << L"\n";

    const std::wstring model =
        s.pcManufacturer.empty() ? s.pcModel : s.pcManufacturer + L" " + s.pcModel;
    if (!model.empty()) out << L"Model: " << model << L"\n";

    if (!s.cpuName.empty())
        out << L"CPU: " << s.cpuName << L" (" << s.cpuCores << L"c/" << s.cpuThreads << L"t, "
            << s.cpuArch << L")\n";

    out << L"RAM: " << pci::FormatBytes(s.ramTotalBytes);
    if (!s.ramType.empty()) out << L" " << s.ramType;
    if (s.ramSpeedMhz > 0) out << L" @ " << s.ramSpeedMhz << L" MHz";
    out << L"\n";

    for (size_t i = 0; i < s.gpus.size(); ++i) {
        out << L"GPU";
        if (s.gpus.size() > 1) out << L" " << (i + 1);
        out << L": " << s.gpus[i].name << L" (" << pci::FormatBytes(s.gpus[i].vramBytes) << L")\n";
    }

    if (!s.motherboard.empty()) out << L"MB: " << s.motherboard << L"\n";
    if (!s.biosVersion.empty())
        out << L"BIOS: " << s.biosVersion << L" (" << s.biosDate << L")\n";

    if (!s.disks.empty()) {
        out << L"Storage: " << s.disks.front().model << L" — "
            << pci::FormatBytes(s.disks.front().sizeBytes);
        if (!s.disks.front().interfaceType.empty())
            out << L" (" << s.disks.front().interfaceType << L")";
        out << L"\n";
    }

    if (!s.netAdapters.empty() && !s.netAdapters.front().ipv4.empty())
        out << L"IPv4: " << s.netAdapters.front().ipv4 << L"\n";

    out << L"Activation: " << MapField(s.activation, mapStatus) << L"\n";
    return out.str();
}

std::wstring BuildFullReportFromSnapshot(const pci::Snapshot& s, StatusMapper mapStatus) {
    std::wstringstream o;
    o << L"=== MakuTweaker++ System Report ===\n\n";
    o << L"[Windows]\nOS: " << s.osCaption << L"\nVersion: " << s.displayVersion
      << L"\nEdition: " << s.edition << L"\nBuild: " << s.build << L"\nArch: " << s.arch
      << L"\nInstalled: " << s.installDate << L"\nActivation: " << MapField(s.activation, mapStatus)
      << L"\nComputer: " << s.computerName << L"\nUser: " << s.username << L"\n";
    const std::wstring model =
        s.pcManufacturer.empty() ? s.pcModel : s.pcManufacturer + L" " + s.pcModel;
    if (!model.empty()) o << L"Model: " << model << L"\n";
    o << L"\n";

    o << L"[Regional]\nLanguage: " << s.uiLanguage << L"\nRegion: " << s.region
      << L"\nTZ: " << s.timezone << L"\n\n";

    o << L"[Security]\nWDAC: " << MapField(s.wdac, mapStatus) << L"\nVBS: "
      << MapField(s.vbs, mapStatus) << L"\nHyper-V: " << MapField(s.hyperv, mapStatus)
      << L"\nDefender: " << MapField(s.defender, mapStatus) << L"\nCore isolation: "
      << MapField(s.coreIsolation, mapStatus) << L"\nUAC: " << MapField(s.uac, mapStatus)
      << L"\nSmartScreen: " << MapField(s.smartScreen, mapStatus) << L"\nTPM: "
      << MapField(s.tpm, mapStatus) << L"\nSecure Boot: " << MapField(s.secureBoot, mapStatus)
      << L"\n\n";

    o << L"[CPU]\n" << s.cpuName << L"\nCores/Threads: " << s.cpuCores << L"/" << s.cpuThreads
      << L"\nArch: " << s.cpuArch << L"\nBase: " << s.cpuBaseGhz << L" GHz\nL3: " << s.cpuL3Mb
      << L" MB\n\n";

    o << L"[RAM]\nTotal: " << pci::FormatBytes(s.ramTotalBytes) << L" " << s.ramType;
    if (s.ramSpeedMhz > 0) o << L" @ " << s.ramSpeedMhz << L" MHz";
    o << L"\n";
    for (size_t i = 0; i < s.ramSticks.size(); ++i) {
        const auto& m = s.ramSticks[i];
        o << L"  Module " << (i + 1) << L": " << pci::FormatBytes(m.capacityBytes) << L", "
          << m.speedMhz << L" MHz, " << m.manufacturer;
        if (!m.partNumber.empty()) o << L" (" << m.partNumber << L")";
        o << L"\n";
    }
    o << L"\n[Motherboard]\n" << s.motherboard << L"\nBIOS: " << s.biosVersion << L" ("
      << s.biosDate << L")\n\n";

    o << L"[GPU]\n";
    for (const auto& g : s.gpus)
        o << L"  " << g.name << L" — " << pci::FormatBytes(g.vramBytes) << L", driver "
          << g.driverVersion << L"\n";
    o << L"\n[Storage]\n";
    for (const auto& d : s.disks)
        o << L"  " << d.model << L" — " << pci::FormatBytes(d.sizeBytes) << L" ("
          << d.mediaType << L", " << d.interfaceType << L")\n";
    for (const auto& v : s.volumes)
        o << L"  " << v.id << L" " << v.fileSystem << L" " << pci::FormatBytes(v.sizeBytes)
          << L", free " << pci::FormatBytes(v.freeBytes) << L"\n";

    o << L"\n[Network]\n";
    for (const auto& n : s.netAdapters)
        o << L"  " << n.description << L"\n    MAC " << n.mac << L"  IPv4 " << n.ipv4 << L"  IPv6 "
          << n.ipv6 << L"\n";

    if (s.hasBattery)
        o << L"\n[Battery]\n" << s.battery.chargePercent << L"% — "
          << MapField(s.battery.status, mapStatus) << L"\n";

    return o.str();
}

std::wstring BuildCompactSummary() {
    return BuildCompactSummaryFromSnapshot(pci::Collect());
}

std::wstring BuildFullReport() { return BuildFullReportFromSnapshot(pci::Collect()); }

std::string BuildFastfetchArt() {
    return R"(  __  __        _         _   _______        _                
 |  \/  | __ _| | ____ _| | |_   _| | _____| | _____ _ __ 
 | |\/| |/ _` | |/ / _` | |   | | | |/ / _ \| |/ / _ \ '__|
 | |  | | (_| |   < (_| | |   | | |   <  __/|   <  __/ |   
 |_|  |_|\__,_|_|\_\__,_|_|   |_| |_|\_\___||_|\_\___|_|   
)";
}

} // namespace maku::report
