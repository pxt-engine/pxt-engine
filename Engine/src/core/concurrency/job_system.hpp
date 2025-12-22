#pragma once

#include "core/pch.hpp"

#include "core/concurrency/job.hpp"
#include "core/concurrency/work_stealing_deque.hpp"

namespace pxt::core {

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
        // Accessed by every Job
        std::atomic<uint32_t> value{0};      //< Current job count
        std::atomic<uint32_t> generation{0}; //< Generation number (incremented on recycle)

        Job job; //< The job that uses this slot

        // Used only for Jobs with dependencies
        PendingJobInfo pendingInfo;

        // Graph data
        std::vector<uint32_t> dependents; //< Jobs that depends on the job in this slot
        std::mutex dependentsMutex;       //< Mutex for protecting dependents list
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

        size_t maxSlots() const { return MAX_SLOTS; }

    private:
        static constexpr size_t MAX_SLOTS = 4096;
        std::array<JobSlot, MAX_SLOTS> m_slots{};
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
        explicit JobSystem(size_t threadCount = std::thread::hardware_concurrency());

        ~JobSystem();

        /**
         * @brief Submits a single job for execution.
         *
         * The job is pushed to a worker's deque in round-robin fashion and
         * one worker is notified to wake up and process it.
         *
         * @tparam Func Any void-returning callable type (lambda, function, functor)
         * @param fn The function to execute
         *
         * @return A JobHandle that can be used to wait for completion
         *
         * @note The data pointer must remain valid until the job completes.
         *
         * Usage examples:
         * \code{.cpp}
         * // Lambda with captures
         * int x = 42;
         * auto h = js.submit([x]() { std::cout << x << std::endl; });
         *
         * // Lambda with mutable references
         * auto h = js.submit([&x]() { x *= 2; });
         *
         * // Function pointer
         * auto h = js.submit(myFunction);
         *
         * // Functor
         * auto h = js.submit(MyFunctor{args});
         * \endcode
         */
        template <VoidCallable Func>
        JobHandle submit(Func&& func) {
            JobHandle handle = acquireSlot(1);

            auto& slot = m_jobRegistry[handle.index];

            slot.job = Job::create(std::forward<Func>(func), handle.index, JobState::Ready);

            pushJob(std::move(slot.job));

            return handle;
        }

        template <VoidCallable Func>
        JobHandle submitWithDependencies(Func&& func, std::vector<JobHandle> dependencies) {
            // Remove invalid dependencies
            dependencies.erase(std::remove_if(dependencies.begin(), dependencies.end(),
                                              [](const JobHandle& h) { return !h.isValid(); }),
                               dependencies.end());

            // If there are no dependencies, submit as a normal job
            if (dependencies.empty()) {
                return submit(std::forward<Func>(func));
            }

            JobHandle handle = acquireSlot(1);

            auto& slot = m_jobRegistry[handle.index];
            slot.pendingInfo.unresolvedDepsCount.store(static_cast<uint32_t>(dependencies.size()),
                                                       std::memory_order_release);

            // Create a pending job with dependencies
            slot.job = Job::create(std::forward<Func>(func), handle.index, JobState::Pending);

            // Register this job as a dependent on each dependency's counter
            for (const auto& dep : dependencies) {
                if (!dep.isValid() || dep.index >= m_jobRegistry.maxSlots()) {
                    continue;
                }

                auto& depSlot = m_jobRegistry[dep.index];

                std::lock_guard lock(depSlot.dependentsMutex);

                // Check if dependency already completed
                if (depSlot.generation.load(std::memory_order_relaxed) != dep.generation ||
                    depSlot.value.load(std::memory_order_acquire) == 0) {

                    // Already done, decrement unresolved count
                    uint32_t remaining =
                        slot.pendingInfo.unresolvedDepsCount.fetch_sub(1, std::memory_order_acq_rel) - 1;

                    if (remaining == 0) {
                        // All dependencies resolved immediately
                        slot.job.state = JobState::Ready;
                        pushJob(std::move(slot.job));

                        return handle;
                    }

                    continue;
                }

                // Dependency still pending, register
                depSlot.dependents.push_back(slot.job.slotIndex);
            }

            return handle;
        }

        template <VoidCallable Func>
        JobHandle submitContinuation(JobHandle parent, Func&& func) {
            return submitWithDependencies(std::forward<Func>(func), {parent});
        }

        /**
         * @brief Submits a batch of jobs, one per item in the container.
         *
         * @tparam Container Any sized, iterable container (vector, array, span, deque, etc.)
         * @tparam Func Callable that accepts a reference to the container's element type
         * @param items Container of items to process
         * @param func Function to call for each item
         * @return A JobHandle representing the entire batch
         *
         * The concepts ensure:
         * - Container is iterable and has a size() method
         * - Container is empty-checkable
         * - Func can be called with the container's value type
         *
         * Usage examples:
         * \code{.cpp}
         * std::vector<int> data = {1, 2, 3, 4, 5};
         * std::array<float, 10> floats = {...};
         * std::deque<MyStruct> structs = {...};
         *
         * // Process each element
         * auto h1 = js.submitBatch(data, [](int& x) { x *= 2; });
         *
         * // With captures
         * int multiplier = 3;
         * auto h2 = js.submitBatch(floats, [multiplier](float& x) { x *= multiplier; });
         *
         * // Complex processing
         * auto h3 = js.submitBatch(structs, [](MyStruct& s) { s.process(); });
         * \endcode
         */
        template <SizedContainer Container, typename Func>
        requires EmptyCheckable<Container> && CallableWith<Func, std::ranges::range_reference_t<Container>>
        JobHandle submitBatch(Container& items, Func&& func) {
            if (items.empty())
                return InvalidJobHandle;

            JobHandle handle = acquireSlot(static_cast<uint32_t>(items.size()));

            size_t workerIdx = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

            for (auto& item : items) {
                // Create a job that calls func with the item
                // We capture by reference since the item must outlive the job
                m_workers[workerIdx]->deque.push({[&item, func]() { func(item); }, handle.index});
                workerIdx = (workerIdx + 1) % m_workers.size();
            }

            m_condition.notify_all();
            return handle;
        }

        /**
         * @brief Submits a batch where each element is a callable.
         *
         * @tparam Container Container of void-returning callables
         * @param callables Container of functions to execute
         * @return A JobHandle representing the entire batch
         *
         * The CallableContainer concept ensures each element can be invoked with no arguments.
         *
         * Usage examples:
         * \code{.cpp}
         * std::vector<std::function<void()>> tasks;
         * tasks.push_back([]() { doWork1(); });
         * tasks.push_back([]() { doWork2(); });
         * tasks.push_back([]() { doWork3(); });
         *
         * auto h = js.submitBatchCallables(tasks);
         * js.wait(h);
         *
         * // Also works with arrays
         * std::array<std::function<void()>, 3> moreTasks = {
         *     []() { taskA(); },
         *     []() { taskB(); },
         *     []() { taskC(); }
         * };
         * auto h2 = js.submitBatchCallables(moreTasks);
         * \endcode
         */
        template <CallableContainer Container>
        requires SizedContainer<Container> && EmptyCheckable<Container>
        JobHandle submitBatchCallables(Container& callables) {
            if (callables.empty())
                return InvalidJobHandle;

            JobHandle handle = acquireSlot(static_cast<uint32_t>(callables.size()));

            size_t workerIdx = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

            for (auto& callable : callables) {
                m_workers[workerIdx]->deque.push({callable, handle.index});
                workerIdx = (workerIdx + 1) % m_workers.size();
            }

            m_condition.notify_all();
            return handle;
        }

        /**
         * @brief Submits a parallel for loop.
         *
         * @tparam Func Callable that takes a size_t index
         * @param start Starting index (inclusive)
         * @param end Ending index (exclusive)
         * @param func Function to call with each index
         * @return A JobHandle for the entire loop
         *
         * The IndexCallable concept ensures func accepts a size_t and returns void.
         *
         * Usage examples:
         * @code
         * std::vector<int> data(1000);
         *
         * // Process indices 0-999 in parallel
         * auto h = js.parallelFor(0, 1000, [&data](size_t i) {
         *     data[i] = i * i;
         * });
         *
         * // With range
         * auto h2 = js.parallelFor(10, 50, [](size_t i) {
         *     std::cout << "Processing " << i << std::endl;
         * });
         *
         * js.wait(h);
         * @endcode
         */
        template <IndexCallable Func>
        JobHandle parallelFor(size_t start, size_t end, Func&& func) {
            if (start >= end)
                return InvalidJobHandle;

            size_t count = end - start;
            JobHandle handle = acquireSlot(static_cast<uint32_t>(count));

            size_t workerIdx = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

            for (size_t i = start; i < end; ++i) {
                // Capture i by value to ensure each job has correct index
                m_workers[workerIdx]->deque.push({[i, func]() { func(i); }, handle.index});
                workerIdx = (workerIdx + 1) % m_workers.size();
            }

            m_condition.notify_all();
            return handle;
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
        void wait(JobHandle handle);

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

        bool hasWork(size_t index) const;

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
