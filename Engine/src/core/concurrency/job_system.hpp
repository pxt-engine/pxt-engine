#pragma once

#include "core/pch.hpp"

#include "core/concurrency/cpu_relax.hpp"
#include "core/concurrency/work_stealing_deque.hpp"

#include <new>

namespace pxt::core {

    using JobFunction = void (*)(void*);

    /**
     * @brief A Job represents a single unit of work to be executed by the JobSystem.
     */
    struct Job {
        JobFunction fn = nullptr;  //< Function to execute
        void* data = nullptr;      //< Data to pass to the function
        uint32_t counterIndex = 0; //< Index into the counter pool for tracking completion

        /**
         * @brief Checks if this job is valid and ready to execute.
         * @return true if both function and data pointers are non-null
         */
        bool isValid() { return fn && data; }
    };

    /**
     * @brief A JobHandle is an opaque identifier for a submitted job or batch of jobs.
     *
     * JobHandles are used to wait for job completion via the wait() function.
     * They internally reference a counter in the CounterPool that tracks how many
     * jobs in the batch are still pending.
     */
    using JobHandle = uint32_t;
    static constexpr JobHandle InvalidJobHandle = 0xFFFFFFFF;

    /**
     * @brief Padded atomic uint32 to avoid false sharing between threads.
     *
     * False sharing occurs when multiple threads access different variables that reside
     * in the same cache line, causing unnecessary cache invalidations and performance degradation.
     *
     * This structure is aligned to the hardware destructive interference size (typically 64 bytes
     * on modern x86-64 architectures) to ensure that each atomic counter resides in its own
     * cache line, eliminating false sharing between threads accessing different counters.
     *
     * The C++17 constant std::hardware_destructive_interference_size provides a portable way
     * to query the cache line size at compile time.
     */
    struct alignas(std::hardware_destructive_interference_size) PaddedAtomicUint32 {
        std::atomic<uint32_t> value{0};
    };

    /**
     * @brief CounterPool manages a pool of atomic counters for tracking job completion.
     *
     * The JobSystem uses reference counting to track when batches of jobs complete.
     * Each JobHandle corresponds to an index in this pool:
     *
     * Workflow:
     * 1. When jobs are submitted, a counter is allocated and initialized to the number of jobs
     * 2. As each job completes, it decrements its associated counter
     * 3. When the counter reaches zero, all jobs in the batch have completed
     * 4. The wait() function spins on a counter until it reaches zero
     *
     * Memory Layout:
     * - Fixed-size array of MAX_COUNTERS=4096 padded counters
     * - Each counter is cache-line aligned (64 bytes on most architectures)
     * - Total size: 4096 * 64 = 256 KB
     *
     * @note Counters are reused in a circular way. If more than MAX_COUNTERS jobs
     *       are in flight simultaneously, counters may be reused prematurely, causing
     *       incorrect behavior (see acquireCounter() for details).
     */
    struct CounterPool {
        /**
         * @brief Accesses a counter by index.
         * @param index The counter index (must be < MAX_COUNTERS)
         * @return Reference to the padded atomic counter
         */
        PaddedAtomicUint32& operator[](size_t index) { return m_counters[index]; }

        size_t maxCounters() const { return MAX_COUNTERS; }

    private:
        static constexpr size_t MAX_COUNTERS = 4096;

        std::array<PaddedAtomicUint32, MAX_COUNTERS> m_counters{};
    };

    /**
     * @brief A Worker represents a single worker thread in the JobSystem.
     *
     * Each worker has:
     * - Its own work-stealing deque for storing jobs
     * - A dedicated thread that processes jobs from its deque
     */
    struct Worker {
        WorkStealingDeque<Job> deque; //< Lock-free deque for storing jobs
        std::jthread thread;          //< The worker thread (automatically joins on destruction)
    };

    /**
     * @brief A lock-free, work-stealing job system for parallel task execution.
     *
     * The JobSystem manages a pool of worker threads that execute jobs in parallel.
     * It uses work-stealing deques to efficiently distribute work among threads with
     * minimal contention and synchronization overhead.
     *
     * Key Features:
     * - Lock-free job submission and execution
     * - Work stealing for automatic load balancing
     * - Batch job submission for fine-grained parallelism
     * - Busy-waiting with helping: threads waiting for jobs actively help execute them
     * - Hybrid spinning/sleeping: workers spin briefly for new work, then sleep to save CPU
     *
     * Architecture:
     * - Each worker thread has its own work-stealing deque
     * - Jobs are distributed round-robin across worker deques
     * - Workers prefer their own deque (LIFO) for cache locality
     * - Workers steal from others (FIFO) when their deque is empty
     */
    class JobSystem {
    public:
        /**
         * @brief Constructs a JobSystem with the specified number of worker threads.
         *
         * @param threadCount Number of worker threads to create.
         *                    Defaults to std::thread::hardware_concurrency() (number of logical cores).
         */
        explicit JobSystem(size_t threadCount = std::thread::hardware_concurrency()) {
            m_workers.reserve(threadCount);

            // Creates worker objects with their deques
            for (size_t i = 0; i < threadCount; ++i) {
                m_workers.push_back(createUnique<Worker>());
            }

            // Spawns worker threads that immediately begin waiting for work
            for (size_t i = 0; i < threadCount; ++i) {
                m_workers[i]->thread = std::jthread([this, i](std::stop_token st) {
                    // Initializes thread-local variables for each worker
                    t_workerIndex = i;

                    workerLoop(i, st);
                });
            }
        }

        ~JobSystem() {
            m_stop.store(true, std::memory_order_release);
            m_condition.notify_all();
        }

        /**
         * @brief Submits a single job for execution.
         *
         * The job is pushed to a worker's deque in round-robin fashion and
         * one worker is notified to wake up and process it.
         *
         * @tparam T The type of data to pass to the job function
         * @param fn The function to execute (must have signature: void (*)(void*))
         * @param data Pointer to the data to pass to the function
         *
         * @return A JobHandle that can be used to wait for completion
         *
         * @note The data pointer must remain valid until the job completes.
         */
        template <typename T>
        [[nodiscard]] JobHandle submit(JobFunction fn, T* data) {
            JobHandle handle = acquireCounter(1);

            pushJob({fn, static_cast<void*>(data), handle});

            return handle;
        }

        /**
         * @brief Submits a batch of jobs for parallel execution.
         *
         * This allows fine-grained parallelism: each item becomes an independent job
         * that can be executed on any worker thread.
         *
         * @tparam T The type of items in the span
         * @param fn The function to execute for each item (must have signature: void(*)(void*))
         * @param items A span of items to process in parallel
         * @return A JobHandle representing the entire batch (wait on this to wait for all jobs)
         *
         * @note All items in the span must remain valid until all jobs complete.
         * @note Returns InvalidJobHandle if the span is empty.
         */
        template <typename T>
        [[nodiscard]] JobHandle submitBatch(JobFunction fn, std::span<T> items) {
            if (items.empty()) {
                return InvalidJobHandle;
            }

            // Allocate a single counter for the entire batch
            JobHandle handle = acquireCounter(static_cast<uint32_t>(items.size()));

            // Distribute jobs round-robin across all worker deques
            // This ensures even distribution and avoids overloading a single worker
            size_t workerIdx = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

            for (auto& item : items) {
                m_workers[workerIdx]->deque.push({fn, static_cast<void*>(&item), handle});
                workerIdx = (workerIdx + 1) % m_workers.size();
            }

            // Notify all workers after all jobs are pushed to avoid excessive wakeup overhead
            m_condition.notify_all();

            return handle;
        }

        /**
         * @brief Waits for a job or batch of jobs to complete.
         *
         * This function implements "busy-waiting with helping": instead of sleeping,
         * the waiting thread actively participates in executing jobs. This has several benefits:
         * 1. Prevents deadlock: if the waiting thread is itself a worker, it won't block
         *    while waiting for jobs it could help execute
         * 2. Improves performance: the waiting thread contributes work instead of idling
         * 3. Better CPU utilization: especially important when the main thread waits
         *
         * @param handle The JobHandle returned from submit() or submitBatch()
         *
         * @note This function is thread-safe and can be called from any thread,
         *       including worker threads and the main thread.
         * @note If the handle is invalid or out of range, the function returns immediately.
         */
        void wait(JobHandle handle) {
            if (handle == InvalidJobHandle || handle >= m_counterPool.maxCounters()) {
                return;
            }

            auto& counter = m_counterPool[handle].value;

            // Busy-wait with helping: actively execute jobs while waiting
            while (counter.load(std::memory_order_acquire) > 0) {
                if (!executeOneJob(t_workerIndex)) {
                    // No work available, yield to avoid burning CPU cycles
                    std::this_thread::yield();
                }
            }
        }

    private:
        /**
         * @brief Pushes a job to a worker's deque and wakes a worker.
         *
         * @param job The job to push
         *
         * Selects a worker in round-robin fashion and pushes the job to its deque.
         * Then wakes one sleeping worker to process it.
         */
        void pushJob(Job job) {
            size_t idx = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

            m_workers[idx]->deque.push(job);

            m_condition.notify_one();
        }

        /**
         * @brief Attempts to execute one job from the given worker's perspective.
         *
         * @param index The index of the worker attempting to execute a job
         * @return true if a job was executed, false if no work was available
         */
        bool executeOneJob(size_t index) {
            Job job;

            // Try to take work from our own deque (LIFO)
            // This provides good cache locality as we work on recently added tasks
            m_workers[index]->deque.pop(job);

            if (job.isValid()) {
                process(job);
                return true;
            }

            // Work stealing from other workers (FIFO)
            // Randomize the steal attempt order to reduce contention when multiple
            // workers try to steal from the same victim simultaneously
            size_t startIdx = t_rng() % m_workers.size();

            for (size_t i = 0; i < m_workers.size(); ++i) {
                size_t target = (startIdx + i) % m_workers.size();

                if (target == index)
                    continue;

                m_workers[target]->deque.steal(job);

                if (job.isValid()) {
                    process(job);
                    return true;
                }
            }

            return false;
        }

        /**
         * @brief Executes a job and updates its completion counter.
         *
         * @param job The job to process
         */
        void process(Job& job) {
            if (!job.isValid()) {
                return;
            }

            // Execute the job
            job.fn(job.data);

            // Signal completion: Release ordering ensures all data writes in fn()
            // are visible to any thread that acquires this counter (e.g., in wait())
            m_counterPool[job.counterIndex].value.fetch_sub(1, std::memory_order_release);
        }

        /**
         * @brief Main loop executed by each worker thread.
         *
         * The worker loop implements a three-phase strategy:
         * Active Execution Phase:
         * - Try to execute a job immediately
         * - If successful, continue without delay
         * Spinning Phase:
         * - If no work found, spin for a short time
         * - Check if work becomes available without expensive synchronization
         * - Use CPU relaxation hints to reduce power consumption
         * - If work appears during spinning, immediately return to Active Execution Phase
         * Sleeping Phase:
         * - If still no work after spinning, go to sleep on condition variable
         * - This prevents wasting CPU cycles when the system is idle
         * - Wake up when: stop requested, work becomes available, or spurious wakeup
         *
         * This hybrid approach balances responsiveness (spinning) with efficiency (sleeping).
         *
         * @param index The worker's index
         * @param st Stop token for graceful shutdown
         */
        void workerLoop(size_t index, std::stop_token st) {
            while (!st.stop_requested() && !m_stop.load(std::memory_order_relaxed)) {

                // Active Execution Phase: Try to execute one job
                if (executeOneJob(index)) {
                    continue; // Work found, keep going immediately
                }

                // Spinning Phase:
                // Stay active for a short burst to catch new jobs without the overhead
                // of a context switch. This is beneficial when work arrives frequently.
                bool foundWork = false;
                constexpr int MAX_SPIN_ITERATIONS = 300;
                for (int spin = 0; spin < MAX_SPIN_ITERATIONS; ++spin) {
                    if (hasWork(index)) {
                        foundWork = true;
                        break;
                    }

                    // Provide a hint to the processor that the code sequence is a spin-wait loop.
                    // This can help improve the performance and power consumption of spin-wait loops.
                    cpuRelax();
                }

                // If work was found during spinning, continue the loop to process it.
                if (foundWork) {
                    continue;
                }

                // Sleeping Phase:
                // No work found after spinning, sleep until notified
                // This prevents burning CPU cycles when the system is idle
                std::unique_lock lock(m_mutex);
                m_condition.wait(lock, [this, index, &st] {
                    return m_stop.load(std::memory_order_relaxed) || st.stop_requested() || hasWork(index);
                });
            }
        }

        bool hasWork(size_t index) const {
            for (size_t i = 0; i < m_workers.size(); ++i) {

                if (!m_workers[i]->deque.isEmpty()) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Acquires a counter from the pool and initializes it.
         *
         * @param initialValue The initial value for the counter (number of jobs in the batch)
         * @return The JobHandle (counter index) for this batch
         */
        JobHandle acquireCounter(uint32_t initialValue) {
            //! Potential Bug: if more than MAX_COUNTERS are allocated without being freed,
            //! counters will be reused leading to incorrect behavior.
            uint32_t index = m_counterAllocIdx.fetch_add(1, std::memory_order_relaxed) % m_counterPool.maxCounters();

            // Initialize the counter with the number of jobs
            // Release ordering ensures this write is visible before jobs start executing
            m_counterPool[index].value.store(initialValue, std::memory_order_release);

            return index;
        }

    private:
        // Pool of atomic counters for tracking job completion
        CounterPool m_counterPool{};

        // Atomic counter for allocating counter indices (circular allocation)
        std::atomic<uint32_t> m_counterAllocIdx{0};

        std::vector<Unique<Worker>> m_workers;

        // Atomic counter for round-robin job distribution across workers
        std::atomic<size_t> m_nextWorker{0};

        // Flag to signal all workers to stop
        std::atomic<bool> m_stop{false};

        // Mutex for condition variable (used for worker sleeping/waking)
        std::mutex m_mutex;

        // Condition variable for waking sleeping workers when work becomes available
        std::condition_variable m_condition;

        // Thread-local variable storing the current worker's index
        // Allows each thread to know which worker it represents without passing parameters
        static inline thread_local size_t t_workerIndex = 0;

        // Thread-local random number generator for randomizing steal attempts
        // Reduces contention by avoiding predictable steal patterns
        static inline thread_local std::mt19937 t_rng{std::random_device{}()};
    };

} // namespace pxt::core
