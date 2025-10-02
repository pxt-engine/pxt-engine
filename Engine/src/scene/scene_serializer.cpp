#include "scene/scene_serializer.hpp"

#include "scene/ecs/component.hpp"
#include "scene/ecs/entity.hpp"

#include <typeindex>

namespace PXTEngine {

	using SerializerFunction = std::function<void(Entity, YAML::Emitter&)>;

	template<typename T, typename Fn>
	static SerializerFunction makeSerializer(Fn&& fn) {
		return [func = std::forward<Fn>(fn)](Entity entity, YAML::Emitter& out) {
			if (!entity.has<T>())
				return;
			auto& component = entity.get<T>();
			func(component, out);
		};
	}

	static std::unordered_map<std::type_index, SerializerFunction> s_ComponentSerializers = {
		// Name Component
		{ typeid(NameComponent), makeSerializer<NameComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "NameComponent" << YAML::Value << c.name;
		})},

		// Color Component
		{ typeid(ColorComponent), makeSerializer<ColorComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "ColorComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "color" << YAML::Value << YAML::Flow << YAML::BeginSeq << c.color.x << c.color.y << c.color.z << YAML::EndSeq;
			out << YAML::EndMap;
		})},

		// Transform Component
		{ typeid(TransformComponent), makeSerializer<TransformComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "translation" << YAML::Value << YAML::Flow << YAML::BeginSeq << c.translation.x << c.translation.y << c.translation.z << YAML::EndSeq;
			out << YAML::Key << "scale" << YAML::Value << YAML::Flow << YAML::BeginSeq << c.scale.x << c.scale.y << c.scale.z << YAML::EndSeq;
			out << YAML::Key << "rotation" << YAML::Value << YAML::Flow << YAML::BeginSeq << c.rotation.x << c.rotation.y << c.rotation.z << YAML::EndSeq;
			out << YAML::EndMap;
		})},

		// Transform2d Component
		{ typeid(Transform2dComponent), makeSerializer<Transform2dComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "Transform2dComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "translation" << YAML::Value << YAML::Flow << YAML::BeginSeq << c.translation.x << c.translation.y << YAML::EndSeq;
			out << YAML::Key << "scale" << YAML::Value << YAML::Flow << YAML::BeginSeq << c.scale.x << c.scale.y << YAML::EndSeq;
			out << YAML::Key << "rotation" << YAML::Value << c.rotation;
			out << YAML::EndMap;
		})},

		// Volume Component
		{ typeid(VolumeComponent), makeSerializer<VolumeComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "VolumeComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "absorption" << YAML::Value << YAML::Flow << YAML::BeginSeq
				<< c.volume.absorption.r << c.volume.absorption.g << c.volume.absorption.b << c.volume.absorption.a << YAML::EndSeq;
			out << YAML::Key << "scattering" << YAML::Value << YAML::Flow << YAML::BeginSeq
				<< c.volume.scattering.r << c.volume.scattering.g << c.volume.scattering.b << c.volume.scattering.a << YAML::EndSeq;
			out << YAML::Key << "phaseFunctionG" << YAML::Value << c.volume.phaseFunctionG;
			//out << YAML::Key << "densityTextureId" << YAML::Value << c.volume.densityTextureId;
			//out << YAML::Key << "detailTextureId" << YAML::Value << c.volume.detailTextureId;
			out << YAML::EndMap;
		})},

		// Mesh Component
		{ typeid(MeshComponent), makeSerializer<MeshComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "meshId" << YAML::Value << c.mesh->id.toString();
			out << YAML::Key << "mesh" << YAML::Value << c.mesh->alias;
			out << YAML::EndMap;
		})},

		// Material Component
		// TODO: maybe reuse material if different entities use the same params
		{ typeid(MaterialComponent), makeSerializer<MaterialComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "MaterialComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "materialId" << YAML::Value << c.material->id.toString();
			out << YAML::Key << "material" << YAML::Value << c.material->alias;
			auto albedoColor = c.material->getAlbedoColor();
			out << YAML::Key << "albedoColor" << YAML::Value << YAML::Flow << YAML::BeginSeq
				<< albedoColor.r << albedoColor.g << albedoColor.b << albedoColor.a << YAML::EndSeq;
			out << YAML::Key << "albedoMap" << YAML::Value << (c.material->getAlbedoMap() ? c.material->getAlbedoMap()->alias : "null");
			out << YAML::Key << "metallic" << YAML::Value << c.material->getMetallic();
			out << YAML::Key << "metallicMap" << YAML::Value << (c.material->getMetallicMap() ? c.material->getMetallicMap()->alias : "null");
			out << YAML::Key << "roughness" << YAML::Value << c.material->getRoughness();
			out << YAML::Key << "roughnessMap" << YAML::Value << (c.material->getRoughnessMap() ? c.material->getRoughnessMap()->alias : "null");
			out << YAML::Key << "normalMap" << YAML::Value << (c.material->getNormalMap() ? c.material->getNormalMap()->alias : "null");
			out << YAML::Key << "ambientOcclusionMap" << YAML::Value << (c.material->getAmbientOcclusionMap() ? c.material->getAmbientOcclusionMap()->alias : "null");
			auto emissiveColor = c.material->getEmissiveColor();
			out << YAML::Key << "emissiveColor" << YAML::Value << YAML::Flow << YAML::BeginSeq
				<< emissiveColor.r << emissiveColor.g << emissiveColor.b << emissiveColor.a << YAML::EndSeq;
			out << YAML::Key << "emissiveMap" << YAML::Value << (c.material->getEmissiveMap() ? c.material->getEmissiveMap()->alias : "null");
			out << YAML::Key << "transmission" << YAML::Value << c.material->getTransmission();
			out << YAML::Key << "indexOfRefraction" << YAML::Value << c.material->getIndexOfRefraction();
			out << YAML::Key << "blinnPhongSpecularIntensity" << YAML::Value << c.material->getBlinnPhongSpecularIntensity();
			out << YAML::Key << "blinnPhongSpecularShininess" << YAML::Value << c.material->getBlinnPhongSpecularShininess();
			out << YAML::Key << "tilingFactor" << YAML::Value << c.tilingFactor;
			out << YAML::Key << "tint" << YAML::Value << YAML::Flow << YAML::BeginSeq
				<< c.tint.r << c.tint.g << c.tint.b << YAML::EndSeq;
			out << YAML::EndMap;
		})},

		// Camera Component
		{ typeid(CameraComponent), makeSerializer<CameraComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "isMainCamera" << YAML::Value << c.isMainCamera;
			out << YAML::Key << "isPerspective" << YAML::Value << c.camera.isPerspective();
			out << YAML::Key << "nearPlane" << YAML::Value << c.camera.getNearPlane();
			out << YAML::Key << "farPlane" << YAML::Value << c.camera.getFarPlane();
			out << YAML::Key << "fovYDegrees" << YAML::Value << c.camera.getFovYDegrees();
			out << YAML::Key << "orthoParams" << YAML::Value << YAML::Flow << YAML::BeginSeq
				<< c.camera.getOrthoLeft() << c.camera.getOrthoRight()
				<< c.camera.getOrthoTop() << c.camera.getOrthoBottom() << YAML::EndSeq;
			out << YAML::EndMap;
		})},

		// PointLight Component
		{ typeid(PointLightComponent), makeSerializer<PointLightComponent>([](auto& c, YAML::Emitter& out) {
			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "lightIntensity" << YAML::Value << c.lightIntensity;
			out << YAML::EndMap;
		})},
	};

	SceneSerializer::SceneSerializer(Scene* scene, ResourceManager* resourceManager) 
		: m_scene(scene), m_resourceManager(resourceManager) {}

	static void serializeEntity(Entity entity, YAML::Emitter& out) {
		// Entity Map
		out << YAML::BeginMap;

		out << YAML::Key << "entity" << YAML::Value << entity.getUUID().toString();

		// Serialize Components
		for (auto& [name, fn] : s_ComponentSerializers)
			fn(entity, out);
		
		// End Entity Map
		out << YAML::EndMap;

	}

	static void serializeEnvironment(Scene* scene, YAML::Emitter& out) {
		// TODO: 
	}

	void SceneSerializer::serialize(const std::string& filepath) {
		YAML::Emitter out;

		// Scene Map
		out << YAML::BeginMap;

		// Scene name
		out << YAML::Key << "scene" << YAML::Value << m_scene->getName();

		serializeEnvironment(m_scene, out);

		// Entities Sequence
		out << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;

		// Iterate through all entities with IDComponent (every entity should have one)
		for (auto& entityHandle : m_scene->getEntitiesWith<IDComponent>()) {
			Entity entity = { entityHandle, m_scene };

			serializeEntity(entity, out);
		}

		// End Entities Sequence
		out << YAML::EndSeq;

		// End Scene Map
		out << YAML::EndMap;

		// Create directory if doesn't exists
		std::filesystem::create_directories(std::filesystem::path(filepath).parent_path());

		// Write to file
		std::ofstream fout(filepath, std::ios::out | std::ios::trunc);

		if (!fout.is_open()) {
			PXT_ERROR("Could not serialize the Scene '{}' in '{}'", m_scene->getName(), filepath);
			return;
		}

		fout << out.c_str();
		fout.close();
	}

	bool SceneSerializer::deserialize(const std::string& filepath) {
		YAML::Node data;

		try {
			data = YAML::LoadFile(filepath);
		} catch (YAML::ParserException& e) {
			PXT_ERROR("Could not deserialize the Scene in '{}': {}", filepath, e.what());
			return false;
		}

		if (!data["scene"]) {
			PXT_ERROR("Could not find 'scene' key in '{}'", filepath);
			return false;
		}

		std::string sceneName = data["scene"].as<std::string>();

		auto entities = data["entities"];

		if (!entities) {
			PXT_ERROR("Could not find 'entities' key in '{}'", filepath);
			return false;
		}

		auto rm = m_resourceManager;

		//TODO: environment
		// ----------------
		std::array<std::string, 6> skyboxTextures;
		skyboxTextures[CubeFace::BACK] = TEXTURES_PATH + "skybox/bluecloud_bk.jpg";
		skyboxTextures[CubeFace::FRONT] = TEXTURES_PATH + "skybox/bluecloud_ft.jpg";
		skyboxTextures[CubeFace::LEFT] = TEXTURES_PATH + "skybox/bluecloud_lf.jpg";
		skyboxTextures[CubeFace::RIGHT] = TEXTURES_PATH + "skybox/bluecloud_rt.jpg";
		skyboxTextures[CubeFace::TOP] = TEXTURES_PATH + "skybox/bluecloud_up.jpg";
		skyboxTextures[CubeFace::BOTTOM] = TEXTURES_PATH + "skybox/bluecloud_dn.jpg";

		auto environment = m_scene->getEnvironment();

		environment->setAmbientLight({ 1.0, 1.0, 1.0, 0.1f });
		environment->setSkybox(skyboxTextures);
		// ----------------

		for (auto entityNode : entities) {
			std::string uuid = entityNode["entity"].as<std::string>();

			// Deserialize NameComponent
			std::string name = "Unnamed-Entity";
			if (auto nameComponentNode = entityNode["NameComponent"]) {
				name = nameComponentNode.as<std::string>();
			}

			Entity entity = m_scene->createEntity(name, UUID(uuid));

			// Deserialize TransformComponent
			if (auto transformComponentNode = entityNode["TransformComponent"]) {
				auto translation = transformComponentNode["translation"].as<std::vector<float>>();
				auto scale = transformComponentNode["scale"].as<std::vector<float>>();
				auto rotation = transformComponentNode["rotation"].as<std::vector<float>>();
				entity.add<TransformComponent>(
					glm::vec3(translation[0], translation[1], translation[2]),
					glm::vec3(scale[0], scale[1], scale[2]),
					glm::vec3(rotation[0], rotation[1], rotation[2])
				);
			}

			// Deserialize Transform2dComponent
			if (auto transform2dComponentNode = entityNode["Transform2dComponent"]) {
				auto translation = transform2dComponentNode["translation"].as<std::vector<float>>();
				auto scale = transform2dComponentNode["scale"].as<std::vector<float>>();
				float rotation = transform2dComponentNode["rotation"].as<float>();
				entity.add<Transform2dComponent>(
					glm::vec2(translation[0], translation[1]),
					glm::vec2(scale[0], scale[1]),
					rotation
				);
			}

			// Deserialize ColorComponent
			if (auto colorComponentNode = entityNode["ColorComponent"]) {
				auto color = colorComponentNode["color"].as<std::vector<float>>();

				entity.add<ColorComponent>(glm::vec3{ color[0], color[1], color[2] });
			}

			// Deserialize VolumeComponent
			if (auto volumeComponentNode = entityNode["VolumeComponent"]) {
				auto absorption = volumeComponentNode["absorption"].as<std::vector<float>>();
				auto scattering = volumeComponentNode["scattering"].as<std::vector<float>>();
				auto phaseFunctionG = volumeComponentNode["phaseFunctionG"].as<float>();

				entity.add<VolumeComponent>(VolumeComponent::Builder()
					.setAbsorption({ absorption[0], absorption[1], absorption[2], absorption[3] })
					.setScattering({ scattering[0], scattering[1], scattering[2], scattering[3] })
					.setPhaseFunctionG(phaseFunctionG)
					.build()
				);
			}
			// Deserialize MeshComponent
			if (auto meshComponentNode = entityNode["MeshComponent"]) {
				std::string meshAlias = meshComponentNode["mesh"].as<std::string>();

				auto mesh = rm->get<Mesh>(meshAlias);

				entity.add<MeshComponent>(mesh);
			}

			// Deserialize MaterialComponent
			if (auto materialComponentNode = entityNode["MaterialComponent"]) {
				std::string materialAlias = materialComponentNode["material"].as<std::string>();

				ImageInfo albedoInfo{};
				albedoInfo.format = RGBA8_SRGB;

				auto albedoColor = materialComponentNode["albedoColor"].as<std::vector<float>>();
				auto albedoMapName = materialComponentNode["albedoMap"].as<std::string>();
				auto metallic = materialComponentNode["metallic"].as<float>();
				auto metallicMapName = materialComponentNode["metallicMap"].as<std::string>();
				auto roughness = materialComponentNode["roughness"].as<float>();
				auto roughnessMapName = materialComponentNode["roughnessMap"].as<std::string>();
				auto normalMapName = materialComponentNode["normalMap"].as<std::string>();
				auto ambientOcclusionMapName = materialComponentNode["ambientOcclusionMap"].as<std::string>();
				auto emissiveColor = materialComponentNode["emissiveColor"].as<std::vector<float>>();
				auto emissiveMapName = materialComponentNode["emissiveMap"].as<std::string>();
				auto transmission = materialComponentNode["transmission"].as<float>();
				auto indexOfRefraction = materialComponentNode["indexOfRefraction"].as<float>();
				auto blinnPhongSpecularIntensity = materialComponentNode["blinnPhongSpecularIntensity"].as<float>();
				auto blinnPhongSpecularShininess = materialComponentNode["blinnPhongSpecularShininess"].as<float>();

				auto materialBuilder = Material::Builder()
					.setAlbedoColor({ albedoColor[0], albedoColor[1], albedoColor[2], albedoColor[3] })
					.setAlbedoMap(rm->get<Image>(albedoMapName, &albedoInfo))
					.setNormalMap(rm->get<Image>(normalMapName))
					.setAmbientOcclusionMap(rm->get<Image>(ambientOcclusionMapName))
					.setEmissiveColor({ emissiveColor[0], emissiveColor[1], emissiveColor[2], emissiveColor[3] })
					.setEmissiveMap(rm->get<Image>(emissiveMapName))
					.setTransmission(transmission)
					.setIndexOfRefraction(indexOfRefraction)
					.setBlinnPhongSpecularIntensity(blinnPhongSpecularIntensity)
					.setBlinnPhongSpecularShininess(blinnPhongSpecularShininess);

				if (metallicMapName == "null") {
					materialBuilder.setMetallic(metallic);
				}
				else {
					materialBuilder.setMetallicMap(rm->get<Image>(metallicMapName));
				}
				if (roughnessMapName == "null") {
					materialBuilder.setRoughness(roughness);
				}
				else {
					materialBuilder.setRoughnessMap(rm->get<Image>(roughnessMapName));
				}
				auto material = materialBuilder.build();

				rm->add(material, "mat-" + name);

				float tilingFactor = 1.0f;
				if (auto tilingFactorNode = materialComponentNode["tilingFactor"]) {
					tilingFactor = tilingFactorNode.as<float>();
				}

				glm::vec3 tint(1.0f);
				if (auto tintNode = materialComponentNode["tint"]) {
					auto tintVec = tintNode.as<std::vector<float>>();
					tint = { tintVec[0], tintVec[1], tintVec[2] };
				}

				entity.add<MaterialComponent>(MaterialComponent::Builder()
					.setMaterial(material)
					.setTilingFactor(tilingFactor)
					.setTint(tint)
					.build()
				);
			}

			// Deserialize CameraComponent
			if (auto cameraComponentNode = entityNode["CameraComponent"]) {
				bool isMainCamera = cameraComponentNode["isMainCamera"].as<bool>();
				bool isPerspective = cameraComponentNode["isPerspective"].as<bool>();
				float nearPlane = cameraComponentNode["nearPlane"].as<float>();
				float farPlane = cameraComponentNode["farPlane"].as<float>();
				float fovYDegrees = cameraComponentNode["fovYDegrees"].as<float>();
				auto orthoParams = cameraComponentNode["orthoParams"].as<std::vector<float>>();
				Camera camera;
				camera.setPerspectiveParams(fovYDegrees, nearPlane, farPlane);
				camera.setOrthographicParams(orthoParams[0], orthoParams[1], orthoParams[2], orthoParams[3], nearPlane, farPlane);
				camera.setIsPerspective(isPerspective);
				entity.add<CameraComponent>(camera);
			}

			// Deserialize PointLightComponent
			if (auto pointLightComponentNode = entityNode["PointLightComponent"]) {
				float lightIntensity = pointLightComponentNode["lightIntensity"].as<float>();
				entity.add<PointLightComponent>(lightIntensity);
			}
		}

		return true;
	}

}