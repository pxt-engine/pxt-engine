#pragma once

#include "core/pch.hpp"
#include "resources/resource_manager.hpp"
#include "scene/ecs/entity.hpp"
#include "scene/scene.hpp"

#include <typeindex>

#include "yaml-cpp/yaml.h"

namespace pxt {

    class SceneSerializer {
    public:
        SceneSerializer(Scene* scene, ResourceManager& resourceManager);
        ~SceneSerializer() = default;

        void init();

        void serialize(const std::string& filepath);
        void serializeEntity(Entity entity, YAML::Emitter& out);

        bool deserialize(const std::string& filepath);

    private:
        Scene* m_scene;
        ResourceManager& m_resourceManager;

        using SerializerFunction = std::function<void(Entity, YAML::Emitter&)>;
        std::unordered_map<std::type_index, SerializerFunction> m_ComponentSerializers;

        template <typename T, typename Fn>
        static SerializerFunction makeSerializer(Fn&& fn) {
            return [func = std::forward<Fn>(fn)](Entity entity, YAML::Emitter& out) {
                if (!entity.has<T>())
                    return;
                auto& component = entity.get<T>();
                func(component, out);
            };
        }
    };
} // namespace pxt
