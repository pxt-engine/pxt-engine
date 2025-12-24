#include "concurrency/job_system.hpp"

#include "concurrency/cpu_relax.hpp"

namespace pxt::concurrency {

    JobSystem::JobSystem(size_t threadCount) {
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

    JobSystem::~JobSystem() {
        m_stop.store(true, std::memory_order_release);
        m_condition.notify_all();
    }

    void JobSystem::wait(JobHandle handle) {
        if (handle == InvalidJobHandle || handle.index >= m_jobRegistry.maxSlots()) {
            return;
        }

        auto& slot = m_jobRegistry[handle.index];

        // Check generation: if mismatch, this handle is stale and job is already done
        uint32_t currentGen = slot.generation.load(std::memory_order_relaxed);
        if (currentGen != handle.generation) {
            // Generation mismatch: the job has already completed in a previous cycle
            return;
        }

        // Busy-wait with helping: actively execute jobs while waiting
        while (slot.value.load(std::memory_order_acquire) > 0) {
            // Double-check generation hasn't changed (counter recycled mid-wait)
            if (slot.generation.load(std::memory_order_relaxed) != handle.generation) {
                return;
            }

            if (!executeOneJob(t_workerIndex)) {
                // No work available, yield to avoid burning CPU cycles
                std::this_thread::yield();
            }
        }
    }

    void JobSystem::pushJob(Job&& job) {
        size_t idx = m_nextWorker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

        m_workers[idx]->deque.push(std::move(job));

        m_condition.notify_one();
    }

    bool JobSystem::executeOneJob(size_t index) {
        Job job;

        // Try to take work from our own deque (LIFO)
        // This provides good cache locality as we work on recently added tasks
        bool isDequeEmpty = !m_workers[index]->deque.pop(job);

        if (isDequeEmpty) {
            return false;
        }

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

            isDequeEmpty = m_workers[target]->deque.steal(job);

            if (isDequeEmpty) {
                continue;
            }

            if (job.isValid() && job.isReady()) {
                process(job);
                return true;
            }
        }

        return false;
    }

    void JobSystem::process(Job& job) {
        if (!job.isValid() || !job.isReady()) {
            return;
        }

        // Execute the job
        job.execute();

        auto& slot = m_jobRegistry[job.slotIndex];

        // Decrement counter and check if this was the last job
        uint32_t remaining = slot.value.fetch_sub(1, std::memory_order_release) - 1;

        if (remaining == 0) {
            // Job batch complete - check for dependent jobs
            std::vector<Job> readyJobs;

            { // Scoped block for locking
                SpinLockGuard lock(slot.dependentsLock);

                // Process all jobs that were waiting (Pending state)
                for (uint32_t dependentIdx : slot.dependents) {
                    auto& dependentSlot = m_jobRegistry[dependentIdx];
                    auto& dependentColdData = m_jobRegistry.getColdDataAt(dependentIdx);

                    uint32_t unresolvedRemaining =
                        dependentColdData.pendingInfo.unresolvedDepsCount.fetch_sub(1, std::memory_order_relaxed) - 1;

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

            // Increment generation
            slot.generation.fetch_add(1, std::memory_order_release);
        }
    }

    void JobSystem::workerLoop(size_t index, std::stop_token st) {
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

    bool JobSystem::hasWork(size_t index) const {
        for (size_t i = 0; i < m_workers.size(); ++i) {

            if (!m_workers[i]->deque.isProbablyEmpty()) {
                return true;
            }
        }
        return false;
    }

    JobHandle JobSystem::acquireSlot(uint32_t initialValue) {
        // Circular allocation of slot indices
        uint32_t index = m_counterAllocIdx.fetch_add(1, std::memory_order_relaxed) % m_jobRegistry.maxSlots();

        auto& slot = m_jobRegistry[index];

        // Read current generation before initializing counter
        // This generation will be incremented when the counter reaches zero
        uint32_t generation = slot.generation.load(std::memory_order_relaxed);

        // Initialize the counter with the number of jobs
        slot.value.store(initialValue, std::memory_order_release);

        return JobHandle{index, generation};
    }

} // namespace pxt::concurrency