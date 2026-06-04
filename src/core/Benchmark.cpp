#include "core/Benchmark.h"
#include <windows.h>
#include <cmath>
#include <thread>
#include <vector>

// Match official MakuTweaker: real math work must not be optimized away in Release.
#if defined(_MSC_VER)
#pragma optimize("", off)
#endif

namespace maku::bench {

namespace {

// Similar cost profile to System.Random.NextDouble() (no STL distribution per call).
class NetRandom {
public:
    explicit NetRandom(int seed) : seed_(seed ? seed : 1) {}
    double NextDouble() {
        seed_ = static_cast<int>(seed_ * 214013L + 2531011L);
        const unsigned u = static_cast<unsigned>(seed_) & 0x7FFFFFFFu;
        return u / 2147483648.0;
    }

private:
    int seed_;
};

volatile long long g_benchSink = 0;

long long RunSingleCore(ULONGLONG startTick, DWORD durationMs) {
    volatile double a = 1.000001;
    volatile double b = 1.000002;
    volatile long long x = 1234567;
    long long ops = 0;
    NetRandom rnd(static_cast<int>(GetTickCount()));

    while (GetTickCount64() - startTick < durationMs) {
        for (int k = 0; k < 200000; ++k) {
            const double sinA = std::sin(a);
            const double cosB = std::cos(b);
            const double sum = sinA * cosB + std::sqrt(std::abs(a + b));
            a = sum;
            b = a * 0.999999 + b * 0.000001 + rnd.NextDouble();
            x = (x * 1664525 + 1013904223) & 0xFFFFFFFFLL;
            ops += 3;
        }
    }
    g_benchSink = x ^ ops;
    return ops;
}

long long RunMultiCore(ULONGLONG startTick, DWORD durationMs) {
    const int threads = static_cast<int>(std::thread::hardware_concurrency());
    if (threads < 1) return RunSingleCore(startTick, durationMs);

    std::vector<long long> perThread(static_cast<size_t>(threads), 0);
    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(threads));

    for (int i = 0; i < threads; ++i) {
        pool.emplace_back([&, i] {
            volatile double a = 1.000001 + i * 0.00001;
            volatile double b = 1.000002 + i * 0.00002;
            volatile long long x = 1234567 + i;
            long long localOps = 0;
            NetRandom rnd(i * 37 + static_cast<int>(GetTickCount()));

            while (GetTickCount64() - startTick < durationMs) {
                for (int k = 0; k < 200000; ++k) {
                    const double sinA = std::sin(a);
                    const double cosB = std::cos(b);
                    const double sum = sinA * cosB + std::sqrt(std::abs(a + b));
                    a = sum;
                    b = a * 0.999999 + b * 0.000001 + rnd.NextDouble();
                    x = (x * 1664525 + 1013904223) & 0xFFFFFFFFLL;
                    localOps += 3;
                }
            }
            perThread[static_cast<size_t>(i)] = localOps;
            g_benchSink ^= x;
        });
    }
    for (auto& t : pool) t.join();

    long long total = 0;
    for (long long v : perThread) total += v;
    return total;
}

} // namespace

Result Run(bool multithreaded, int durationMs) {
    const ULONGLONG startTick = GetTickCount64();
    const DWORD duration = static_cast<DWORD>(durationMs > 0 ? durationMs : 10000);

    const long long totalOps =
        multithreaded ? RunMultiCore(startTick, duration) : RunSingleCore(startTick, duration);

    const ULONGLONG endTick = GetTickCount64();
    const double seconds = (endTick - startTick) / 1000.0;

    Result r;
    r.totalOps = totalOps;
    r.elapsedMs = static_cast<long long>(endTick - startTick);
    if (seconds > 0.0)
        r.score = (static_cast<double>(totalOps) / seconds) / 100000.0;
    return r;
}

} // namespace maku::bench

#if defined(_MSC_VER)
#pragma optimize("", on)
#endif
