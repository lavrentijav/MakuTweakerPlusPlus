#include "core/HwMonitor.h"
#include "core/Wmi.h"
#include <vector>

namespace maku::hwmon {
namespace {

int KelvinWmiToC(const std::wstring& v) {
    try {
        const int k = std::stoi(v);
        return (k - 2732) / 10;
    } catch (...) {
        return -1;
    }
}

} // namespace

Snapshot Collect() {
    Snapshot s;
    std::vector<std::vector<wmi::Row>> tz;
    if (wmi::Query(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature", tz,
                   L"ROOT\\WMI")) {
        int i = 0;
        for (const auto& row : tz) {
            for (const auto& c : row) {
                if (_wcsicmp(c.name.c_str(), L"CurrentTemperature") == 0) {
                    TempReading t;
                    t.name = L"Thermal zone " + std::to_wstring(i++);
                    t.celsius = KelvinWmiToC(c.value);
                    if (t.celsius > -40 && t.celsius < 150) s.temps.push_back(t);
                }
            }
        }
    }

    std::vector<std::vector<wmi::Row>> smart;
    if (wmi::Query(L"SELECT InstanceName, PredictFailure, Reason FROM MSStorageDriver_FailurePredictStatus",
                   smart, L"ROOT\\WMI")) {
        for (const auto& row : smart) {
            SmartDisk d;
            for (const auto& c : row) {
                if (_wcsicmp(c.name.c_str(), L"InstanceName") == 0) d.name = c.value;
                if (_wcsicmp(c.name.c_str(), L"PredictFailure") == 0)
                    d.status = (c.value == L"0" || c.value == L"False") ? L"OK" : L"Warning";
            }
            d.healthPercent = d.status == L"OK" ? 100 : 50;
            if (!d.name.empty()) s.disks.push_back(d);
        }
    }

    if (s.disks.empty()) {
        auto models = wmi::QueryList(L"SELECT Model FROM Win32_DiskDrive", L"Model");
        for (const auto& m : models) {
            SmartDisk d;
            d.name = m;
            d.status = L"N/A";
            d.healthPercent = -1;
            s.disks.push_back(d);
        }
    }
    return s;
}

} // namespace maku::hwmon
