#pragma once

#include "core/SysInfo.h"

#include <functional>
#include <string>

namespace maku::report {

using StatusMapper = std::function<std::wstring(const std::wstring&)>;

/// Key specs for forums / support tickets.
std::wstring BuildCompactSummaryFromSnapshot(const pci::Snapshot& s,
                                             StatusMapper mapStatus = nullptr);

/// Full text dump of all collected fields.
std::wstring BuildFullReportFromSnapshot(const pci::Snapshot& s, StatusMapper mapStatus = nullptr);

std::wstring BuildCompactSummary();
std::wstring BuildFullReport();
std::string BuildFastfetchArt();

} // namespace maku::report
