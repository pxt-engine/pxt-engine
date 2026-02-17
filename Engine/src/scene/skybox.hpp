#pragma once

#include "core/pch.hpp"

namespace pxt {

    class Skybox {
    public:
        virtual ~Skybox() = default;

        virtual void replace(const std::array<std::string, 6>& skyboxTextures) = 0;
    };
} // namespace pxt