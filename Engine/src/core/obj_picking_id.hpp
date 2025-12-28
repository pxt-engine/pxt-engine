#pragma once

#include "core/pch.hpp"

namespace pxt::core {
    class ObjPickingId {
    public:
        ObjPickingId();

        static uint32_t s_invalidId;

        static uint32_t getIdFromColor(const glm::u8vec3& color);
        static glm::u8vec3 getColorFromId(uint32_t id);
        static glm::vec4 getColorVec4FromId(uint32_t id);

        glm::u8vec3 getColorFromId();

        [[nodiscard]] uint32_t getObjPickingId() const { return m_objPickingId; }

    private:
        uint32_t getNextId() {
            static std::atomic_uint32_t s_lastId{1}; // should be thread safe
            return s_lastId++;
        }

        uint32_t m_objPickingId;
    };
} // namespace pxt::core