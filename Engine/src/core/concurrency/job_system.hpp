#pragma once

#include "core/pch.hpp"

#include "core/concurrency/cpu_relax.hpp"
#include "core/concurrency/work_stealing_deque.hpp"

#include <new>

namespace pxt::core {

    /**
     * @brief A type-erased callable with small function optimization.
     *
     * This class avoids heap allocations for small callables (lambdas, functors)
     * that fit within the inline buffer. Larger callables fall back to heap allocation.
     *
     * Small Function Optimization (SFO) Benefits:
     * - Zero heap allocations for most lambdas
     * - Better cache locality (data stored inline)
     * - Reduced memory fragmentation
     * - Faster job submission (no malloc/free overhead)
     *
     * Memory Layout:
     * - Inline buffer
     * - Stores small callables directly in this buffer
     * - Large callables stored on heap with pointer in buffer
     */
    class JobFunction {
    public:
        JobFunction() = default;

        /**
         * @brief Constructs from any callable object.
         *
         * Uses small function optimization: if the callable fits in the inline buffer,
         * it's stored directly. Otherwise, it's heap-allocated.
         */
        template <typename Func>
        requires(!std::is_same_v<std::decay_t<Func>, JobFunction>)
        JobFunction(Func&& func) {
            using DecayedFunc = std::decay_t<Func>;

            static_assert(std::is_invocable_r_v<void, DecayedFunc>, "Func must be callable with signature void()");

            constexpr bool fits_inline = sizeof(DecayedFunc) <= BUFFER_SIZE &&
                                         alignof(DecayedFunc) <= alignof(Storage) &&
                                         std::is_nothrow_move_constructible_v<DecayedFunc>;

            if constexpr (fits_inline) {
                // Small function optimization: construct in-place
                new (&m_storage) DecayedFunc(std::forward<Func>(func));

                m_invoke = [](const Storage& storage) { (*reinterpret_cast<const DecayedFunc*>(&storage))(); };

                m_destroy = [](Storage& storage) { reinterpret_cast<DecayedFunc*>(&storage)->~DecayedFunc(); };

                m_move = [](Storage& dst, Storage& src) {
                    new (&dst) DecayedFunc(std::move(*reinterpret_cast<DecayedFunc*>(&src)));
                };
            } else {
                // Large callable: heap allocate
                auto* ptr = new DecayedFunc(std::forward<Func>(func));
                *reinterpret_cast<DecayedFunc**>(&m_storage) = ptr;

                m_invoke = [](const Storage& storage) { (*(*reinterpret_cast<DecayedFunc* const*>(&storage)))(); };

                m_destroy = [](Storage& storage) { delete *reinterpret_cast<DecayedFunc**>(&storage); };

                m_move = [](Storage& dst, Storage& src) {
                    *reinterpret_cast<DecayedFunc**>(&dst) = *reinterpret_cast<DecayedFunc**>(&src);
                    *reinterpret_cast<DecayedFunc**>(&src) = nullptr;
                };
            }
        }

        // Move constructor
        JobFunction(JobFunction&& other) noexcept
            : m_invoke(other.m_invoke), m_destroy(other.m_destroy), m_move(other.m_move) {
            if (m_move) {
                m_move(m_storage, other.m_storage);
                other.m_invoke = nullptr;
                other.m_destroy = nullptr;
                other.m_move = nullptr;
            }
        }

        // Move assignment
        JobFunction& operator=(JobFunction&& other) noexcept {
            if (this != &other) {
                reset();

                m_invoke = other.m_invoke;
                m_destroy = other.m_destroy;
                m_move = other.m_move;

                if (m_move) {
                    m_move(m_storage, other.m_storage);
                    other.m_invoke = nullptr;
                    other.m_destroy = nullptr;
                    other.m_move = nullptr;
                }
            }
            return *this;
        }

        ~JobFunction() { reset(); }

        // Delete copy operations (jobs should be moved, not copied)
        JobFunction(const JobFunction&) = delete;
        JobFunction& operator=(const JobFunction&) = delete;

        /**
         * @brief Invokes the stored callable.
         */
        void operator()() const {
            if (m_invoke) {
                m_invoke(m_storage);
            }
        }

        /**
         * @brief Checks if this JobFunction contains a valid callable.
         */
        explicit operator bool() const noexcept { return m_invoke != nullptr; }

        /**
         * @brief Resets to empty state, destroying any stored callable.
         */
        void reset() noexcept {
            if (m_destroy) {
                m_destroy(m_storage);
            }
            m_invoke = nullptr;
            m_destroy = nullptr;
            m_move = nullptr;
        }

    private:
        // Size tuned for typical lambda captures
        // TODO: The workload can be profiled to find optimal size
        static constexpr size_t BUFFER_SIZE = 32;

        using Storage = std::aligned_storage_t<BUFFER_SIZE, alignof(std::max_align_t)>;

        // Inline storage for small callables
        mutable Storage m_storage{};

        // Type-erased function pointers
        void (*m_invoke)(const Storage&) = nullptr;
        void (*m_destroy)(Storage&) = nullptr;
        void (*m_move)(Storage&, Storage&) = nullptr;
    };

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
     * @brief A JobHandle is an identifier for a submitted job or batch of jobs.
     *
     * JobHandles use a generation counter to prevent the ABA problem where a counter
     * index is reused while an old handle still references it. Each time a counter
     * completes and is recycled, its generation is incremented.
     *
     * Structure:
     * - index: The counter pool index (0 to MAX_COUNTERS-1)
     * - generation: The generation number of this allocation
     *
     * A handle is valid only if both the index AND generation match the counter pool.
     */
    struct JobHandle {
        uint32_t index = 0xFFFFFFFF;
        uint32_t generation = 0;

        bool operator==(const JobHandle& other) const { return index == other.index && generation == other.generation; }

        bool operator!=(const JobHandle& other) const { return !(*this == other); }

        /**
         * @brief Checks if this handle is valid (not the invalid sentinel value).
         */
        bool isValid() const { return index != 0xFFFFFFFF; }
    };

    static constexpr JobHandle InvalidJobHandle = {0xFFFFFFFF, 0xFFFFFFFF};

    enum class JobState : uint8_t {
        Ready,   //< Job is ready to execute
        Pending, //< Job is pending execution (waiting for dependencies)
    };

    /**
     * @brief A Job represents a single unit of work to be executed by the JobSystem.
     */
    struct Job {
        JobFunction execute;              //< Function to execute
        JobState state = JobState::Ready; //< Current state of the job
        uint32_t counterIndex = 0;        //< Index into the counter pool for tracking completion

        // For Pending jobs with dependencies:
        std::vector<JobHandle> dependencies{}; //< Jobs that must complete before this job can run

        //? unresolvedDependenciesCount is non-atomic
        //? But it must be accessed under dependentsMutex (from CounterPool)
        uint32_t unresolvedDependenciesCount{0}; //< Count of unfinished dependencies

        // Default constructor for invalid jobs
        Job() = default;

        // Contructor for Ready jobs
        Job(JobFunction&& fn, uint32_t cIdx) : execute(std::move(fn)), counterIndex(cIdx), state(JobState::Ready) {}

        // Contructor for Pending jobs with dependencies
        Job(JobFunction&& fn, uint32_t cIdx, std::vector<JobHandle>&& deps)
            : execute(std::move(fn)), counterIndex(cIdx), state(JobState::Pending), dependencies(std::move(deps)),
              unresolvedDependenciesCount(static_cast<uint32_t>(dependencies.size())) {}

        /**
         * @brief Checks if this job is valid and ready to execute.
         * @return true if the function is non-null
         */
        bool isValid() const { return static_cast<bool>(execute); }

        bool isReady() const { return state == JobState::Ready; }

        bool isPending() const { return state == JobState::Pending; }
    };

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
    struct alignas(std::hardware_destructive_interference_size) PaddedAtomicCounter {
        std::atomic<uint32_t> value{0};      //< Current job count
        std::atomic<uint32_t> generation{0}; //< Generation number (incremented on recycle)

        std::vector<Shared<Job>> dependents; //< Jobs that depend on this counter
        std::mutex dependentsMutex;          //< Mutex for protecting dependents list
    };

    /**
     * @brief CounterPool manages a pool of atomic counters with generation tracking.
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
     * - Fixed-size array of MAX_COUNTERS=4096 padded counters
     * - Each counter is cache-line aligned (64 bytes)
     * - Each counter includes both value and generation
     * - Total size: 4096 * 64 = 256 KB
     */
    struct CounterPool {
        /**
         * @brief Accesses a counter by index.
         * @param index The counter index (must be < MAX_COUNTERS)
         * @return Reference to the padded atomic counter
         */
        PaddedAtomicCounter& operator[](size_t index) { return m_counters[index]; }

        const PaddedAtomicCounter& operator[](size_t index) const { return m_counters[index]; }

        size_t maxCounters() const { return MAX_COUNTERS; }

    private:
        static constexpr size_t MAX_COUNTERS = 4096;
        std::array<PaddedAtomicCounter, MAX_COUNTERS> m_counters{};
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
            JobHandle handle = acquireCounter(1);

            pushJob({std::forward<Func>(func), handle.index});

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

            JobHandle handle = acquireCounter(1);

            // Create a pending job with dependencies
            auto job = createShared<Job>(std::forward<Func>(func), handle.index, std::move(dependencies));

            // Register this job as a dependent on each dependency's counter
            for (const auto& dep : job->dependencies) {
                if (!dep.isValid() || dep.index >= m_counterPool.maxCounters()) {
                    continue;
                }

                auto& depCounter = m_counterPool[dep.index];

                std::lock_guard lock(depCounter.dependentsMutex);

                // Check if dependency already completed
                if (depCounter.generation.load(std::memory_order_relaxed) != dep.generation ||
                    depCounter.value.load(std::memory_order_acquire) == 0) {

                    // Already done, decrement unresolved count
                    uint32_t remaining = --job->unresolvedDependenciesCount;

                    if (remaining == 0) {
                        // All dependencies resolved immediately
                        job->state = JobState::Ready;
                        pushJob(std::move(*job));

                        return handle;
                    }

                    continue;
                }

                // Dependency still pending, register
                depCounter.dependents.push_back(job);
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

            JobHandle handle = acquireCounter(static_cast<uint32_t>(items.size()));

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

            JobHandle handle = acquireCounter(static_cast<uint32_t>(callables.size()));

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
            JobHandle handle = acquireCounter(static_cast<uint32_t>(count));

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
        void wait(JobHandle handle) {
            if (handle == InvalidJobHandle || handle.index >= m_counterPool.maxCounters()) {
                return;
            }

            auto& counter = m_counterPool[handle.index];

            // Check generation: if mismatch, this handle is stale and job is already done
            uint32_t currentGen = counter.generation.load(std::memory_order_relaxed);
            if (currentGen != handle.generation) {
                // Generation mismatch: the job has already completed in a previous cycle
                return;
            }

            // Busy-wait with helping: actively execute jobs while waiting
            while (counter.value.load(std::memory_order_acquire) > 0) {
                // Double-check generation hasn't changed (counter recycled mid-wait)
                if (counter.generation.load(std::memory_order_relaxed) != handle.generation) {
                    return;
                }

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
        void pushJob(Job&& job) {
            size_t idx = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

            m_workers[idx]->deque.push(std::move(job));

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

            if (job.isValid() && job.isReady()) {
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

                if (job.isValid() && job.isReady()) {
                    process(job);
                    return true;
                }
            }

            return false;
        }

        /**
         * @brief Executes a job and updates its completion counter.
         *
         * When the counter reaches zero, the generation is incremented to invalidate
         * any stale handles that might reference this counter index.
         *
         * @param job The job to process
         */
        void process(Job& job) {
            if (!job.isValid() || !job.isReady()) {
                return;
            }

            // Execute the job
            job.execute();

            auto& counter = m_counterPool[job.counterIndex];

            // Decrement counter and check if this was the last job
            uint32_t remaining = counter.value.fetch_sub(1, std::memory_order_release) - 1;

            if (remaining == 0) {
                // Job batch complete - check for dependent jobs
                std::vector<std::shared_ptr<Job>> readyJobs;

                {
                    std::lock_guard lock(counter.dependentsMutex);

                    // Process all jobs that were waiting (Pending state)
                    for (auto& dependent : counter.dependents) {
                        uint32_t unresolvedRemaining = --dependent->unresolvedDependenciesCount;

                        if (unresolvedRemaining == 0) {
                            // All dependencies resolved, transition to Ready
                            dependent->state = JobState::Ready;
                            readyJobs.push_back(std::move(dependent));
                        }
                    }

                    counter.dependents.clear();
                }

                // Schedule all newly ready jobs
                for (auto& ready : readyJobs) {
                    pushJob(std::move(*ready));
                }

                // Increment generation
                counter.generation.fetch_add(1, std::memory_order_release);
            }
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
         * @brief Acquires a counter from the pool and initializes it with generation tracking.
         *
         * The counter is initialized with the job count and the current generation number
         * is captured. The generation will be incremented when the counter reaches zero,
         * ensuring that any handles created now will detect completion via generation mismatch.
         *
         * @param initialValue The initial value for the counter (number of jobs in the batch)
         * @return A JobHandle containing both the counter index and generation number
         */
        JobHandle acquireCounter(uint32_t initialValue) {
            // Circular allocation of counter indices
            uint32_t index = m_counterAllocIdx.fetch_add(1, std::memory_order_relaxed) % m_counterPool.maxCounters();

            auto& counter = m_counterPool[index];

            // Read current generation before initializing counter
            // This generation will be incremented when the counter reaches zero
            uint32_t generation = counter.generation.load(std::memory_order_relaxed);

            // Initialize the counter with the number of jobs
            counter.value.store(initialValue, std::memory_order_release);

            return JobHandle{index, generation};
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
