#pragma once

#include "core/pch.hpp"

namespace pxt::concurrency {

    /**
     * @brief A SmallFunction is a lightweight, non-allocating function wrapper for callable objects.
     *
     * It uses a fixed-size buffer to store the callable object (e.g., lambda or function object)
     * and a function pointer to invoke it. The buffer size is set to 'BufferSize' bytes, which should
     * accommodate most small callable objects.
     */
    template <typename Signature, size_t BufferSize>
    struct SmallFunction;

    // Specialization for function signatures
    template <size_t BufferSize, typename Ret, typename... Args>
    struct SmallFunction<Ret(Args...), BufferSize> {

        SmallFunction() = default;

        template <typename Func>
        requires(!std::is_same_v<std::decay_t<Func>, SmallFunction> && std::invocable<Func, Args...>)
        SmallFunction(Func&& f) {
            using DecayedFunc = std::decay_t<Func>;

            PXT_STATIC_ASSERT(sizeof(DecayedFunc) <= BufferSize, "Lambda too large for buffer");
            PXT_STATIC_ASSERT(std::is_trivially_copyable_v<DecayedFunc>, "Captures must be trivially copyable");

            // Construct the lambda into the buffer (there is no heap allocation)
            new (m_buffer) DecayedFunc(std::forward<Func>(f));

            // Create the invoker wrapper
            m_invoke = [](const void* ptr, Args... args) -> Ret {
                return (*reinterpret_cast<const DecayedFunc*>(ptr))(std::forward<Args>(args)...);
            };
        }

        Ret operator()(Args... args) const {
            PXT_ASSERT(m_invoke);

            return m_invoke(m_buffer, std::forward<Args>(args)...);
        }

        bool isValid() const { return m_invoke != nullptr; }

    private:
        Ret (*m_invoke)(const void*, Args...) = nullptr;
        alignas(std::max_align_t) std::byte m_buffer[BufferSize];
    };

    // Fits 3 pointers (Standard for most tasks)
    static constexpr size_t ParallelBufferSize = sizeof(void*) * 3;

    using JobParallelForFunction = SmallFunction<void(size_t, size_t), ParallelBufferSize>;

    // The generic job function must fit:
    // - A parallel job function
    // - Two size_t parameters (for range start and end)
    // So we can wrap a ParallelFor function inside a generic job.
    static constexpr size_t GenericBufferSize = sizeof(JobParallelForFunction) + (sizeof(size_t) * 2);

    using JobFunction = SmallFunction<void(), GenericBufferSize>;

    /**
     * @brief A JobHandle is an identifier for a submitted job or batch of jobs.
     *
     * JobHandles use a generation counter to prevent the ABA problem where a counter
     * index is reused while an old handle still references it. Each time a counter
     * completes and is recycled, its generation is incremented.
     *
     * The JobHandle is a 64-bit value with the following layout:
     * - Bits 0-30: Index (31 bits)
     * - Bits 31-61: Generation (31 bits)
     * - Bit 62: Batch flag (1 bit)
     * - Bit 63: Valid flag (1 bit)
     */
    struct JobHandle {
        uint64_t value = 0;

        // Bit sizes
        static constexpr uint32_t IndexBits = 31;
        static constexpr uint32_t GenerationBits = 31;

        // Shifts
        static constexpr uint32_t IndexShift = 0;
        static constexpr uint32_t GenerationShift = IndexBits;
        static constexpr uint32_t BatchShift = IndexBits + GenerationBits;
        static constexpr uint32_t ValidShift = BatchShift + 1;

        // Masks
        static constexpr uint64_t IndexMask = ((1ull << IndexBits) - 1ull) << IndexShift;

        static constexpr uint64_t GenerationMask = ((1ull << GenerationBits) - 1ull) << GenerationShift;

        static constexpr uint64_t ValidMask = 1ull << ValidShift;

        static constexpr uint64_t BatchMask = 1ull << BatchShift;

        // Accessors
        constexpr bool isValid() const { return (value & ValidMask) != 0; }

        constexpr bool isBatch() const { return (value & BatchMask) != 0; }

        constexpr uint32_t index() const { return static_cast<uint32_t>((value & IndexMask) >> IndexShift); }

        constexpr uint32_t generation() const {
            return static_cast<uint32_t>((value & GenerationMask) >> GenerationShift);
        }

        // Factory
        static constexpr JobHandle make(uint32_t index, uint32_t generation, bool isBatch = false) {
            return JobHandle{((uint64_t(index) << IndexShift) & IndexMask) |
                             ((uint64_t(generation) << GenerationShift) & GenerationMask) | ValidMask |
                             (isBatch ? BatchMask : 0ull)};
        }

        static constexpr JobHandle invalid() { return JobHandle{0}; }
    };

    enum class JobPriority : uint8_t { Low, Normal, High };

} // namespace pxt::concurrency