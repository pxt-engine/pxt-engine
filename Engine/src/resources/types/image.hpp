#pragma once

#include "core/pch.hpp"
#include "resources/resource.hpp"

namespace PXTEngine {

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
		Nearest = 0,  // Nearest neighbor filtering
		Linear,       // Linear filtering
	};

	/**
	 * @enum ImageFlags
	 *
	 * @brief Enum representing different image flags.
	 * This enum is used to specify additional properties or behaviors of images.
	 */
	enum class ImageFlags : int32_t {
		None = 0,          // No flags set
		UnnormalizedCoordinates = 1 << 0, // Use unnormalized coordinates for sampling
	};

	inline ImageFlags operator&(ImageFlags a, ImageFlags b)
	{
		return static_cast<ImageFlags>(static_cast<int>(a) & static_cast<int>(b));
	}

	inline ImageFlags operator|(ImageFlags a, ImageFlags b)
	{
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
		ImageFormat format = RGBA8_SRGB;
		ImageFiltering filtering = ImageFiltering::Linear;
		ImageFlags flags = ImageFlags::None;

		ImageInfo() = default;
		ImageInfo(const uint32_t width, const uint32_t height, const uint16_t channels, 
			const ImageFormat format = RGBA8_SRGB, ImageFiltering filtering = ImageFiltering::Linear,
			ImageFlags flags = ImageFlags::None)
			: width(width), height(height), channels(channels), format(format),
			filtering(filtering), flags(flags) {}
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
}