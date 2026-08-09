#include "ui/Pages.h"
#include "ui/UiCommon.h"
#include "ui/Theme.h"
#include "app/Application.h"
#include "app/Branding.h"
#include "core/Benchmark.h"
#include "core/JobQueue.h"
#include "core/Localization.h"
#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include "core/SysInfoFormat.h"
#include "core/Wmi.h"
#include "core/Analytics.h"
#include <imgui.h>
#include <atomic>
#include <cmath>
#include <fstream>
#include <mutex>
#include <string>

namespace maku::ui::pages {
namespace {

using app::Application;

static std::string FormatScore(double score) {
    long long v = static_cast<long long>(std::llround(score));
    std::string s = std::to_string(v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), " ");
    return s;
}

struct PciRamStick {
    std::wstring manufacturer;
    std::wstring capacityGb;
    std::wstring partNumber;
    std::wstring speedMhz;
    std::wstring ddrType;
};

struct PciDiskRow {
    std::wstring model;
    std::wstring sizeGb;
};

struct PciCache {
    bool ready = false;
    bool loading = false;
    std::wstring cpuName;
    std::wstring cpuCoresThreads;
    std::wstring cpuFreqMhz;
    std::wstring cpuL3Cache;
    std::wstring ramTotalGb;
    std::wstring ramDdr;
    std::wstring ramFreq;
    std::wstring boardName;
    std::wstring biosVersion;
    std::wstring biosDate;
    std::wstring deviceManufacturer;
    std::wstring deviceModel;
    std::wstring tpmStatus;
    std::vector<std::wstring> gpuNames;
    std::vector<std::wstring> gpuVramGb;
    std::vector<PciRamStick> ramSticks;
    std::vector<PciDiskRow> disks;
};

static PciCache g_pci;
static std::mutex g_pciMutex;

static std::wstring PciRowValue(const std::vector<wmi::Row>& row, const wchar_t* key) {
    for (const auto& r : row)
        if (_wcsicmp(r.name.c_str(), key) == 0) return r.value;
    return {};
}

static std::wstring PciFormatGbFromBytes(const std::wstring& bytesStr) {
    if (bytesStr.empty()) return {};
    try {
        const long double gb =
            std::stoull(bytesStr) / (1024.0L * 1024.0L * 1024.0L);
        wchar_t buf[64]{};
        swprintf_s(buf, L"%.2f", static_cast<double>(gb));
        return buf;
    } catch (...) {
        return {};
    }
}

static void LoadPciCacheOnce() {
    {
        std::lock_guard lock(g_pciMutex);
        if (g_pci.ready || g_pci.loading) return;
        g_pci.loading = true;
    }

    jobs::JobQueue::Instance().Enqueue([] {
        PciCache loaded;
        loaded.cpuName = wmi::QueryScalar(L"SELECT Name FROM Win32_Processor", L"Name");
        const std::wstring cores =
            wmi::QueryScalar(L"SELECT NumberOfCores FROM Win32_Processor", L"NumberOfCores");
        const std::wstring threads = wmi::QueryScalar(
            L"SELECT NumberOfLogicalProcessors FROM Win32_Processor", L"NumberOfLogicalProcessors");
        if (!cores.empty() || !threads.empty())
            loaded.cpuCoresThreads = cores + L" / " + threads;
        loaded.cpuFreqMhz =
            wmi::QueryScalar(L"SELECT MaxClockSpeed FROM Win32_Processor", L"MaxClockSpeed");
        loaded.cpuL3Cache =
            wmi::QueryScalar(L"SELECT L3CacheSize FROM Win32_Processor", L"L3CacheSize");

        const std::wstring ramBytes = wmi::QueryScalar(
            L"SELECT TotalPhysicalMemory FROM Win32_ComputerSystem", L"TotalPhysicalMemory");
        loaded.ramTotalGb = PciFormatGbFromBytes(ramBytes);
        loaded.deviceManufacturer =
            wmi::QueryScalar(L"SELECT Manufacturer FROM Win32_ComputerSystem", L"Manufacturer");
        loaded.deviceModel =
            wmi::QueryScalar(L"SELECT Model FROM Win32_ComputerSystem", L"Model");
        loaded.boardName =
            wmi::QueryScalar(L"SELECT Product FROM Win32_BaseBoard", L"Product");
        loaded.biosVersion =
            wmi::QueryScalar(L"SELECT SMBIOSBIOSVersion FROM Win32_BIOS", L"SMBIOSBIOSVersion");
        const std::wstring biosDateRaw =
            wmi::QueryScalar(L"SELECT ReleaseDate FROM Win32_BIOS", L"ReleaseDate");
        loaded.biosDate = util::ToWide(sysfmt::FormatCimDate(util::ToUtf8(biosDateRaw)));

        const std::wstring tpm =
            wmi::QueryScalar(L"SELECT IsEnabled_InitialValue FROM Win32_Tpm",
                             L"IsEnabled_InitialValue");
        loaded.tpmStatus = tpm.empty() ? L"" : (tpm == L"TRUE" ? L"yes" : L"no");

        loaded.gpuNames = wmi::QueryList(L"SELECT Name FROM Win32_VideoController", L"Name");
        auto vramList =
            wmi::QueryList(L"SELECT AdapterRAM FROM Win32_VideoController", L"AdapterRAM");
        for (auto& v : vramList) {
            const std::wstring fmt = sysfmt::FormatAdapterVram(v);
            loaded.gpuVramGb.push_back(fmt.empty() ? L"" : fmt);
        }

        std::vector<std::vector<wmi::Row>> memRows;
        int speedSum = 0;
        int speedCount = 0;
        if (wmi::Query(L"SELECT Manufacturer,Capacity,PartNumber,Speed,SMBIOSMemoryType,"
                       L"MemoryType FROM Win32_PhysicalMemory",
                       memRows)) {
            for (auto& row : memRows) {
                PciRamStick stick;
                stick.manufacturer = PciRowValue(row, L"Manufacturer");
                stick.capacityGb = PciFormatGbFromBytes(PciRowValue(row, L"Capacity"));
                stick.partNumber = PciRowValue(row, L"PartNumber");
                stick.speedMhz = PciRowValue(row, L"Speed");
                int stickSpeed = 0;
                if (!stick.speedMhz.empty()) {
                    try {
                        stickSpeed = std::stoi(util::ToUtf8(stick.speedMhz));
                        speedSum += stickSpeed;
                        ++speedCount;
                    } catch (...) {
                    }
                }
                stick.ddrType = sysfmt::MemoryTypeFromWmi(PciRowValue(row, L"SMBIOSMemoryType"),
                                                          PciRowValue(row, L"MemoryType"),
                                                          stickSpeed);
                loaded.ramSticks.push_back(std::move(stick));
            }
        }
        const int avgSpeed = speedCount > 0 ? speedSum / speedCount : 0;
        if (!memRows.empty()) {
            loaded.ramDdr = sysfmt::MemoryTypeFromWmi(
                PciRowValue(memRows[0], L"SMBIOSMemoryType"), PciRowValue(memRows[0], L"MemoryType"),
                avgSpeed);
            if (!memRows[0].empty()) {
                const std::wstring spd = PciRowValue(memRows[0], L"Speed");
                if (!spd.empty()) loaded.ramFreq = spd;
            }
        }

        std::vector<std::vector<wmi::Row>> diskRows;
        if (wmi::Query(L"SELECT Model,Size FROM Win32_DiskDrive", diskRows)) {
            for (auto& row : diskRows) {
                PciDiskRow disk;
                disk.model = PciRowValue(row, L"Model");
                disk.sizeGb = PciFormatGbFromBytes(PciRowValue(row, L"Size"));
                loaded.disks.push_back(std::move(disk));
            }
        }

        loaded.ready = true;
        loaded.loading = false;
        std::lock_guard lock(g_pciMutex);
        g_pci = std::move(loaded);
    });
}

static void RequestPciCacheReload() {
    {
        std::lock_guard lock(g_pciMutex);
        g_pci = {};
    }
    LoadPciCacheOnce();
}

static size_t PrimaryGpuIndex(const PciCache& s) {
    if (s.gpuNames.empty()) return 0;
    size_t best = 0;
    double bestVram = -1.0;
    for (size_t i = 0; i < s.gpuNames.size(); ++i) {
        double vram = 0.0;
        if (i < s.gpuVramGb.size() && !s.gpuVramGb[i].empty()) {
            try {
                vram = std::stod(util::ToUtf8(s.gpuVramGb[i]));
            } catch (...) {
            }
        }
        if (vram > bestVram) {
            bestVram = vram;
            best = i;
        }
    }
    return best;
}

static std::string RamTileValue(const PciCache& s) {
    std::string out;
    if (!s.ramDdr.empty() && s.ramDdr != L"N/A") out = util::ToUtf8(s.ramDdr);
    if (!s.ramTotalGb.empty()) {
        if (!out.empty()) out += ' ';
        out += util::ToUtf8(s.ramTotalGb) + " GB";
    }
    if (out.empty()) out = "—";
    return out;
}

static std::string StorageTileValue(const PciCache& s) {
    if (s.disks.empty()) return "—";
    if (s.disks.size() == 1 && !s.disks[0].model.empty())
        return util::ToUtf8(s.disks[0].model);
    double sum = 0.0;
    int count = 0;
    for (const auto& d : s.disks) {
        if (d.sizeGb.empty()) continue;
        try {
            sum += std::stod(util::ToUtf8(d.sizeGb));
            ++count;
        } catch (...) {
        }
    }
    if (count > 0) {
        char buf[64]{};
        snprintf(buf, sizeof(buf), "%.0f GB (%d)", sum, count);
        return buf;
    }
    return util::ToUtf8(s.disks[0].model);
}

static std::wstring BuildFullReport(const PciCache& s) {
    std::wstring out;
    out += L"CPU: " + s.cpuName + L"\n";
    if (!s.cpuCoresThreads.empty()) out += L"Cores/Threads: " + s.cpuCoresThreads + L"\n";
    if (!s.cpuFreqMhz.empty()) out += L"Frequency: " + s.cpuFreqMhz + L" MHz\n";
    if (!s.cpuL3Cache.empty()) out += L"L3: " + s.cpuL3Cache + L" KB\n";
    out += L"\nRAM: " + s.ramTotalGb + L" GB\n";
    if (!s.ramDdr.empty()) out += L"DDR: " + s.ramDdr + L"\n";
    if (!s.ramFreq.empty()) out += L"RAM MHz: " + s.ramFreq + L"\n";
    for (size_t i = 0; i < s.ramSticks.size(); ++i) {
        out += L"  Stick " + std::to_wstring(i + 1) + L": " + s.ramSticks[i].manufacturer + L" " +
               s.ramSticks[i].capacityGb + L" GB\n";
    }
    out += L"\nBoard: " + s.boardName + L"\n";
    if (!s.biosVersion.empty()) out += L"BIOS: " + s.biosVersion + L"\n";
    if (!s.biosDate.empty()) out += L"BIOS date: " + s.biosDate + L"\n";
    for (size_t i = 0; i < s.gpuNames.size(); ++i) {
        out += L"\nGPU: " + s.gpuNames[i];
        if (i < s.gpuVramGb.size() && !s.gpuVramGb[i].empty()) out += L" (" + s.gpuVramGb[i] + L")";
        out += L"\n";
    }
    for (const auto& d : s.disks) {
        out += L"\nDisk: " + d.model + L" " + d.sizeGb + L" GB\n";
    }
    if (!s.deviceModel.empty()) out += L"\nDevice: " + s.deviceModel + L"\n";
    return out;
}

static std::wstring BuildShortReport(const PciCache& s) {
    std::wstring line;
    line = s.cpuName;
    if (!s.cpuCoresThreads.empty()) line += L" (" + s.cpuCoresThreads + L")";

    std::wstring ram = s.ramTotalGb;
    if (!ram.empty()) ram += L" GB";
    if (!s.ramDdr.empty() && s.ramDdr != L"N/A") ram = s.ramDdr + L" " + ram;

    const size_t gi = PrimaryGpuIndex(s);
    std::wstring gpu = gi < s.gpuNames.size() ? s.gpuNames[gi] : L"";
    if (gi < s.gpuVramGb.size() && !s.gpuVramGb[gi].empty()) gpu += L" (" + s.gpuVramGb[gi] + L")";

    std::wstring device = s.deviceModel;
    if (!s.boardName.empty()) {
        if (!device.empty()) device += L" / ";
        device += s.boardName;
    }

    return line + L"\n" + ram + L"\n" + gpu + L"\n" + device;
}

static void SavePciReport(const PciCache& snap, const l10n::Localization& l, HWND hwnd) {
    const std::wstring path = util::GetExeDirectory() + L"\\pci_export.txt";
    std::wofstream out(path);
    out << BuildFullReport(snap);
    MessageBoxW(hwnd, util::ToWide(l.Get("pci", "main", "save_done")).c_str(), brand::kDisplayName,
                MB_OK);
}

} // namespace

void DrawPci() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    const bool dark = IsDarkTheme(app.GetSettings().theme);

    auto saveReport = [&]() {
        PciCache snap;
        {
            std::lock_guard lock(g_pciMutex);
            snap = g_pci;
        }
        if (snap.ready) SavePciReport(snap, l, app.Hwnd());
    };
    auto copyReport = [&]() {
        PciCache snap;
        {
            std::lock_guard lock(g_pciMutex);
            snap = g_pci;
        }
        if (snap.ready) SetClipboardText(app.Hwnd(), BuildShortReport(snap));
    };

    PageTitleWithActions(l.Get("pci", "main", "label"), l.Get("pci", "main", "save_tooltip").c_str(),
                         l.Get("pci", "main", "copy_tooltip").c_str(), saveReport, copyReport);

    LoadPciCacheOnce();

    PciCache snap;
    {
        std::lock_guard lock(g_pciMutex);
        snap = g_pci;
    }

    if (!snap.ready) {
        ImGui::TextWrapped("%s", l.Get("pci", "main", "loading").c_str());
        if (ImGui::Button(l.Def("apply").c_str())) RequestPciCacheReload();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F5) ||
        (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)))
        saveReport();

    const size_t primaryGpu = PrimaryGpuIndex(snap);
    static int gpuIdx = 0;
    static int diskIdx = 0;
    if (gpuIdx >= static_cast<int>(snap.gpuNames.size())) gpuIdx = 0;
    if (diskIdx >= static_cast<int>(snap.disks.size())) diskIdx = 0;

    const std::string cpuTile = snap.cpuName.empty() ? "—" : util::ToUtf8(snap.cpuName);
    const std::string gpuTile =
        snap.gpuNames.empty() ? "—" : util::ToUtf8(snap.gpuNames[primaryGpu]);
    std::string vramTile = "—";
    if (primaryGpu < snap.gpuVramGb.size() && !snap.gpuVramGb[primaryGpu].empty())
        vramTile = util::ToUtf8(snap.gpuVramGb[primaryGpu]);
    const std::string manuTile =
        snap.deviceManufacturer.empty() ? "—" : util::ToUtf8(snap.deviceManufacturer);
    const std::string modelTile =
        snap.deviceModel.empty() ? "—" : util::ToUtf8(snap.deviceModel);

    const float gap = 8.f * UiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(gap * 0.5f, gap * 0.5f));
    if (ImGui::BeginTable("pci_tiles", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawMetricCard("pci_cpu", l.Get("pci", "main", "processorlabel").c_str(), cpuTile.c_str(),
                       MetricCardIcon::Cpu, dark);
        ImGui::TableSetColumnIndex(1);
        DrawMetricCard("pci_ram", l.Get("pci", "main", "ram_short").c_str(),
                       RamTileValue(snap).c_str(), MetricCardIcon::Ram, dark);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawMetricCard("pci_gpu", l.Get("pci", "main", "vlabel").c_str(), gpuTile.c_str(),
                       MetricCardIcon::Gpu, dark);
        ImGui::TableSetColumnIndex(1);
        DrawMetricCard("pci_vram", l.Get("pci", "main", "vmem").c_str(), vramTile.c_str(),
                       MetricCardIcon::Storage, dark);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawMetricCard("pci_manu", l.Get("pci", "main", "devicemanu").c_str(), manuTile.c_str(),
                       MetricCardIcon::Device, dark);
        ImGui::TableSetColumnIndex(1);
        DrawMetricCard("pci_model", l.Get("pci", "main", "modeln").c_str(), modelTile.c_str(),
                       MetricCardIcon::Device, dark);

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.f, 4.f * UiScale()));

    if (BeginCollapsibleSection("pci_cpu_sec", l.Get("pci", "main", "processorlabel").c_str(), dark,
                                true, MetricCardIcon::Cpu)) {
        DrawSettingsCard("cpu_name", l.Get("pci", "main", "processorname").c_str(),
                         util::ToUtf8(snap.cpuName).c_str(), MetricCardIcon::Generic, dark);
        if (!snap.cpuCoresThreads.empty())
            DrawSettingsCard("cpu_ct", l.Get("pci", "main", "processorcores").c_str(),
                             util::ToUtf8(snap.cpuCoresThreads).c_str(), MetricCardIcon::Cpu, dark);
        if (!snap.cpuFreqMhz.empty()) {
            const std::string freq = util::ToUtf8(snap.cpuFreqMhz) + " MHz";
            DrawSettingsCard("cpu_freq", l.Get("pci", "main", "processorfreq").c_str(), freq.c_str(),
                             MetricCardIcon::Generic, dark);
        }
        if (!snap.cpuL3Cache.empty()) {
            const std::string cache = util::ToUtf8(snap.cpuL3Cache) + " KB";
            DrawSettingsCard("cpu_l3", l.Get("pci", "main", "processorcache").c_str(), cache.c_str(),
                             MetricCardIcon::Storage, dark);
        }
        EndCollapsibleSection();
    }

    if (!snap.gpuNames.empty() &&
        BeginCollapsibleSection("pci_gpu_sec", l.Get("pci", "main", "vlabel").c_str(), dark, true,
                                MetricCardIcon::Gpu)) {
        DrawSettingsCard("gpu_name", l.Get("pci", "main", "vname").c_str(),
                         util::ToUtf8(snap.gpuNames[static_cast<size_t>(gpuIdx)]).c_str(),
                         MetricCardIcon::Generic, dark);
        if (static_cast<size_t>(gpuIdx) < snap.gpuVramGb.size() &&
            !snap.gpuVramGb[static_cast<size_t>(gpuIdx)].empty())
            DrawSettingsCard("gpu_vram", l.Get("pci", "main", "vmem").c_str(),
                             util::ToUtf8(snap.gpuVramGb[static_cast<size_t>(gpuIdx)]).c_str(),
                             MetricCardIcon::Storage, dark);
        if (snap.gpuNames.size() > 1) {
            static std::vector<std::string> gpuLabels;
            static std::vector<const char*> gpuItems;
            gpuLabels.clear();
            gpuItems.clear();
            for (size_t i = 0; i < snap.gpuNames.size(); ++i) {
                gpuLabels.push_back(std::to_string(i + 1) + ". " + util::ToUtf8(snap.gpuNames[i]));
                gpuItems.push_back(gpuLabels.back().c_str());
            }
            if (BeginSettingsCard("gpu_pick_card", dark, 44.f * UiScale())) {
                ImGui::SetNextItemWidth(-1.f);
                ImGui::Combo("##gpu_pick", &gpuIdx, gpuItems.data(), static_cast<int>(gpuItems.size()));
            }
            EndSettingsCard();
        }
        EndCollapsibleSection();
    }

    if (!snap.disks.empty() &&
        BeginCollapsibleSection("pci_disk_sec", l.Get("pci", "main", "ssdl").c_str(), dark, true,
                                MetricCardIcon::Storage)) {
        const auto& disk = snap.disks[static_cast<size_t>(diskIdx)];
        if (!disk.model.empty())
            DrawSettingsCard("disk_name", l.Get("pci", "main", "sname").c_str(),
                             util::ToUtf8(disk.model).c_str(), MetricCardIcon::Generic, dark);
        if (!disk.sizeGb.empty()) {
            const std::string mem = util::ToUtf8(disk.sizeGb) + " GB";
            DrawSettingsCard("disk_cap", l.Get("pci", "main", "smem").c_str(), mem.c_str(),
                             MetricCardIcon::Storage, dark);
        }
        if (snap.disks.size() > 1) {
            static std::vector<std::string> diskLabels;
            static std::vector<const char*> diskItems;
            diskLabels.clear();
            diskItems.clear();
            for (size_t i = 0; i < snap.disks.size(); ++i) {
                diskLabels.push_back(std::to_string(i + 1) + ". " + util::ToUtf8(snap.disks[i].model));
                diskItems.push_back(diskLabels.back().c_str());
            }
            if (BeginSettingsCard("disk_pick_card", dark, 44.f * UiScale())) {
                ImGui::SetNextItemWidth(-1.f);
                ImGui::Combo("##disk_pick", &diskIdx, diskItems.data(), static_cast<int>(diskItems.size()));
            }
            EndSettingsCard();
        }
        EndCollapsibleSection();
    }

    static int ramStickIdx = 0;
    if (!snap.ramSticks.empty() &&
        BeginCollapsibleSection("pci_ram_stick_sec", l.Get("pci", "main", "ramsticktitle").c_str(),
                                dark, false, MetricCardIcon::Ram)) {
        if (ramStickIdx >= static_cast<int>(snap.ramSticks.size())) ramStickIdx = 0;
        const auto& stick = snap.ramSticks[static_cast<size_t>(ramStickIdx)];
        if (!stick.manufacturer.empty())
            DrawSettingsCard("ram_manu", l.Get("pci", "main", "manu").c_str(),
                             util::ToUtf8(stick.manufacturer).c_str(), MetricCardIcon::Device, dark);
        if (!stick.partNumber.empty())
            DrawSettingsCard("ram_part", l.Get("pci", "main", "partnum").c_str(),
                             util::ToUtf8(stick.partNumber).c_str(), MetricCardIcon::Generic, dark);
        if (!stick.capacityGb.empty()) {
            const std::string cap = util::ToUtf8(stick.capacityGb) + " GB";
            DrawSettingsCard("ram_cap", l.Get("pci", "main", "capac").c_str(), cap.c_str(),
                             MetricCardIcon::Storage, dark);
        }
        if (snap.ramSticks.size() > 1) {
            static std::vector<std::string> ramLabels;
            static std::vector<const char*> ramItems;
            ramLabels.clear();
            ramItems.clear();
            for (size_t i = 0; i < snap.ramSticks.size(); ++i) {
                const auto& s = snap.ramSticks[i];
                ramLabels.push_back(std::to_string(i + 1) + ". " + util::ToUtf8(s.capacityGb) +
                                    " GB — " + util::ToUtf8(s.manufacturer));
                ramItems.push_back(ramLabels.back().c_str());
            }
            if (BeginSettingsCard("ram_pick_card", dark, 44.f * UiScale())) {
                ImGui::SetNextItemWidth(-1.f);
                ImGui::Combo("##ram_pick", &ramStickIdx, ramItems.data(),
                             static_cast<int>(ramItems.size()));
            }
            EndSettingsCard();
        }
        EndCollapsibleSection();
    }

    if (BeginCollapsibleSection("pci_mb_sec", l.Get("pci", "main", "mblabel").c_str(), dark, false,
                                MetricCardIcon::Board)) {
        if (!snap.boardName.empty())
            DrawSettingsCard("mb_name", l.Get("pci", "main", "mbname").c_str(),
                             util::ToUtf8(snap.boardName).c_str(), MetricCardIcon::Generic, dark);
        if (!snap.biosVersion.empty())
            DrawSettingsCard("mb_bios", l.Get("pci", "main", "mbver").c_str(),
                             util::ToUtf8(snap.biosVersion).c_str(), MetricCardIcon::Generic, dark);
        if (!snap.biosDate.empty())
            DrawSettingsCard("mb_date", l.Get("pci", "main", "mbdate").c_str(),
                             util::ToUtf8(snap.biosDate).c_str(), MetricCardIcon::Generic, dark);
        if (!snap.tpmStatus.empty()) {
            const bool tpmOn = snap.tpmStatus == L"yes";
            DrawSettingsCard(
                "mb_tpm", l.Get("pci", "main", "tpmtitle").c_str(),
                (tpmOn ? l.Get("pci", "main", "tpmy") : l.Get("pci", "main", "tpmn")).c_str(),
                MetricCardIcon::Generic, dark);
        }
        EndCollapsibleSection();
    }

    {
        const float scale = UiScale();
        ImGui::Dummy(ImVec2(0.f, 18.f * scale));
        ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());
        ImGui::TextUnformatted(l.Get("pci", "main", "benchtitle").c_str());
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.f, 6.f * scale));
        static std::atomic<bool> benchRunning{false};
        static std::string benchResult;
        static std::mutex benchMutex;
        // Last finished run, kept so the opt-in "share" button below has
        // something concrete to send when analytics are switched off.
        static std::string lastCpu, lastScoreType, lastScore;
        static std::string shareStatus;
        if (benchResult.empty()) benchResult = l.Get("pci", "main", "benchtip");

        const std::string cpuNameUtf8 = util::ToUtf8(snap.cpuName);
        auto startBench = [&](bool multi) {
            if (benchRunning.exchange(true)) return;
            const std::string running =
                multi ? l.Get("pci", "main", "running_multicore") : l.Get("pci", "main", "running");
            const std::string done =
                multi ? l.Get("pci", "main", "test1multi") : l.Get("pci", "main", "test1");
            const std::string scoreLbl = l.Get("pci", "main", "test2");
            const std::string opsLbl = l.Get("pci", "main", "test3");
            const std::string scoreType = multi ? "multi" : "single";
            {
                std::lock_guard lock(benchMutex);
                benchResult = running;
            }
            jobs::JobQueue::Instance().Enqueue(
                [multi, done, scoreLbl, opsLbl, cpuNameUtf8, scoreType]() {
                    try {
                        const auto r = bench::Run(multi);
                        const std::string scoreStr = FormatScore(r.score);
                        const std::string line =
                            done + " " + scoreLbl + " " + scoreStr + " " + opsLbl;
                        analytics::TrackBenchmark(cpuNameUtf8, scoreType, scoreStr);
                        std::lock_guard lock(benchMutex);
                        benchResult = line;
                        lastCpu = cpuNameUtf8;
                        lastScoreType = scoreType;
                        lastScore = scoreStr;
                        shareStatus.clear();
                    } catch (...) {
                        std::lock_guard lock(benchMutex);
                        benchResult = done + " (error)";
                    }
                    benchRunning = false;
                });
        };

        PushCompactToolbarStyle();
        const bool busy = benchRunning.load();
        if (busy) ImGui::BeginDisabled();
        if (ImGui::Button(l.Get("pci", "main", "benchbutton").c_str())) startBench(false);
        ImGui::SameLine();
        if (ImGui::Button(l.Get("pci", "main", "benchbutton2").c_str())) startBench(true);
        ImGui::SameLine();
        if (ImGui::Button(l.Get("pci", "main", "lookresulbutton").c_str()))
            proc::OpenUrl(L"https://adderly.top/makubench");
        if (busy) ImGui::EndDisabled();
        PopCompactToolbarStyle();

        {
            std::lock_guard lock(benchMutex);
            ImGui::TextWrapped("%s", benchResult.c_str());
        }

        // When analytics are off, results are never sent automatically. Offer a
        // one-shot share instead so declining the prompt does not mean the
        // author's benchmark database loses this machine entirely.
        if (!analytics::IsEnabled()) {
            std::lock_guard lock(benchMutex);
            const bool haveResult = !lastScore.empty();
            ImGui::Dummy(ImVec2(0.f, 6.f * scale));
            ImGui::BeginDisabled(!haveResult || busy);
            if (ImGui::Button(l.Get("pci", "main", "sharewithauthor").c_str())) {
                shareStatus = analytics::ShareBenchmarkWithAuthor(lastCpu, lastScoreType, lastScore)
                                  ? l.Get("pci", "main", "sharesent")
                                  : l.Get("pci", "main", "sharefailed");
            }
            ImGui::EndDisabled();
            if (!haveResult)
                ImGui::TextDisabled("%s", l.Get("pci", "main", "sharehint").c_str());
            else if (!shareStatus.empty())
                ImGui::TextDisabled("%s", shareStatus.c_str());
        }
    }
}

} // namespace maku::ui::pages
