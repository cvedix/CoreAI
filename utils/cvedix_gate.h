/**
 * @file cvedix_gate.h
 * @brief Synchronization gate for pause/resume control
 */

#pragma once

#include <condition_variable>
#include <mutex>

namespace cvedix_utils {
    /**
     * @brief Thread gate for pause/resume
     */
    class cvedix_gate
    {

    public:
        cvedix_gate() {
            opened_ = false;
        }

        void open() {
            std::unique_lock<std::mutex> lock(mutex_);
            opened_ = true;
            cv_.notify_one();
        }

        // wait until opened
        void knock() {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [=] { return opened_; });
        }

        void close() {
            std::unique_lock<std::mutex> lock(mutex_);
            opened_ = false;
        }

        bool is_open() {
            return opened_;
        }
    private:
        std::mutex mutex_;
        std::condition_variable cv_;
        bool opened_;
    };
}