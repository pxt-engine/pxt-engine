#pragma once

// clang-format off
namespace pxt::concurrency {

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #include <immintrin.h>
#endif

/**
 * @brief Provides a CPU relaxation hint to improve performance in spin-wait loops.
 *
 * This function issues architecture-specific instructions to reduce power consumption
 * and improve efficiency while waiting. If the architecture is not recognized, it
 * falls back to yielding the current thread.
 */
inline void cpuRelax() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    _mm_pause();

#elif defined(__aarch64__) || defined(__arm__)
    // ARM
    asm volatile("yield");

#elif defined(__riscv)
    // RISC-V
    asm volatile("pause");

#else
    // Portable Fallback
    std::this_thread::yield();
#endif
}

} // namespace pxt::concurrency

// clang-format on