#pragma once
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <atomic>

namespace maku::jobs {

class JobQueue {
public:
    using Job = std::function<void()>;

    static JobQueue& Instance();
    JobQueue(const JobQueue&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;

    void Enqueue(Job job);
    void SetProgress(const std::wstring& text, float value = -1.f);
    std::wstring ProgressText();
    float ProgressValue();
    bool IsBusy() const { return busy_.load(); }

    JobQueue();
    ~JobQueue() = default;

private:
    std::mutex mutex_;
    std::queue<Job> queue_;
    std::thread worker_;
    std::atomic<bool> busy_{false};
    std::wstring progressText_;
    float progressValue_ = -1.f;
    void WorkerLoop();
};

} // namespace maku::jobs
