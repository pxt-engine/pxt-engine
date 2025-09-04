#pragma once

#include "core/pch.hpp"
#include "scene/scene.hpp"
#include "resources/resource_manager.hpp"

#include "yaml-cpp/yaml.h"

namespace PXTEngine {

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
}

