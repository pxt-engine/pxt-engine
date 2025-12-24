#pragma once

#include "core/pch.hpp"

#include "concurrency/cpu_relax.hpp"

namespace pxt::concurrency {

    /**
     * @brief A guard that provides RAII-style locking for a spin lock implemented with std::atomic_flag.
     *
     * Upon construction, the SpinLockGuard attempts to acquire the lock by repeatedly calling test_and_set on
     * the atomic_flag.
     * If the lock is already held by another thread, it will call cpuRelax() to yield the CPU and reduce contention.
     * Upon destruction, the SpinLockGuard releases the lock by setting the atomic_flag to false.
     */
    class SpinLockGuard {
    public:
        explicit SpinLockGuard(std::atomic_flag& lock) noexcept : m_lock(lock) {
            while (m_lock.test_and_set(std::memory_order_acquire)) {
                cpuRelax();
            }
        }

        ~SpinLockGuard() noexcept { m_lock.clear(std::memory_order_release); }

    private:
        std::atomic_flag& m_lock;
    };

} // namespace pxt::concurrency