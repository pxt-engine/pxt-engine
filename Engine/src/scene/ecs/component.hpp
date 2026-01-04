#pragma once

#include "core/obj_picking_id.hpp"
#include "core/pch.hpp"
#include "core/uuid.hpp"
#include "resources/types/material.hpp"
#include "resources/types/mesh.hpp"
#include "scene/camera.hpp"

namespace pxt {
    struct IDComponent {
        core::UUID uuid;

        IDComponent(core::UUID uuid) : uuid(uuid) {}

        IDComponent(const IDComponent&) = default;

        // Conversion operators
        operator core::UUID&() { return uuid; }

        operator const core::UUID&() const { return uuid; }
    };

    struct ObjPickingIdComponent {
        core::ObjPickingId objPickingId;
        glm::u8vec3 color;

        ObjPickingIdComponent() : ObjPickingIdComponent(core::ObjPickingId()) {}

        ObjPickingIdComponent(core::ObjPickingId id) : objPickingId(id) {
            color = objPickingId.getColorFromId();
            /*PXT_INFO("Created ObjPickingIdComponent with ID: {} and Color: ({}, {}, {})",
                    objPickingId.getObjPickingId(), color.r, color.g, color.b);
            PXT_INFO("ID retrieved from color: {}", core::ObjPickingId::getIdFromColor(color));*/
        }

        glm::vec4 getColorAsVec4() const {
            return glm::vec4(static_cast<float>(color.r) / 255.0f, static_cast<float>(color.g) / 255.0f,
                             static_cast<float>(color.b) / 255.0f, 1.0f);
        }

        operator glm::u8vec3&() { return color; }

        operator const glm::u8vec3&() const { return color; }
    };

    struct NameComponent {
        std::string name;

        NameComponent() = default;
        NameComponent(const NameComponent&) = default;

        NameComponent(const std::string& name) : name(name) {}

        // Conversion operators
        operator std::string&() { return name; }

        operator const std::string&() const { return name; }
    };

    struct ColorComponent {
        glm::vec3 color;

        ColorComponent() = default;
        ColorComponent(const ColorComponent&) = default;

        ColorComponent(const glm::vec3& color) : color(color) {}

        // Conversion operators
        operator glm::vec3&() { return color; }

        operator const glm::vec3&() const { return color; }
    };

    struct VolumeComponent {
        struct Volume {
            glm::vec4 absorption{0.0f};
            glm::vec4 scattering{0.0f};
            // Henyey-Greenstein phase function parameter [-1.0, 1.0].
            // phaseFunctionG = 0.0 for isotropic scattering
            // phaseFunctionG > 0.0 for forward scattering
            // phaseFunctionG < 0.0 for backward scattering
            float phaseFunctionG = 0;
            Shared<Image> densityTexture{};
            Shared<Image> detailTexture{}; // for edge details of the volume
        };

        Volume volume;

        VolumeComponent() = default;
        VolumeComponent(const VolumeComponent&) = default;

        VolumeComponent(const Volume volume) : volume(volume) {}

        struct Builder {
            Volume volume;

            Builder& setAbsorption(const glm::vec4& absorption) {
                volume.absorption = absorption;
                return *this;
            }

            Builder& setScattering(const glm::vec4& scattering) {
                volume.scattering = scattering;
                return *this;
            }

            Builder& setPhaseFunctionG(float phaseFunctionG) {
                volume.phaseFunctionG = phaseFunctionG;
                return *this;
            }

            Builder& setDensityTexture(Shared<Image> texture) {
                volume.densityTexture = texture;
                return *this;
            }

            Builder& setDetailTexture(Shared<Image> texture) {
                volume.detailTexture = texture;
                return *this;
            }

            VolumeComponent build() { return VolumeComponent(volume); }
        };
    };

    struct MaterialComponent {
        Shared<Material> material;
        float tilingFactor = 1.0f;
        glm::vec3 tint{1.0f};

        MaterialComponent();

        MaterialComponent(const MaterialComponent&) = default;

        MaterialComponent(const Shared<Material>& material, float tilingFactor, const glm::vec3& tint)
            : material(material), tilingFactor(tilingFactor), tint(tint) {}

        struct Builder {
            Shared<Material> material;
            float tilingFactor = 1.0f;
            glm::vec3 tint{1.0f};

            Builder& setMaterial(const Shared<Material>& material) {
                this->material = material;
                return *this;
            }

            Builder& setTilingFactor(float tilingFactor) {
                this->tilingFactor = tilingFactor;
                return *this;
            }

            Builder& setTint(const glm::vec3& tint) {
                this->tint = tint;
                return *this;
            }

            MaterialComponent build() { return MaterialComponent(material, tilingFactor, tint); }
        };
    };

    struct Transform2dComponent {
        glm::vec2 translation{};
        glm::vec2 scale{1.f, 1.f};
        float rotation = 0.0f;

        glm::mat2 mat2() const;

        Transform2dComponent() = default;
        Transform2dComponent(const Transform2dComponent&) = default;

        Transform2dComponent(const glm::vec2& translation) : translation(translation) {}

        Transform2dComponent(const glm::vec2& translation, const glm::vec2& scale)
            : translation(translation), scale(scale) {}

        Transform2dComponent(const glm::vec2& translation, const glm::vec2& scale, const float rotation)
            : translation(translation), scale(scale), rotation(rotation) {}

        operator glm::mat2() { return mat2(); }
    };

    struct TransformComponent {
        glm::vec3 translation{};
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::vec3 rotation{};

        /**
         * @brief Computes the entity's world-space 4x4 transformation matrix.
         *
         * This function follows the standard computer graphics convention for Column-Major matrices:
         * Matrix = Translation * Rotation * Scale (applied in that order).
         *
         * @details
         * - Rotation: Uses Euler angles in RADIANS, converted to a Quaternion to avoid gimbal lock.
         * - Order: Corresponds to Intrinsic Y -> X -> Z (Tait-Bryan) rotation sequence.
         * - Coordinate System: Right-handed.
         * 
         * * @return glm::mat4 Combined transformation matrix.
         */
        glm::mat4 mat4() const;
        glm::mat3 normalMatrix(const glm::mat4& modelMatrix) const;

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;

        TransformComponent(const glm::vec3& translation) : translation(translation) {}

        TransformComponent(const glm::vec3& translation, const glm::vec3& scale)
            : translation(translation), scale(scale) {}

        TransformComponent(const glm::vec3& translation, const glm::vec3& scale, const glm::vec3& rotation)
            : translation(translation), scale(scale), rotation(rotation) {}

        // Conversion operator calling the mat4 function
        operator glm::mat4() { return mat4(); }
    };

    struct MeshComponent {
        Shared<Mesh> mesh;

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;

        MeshComponent(const Shared<Mesh>& mesh) : mesh(mesh) {}
    };

    class Script; // Forward declaration of Script class

    struct ScriptComponent {
        Script* script = nullptr;

        // Type-erased factory and destructor
        Script* (*create)() = nullptr;
        void (*destroy)(Script*) = nullptr;

        template <typename T>
        requires(std::is_base_of_v<Script, T>)
        void bind() {
            create = []() -> Script* { return new T(); };

            destroy = [](Script* s) { delete static_cast<T*>(s); };
        }
    };

    struct CameraComponent {
        Camera camera;
        float aspectRatio = 1.f;
        bool useViewportAspectRatio = true;

        CameraComponent();

        CameraComponent(float aspectRatio) : aspectRatio(aspectRatio), useViewportAspectRatio(false) {}

        CameraComponent(const Camera& camera)
            : camera(camera) {}

        CameraComponent(const Camera& camera, const float aspectRatio)
            : camera(camera), aspectRatio(aspectRatio), useViewportAspectRatio(false) {}

        CameraComponent(const CameraComponent&) = default;
    };

    struct PointLightComponent {
        float lightIntensity = 1.0f;

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent&) = default;

        PointLightComponent(const float intensity) : lightIntensity(intensity) {}
    };
} // namespace pxt