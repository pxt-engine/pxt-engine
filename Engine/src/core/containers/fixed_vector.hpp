#pragma once

#include "core/pch.hpp"

namespace pxt::core {

    /**
     * @brief Helper to select the smallest unsigned integer type
     * capable of holding a value up to N.
     */
    template <size_t N>
    consteval auto pick_best_size_type() {
        if constexpr (N <= 255)
            return uint8_t{};
        else if constexpr (N <= 65535)
            return uint16_t{};
        else
            return uint32_t{};
    }

    /**
     * @brief A fixed-capacity, stack-allocated vector that behaves like std::vector.
     * * Unlike std::vector, FixedVector does not perform any heap allocations. It stores
     * its elements inline within an internal std::array. The class uses C++20 metaprogramming
     * to automatically select the smallest possible integer type for its internal size counter,
     * maximizing cache density and minimizing structure padding.
     *
     * @tparam T The type of elements to store.
     * @tparam Capacity The maximum number of elements the vector can hold.
     */
    template <typename T, size_t Capacity>
    class FixedVector {
        using SizeType = decltype(pick_best_size_type<Capacity>());

    public:
        FixedVector() = default;

        // Element access

        constexpr T& operator[](size_t index) noexcept { return m_data[index]; }

        constexpr const T& operator[](size_t index) const noexcept { return m_data[index]; }

        constexpr T& back() noexcept { return m_data[m_size - 1]; }

        constexpr const T& back() const noexcept { return m_data[m_size - 1]; }

        // Modifiers

        constexpr void push_back(const T& value) {
            if (m_size >= Capacity)
                return; // Or handle error
            m_data[m_size++] = value;
        }

        constexpr void push_back(T&& value) {
            if (m_size >= Capacity)
                return;
            m_data[m_size++] = std::move(value);
        }

        template <typename... Args>
        constexpr T& emplace_back(Args&&... args) {
            if (m_size >= Capacity)
                return m_data[Capacity - 1]; // Safety fallback
            m_data[m_size] = T(std::forward<Args>(args)...);
            return m_data[m_size++];
        }

        /**
         * @brief Removes the last element from the vector.
         *
         * @note If the type T is not trivially destructible, this manually invokes
         * the destructor for the element before decrementing the size.
         * Calling this on an empty vector is undefined behavior (matching std::vector).
         */
        constexpr void pop_back() {
            if (m_size > 0) {
                if constexpr (!std::is_trivially_destructible_v<T>) {
                    m_data[m_size - 1].~T();
                }
                --m_size;
            }
        }

        /**
         * @brief Resets the vector to empty, destroying all contained elements.
         */
        constexpr void clear() noexcept {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i = 0; i < m_size; ++i) {
                    m_data[i].~T();
                }
            }
            m_size = 0;
        }

        // Capacity

        [[nodiscard]] constexpr size_t size() const noexcept { return static_cast<size_t>(m_size); }

        [[nodiscard]] constexpr size_t capacity() const noexcept { return Capacity; }

        [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

        [[nodiscard]] constexpr bool full() const noexcept { return m_size == Capacity; }

        // Iterators

        constexpr T* begin() noexcept { return m_data.data(); }

        constexpr const T* begin() const noexcept { return m_data.data(); }

        constexpr T* end() noexcept { return m_data.data() + m_size; }

        constexpr const T* end() const noexcept { return m_data.data() + m_size; }

    private:
        std::array<T, Capacity> m_data{};

        SizeType m_size = 0;
    };

} // namespace pxt::core