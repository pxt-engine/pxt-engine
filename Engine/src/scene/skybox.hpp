#pragma once

#include "core/pch.hpp"

namespace pxt {

    class Skybox {
    public:
        virtual ~Skybox() = default;

        virtual void replace(const std::array<std::string, 6>& skyboxTextures, uint32_t frameIndex) = 0;
    };
} // namespace pxt