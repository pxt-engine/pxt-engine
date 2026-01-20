#pragma once

#include "core/pch.hpp"
#include "core/uuid.hpp"

namespace pxt {

    using ResourceId = core::UUID;

    /**
     * @struct ResourceInfo
     *
     * @brief Base class for resource information.
     * It's used to store additional information about resources.
     */
    struct ResourceInfo {
        virtual ~ResourceInfo() = default;
    };

    /**
     * @class Resource
     *
     * @brief Base class for all resources in the engine.
     */
    class Resource {
    public:
        enum class Type : uint8_t {
            Image,
            Model,
            Mesh,
            Material,
        };

        const std::string typeToString() { return Resource::typeToString(getType()); }

        static const std::string typeToString(Type resourceType) {
            switch (resourceType) {
            case Type::Image:
                return "Image";
            case Type::Model:
                return "Model";
            case Type::Mesh:
                return "Mesh";
            case Type::Material:
                return "Material";
            default:
                return "Unknown";
            }
        }

        Resource() = default;
        virtual ~Resource() = default;

        virtual Type getType() const = 0;

        // The default constructor is called and the ID is set to a new UUID.
        ResourceId id;
        std::string alias = "";
    };

} // namespace pxt