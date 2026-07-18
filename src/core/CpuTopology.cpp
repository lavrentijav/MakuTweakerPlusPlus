#include "core/CpuTopology.h"
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <windows.h>

namespace maku::cpu {
namespace {

using ProcInfoFn = BOOL(WINAPI*)(LOGICAL_PROCESSOR_RELATIONSHIP, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,
                                 PDWORD);

ProcInfoFn GetProcInfoEx() {
    static ProcInfoFn fn = reinterpret_cast<ProcInfoFn>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetLogicalProcessorInformationEx"));
    return fn;
}

std::vector<BYTE> QueryAllBytes() {
    std::vector<BYTE> buf;
    const auto fn = GetProcInfoEx();
    if (!fn) return buf;
    DWORD size = 0;
    fn(RelationAll, nullptr, &size);
    if (size == 0) return buf;
    buf.resize(size);
    if (!fn(RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()),
            &size))
        return {};
    buf.resize(size);
    return buf;
}

void MarkLogical(KAFFINITY mask, WORD group, std::map<int, int>& logicalToCore, int coreId,
                 int& maxLogical) {
    for (int bit = 0; bit < 64; ++bit) {
        if ((mask >> bit) & 1) {
            const int idx = static_cast<int>(group) * 64 + bit;
            logicalToCore[idx] = coreId;
            maxLogical = std::max(maxLogical, idx);
        }
    }
}

CpuLayout FallbackLayout() {
    CpuLayout layout;
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    layout.logicalCount = static_cast<int>(si.dwNumberOfProcessors);
    layout.coreCount = layout.logicalCount;
    layout.numaNodeCount = 1;
    layout.packageCount = 1;
    layout.logicalToCore.resize(layout.logicalCount);
    layout.logicalToNuma.resize(layout.logicalCount);
    layout.logicalToPackage.resize(layout.logicalCount);
    for (int i = 0; i < layout.logicalCount; ++i) {
        layout.logicalToCore[i] = i;
        layout.logicalToNuma[i] = 0;
        layout.logicalToPackage[i] = 0;
    }
    layout.coreToLogical = layout.logicalToCore;
    return layout;
}

} // namespace

CpuLayout DetectLayout() {
    const auto buf = QueryAllBytes();
    if (buf.empty()) return FallbackLayout();

    std::map<int, int> logicalToCore;
    std::map<int, int> logicalToNuma;
    std::map<int, int> logicalToPackage;
    int coreId = 0;
    int maxLogical = 0;
    int pkgId = 0;

    const BYTE* base = buf.data();
    const BYTE* end = base + buf.size();
    for (const BYTE* p = base; p < end;) {
        const size_t remaining = static_cast<size_t>(end - p);
        if (remaining < sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)) break;

        const auto* info = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(p);
        if (info->Size == 0 || info->Size > remaining) break;

        if (info->Relationship == RelationProcessorCore) {
            const auto& core = info->Processor;
            for (WORD g = 0; g < core.GroupCount; ++g)
                MarkLogical(core.GroupMask[g].Mask, core.GroupMask[g].Group, logicalToCore, coreId,
                            maxLogical);
            ++coreId;
        } else if (info->Relationship == RelationNumaNode) {
            const auto& numa = info->NumaNode;
            const KAFFINITY mask = numa.GroupMask.Mask;
            const WORD group = numa.GroupMask.Group;
            for (int bit = 0; bit < 64; ++bit) {
                if ((mask >> bit) & 1) {
                    const int idx = static_cast<int>(group) * 64 + bit;
                    logicalToNuma[idx] = static_cast<int>(numa.NodeNumber);
                    maxLogical = std::max(maxLogical, idx);
                }
            }
        } else if (info->Relationship == RelationProcessorPackage) {
            const auto& pkg = info->Processor;
            for (WORD g = 0; g < pkg.GroupCount; ++g) {
                const auto& gi = pkg.GroupMask[g];
                for (int bit = 0; bit < 64; ++bit) {
                    if ((gi.Mask >> bit) & 1) {
                        const int idx = static_cast<int>(gi.Group) * 64 + bit;
                        logicalToPackage[idx] = pkgId;
                        maxLogical = std::max(maxLogical, idx);
                    }
                }
            }
            ++pkgId;
        }

        p += info->Size;
    }

    if (logicalToCore.empty()) return FallbackLayout();

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const int sysLogical = static_cast<int>(si.dwNumberOfProcessors);

    int maxIdx = 0;
    for (const auto& kv : logicalToCore) maxIdx = std::max(maxIdx, kv.first);
    for (const auto& kv : logicalToNuma) maxIdx = std::max(maxIdx, kv.first);
    const int span = std::max(sysLogical, maxIdx + 1);

    CpuLayout layout;
    layout.logicalCount = sysLogical > 0 ? sysLogical : maxIdx + 1;
    layout.coreCount = coreId > 0 ? coreId : layout.logicalCount;

    std::set<int> numaIds;
    for (const auto& kv : logicalToNuma) numaIds.insert(kv.second);
    std::set<int> pkgIds;
    for (const auto& kv : logicalToPackage) pkgIds.insert(kv.second);
    layout.numaNodeCount = std::max(1, static_cast<int>(numaIds.empty() ? 1 : numaIds.size()));
    layout.packageCount = std::max(1, static_cast<int>(pkgIds.empty() ? 1 : pkgIds.size()));

    layout.logicalToCore.resize(span);
    layout.logicalToNuma.resize(span);
    layout.logicalToPackage.resize(span);
    for (int i = 0; i < span; ++i) {
        layout.logicalToCore[i] = logicalToCore.count(i) ? logicalToCore[i] : i % layout.coreCount;
        layout.logicalToNuma[i] = logicalToNuma.count(i) ? logicalToNuma[i] : 0;
        layout.logicalToPackage[i] = logicalToPackage.count(i) ? logicalToPackage[i] : 0;
    }

    layout.coreToLogical.assign(layout.coreCount, -1);
    for (int i = 0; i < span; ++i) {
        const int c = layout.logicalToCore[i];
        if (c >= 0 && c < layout.coreCount && layout.coreToLogical[c] < 0)
            layout.coreToLogical[c] = i;
    }
    return layout;
}

std::vector<float> AggregateToCores(const CpuLayout& layout, const std::vector<float>& logical) {
    std::vector<float> cores(layout.coreCount, 0.f);
    for (int i = 0; i < layout.logicalCount && i < static_cast<int>(logical.size()); ++i) {
        const int c = layout.logicalToCore[i];
        if (c >= 0 && c < layout.coreCount)
            cores[c] = std::max(cores[c], logical[i]);
    }
    return cores;
}

std::vector<std::vector<int>> GroupByNuma(const CpuLayout& layout) {
    std::vector<std::vector<int>> groups(layout.numaNodeCount);
    for (int i = 0; i < layout.logicalCount; ++i) {
        const int n = layout.logicalToNuma[i];
        if (n >= 0 && n < layout.numaNodeCount) groups[n].push_back(i);
    }
    return groups;
}

} // namespace maku::cpu
