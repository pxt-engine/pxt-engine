#include "core/obj_picking_id.hpp"

namespace pxt::core {
	// 24-bit mask
	constexpr uint32_t COLOR_MASK = 0xFFFFFF;

    constexpr uint32_t rot_left(uint32_t v, unsigned r) {
        return ((v << r) | (v >> (24 - r))) & COLOR_MASK;
    }

    constexpr uint32_t rot_right(uint32_t v, unsigned r) {
        return ((v >> r) | ((v << (24 - r)) & COLOR_MASK)) & COLOR_MASK;
    }

    // Undo left-xor: x ^= (x << s) & MASK
    constexpr uint32_t undo_left_xor(uint32_t x, unsigned s) {
        // iterate doubling s until >= 24
        unsigned shift = s;
        while (shift < 24) {
            x ^= ((x << shift) & COLOR_MASK);
            shift *= 2;
        }
        return x & COLOR_MASK;
    }

    // Undo right-xor: x ^= (x >> s)
    constexpr uint32_t undo_right_xor(uint32_t x, unsigned s) {
        unsigned shift = s;
        while (shift < 24) {
            x ^= (x >> shift);
            shift *= 2;
        }
        return x & COLOR_MASK;
    }

    // Gray-code inverse (for id ^= id >> 1; inverse done by series of right-xors)
    constexpr uint32_t invert_gray(uint32_t v) {
        uint32_t x = v;
        x ^= (x >> 1);
        x ^= (x >> 2);
        x ^= (x >> 4);
        x ^= (x >> 8);
        x ^= (x >> 16);
        return x & COLOR_MASK;
    }

    constexpr uint32_t scramble24(uint32_t id) {
        id &= COLOR_MASK;

        // Light reversible mix
        id ^= ((id << 13) & COLOR_MASK);
        id ^= (id >> 7);
        id ^= ((id << 17) & COLOR_MASK);

        // Rotate left 9 bits across 24-bit space
        id = rot_left(id, 9);

        // Mild Gray transform
        id ^= (id >> 1);

        return id & COLOR_MASK;
    }

    constexpr uint32_t unscramble24(uint32_t v) {
        v &= COLOR_MASK;

        // Reverse Gray transform (apply inverse)
        uint32_t x = invert_gray(v);

        // Reverse rotation (rotate right 9)
        x = rot_right(x, 9);

        // Reverse the XOR mix in reverse order of application:
        // scramble: L13, R7, L17  ->  we must undo L17, R7, L13
        x = undo_left_xor(x, 17);   // undo id ^= (id << 17)
        x = undo_right_xor(x, 7);   // undo id ^= (id >> 7)
        x = undo_left_xor(x, 13);   // undo id ^= (id << 13)

        return x & COLOR_MASK;
    }

	uint32_t ObjPickingId::s_invalidId = 0;

	ObjPickingId::ObjPickingId() : m_objPickingId(getNextId()) {}

	uint32_t ObjPickingId::getIdFromColor(const glm::u8vec3& color) {
        uint32_t c =
            (uint32_t(color.r) << 16) |
            (uint32_t(color.g) << 8) |
            color.b;
        // zero is reserved for no object
        return c == 0 ? 0 : unscramble24(c);
	}

	glm::u8vec3 ObjPickingId::getColorFromId(uint32_t id) {
        // reserved for no object
        if (id == 0) return { 0, 0, 0 };

        uint32_t c = scramble24(id);

        return {
            uint8_t((c >> 16) & 0xFF),
            uint8_t((c >> 8) & 0xFF),
            uint8_t(c & 0xFF)
        };
	}

    glm::u8vec3 ObjPickingId::getColorFromId() {
        // reserved for no object
        if (m_objPickingId == 0) return { 0, 0, 0 };

        uint32_t c = scramble24(m_objPickingId);

        return {
            uint8_t((c >> 16) & 0xFF),
            uint8_t((c >> 8) & 0xFF),
            uint8_t(c & 0xFF)
        };
    }

    static_assert(unscramble24(scramble24(1)) == 1);
    static_assert(unscramble24(scramble24(123456)) == 123456);
    static_assert(unscramble24(scramble24(0xFFFFFF)) == 0xFFFFFF);
}