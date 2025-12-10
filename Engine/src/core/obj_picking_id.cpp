#include "core/obj_picking_id.hpp"

namespace pxt::core {
	// 24-bit mask
	constexpr uint32_t COLOR_MASK = 0xFFFFFF;

	// Large prime for scrambling bits
	constexpr uint32_t SCRAMBLING_PRIME = 11400007;

    // The pre-calculated inverse of 11400007 mod 2^24
    constexpr uint32_t INVERSE_PRIME = 2330767;

	ObjPickingId::ObjPickingId() : m_objPickingId(getNextId()) {}

	uint32_t ObjPickingId::getIdFromColor(const glm::u8vec3& color) {
        uint32_t u32color =
            (static_cast<uint32_t>(color.r) << 16) |  // R is the most significant 8 bits
            (static_cast<uint32_t>(color.g) << 8) |  // G is the middle 8 bits
            (static_cast<uint32_t>(color.b));         // B is the least significant 8 bits

        if ((u32color & COLOR_MASK) == 0) {
            return 0; // Pure black maps back to ID 0
        }

        // Reverse the scrambling: Multiply by the inverse prime and mask.
        uint32_t entityID = (u32color * INVERSE_PRIME) & COLOR_MASK;

        return entityID;
	}

	glm::u8vec3 ObjPickingId::getColorFromId(uint32_t id) {
        if (id == 0) {
            // reserved for no object
            return { 0, 0, 0 };
        }

        // scramble the sequential ID to get a pseudo-random color integer.
        uint32_t scrambledColor = (id * SCRAMBLING_PRIME) & COLOR_MASK;

        // map the 24-bit integer back to RGB components.
        glm::u8vec3 color;
        color.r = (scrambledColor >> 16) & 0xFF; // Top 8 bits (Red)
        color.g = (scrambledColor >> 8) & 0xFF; // Middle 8 bits (Green)
        color.b = (scrambledColor) & 0xFF; // Bottom 8 bits (Blue)

        return color;
	}

    glm::u8vec3 ObjPickingId::getColorFromId() {
        if (m_objPickingId == 0) {
            // reserved for no object
            return { 0, 0, 0 };
        }

        // scramble the sequential ID to get a pseudo-random color integer.
        uint32_t scrambledColor = (m_objPickingId * SCRAMBLING_PRIME) & COLOR_MASK;

        // map the 24-bit integer back to RGB components.
        glm::u8vec3 color;
        color.r = (scrambledColor >> 16) & 0xFF; // Top 8 bits (Red)
        color.g = (scrambledColor >> 8) & 0xFF; // Middle 8 bits (Green)
        color.b = (scrambledColor) & 0xFF; // Bottom 8 bits (Blue)

        return color;
    }
}