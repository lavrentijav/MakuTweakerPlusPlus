#pragma once
#include <vector>

namespace maku::cpu {

struct CpuLayout {
    int logicalCount = 0;
    int coreCount = 0;
    int numaNodeCount = 0;
    int packageCount = 0;
    std::vector<int> logicalToCore;    // proc_index -> core_id
    std::vector<int> logicalToNuma;    // proc_index -> numa_id
    std::vector<int> logicalToPackage; // proc_index -> socket_id
    std::vector<int> coreToLogical;    // first logical per core
};

/// Detect CPU topology via GetLogicalProcessorInformationEx.
CpuLayout DetectLayout();

/// Aggregate per-logical usage into per-core (max per core).
std::vector<float> AggregateToCores(const CpuLayout& layout, const std::vector<float>& logical);

/// Group logical indices by NUMA node.
std::vector<std::vector<int>> GroupByNuma(const CpuLayout& layout);

} // namespace maku::cpu
