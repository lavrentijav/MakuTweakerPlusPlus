#pragma once

namespace maku::bench {

// Same scoring as official MakuTweaker: normalized ops/sec index (not raw loop count).
struct Result {
    double score = 0;
    long long totalOps = 0;
    long long elapsedMs = 0;
};

Result Run(bool multithreaded, int durationMs = 10000);

} // namespace maku::bench
