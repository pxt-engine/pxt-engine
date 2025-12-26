#pragma once

#include "core/pch.hpp"

#include "concurrency/job.hpp"
#include "concurrency/job_system.hpp"
#include "concurrency/spin_lock_guard.hpp"
#include "concurrency/work_stealing_deque.hpp"
#include "core/containers/fixed_vector.hpp"

namespace pxt::concurrency {

    template <typename F>
    concept VoidCallable = std::invocable<F> && std::same_as<std::invoke_result_t<F>, void>;

    template <typename F, typename T>
    concept CallableWith = std::invocable<F, T> && std::same_as<std::invoke_result_t<F, T>, void>;

    template <typename F>
    concept IndexCallable = std::invocable<F, size_t> && std::same_as<std::invoke_result_t<F, size_t>, void>;

    template <typename C>
    concept IterableContainer = std::ranges::range<C> && requires(C c) {
        { c.begin() } -> std::input_or_output_iterator;
        { c.end() } -> std::sentinel_for<decltype(c.begin())>;
    };

    template <typename C>
    concept SizedContainer = IterableContainer<C> && requires(C c) {
        { c.size() } -> std::convertible_to<size_t>;
    };

    template <typename C>
    concept EmptyCheckable = requires(C c) {
        { c.empty() } -> std::convertible_to<bool>;
    };

    template <typename C>
    concept CallableContainer = IterableContainer<C> && VoidCallable<std::ranges::range_value_t<C>>;

    /**
     * @brief Padded atomic counter with generation tracking to avoid false sharing between threads.
     *
     * Each counter includes a generation number that increments each time
     * the counter is recycled. This prevents stale handles from accidentally
     * waiting on new work that happens to use the same counter index.
     *
     * False sharing prevention: aligned to cache line size to ensure each counter
     * resides in its own cache line.
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
    struct alignas(std::hardware_destructive_interference_size) JobSlot {
        // -- Synchronization (8 bytes) --
        std::atomic<uint32_t> value{0};      //< Current job count (4 bytes)
        std::atomic<uint32_t> generation{0}; //< Generation number, incremented on recycle (4 bytes)

        // -- Cold data index (4 bytes) --
        uint32_t coldDataIndex; //< Index into cold data array (4 bytes)

        // -- Dependency management (52 bytes) --

        /**
         * @brief Lock for protecting dependents list.
         *
         * A Lock is needed because multiple jobs might finish and try to
         * update the dependents list concurrently. This atomic_flag provides
         * a lightweight spinlock to ensure thread-safe access to the dependents list.
         */
        std::atomic_flag dependentsLock = ATOMIC_FLAG_INIT; //< Lock for protecting dependents list (1-4 byte)

        /**
         * @brief A fixed-size vector that holds up to 11 unsigned 32-bit integers.
         *
         * This structure is designed to store a small number of dependent job indices
         * efficiently, minimizing dynamic memory allocations. It uses a static array
         * to hold the elements and keeps track of the current size.
         *
         * Memory Layout:
         * * data: Array of MAX_JOB_DEPENDENCIES uint32_t elements (44 bytes)
         * * size: Current number of elements in the vector (1-4 bytes)
         * * Total Size: 48 bytes
         */
        core::FixedVector<uint32_t, MAX_JOB_DEPENDENCIES>
            dependents; //< Jobs that depends on the job in this slot (48 bytes)

        // 8 + 4 + 52 = 64 bytes total (cache line size)
        // With max 64 bytes in total the JobSlot fits perfectly into one cache line
        // preventing false sharing between threads accessing different slots.
    };

    struct PendingJobInfo {
        std::atomic<uint32_t> unresolvedDepsCount{0}; //< Count of unfinished dependencies
    };

    struct JobSlotColdData {
        // The job that uses this slot
        Job job;

        // Used only for Jobs with dependencies
        PendingJobInfo pendingInfo;
    };

    /**
     * @brief JobRegistry manages a pool of job slots with atomic counters and generation tracking.
     *
     * Generation Tracking:
     * - Each counter has a generation number that starts at 0
     * - When a counter reaches zero and is about to be recycled, generation increments
     * - Handles store both index and generation
     * - wait() validates that the handle's generation matches before waiting
     *
     * This prevents the ABA problem:
     * 1. Thread A gets handle {index: 5, generation: 0} for a job
     * 2. Job completes, counter at index 5 is recycled with generation 1
     * 3. New job allocated to index 5 with generation 1
     * 4. Thread A calls wait() with old handle {index: 5, generation: 0}
     * 5. Generation mismatch detected -> wait returns immediately (job already done)
     *
     * Memory Layout:
     * - Fixed-size array of MAX_SLOTS=4096 padded counters
     * - Each counter is cache-line aligned (64 bytes)
     * - Each counter includes both value and generation
     * - Total size: 4096 * 64 = 256 KB
     */
    struct JobRegistry {
        /**
         * @brief Accesses a counter by index.
         * @param index The counter index (must be < MAX_SLOTS)
         * @return Reference to the padded atomic counter
         */
        JobSlot& operator[](size_t index) { return m_slots[index]; }

        const JobSlot& operator[](size_t index) const { return m_slots[index]; }

        JobSlotColdData& getColdDataAt(size_t index) { return m_coldData[index]; }

        size_t maxSlots() const { return MAX_SLOTS; }

    private:
        static constexpr size_t MAX_SLOTS = 4096;
        std::array<JobSlot, MAX_SLOTS> m_slots{};
        std::array<JobSlotColdData, MAX_SLOTS> m_coldData{};
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
    class MultiThreadedJobSystem final : public JobSystemBackend {
    public:
        /**
         * @brief Constructs a JobSystem with the specified number of worker threads.
         *
         * @param threadCount Number of worker threads to create.
         *                    Defaults to std::thread::hardware_concurrency() (number of logical cores).
         */
        explicit MultiThreadedJobSystem(size_t threadCount = std::thread::hardware_concurrency());

        ~MultiThreadedJobSystem() override;

        JobHandle submit(const JobDescription& desc) override {
            JobHandle handle = acquireSlot(1);
            auto& slotColdData = m_jobRegistry.getColdDataAt(handle.index);

            slotColdData.job.function = desc.function;
            slotColdData.job.priority = desc.priority;
            slotColdData.job.slotIndex = handle.index;

            if (desc.dependencies.size() == 0) {
                slotColdData.job.state = JobState::Ready;

                pushJob(std::move(slotColdData.job));
            } else {
                linkDependencies(handle, std::move(desc.dependencies));
            }

            return handle;
        }

        void linkDependencies(JobHandle handle, core::FixedVector<JobHandle, MAX_JOB_DEPENDENCIES> deps) {
            // Remove invalid dependencies
            deps.erase(std::remove_if(deps.begin(), deps.end(), [](const JobHandle& h) { return !h.isValid(); }),
                       deps.end());

            auto& coldData = m_jobRegistry.getColdDataAt(handle.index);

            // If there are no dependencies, submit as a normal job
            if (deps.empty()) {
                coldData.job.state = JobState::Ready;

                pushJob(std::move(coldData.job));
            }

            coldData.job.state = JobState::Pending;
            coldData.pendingInfo.unresolvedDepsCount.store(static_cast<uint32_t>(deps.size()),
                                                           std::memory_order_release);

            //? Initialize unresolvedDepsCount BEFORE registering with any dependency.
            //? We start with the total count and will adjust downward for already-completed deps
            //? This prevents the race where a dependency completes and tries to decrement
            //? before we've initialized the counter
            coldData.pendingInfo.unresolvedDepsCount.store(static_cast<uint32_t>(deps.size()),
                                                           std::memory_order_release);

            uint32_t completedDeps = 0;

            //? Check completion status BEFORE registering as dependent
            //? This prevents the race where:
            //? 1. We register as dependent
            //? 2. Dependency completes and scans dependents (we're there but shouldn't be)
            //? 3. We check and see it's complete, try to remove ourselves (already processed)
            for (const auto& dep : deps) {
                // Skip invalid dependencies
                if (!dep.isValid() || dep.index >= m_jobRegistry.maxSlots()) {
                    ++completedDeps;
                    continue;
                }

                auto& depSlot = m_jobRegistry[dep.index];

                //? Check if dependency is already completed before registering
                //? Use acquire to ensure we see all writes from the completing job
                uint32_t depGen = depSlot.generation.load(std::memory_order_acquire);
                uint32_t depVal = depSlot.value.load(std::memory_order_acquire);

                const bool isAlreadyCompleted = (depGen != dep.generation) || (depVal == 0);

                if (isAlreadyCompleted) {
                    // Already completed - don't register at all
                    ++completedDeps;
                    continue;
                }

                //? Register as dependent only if not completed
                { // Scoped block for locking
                    SpinLockGuard lock(depSlot.dependentsLock);

                    // Double-check completion status while holding lock
                    // The dependency could have completed between our check and acquiring the lock
                    depGen = depSlot.generation.load(std::memory_order_acquire);
                    depVal = depSlot.value.load(std::memory_order_acquire);

                    const bool completedWhileLocking = (depGen != dep.generation) || (depVal == 0);

                    if (completedWhileLocking) {
                        // Completed while we were acquiring the lock - don't register
                        ++completedDeps;
                    } else {
                        // Still not complete - safe to register now
                        // At this point, unresolvedDepsCount is already initialized,
                        // so if this dependency completes, it can safely decrement it
                        depSlot.dependents.push_back(coldData.job.slotIndex);
                    }
                } // End scoped block for locking
            }

            // Adjust for dependencies that were already completed
            // We initialized with dependencies.size(), now subtract the completed ones
            if (completedDeps > 0) {
                uint32_t remaining =
                    coldData.pendingInfo.unresolvedDepsCount.fetch_sub(completedDeps, std::memory_order_acq_rel) -
                    completedDeps;

                // If all dependencies were already completed, submit immediately as ready
                if (remaining == 0) {
                    coldData.job.state = JobState::Ready;
                    pushJob(std::move(coldData.job));
                }
            }
        }

        /**
         * @brief Waits for a job or batch of jobs to complete.
         *
         * Generation Validation:
         * Before waiting, this function validates that the handle's generation matches
         * the counter's current generation. If they don't match, the handle is stale
         * and the jobs have already completed (or the handle was never valid).
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
        void wait(const JobHandle handle) override;

    private:
        /**
         * @brief Pushes a job to a worker's deque and wakes a worker.
         *
         * @param job The job to push
         *
         * Selects a worker in round-robin fashion and pushes the job to its deque.
         * Then wakes one sleeping worker to process it.
         */
        void pushJob(Job&& job);

        /**
         * @brief Attempts to execute one job from the given worker's perspective.
         *
         * @param index The index of the worker attempting to execute a job
         * @return true if a job was executed, false if no work was available
         */
        bool executeOneJob(size_t index);

        /**
         * @brief Executes a job and updates its completion counter.
         *
         * When the counter reaches zero, the generation is incremented to invalidate
         * any stale handles that might reference this counter index.
         *
         * @param job The job to process
         */
        void process(Job& job);

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
        void workerLoop(size_t index, std::stop_token st);

        /**
         * @brief Heuristic check for available work
         *
         * @param index Worker index
         * @return True if there is probably work available
         */
        bool hasWork(size_t index) const;

        /**
         * @brief Notifies one or more workers that new jobs are available.
         *
         * @param jobCount The number of newly submitted jobs
         *
         * Wakes up sleeping workers to ensure timely processing of new jobs.
         * The number of workers notified may depend on the job count and current load.
         *
         * This prevents the thundering herd problem by only waking as many workers as needed.
         */
        void notifyWorkers(size_t jobCount);

        /**
         * @brief Acquires a slot from the registry and initializes it with generation tracking.
         *
         * The slot is initialized with the job count and the current generation number
         * is captured. The generation will be incremented when the counter reaches zero,
         * ensuring that any handles created now will detect completion via generation mismatch.
         *
         * @param initialValue The initial value for the counter (number of jobs in the batch)
         * @return A JobHandle containing both the counter index and generation number
         */
        JobHandle acquireSlot(uint32_t initialValue);

    private:
        JobRegistry m_jobRegistry{};

        // Atomic counter for allocating counter indices (circular allocation)
        std::atomic<uint32_t> m_counterAllocIdx{0};

        // Atomic counter for round-robin job distribution across workers
        std::atomic<size_t> m_nextWorker{0};

        // Global counter of pending jobs across all workers
        // This is a performance optimization to avoid scanning all worker deques
        // The counter is approximate - it may briefly be inaccurate, but that's fine
        // for a heuristic used in spin-waiting
        std::atomic<uint32_t> m_pendingJobCount{0};

        // Flag to signal all workers to stop
        std::atomic<bool> m_stop{false};

        // Mutex for condition variable (used for worker sleeping/waking)
        std::mutex m_mutex;

        // Condition variable for waking sleeping workers when work becomes available
        std::condition_variable m_condition;

        std::vector<Unique<Worker>> m_workers;

        // Thread-local variable storing the current worker's index
        // Allows each thread to know which worker it represents without passing parameters
        static inline thread_local size_t t_workerIndex = 0;

        // Thread-local random number generator for randomizing steal attempts
        // Reduces contention by avoiding predictable steal patterns
        static inline thread_local std::mt19937 t_rng{std::random_device{}()};
    };

} // namespace pxt::concurrency
