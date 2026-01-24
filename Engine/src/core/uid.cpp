#include "core/uid.hpp"

namespace pxt::core {

    UID UID::s_invalidId = UID(0);

    UID::UID() { m_id = generateV7(); }

    UID::UID(UIDVersion version) {
        switch (version) {
        case UIDVersion::V4:
            m_id = generateV4();
            break;
        case UIDVersion::V7:
            m_id = generateV7();
            break;
        }
    }

    UID::UID(const std::string& UIDString) {
        // Support both "XXXXXXXX-XXXXXXXX" and "XXXXXXXXXXXXXXXX"
        std::string cleaned = UIDString;
        std::erase(cleaned, '-');

        if (cleaned.length() != 16) {
            m_id = 0;
            return;
        }

        try {
            m_id = std::stoull(cleaned, nullptr, 16);
        } catch (...) {
            m_id = 0;
        }
    }

    uint64_t UID::generateV4() {
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;
        return dist(gen);
    }

    uint64_t UID::generateV7() {
        // High 32 bits: Seconds since epoch (lasts until year 2106)
        auto now = std::chrono::system_clock::now();
        uint64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        // Low 32 bits: Entropy
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist;
        uint32_t entropy = dist(gen);

        return (seconds << 32) | entropy;
    }

    std::string UID::toString() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        // Split into two 8-character blocks for readability
        ss << std::setw(8) << (m_id >> 32) << "-";
        ss << std::setw(8) << (m_id & 0xFFFFFFFF);
        return ss.str();
    }

} // namespace pxt::core