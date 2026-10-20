#pragma once

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <optional>

namespace rdt {

/**
 * @brief High-resolution single retransmission timer.
 *        Used in Stop-and-Wait ARQ and Go-Back-N ARQ (tracking the oldest unacked packet).
 */
class SingleTimer {
public:
    SingleTimer() = default;

    /**
     * @brief Start or restart timer with a specific duration.
     */
    void start(std::chrono::milliseconds duration);

    /**
     * @brief Stop the timer.
     */
    void stop();

    /**
     * @brief Restart timer with the previous or new duration.
     */
    void restart(std::chrono::milliseconds duration);

    /**
     * @brief Check whether the timer is currently armed.
     */
    [[nodiscard]] bool is_running() const { return is_running_; }

    /**
     * @brief Check whether the timer is running and its deadline has passed.
     */
    [[nodiscard]] bool is_expired() const;

    /**
     * @brief Get time remaining until expiration. Returns 0ms if expired or not running.
     */
    [[nodiscard]] std::chrono::milliseconds remaining_time() const;

    /**
     * @brief Get elapsed time since the timer was started.
     */
    [[nodiscard]] std::chrono::milliseconds elapsed_time() const;

private:
    bool is_running_{false};
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point deadline_;
    std::chrono::milliseconds duration_{0};
};

/**
 * @brief Multi-packet timer manager for Selective Repeat ARQ.
 *        Allows individual timeout tracking per in-flight sequence number.
 */
class MultiTimerManager {
public:
    MultiTimerManager() = default;

    /**
     * @brief Start or re-arm timer for a specific sequence number.
     */
    void start_timer(uint32_t seq_num, std::chrono::milliseconds duration);

    /**
     * @brief Stop and remove timer for a specific sequence number (e.g. on ACK).
     */
    void stop_timer(uint32_t seq_num);

    /**
     * @brief Check if a timer is active for a sequence number.
     */
    [[nodiscard]] bool is_running(uint32_t seq_num) const;

    /**
     * @brief Check if a specific sequence number has timed out.
     */
    [[nodiscard]] bool is_expired(uint32_t seq_num) const;

    /**
     * @brief Returns a list of all sequence numbers that have expired up to current time.
     *        Optionally auto-stops or leaves them for retransmission re-arming.
     */
    [[nodiscard]] std::vector<uint32_t> get_expired_packets() const;

    /**
     * @brief Returns duration until the next earliest packet deadline.
     *        Useful for setting socket select()/poll() timeout without busy-waiting.
     * @return std::nullopt if no timers are active.
     */
    [[nodiscard]] std::optional<std::chrono::milliseconds> time_until_earliest_timeout() const;

    /**
     * @brief Get count of currently active timers.
     */
    [[nodiscard]] size_t active_count() const { return timers_.size(); }

    /**
     * @brief Clear all active timers.
     */
    void clear();

private:
    struct TimerEntry {
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point deadline;
        std::chrono::milliseconds duration;
    };

    std::unordered_map<uint32_t, TimerEntry> timers_;
};

} // namespace rdt
