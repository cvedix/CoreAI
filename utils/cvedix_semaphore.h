/**
 * @file cvedix_semaphore.h
 * @brief Counting semaphore for producer-consumer patterns
 */

#pragma once

#include <condition_variable>
#include <mutex>

namespace cvedix_utils {
    /**
     * @brief Semaphore for queue synchronization
     */
    class cvedix_semaphore
    {

    public:
        cvedix_semaphore() {
            count_ = 0;
        }

        void signal() {
            std::unique_lock<std::mutex> lock(mutex_);
            ++count_;
            cv_.notify_one();
        }

        void wait() {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [=] { return count_ > 0; });
            --count_;
        }
        
        void reset() {
            std::unique_lock<std::mutex> lock(mutex_);
            count_ = 0;
            cv_.notify_one();
        }
    private:
        std::mutex mutex_;
        std::condition_variable cv_;
        int count_;
    };
}