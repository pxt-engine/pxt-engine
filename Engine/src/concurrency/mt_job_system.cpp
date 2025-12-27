#include "concurrency/mt_job_system.hpp"

#include "concurrency/cpu_relax.hpp"

namespace pxt::concurrency {

    MultiThreadedJobSystem::MultiThreadedJobSystem(size_t threadCount) {
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

    MultiThreadedJobSystem::~MultiThreadedJobSystem() {
        m_stop.store(true, std::memory_order_release);
        m_condition.notify_all();
    }

    void MultiThreadedJobSystem::wait(const JobHandle handle) {
        if (!handle.isValid() || handle.index() >= m_jobRegistry.maxSlots()) {
            return;
        }

        auto& slot = m_jobRegistry[handle.index()];

        // Check generation: if mismatch, this handle is stale and job is already done
        uint32_t currentGen = slot.generation.load(std::memory_order_relaxed);
        if (currentGen != handle.generation()) {
            // Generation mismatch: the job has already completed in a previous cycle
            return;
        }

        // Busy-wait with helping: actively execute jobs while waiting
        while (slot.value.load(std::memory_order_acquire) > 0) {
            // Double-check generation hasn't changed (counter recycled mid-wait)
            if (slot.generation.load(std::memory_order_relaxed) != handle.generation()) {
                return;
            }

            if (!executeOneJob(t_workerIndex)) {
                // No work available, yield to avoid burning CPU cycles
                std::this_thread::yield();
            }
        }

        // One final acquire fence to ensure we see all side effects
        // from the completed job(s) before returning to the caller
        std::atomic_thread_fence(std::memory_order_acquire);
    }

    void MultiThreadedJobSystem::pushJob(Job&& job) {
        size_t idx = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

        m_workers[idx]->deque.push(std::move(job));

        // Increment pending job counter
        // Use relaxed ordering - this is just a heuristic, exact ordering not critical
        m_pendingJobCount.fetch_add(1, std::memory_order_relaxed);

        m_condition.notify_one();
    }

    bool MultiThreadedJobSystem::executeOneJob(size_t index) {
        Job job;

        // Try to take work from our own deque (LIFO)
        // This provides good cache locality as we work on recently added tasks
        bool foundWork = m_workers[index]->deque.pop(job);

        if (foundWork && job.isValid() && job.isReady()) {
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

            foundWork = m_workers[target]->deque.steal(job);

            if (foundWork) {
                break;
            }
        }

        if (foundWork && job.isValid() && job.isReady()) {
            process(job);

            return true;
        }

        // We haven't found work or the Job was invalid or pending - we still consumed it
        // from the deque but didn't process it, so the counter decrement was correct
        return false;
    }

    void MultiThreadedJobSystem::process(Job& job) {
        if (!job.isValid() || !job.isReady()) {
            return;
        }

        // Decrement counter - we consumed a job from a deque
        // Use relaxed ordering - this is just a heuristic
        m_pendingJobCount.fetch_sub(1, std::memory_order_relaxed);

        // Execute the job
        job.execute();

        auto& slot = m_jobRegistry[job.slotIndex];

        //? Use std::memory_order_acq_rel ordering for the final decrement
        //? - Release: Ensures all writes from job.execute() are visible to threads that observe remaining=0
        //? - Acquire: Ensures we see all writes from other jobs in this batch
        // Decrement counter and check if this was the last job
        uint32_t remaining = slot.value.fetch_sub(1, std::memory_order_acq_rel) - 1;

        if (remaining == 0) {
            // Job batch complete - check for dependent jobs
            std::vector<Job> readyJobs;

            { // Scoped block for locking
                SpinLockGuard lock(slot.dependentsLock);

                // Process all jobs that were waiting (Pending state)
                for (uint32_t dependentIdx : slot.dependents) {
                    auto& dependentSlot = m_jobRegistry[dependentIdx];
                    auto& dependentColdData = m_jobRegistry.getColdDataAt(dependentIdx);

                    //? Use std::memory_order_acq_rel ordering for the final decrement
                    //? - Release: Our completion is visible to the dependent
                    //? - Acquire: We see all writes from other dependencies of this job
                    uint32_t unresolvedRemaining =
                        dependentColdData.pendingInfo.unresolvedDepsCount.fetch_sub(1, std::memory_order_acq_rel) - 1;

                    if (unresolvedRemaining == 0) {
                        // All dependencies resolved, transition to Ready
                        dependentColdData.job.state = JobState::Ready;
                        readyJobs.push_back(std::move(dependentColdData.job));
                    }
                }

                slot.dependents.clear();
            } // End of scoped block for locking

            // Schedule all newly ready jobs
            for (auto& ready : readyJobs) {
                pushJob(std::move(ready));
            }

            // Increment generation with release semantics to make it visible to wait()
            // This invalidates handles that were created during this generation
            slot.generation.fetch_add(1, std::memory_order_release);
        }
    }

    void MultiThreadedJobSystem::workerLoop(size_t index, std::stop_token st) {
        while (!st.stop_requested() && !m_stop.load(std::memory_order_relaxed)) {

            // Active Execution Phase: Try to execute one job
            if (executeOneJob(index)) {
                continue; // Work found, keep going immediately
            }

            // Spinning Phase: staged spin-wait
            // First spin aggressively for bursty workloads,
            // then progressively reduce polling frequency before sleeping.
            constexpr uint32_t FAST_SPIN_ITERATIONS = 100;
            constexpr uint32_t SLOW_SPIN_ITERATIONS = 200;
            constexpr uint32_t SPINS_PER_BACKOFF_STEP = 50;

            // Small deterministic jitter to avoid lockstep spinning
            // When multiple threads enter this phase simultaneously,
            // this helps spread out their checks slightly
            const uint32_t jitter = (index * 13) & 7;

            bool foundWork = false;

            // Fast spinning: check frequently (every iteration)
            for (int spin = 0; spin < FAST_SPIN_ITERATIONS + jitter; ++spin) {
                if (hasWork(index)) {
                    foundWork = true;
                    break;
                }
                cpuRelax();
            }

            if (foundWork) {
                continue;
            }

            // Slow spinning: exponential pause increase
            // Check less frequently, pause longer between checks
            for (int spin = 0; spin < SLOW_SPIN_ITERATIONS; ++spin) {
                if (hasWork(index)) {
                    foundWork = true;
                    break;
                }

                const uint32_t backoffStep = spin / SPINS_PER_BACKOFF_STEP;
                const uint32_t pauseCount = 1 << backoffStep;

                // Progressive backoff: pause longer as we spin more
                // This reduces CPU usage while maintaining some responsiveness
                for (int pause = 0; pause < pauseCount; ++pause) {
                    cpuRelax();
                }
            }

            if (foundWork) {
                continue;
            }

            // Sleeping Phase:
            // No work found after spinning, sleep until notified
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this, index, &st] {
                return m_stop.load(std::memory_order_relaxed) || st.stop_requested() || hasWork(index);
            });
        }
    }

    bool MultiThreadedJobSystem::hasWork(size_t index) const {
        // If counter says no work, we can skip expensive deque checks
        // Use relaxed - we don't need strict ordering for this heuristic
        if (m_pendingJobCount.load(std::memory_order_relaxed) > 0) {
            return true;
        }

        // Counter says no work, but it might be slightly stale due to races
        // Do a quick verification by checking our own deque
        if (!m_workers[index]->deque.isProbablyEmpty()) {
            return true;
        }

        return false;
    }

    void MultiThreadedJobSystem::notifyWorkers(size_t jobCount) {
        if (jobCount == 0) {
            return;
        }

        // For a single job, wake one worker
        if (jobCount == 1) {
            m_condition.notify_one();
            return;
        }

        // For batches, wake min(jobs, workers) workers
        // No point waking more workers than we have jobs
        // Also no point waking more workers than we have threads
        size_t workersToWake = std::min(jobCount, m_workers.size());

        // Cap at a reasonable maximum to avoid notification overhead
        // Even for huge batches, waking ~half the workers is usually sufficient
        // due to work stealing - awake workers will wake others if needed
        constexpr size_t MAX_INITIAL_WAKEUPS = 8;
        workersToWake = std::min(workersToWake, MAX_INITIAL_WAKEUPS);

        // Notify the calculated number of workers
        // Each notify_one wakes a single sleeping thread
        // This loop is preferred over notify_all to avoid waking too many threads
        for (size_t i = 0; i < workersToWake; ++i) {
            m_condition.notify_one();
        }
    }

    JobHandle MultiThreadedJobSystem::acquireSlot(uint32_t jobsCount) {
        // Circular allocation of slot indices
        uint32_t index = m_counterAllocIdx.fetch_add(1, std::memory_order_relaxed) % m_jobRegistry.maxSlots();

        auto& slot = m_jobRegistry[index];

        // Read current generation before initializing counter
        // This generation will be incremented when the counter reaches zero
        uint32_t generation = slot.generation.load(std::memory_order_relaxed);

        // Initialize the counter with the number of jobs
        slot.value.store(jobsCount, std::memory_order_release);

        const bool isBatch = jobsCount > 1;

        return JobHandle::make(index, generation, isBatch);
    }

} // namespace pxt::concurrency