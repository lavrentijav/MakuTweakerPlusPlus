#include "core/JobQueue.h"
#include <chrono>
#include <thread>

namespace maku::jobs {

JobQueue& JobQueue::Instance() {
    static JobQueue q;
    return q;
}

JobQueue::JobQueue() { worker_ = std::thread([this] { WorkerLoop(); }); }

void JobQueue::Enqueue(Job job) {
    std::lock_guard lock(mutex_);
    queue_.push(std::move(job));
}

void JobQueue::SetProgress(const std::wstring& text, float value) {
    std::lock_guard lock(mutex_);
    progressText_ = text;
    progressValue_ = value;
}

std::wstring JobQueue::ProgressText() {
    std::lock_guard lock(mutex_);
    return progressText_;
}

float JobQueue::ProgressValue() {
    std::lock_guard lock(mutex_);
    return progressValue_;
}

void JobQueue::WorkerLoop() {
    for (;;) {
        Job job;
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) {
                busy_ = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            busy_ = true;
            job = std::move(queue_.front());
            queue_.pop();
        }
        job();
    }
}

} // namespace maku::jobs
