#pragma once

#include "core/pch.hpp"
#include "resources/resource.hpp"

namespace pxt {

    /**
     * @enum ImageFormat
     *
     * @brief Enum representing different image formats.
     * This enum is used to specify the format of images in the engine.
     */
    enum ImageFormat : uint8_t {
        RGB8_SRGB = 0,
        RGBA8_SRGB,
        RGB8_LINEAR,
        RGBA32_LINEAR,
        RGBA8_LINEAR,
    };

    // TODO: better way to get image format names, with array length check
    static const char* s_imageFormatNames[] = {"RGB8_SRGB", "RGBA8_SRGB", "RGB8_LINEAR", "RGBA32_LINEAR",
                                               "RGBA8_LINEAR"};

    enum ImageType : uint8_t {
        TEXTURE_2D = 0,
        CUBE_MAP,
    };

    static const char* s_imageTypeNames[] = {
        "TEXTURE_2D",
        "CUBE_MAP",
    };

    enum CubeMapLayout : uint8_t {
        LAYOUT_1x6 = 0, // 1 row,  6 columns
        LAYOUT_3x2,     // 3 rows, 2 columns
        LAYOUT_2x3,     // 2 rows, 3 columns
        LAYOUT_6x1,     // 6 rows, 1 column
        SEPARATE_FILES  // Separate files for each face
    };

    inline uint32_t getChannelBytePerPixelForFormat(ImageFormat format) {
        switch (format) {
        case ImageFormat::RGB8_SRGB:
        case ImageFormat::RGB8_LINEAR:
        case ImageFormat::RGBA8_SRGB:
        case ImageFormat::RGBA8_LINEAR:
            return 1;
        case ImageFormat::RGBA32_LINEAR:
            return 4;
        default:
            throw std::runtime_error("Unknown image format");
        }
    }

    /**
     * @enum ImageFiltering
     *
     * @brief Enum representing different image filtering options.
     * This enum is used to specify how images should be filtered when sampled.
     */
    enum class ImageFiltering : uint8_t {
        Nearest = 0, // Nearest neighbor filtering
        Linear,      // Linear filtering
    };

    /**
     * @enum ImageFlags
     *
     * @brief Enum representing different image flags.
     * This enum is used to specify additional properties or behaviors of images.
     */
    enum class ImageFlags : int32_t {
        None = 0,                         // No flags set
        UnnormalizedCoordinates = 1 << 0, // Use unnormalized coordinates for sampling
    };

    inline ImageFlags operator&(ImageFlags a, ImageFlags b) {
        return static_cast<ImageFlags>(static_cast<int>(a) & static_cast<int>(b));
    }

    inline ImageFlags operator|(ImageFlags a, ImageFlags b) {
        return static_cast<ImageFlags>(static_cast<int>(a) | static_cast<int>(b));
    }

    /**
     * @struct ImageInfo
     *
     * @brief Struct representing additional information about an image resource.
     * This struct can be used to store metadata or other relevant information about the image.
     */
    struct ImageInfo : public ResourceInfo {
        uint32_t width = 0;
        uint32_t height = 0;
        uint16_t channels = 0;
        ImageFormat format = RGBA8_LINEAR;
        ImageType type = TEXTURE_2D;
        ImageFiltering filtering = ImageFiltering::Linear;
        ImageFlags flags = ImageFlags::None;
        CubeMapLayout cubeMapLayout = CubeMapLayout::LAYOUT_1x6;

        ImageInfo() = default;

        ImageInfo(const uint32_t width, const uint32_t height, const uint16_t channels,
                  const ImageFormat format = RGBA8_SRGB, ImageFiltering filtering = ImageFiltering::Linear,
                  ImageFlags flags = ImageFlags::None)
            : width(width), height(height), channels(channels), format(format), filtering(filtering), flags(flags) {}

        ImageInfo(const ImageInfo& other) = default;
    };

    /**
     * @class Image
     *
     * @brief Represents an image resource used for rendering.
     */
    class Image : public Resource {
    public:
        virtual uint32_t getWidth() = 0;
        virtual uint32_t getHeight() = 0;
        virtual uint16_t getChannels() = 0;
        virtual ImageFormat getFormat() = 0;

        static Type getStaticType() { return Type::Image; }
    };
} // namespace pxt