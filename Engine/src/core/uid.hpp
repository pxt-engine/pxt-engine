#pragma once

#include "core/pch.hpp"

namespace pxt::core {

    /**
     * @brief Enumeration representing versions for 64-bit UIDs.
     * * V4: 64 bits of pure randomness.
     * V7: 32-bit timestamp (seconds) + 32-bit random entropy.
     */
    enum class UIDVersion { V4, V7 };

    /**
     * @brief Class representing a 64-bit Unique Identifier.
     * Fits in a single CPU register; highly efficient for ECS entities.
     */
    class UID {
    public:
        static UID s_invalidId;

        /** @brief Default constructor generates a Version 7 UID. */
        UID();

        /** @brief Constructs a UID of the specified version. */
        explicit UID(UIDVersion version);

        /** @brief Constructs a UID from a raw 64-bit integer. */
        explicit UID(uint64_t value) : m_id(value) {}

        /** * @brief Constructs a UID from a hex string (e.g., "XXXXXXXX-XXXXXXXX").
         */
        explicit UID(const std::string& UIDString);

        // Comparisons
        bool operator==(const UID& other) const { return m_id == other.m_id; }

        bool operator!=(const UID& other) const { return m_id != other.m_id; }

        bool operator<(const UID& other) const { return m_id < other.m_id; }

        /** @brief Implicit conversion to uint64_t for easy use in math/indexing. */
        operator uint64_t() const { return m_id; }

        /** @brief Converts to hex string: "00000000-00000000". */
        [[nodiscard]] std::string toString() const;

    private:
        uint64_t m_id = 0;

        static uint64_t generateV4();
        static uint64_t generateV7();

        friend struct std::hash<UID>;
    };
} // namespace pxt::core

template <>
struct std::hash<pxt::core::UID> {
    std::size_t operator()(const pxt::core::UID& uid) const noexcept {
        return std::hash<uint64_t>{}(static_cast<uint64_t>(uid));
    }
};