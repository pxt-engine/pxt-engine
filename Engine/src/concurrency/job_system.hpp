#pragma once

#include "core/pch.hpp"

#include "concurrency/job.hpp"
#include "core/containers/fixed_vector.hpp"

namespace pxt::concurrency {

    static constexpr size_t MAX_JOB_DEPENDENCIES = 9;

    template <typename F>
    concept RangeCallable = std::invocable<F, size_t, size_t>;

    struct JobParallelForFunction {
        void (*invoke)(const void*, size_t, size_t) = nullptr;
        alignas(std::max_align_t) std::byte buffer[24];

        JobParallelForFunction() = default;

        template <typename Func>
        requires(!std::is_same_v<std::decay_t<Func>, JobParallelForFunction> && RangeCallable<Func>)
        JobParallelForFunction(Func&& f) {
            using DecayedFunc = std::decay_t<Func>;

            static_assert(sizeof(DecayedFunc) <= sizeof(buffer), "Lambda too large for buffer");
            static_assert(std::is_trivially_copyable_v<DecayedFunc>, "Captures must be trivially copyable");

            // Construct the lambda into the buffer
            new (buffer) DecayedFunc(std::forward<Func>(f));

            // Map the invoker
            invoke = [](const void* ptr, size_t start, size_t end) {
                (*reinterpret_cast<const DecayedFunc*>(ptr))(start, end);
            };
        }

        void operator()(size_t start, size_t end) const {
            if (invoke) {
                invoke(buffer, start, end);
            }
        }

        explicit operator bool() const { return invoke != nullptr; }
    };

    struct JobDescription {
        JobFunction function;
        JobPriority priority = JobPriority::Normal;
        core::FixedVector<JobHandle, MAX_JOB_DEPENDENCIES> dependencies;
    };

    struct JobBatchDescription {
        std::vector<JobFunction> functions;
        JobPriority priority = JobPriority::Normal;
        core::FixedVector<JobHandle, MAX_JOB_DEPENDENCIES> dependencies;
    };

    struct JobParallelForDescription {
        size_t start = 0;
        size_t end = 0;
        size_t grainSize = 1;
        JobParallelForFunction function;
        JobPriority priority = JobPriority::Normal;
        core::FixedVector<JobHandle, MAX_JOB_DEPENDENCIES> dependencies;
    };

    class JobSystemBackend {
    public:
        virtual ~JobSystemBackend() = default;
        virtual JobHandle submit(const JobDescription& desc) = 0;
        virtual JobHandle submit(const JobBatchDescription& desc) = 0;
        virtual JobHandle parallelFor(const JobParallelForDescription& desc) = 0;
        virtual void wait(const JobHandle handle) = 0;
    };

    class JobSystem {
    public:
        static void initialize(JobSystemBackend* backendImpl) {
            PXT_ASSERT(s_backend == nullptr);

            s_backend = backendImpl;
        }

        static JobHandle submit(const JobDescription& desc) {
            PXT_ASSERT(s_backend && "JobSystem not initialized");

            return s_backend->submit(desc);
        }

        static JobHandle submit(const JobBatchDescription& desc) {
            PXT_ASSERT(s_backend && "JobSystem not initialized");

            return s_backend->submit(desc);
        }

        static JobHandle parallelFor(const JobParallelForDescription& desc) {
            PXT_ASSERT(s_backend && "JobSystem not initialized");

            return s_backend->parallelFor(desc);
        }

        static void wait(const JobHandle handle) {
            PXT_ASSERT(s_backend && "JobSystem not initialized");

            s_backend->wait(handle);
        }

        static void wait(std::span<const JobHandle> handles) {
            for (const auto& handle : handles) {
                wait(handle);
            }
        }

        static void wait(std::initializer_list<JobHandle> handles) {
            wait(std::span<const JobHandle>(handles.begin(), handles.end()));
        }

    private:
        static inline JobSystemBackend* s_backend = nullptr;
    };

} // namespace pxt::concurrency