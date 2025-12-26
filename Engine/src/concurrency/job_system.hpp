#pragma once

#include "core/pch.hpp"

#include "concurrency/job.hpp"
#include "core/containers/fixed_vector.hpp"

namespace pxt::concurrency {

    static constexpr size_t MAX_JOB_DEPENDENCIES = 11;

    struct JobDescription {
        JobFunction function;
        JobPriority priority = JobPriority::Normal;
        core::FixedVector<JobHandle, MAX_JOB_DEPENDENCIES> dependencies;
    };

    struct JobBatchDescription {
        std::span<const JobFunction> functions;
        JobPriority priority = JobPriority::Normal;
        core::FixedVector<JobHandle, MAX_JOB_DEPENDENCIES> dependencies;
    };

    class JobSystemBackend {
    public:
        virtual ~JobSystemBackend() = default;
        virtual JobHandle submit(const JobDescription& desc) = 0;
        virtual void wait(JobHandle handle) = 0;
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