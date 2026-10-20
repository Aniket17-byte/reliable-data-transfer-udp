#include "timer_manager.hpp"
#include <algorithm>

namespace rdt {

// ================= SingleTimer Implementation =================

void SingleTimer::start(std::chrono::milliseconds duration) {
    start_time_ = std::chrono::steady_clock::now();
    duration_ = duration;
    deadline_ = start_time_ + duration;
    is_running_ = true;
}

void SingleTimer::stop() {
    is_running_ = false;
}

void SingleTimer::restart(std::chrono::milliseconds duration) {
    start(duration);
}

bool SingleTimer::is_expired() const {
    if (!is_running_) {
        return false;
    }
    return std::chrono::steady_clock::now() >= deadline_;
}

std::chrono::milliseconds SingleTimer::remaining_time() const {
    if (!is_running_) {
        return std::chrono::milliseconds(0);
    }
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline_) {
        return std::chrono::milliseconds(0);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now);
}

std::chrono::milliseconds SingleTimer::elapsed_time() const {
    if (!is_running_) {
        return std::chrono::milliseconds(0);
    }
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
}

// ================= MultiTimerManager Implementation =================

void MultiTimerManager::start_timer(uint32_t seq_num, std::chrono::milliseconds duration) {
    auto now = std::chrono::steady_clock::now();
    TimerEntry entry;
    entry.start_time = now;
    entry.duration = duration;
    entry.deadline = now + duration;
    timers_[seq_num] = entry;
}

void MultiTimerManager::stop_timer(uint32_t seq_num) {
    timers_.erase(seq_num);
}

bool MultiTimerManager::is_running(uint32_t seq_num) const {
    return timers_.find(seq_num) != timers_.end();
}

bool MultiTimerManager::is_expired(uint32_t seq_num) const {
    auto it = timers_.find(seq_num);
    if (it == timers_.end()) {
        return false;
    }
    return std::chrono::steady_clock::now() >= it->second.deadline;
}

std::vector<uint32_t> MultiTimerManager::get_expired_packets() const {
    std::vector<uint32_t> expired;
    auto now = std::chrono::steady_clock::now();
    for (const auto& [seq, entry] : timers_) {
        if (now >= entry.deadline) {
            expired.push_back(seq);
        }
    }
    return expired;
}

std::optional<std::chrono::milliseconds> MultiTimerManager::time_until_earliest_timeout() const {
    if (timers_.empty()) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    auto earliest_deadline = std::chrono::steady_clock::time_point::max();

    for (const auto& [seq, entry] : timers_) {
        if (entry.deadline < earliest_deadline) {
            earliest_deadline = entry.deadline;
        }
    }

    if (now >= earliest_deadline) {
        return std::chrono::milliseconds(0);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(earliest_deadline - now);
}

void MultiTimerManager::clear() {
    timers_.clear();
}

} // namespace rdt
