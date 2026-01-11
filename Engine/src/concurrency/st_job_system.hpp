#pragma once

#include "core/pch.hpp"

#include "concurrency/job.hpp"
#include "concurrency/job_system.hpp"

namespace pxt::concurrency {
    class SingleThreadedJobSystem final : public JobSystemBackend {
    public:
        SingleThreadedJobSystem() = default;

        ~SingleThreadedJobSystem() override = default;

        JobHandle submit(JobDescription desc) override {
            desc.function();

            return JobHandle::invalid();
        }

        JobHandle submit(JobBatchDescription desc) override {
            for (auto& function : desc.functions) {
                function();
            }

            return JobHandle::invalid();
        }

        JobHandle parallelFor(JobParallelForDescription desc) override {
            for (size_t start = desc.start; start < desc.end; start++) {
                desc.function(start, desc.end);
            }

            return JobHandle::invalid();
        }

        void wait([[maybe_unused]] const JobHandle handle) override {
            // In a single-threaded job system, jobs are executed immediately upon submission,
            // so there's nothing to wait for.
        }
    };
} // namespace pxt::concurrency