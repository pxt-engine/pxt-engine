#pragma once

#include "core/pch.hpp"

/**
 * @brief A lock-free work-stealing deque implementation based on the Chase-Lev algorithm.
 *
 * This data structure is designed for efficient task scheduling in multi-threaded job systems.
 * It supports three operations:
 * - push(): Owner thread adds work to its own deque (top side, LIFO order)
 * - pop(): Owner thread retrieves work from its own deque (top side, LIFO for cache locality)
 * - steal(): Other threads steal work from this deque (bottom side, FIFO order)
 *
 * The deque is structured as a circular buffer where:
 * - m_top: Index where the owner pushes/pops (grows upward)
 * - m_bottom: Index where thieves steal from (grows upward)
 * - Valid items exist in the range [bottom, top)
 *
 * Thread Safety:
 * - Only ONE thread (the owner) can call push() and pop()
 * - Multiple threads can call steal() concurrently
 * - All operations are lock-free and use atomic operations
 *
 * @tparam T The type of items stored in the deque (typically a Job struct)
 */
template <typename T>
class WorkStealingDeque {
    PXT_STATIC_ASSERT(std::is_trivially_copyable<T>::value, "WorkStealingDeque requires trivially copyable types");

public:
    /**
     * @brief Constructs a work-stealing deque with the specified capacity.
     *
     * @param capacity The maximum number of items the deque can hold.
     *                 Must be a power of 2 for optimal performance (enables bitwise AND masking).
     */
    explicit WorkStealingDeque(size_t capacity = 1024) : m_buffer(capacity), m_mask(capacity - 1) {
        // Ensure capacity is power of 2
        PXT_ASSERT((capacity & (capacity - 1)) == 0 && "Capacity must be a power of 2");
    }

    /**
     * @brief Pushes an item onto the deque (owner thread only).
     *
     * This operation adds work to the top of the deque. The owner always pushes and pops
     * from the same end (top), providing LIFO behavior for its own work, which improves
     * cache locality by working on recently added tasks first.
     *
     * @param item The item to push onto the deque
     *
     * @note This function must only be called by the owner thread.
     * @note No bounds checking is performed - ensure capacity is not exceeded.
     */
    void push(T item) {
        size_t t = m_top.load(std::memory_order_relaxed);

        m_buffer[t & m_mask] = std::move(item);

        std::atomic_thread_fence(std::memory_order_release);
        m_top.store(t + 1, std::memory_order_relaxed);
    }

    /**
     * @brief Pops an item from the deque (owner thread only, LIFO order).
     *
     * The owner retrieves work from the top of its deque in LIFO order (most recently added).
     * This provides good cache locality since the owner works on tasks it just created.
     *
     * Algorithm steps:
     * 1. Check if deque is empty (bottom >= top)
     * 2. Decrement top speculatively
     * 3. Re-check if there are items after decrement (handles races with steal)
     * 4. If this is the last item (bottom == top), use CAS (Compare And Swap) to compete with thieves
     *
     * @param item Output parameter to store the popped item
     * @return true if an item was successfully popped, false if deque was empty
     *
     * @note This function must only be called by the owner thread.
     */
    [[nodiscard]] bool pop(T& item) {
        size_t t = m_top.load(std::memory_order_relaxed);
        size_t b = m_bottom.load(std::memory_order_relaxed);

        // Early exit: deque is empty
        if (b >= t) {
            return false;
        }

        // Decrement top
        t = t - 1;
        m_top.store(t, std::memory_order_relaxed);

        // Sequential consistency fence ensures proper ordering with concurrent steal operations
        std::atomic_thread_fence(std::memory_order_seq_cst);

        // Re-read bottom to detect if a thief stole the last item concurrently
        b = m_bottom.load(std::memory_order_relaxed);

        // Check if deque became empty after our speculative decrement
        if (b > t) {
            // Deque is empty, restore top and fail
            m_top.store(t + 1, std::memory_order_relaxed);

            return false;
        }

        // Special case: this is the last item in the deque
        if (b == t) {

            // Race condition: both owner (pop) and a thief (steal) may try to take the last item
            // Use compare-and-swap on bottom to determine the winner
            if (!m_bottom.compare_exchange_strong(b, b + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                // CAS failed: a thief won the race and took the item
                m_top.store(t + 1, std::memory_order_relaxed);

                return false;
            }

            // CAS succeeded: we won the race, restore top to maintain consistency
            m_top.store(t + 1, std::memory_order_relaxed);
        }

        // We won the race, so it's safe to move
        item = std::move(m_buffer[t & m_mask]);

        return true;
    }

    /**
     * @brief Steals an item from the deque (thief threads, FIFO order).
     *
     * Other threads (thieves) steal work from the bottom of the deque in FIFO order.
     * This means they get the oldest work, which is good for load balancing as it
     * represents work that might have larger subtasks.
     *
     * Algorithm steps:
     * 1. Read bottom (oldest item)
     * 2. Read top (newest item boundary)
     * 3. Check if items exist (bottom < top)
     * 4. Attempt to increment bottom using CAS to claim the item
     * 5. If CAS succeeds, the item is stolen; if it fails, another thief got it
     *
     * @param item Output parameter to store the stolen item
     * @return true if an item was successfully stolen, false if deque was empty or contention occurred
     *
     * @note Multiple threads can call this function concurrently.
     */
    [[nodiscard]] bool steal(T& item) {
        // Acquire ensures we see all writes to items before this bottom value
        size_t b = m_bottom.load(std::memory_order_acquire);

        // Sequential consistency fence ensures proper ordering with pop operations
        std::atomic_thread_fence(std::memory_order_seq_cst);

        // Acquire ensures we see the most recent top value
        size_t t = m_top.load(std::memory_order_acquire);

        // Check if there are items to steal
        if (b < t) {

            // Try to claim this item by incrementing bottom atomically
            // If CAS fails, another thief or the owner (in pop's last-item case) got it first
            if (!m_bottom.compare_exchange_strong(b, b + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return false; // Lost the race
            }

            // Retrieve the item at bottom (oldest item)
            // We won the race, so it's safe to move
            item = std::move(m_buffer[b & m_mask]);

            return true; // Successfully stolen
        }

        return false; // Deque is empty
    }

    /**
     * @brief Checks if the deque is empty.
     *
     * @return true if the deque is empty (no items available), false otherwise
     *
     * @note This is a relaxed check and may not reflect concurrent modifications.
     *       It's primarily used as a hint for work-stealing decisions.
     */
    bool isProbablyEmpty() const {
        size_t b = m_bottom.load(std::memory_order_relaxed);
        size_t t = m_top.load(std::memory_order_relaxed);

        return b >= t;
    }

private:
    /**
     * @brief Bitmask for fast modulo operation (capacity - 1, works when capacity is power of 2)
     */
    const size_t m_mask;

    /**
     * @brief Circular buffer holding the items
     */
    std::vector<T> m_buffer;

    //? Cache line alignment prevents false sharing between owner and thief threads.
    //? m_top is modified by the owner, m_bottom by thieves - keeping them in separate
    //? cache lines avoids expensive cache coherency traffic.

    /**
     * @brief The top index where the owner pushes/pops (grows upward)
     */
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> m_top{0};

    /**
     * @brief The bottom index where thieves steal from (grows upward)
     */
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> m_bottom{0};
};