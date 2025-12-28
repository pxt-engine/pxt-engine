#pragma once

#include "core/pch.hpp"
#include "resources/resource_manager.hpp"
#include "scene/scene.hpp"

#include "yaml-cpp/yaml.h"

namespace pxt {

    class SceneSerializer {
    public:
        SceneSerializer(Scene* scene, ResourceManager* resourceManager);
        ~SceneSerializer() = default;

        void serialize(const std::string& filepath);

        bool deserialize(const std::string& filepath);

    private:
        Scene* m_scene;
        ResourceManager* m_resourceManager;
    };
} // namespace pxt
