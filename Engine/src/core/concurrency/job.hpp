#include "core/pch.hpp"

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

} // namespace pxt::core