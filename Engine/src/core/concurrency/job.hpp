#include "core/pch.hpp"

namespace pxt::core {

    struct JobFunction {
        void (*invoke)(const void*) = nullptr;
        alignas(std::max_align_t) std::byte buffer[48];
    };

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
        JobFunction function;             //< Function to execute
        JobState state = JobState::Ready; //< Current state of the job
        uint32_t counterIndex = 0;        //< Index into the counter pool for tracking completion

        template <typename Func>
        static Job create(Func&& f, uint32_t cIdx, JobState state = JobState::Ready) {
            Job job;
            job.counterIndex = cIdx;
            job.state = state;

            using DecayedFunc = std::decay_t<Func>;

            PXT_STATIC_ASSERT(sizeof(DecayedFunc) <= sizeof(job.function.buffer), "Lambda too large");
            PXT_STATIC_ASSERT(std::is_trivially_copyable_v<DecayedFunc>, "Lambda must be trivially copyable");

            new (job.function.buffer) DecayedFunc(std::forward<Func>(f));
            job.function.invoke = [](const void* ptr) { (*reinterpret_cast<const DecayedFunc*>(ptr))(); };

            return job;
        }

        void execute() { function.invoke(function.buffer); }

        /**
         * @brief Checks if this job is valid and ready to execute.
         * @return true if the function is non-null
         */
        bool isValid() const { return function.invoke != nullptr; }

        /**
         * @brief Checks if this job is in the Ready state.
         * @return true if state is Ready
         */
        bool isReady() const { return state == JobState::Ready; }

        /**
         * @brief Checks if this job is in the Pending state.
         * @return true if state is Pending
         */
        bool isPending() const { return state == JobState::Pending; }
    };

    struct PendingJobInfo {
        std::atomic<uint32_t> unresolvedDepsCount{0}; //< Count of unfinished dependencies
    };

} // namespace pxt::core