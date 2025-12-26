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

        constexpr FixedVector(std::initializer_list<T> init) noexcept {
            PXT_ASSERT(init.size() <= Capacity && "Initializer list exceeds FixedVector capacity");

            for (const auto& item : init) {
                push_back(item);
            }
        }

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

        // Iterator types
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        // Forward iterators
        constexpr iterator begin() noexcept { return m_data.data(); }

        constexpr const_iterator begin() const noexcept { return m_data.data(); }

        constexpr const_iterator cbegin() const noexcept { return m_data.data(); }

        constexpr iterator end() noexcept { return m_data.data() + m_size; }

        constexpr const_iterator end() const noexcept { return m_data.data() + m_size; }

        constexpr const_iterator cend() const noexcept { return m_data.data() + m_size; }

        // Reverse iterators
        constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

        constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

        constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

        constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

        constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

        constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

        constexpr iterator erase(const_iterator pos) {
            size_t index = static_cast<size_t>(pos - begin());

            if (index >= m_size)
                return end();

            // Shift elements left
            for (size_t i = index; i + 1 < m_size; ++i) {
                m_data[i] = std::move(m_data[i + 1]);
            }

            --m_size;
            return begin() + index;
        }

        constexpr iterator erase(const_iterator first, const_iterator last) {
            size_t start = static_cast<size_t>(first - begin());
            size_t finish = static_cast<size_t>(last - begin());

            if (start >= m_size || start >= finish)
                return begin() + start;

            size_t count = finish - start;

            // Shift remaining elements left
            for (size_t i = start; i + count < m_size; ++i) {
                m_data[i] = std::move(m_data[i + count]);
            }

            m_size -= static_cast<SizeType>(count);
            return begin() + start;
        }

    private:
        std::array<T, Capacity> m_data{};

        SizeType m_size = 0;
    };

} // namespace pxt::core