#pragma once

#include "core/constants.hpp"
#include "core/obj_picking_id.hpp"
#include "core/pch.hpp"
#include "core/uid.hpp"
#include "resources/asset_handle.hpp"
#include "resources/types/material.hpp"
#include "resources/types/mesh.hpp"
#include "scene/camera_data.hpp"

namespace pxt {
    struct IDComponent {
        core::UID uid;

        IDComponent() : uid(core::UID()) {}

        IDComponent(core::UID uid) : uid(uid) {}

        IDComponent(const IDComponent&) = default;

        // Conversion operators
        operator core::UID&() { return uid; }

        operator const core::UID&() const { return uid; }
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

        NameComponent(const NameComponent&) = default;

        NameComponent(const std::string& name) : name(name) {}

        // Conversion operators
        operator std::string&() { return name; }

        operator const std::string&() const { return name; }
    };

    // TODO: remember to enforce that an entity cannot have both a 2D and a 3D component
    struct Transform2dComponent {
        glm::vec2 translation{0.f, 0.f};
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
        glm::vec3 translation{0.f, 0.f, 0.f};
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::vec3 rotation{0.f, 0.f, 0.f};

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
    
    //! These three components should not be here, but rather in a separate module for editor-only components
    //! The properties is fine but editor-only fields should be excluded, as well as the visibility tag
    // TODO: when we will do reflection (entt::meta or other), we can move these to editor-only module
    struct PropertiesComponent {
        std::string tag = "Default"; // For searching/grouping

        // User Intent
        bool isEditorVisible = true;    // Affects the viewport/Editor
        bool isRenderable = true;       // Affects the Vulkan pass
        bool isStatic = false;          // Optimization hint (especially T/BLAS)
        
        // Prevents accidental movement in UI
        // TODO: this should be a per-axis && operation lock, but for now it's a simple global lock for the entire transform
        // component
        bool isLocked = false;

        PropertiesComponent() = default;
    };

    // if an entity has this tag, it will be rendered
    struct RenderableTag {};

    // if an entity has this tag, it will be considered visible in the editor viewport
    struct VisibilityTag {};

    struct ColorComponent {
        glm::vec3 color{1.f, 1.f, 1.f};

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
            Shared<Image> densityTexture = nullptr;
            Shared<Image> detailTexture = nullptr; // for edge details of the volume
        };

        Volume volume{};

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
        AssetHandle material{};
        float tilingFactor = 1.0f;
        glm::vec3 tint{1.0f};

        MaterialComponent() = default;

        MaterialComponent(const MaterialComponent&) = default;

        MaterialComponent(const AssetHandle material, float tilingFactor, const glm::vec3& tint)
            : material(material), tilingFactor(tilingFactor), tint(tint) {}

        struct Builder {
            AssetHandle material;
            float tilingFactor = 1.0f;
            glm::vec3 tint{1.0f};

            Builder& setMaterial(const AssetHandle mat) {
                material = mat;
                return *this;
            }

            Builder& setTilingFactor(float factor) {
                tilingFactor = factor;
                return *this;
            }

            Builder& setTint(const glm::vec3& color) {
                tint = color;
                return *this;
            }

            MaterialComponent build() { return MaterialComponent(material, tilingFactor, tint); }
        };
    };

    struct MeshComponent {
        AssetHandle mesh{};

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;

        MeshComponent(const AssetHandle mesh) : mesh(mesh) {}
    };

    struct CameraComponent {
        CameraData cameraData;
        float aspectRatio = 1.f;
        bool useViewportAspectRatio = true;

        CameraComponent();

        CameraComponent(float aspectRatio) : aspectRatio(aspectRatio), useViewportAspectRatio(false) {}

        CameraComponent(const CameraData& cameraData) : cameraData(cameraData) {}

        CameraComponent(const CameraData& cameraData, const float aspectRatio)
            : cameraData(cameraData), aspectRatio(aspectRatio), useViewportAspectRatio(false) {}

        CameraComponent(const CameraComponent&) = default;
    };

    struct PointLightComponent {
        float lightIntensity = 1.0f;
        glm::vec3 lightColor = glm::vec3(1.f, 1.f, 1.f);

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent&) = default;

        PointLightComponent(const float intensity) : lightIntensity(intensity), lightColor(glm::vec3(1.f)) {}

        PointLightComponent(float intensity, const glm::vec3& lightColor)
            : lightIntensity(intensity), lightColor(lightColor) {}
    };

    class Script; // Forward declaration of Script class

    struct ScriptComponent {
        Script* script = nullptr;

        ScriptComponent() = default;

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

    template <typename... Component>
    struct ComponentList {};

    using CoreComponents = ComponentList<IDComponent, NameComponent, ObjPickingIdComponent, PropertiesComponent>;

    using AttachableComponents =
        ComponentList<TransformComponent, Transform2dComponent, ColorComponent, VolumeComponent, MaterialComponent,
                      MeshComponent, CameraComponent, PointLightComponent, ScriptComponent>;

    // Helper to check if T is in a pack of Us
    template <typename T, typename List>
    struct IsInList;

    // disjunction is like a logical OR for type traits
    template <typename T, typename... Us>
    struct IsInList<T, ComponentList<Us...>> : std::disjunction<std::is_same<T, Us>...> {};

    template <typename Component>
    struct IsCoreComponent : IsInList<Component, CoreComponents> {};
} // namespace pxt