#include "resources/importers/texture_importer.hpp"

#include "core/pch.hpp"
#include "core/buffer.hpp"
#include "graphics/resources/texture2d.hpp"

#include <stb_image.h>

namespace PXTEngine {

	Shared<Image> TextureImporter::import(ResourceManager& rm, const std::filesystem::path& filePath,
		ResourceInfo* resourceInfo) {

        ImageInfo imageInfo{};

        if (resourceInfo != nullptr) {
            if (const auto* info = dynamic_cast<ImageInfo*>(resourceInfo)) {
                imageInfo = *info;
            }
            else {
                throw std::runtime_error("TextureImporter - Invalid resourceInfo type: not ImageInfo");
            }
        }
        else {
			imageInfo.format = ImageFormat::RGBA8_SRGB; // Default format
        }

		int width, height, channels;

        // Currently every image is loaded as RGBA
		constexpr uint16_t requestedChannels = STBI_rgb_alpha;

		Buffer pixels;

		uint32_t channelBitsPerPixel = getChannelBytePerPixelForFormat(imageInfo.format);

		switch (channelBitsPerPixel) {
		case 1:
			pixels.bytes = stbi_load(
				filePath.string().c_str(),
				&width,
				&height,
				&channels,
				requestedChannels
			);
			break;
		case 2:
			pixels.bytes = (uint8_t*) stbi_load_16(
				filePath.string().c_str(),
				&width,
				&height,
				&channels,
				requestedChannels
			);
			break;
		case 4:
			pixels.bytes = (uint8_t*) stbi_loadf(
				filePath.string().c_str(),
				&width,
				&height,
				&channels,
				requestedChannels
			);
			break;
		default:
			throw std::runtime_error("Unsupported channel bits per pixel: " + std::to_string(channelBitsPerPixel));
		}

		pixels.size = width * height * requestedChannels * channelBitsPerPixel;
		
		if (!pixels) {
            pixels.release();
			throw std::runtime_error("failed to load image from file: " + filePath.string());
		}

		imageInfo.width = static_cast<uint32_t>(width);
		imageInfo.height = static_cast<uint32_t>(height);
		imageInfo.channels = static_cast<uint16_t>(requestedChannels);

		return Texture2D::create(imageInfo, pixels);
	}
}