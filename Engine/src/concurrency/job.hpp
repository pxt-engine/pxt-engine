#pragma once

#include "core/pch.hpp"

namespace pxt::concurrency {

    /**
     * @brief A JobFunction encapsulates a callable job function.
     *
     * It uses a fixed-size buffer to store the callable object (e.g., lambda or function object)
     * and a function pointer to invoke it. The buffer size is set to 48 bytes, which should
     * accommodate most small callable objects.
     */
    struct JobFunction {
        void (*invoke)(const void*) = nullptr;
        alignas(std::max_align_t) std::byte buffer[48];

        JobFunction() = default;

        template <typename Func>
        requires(!std::is_same_v<std::decay_t<Func>, JobFunction>)
        JobFunction(Func&& f) {
            using DecayedFunc = std::decay_t<Func>;

            static_assert(sizeof(DecayedFunc) <= sizeof(buffer), "Lambda too large for buffer");
            static_assert(std::is_trivially_copyable_v<DecayedFunc>, "Captures must be trivially copyable");

            // Construct the lambda into the buffer
            new (buffer) DecayedFunc(std::forward<Func>(f));

            // Map the invoker
            invoke = [](const void* ptr) { (*reinterpret_cast<const DecayedFunc*>(ptr))(); };
        }

        void operator()() const {
            if (invoke) {
                invoke(buffer);
            }
        }

        explicit operator bool() const { return invoke != nullptr; }
    };

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