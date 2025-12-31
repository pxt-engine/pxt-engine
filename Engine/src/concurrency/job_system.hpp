#pragma once

#include "core/pch.hpp"

#include "concurrency/job.hpp"
#include "core/containers/fixed_vector.hpp"

namespace pxt::concurrency {

    static constexpr size_t MAX_JOB_DEPENDENCIES = 9;

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
        virtual JobHandle submit(JobDescription desc) = 0;
        virtual JobHandle submit(JobBatchDescription desc) = 0;
        virtual JobHandle parallelFor(JobParallelForDescription desc) = 0;
        virtual void wait(const JobHandle handle) = 0;
    };

    class JobSystem {
    public:
        static void initialize(JobSystemBackend* backendImpl) {
            PXT_ASSERT(s_backend == nullptr);

            s_backend = backendImpl;
        }

        static JobHandle submit(JobDescription desc) {
            PXT_ASSERT(s_backend && "JobSystem not initialized");

            return s_backend->submit(std::move(desc));
        }

        static JobHandle submit(JobBatchDescription desc) {
            PXT_ASSERT(s_backend && "JobSystem not initialized");

            return s_backend->submit(std::move(desc));
        }

        static JobHandle parallelFor(JobParallelForDescription desc) {
            PXT_ASSERT(s_backend && "JobSystem not initialized");

            return s_backend->parallelFor(std::move(desc));
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