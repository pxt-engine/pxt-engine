#include "resources/importers/texture_importer.hpp"

#include "core/pch.hpp"
#include "graphics/resources/texture2d.hpp"
#include "ui/widgets/dropdown.hpp"

#include <stb_image.h>

namespace pxt {

    Shared<Image> TextureImporter::import(ResourceManager& rm, const std::filesystem::path& filePath,
                                          ResourceInfo* resourceInfo) {

        ImageInfo imageInfo{};

        if (resourceInfo != nullptr) {
            if (const auto* info = dynamic_cast<ImageInfo*>(resourceInfo)) {
                imageInfo = *info;
            } else {
                throw std::runtime_error("TextureImporter - Invalid resourceInfo type: not ImageInfo");
            }
        } else {
            imageInfo.format = ImageFormat::RGBA8_LINEAR; // Default format
        }

        int width, height, channels;

        // Currently every image is loaded as RGBA
        constexpr uint16_t requestedChannels = STBI_rgb_alpha;

        uint8_t* bytes;

        uint32_t channelBitsPerPixel = getChannelBytePerPixelForFormat(imageInfo.format);

        switch (channelBitsPerPixel) {
        case 1:
            bytes = stbi_load(filePath.string().c_str(), &width, &height, &channels, requestedChannels);
            break;
        case 2:
            bytes = (uint8_t*)stbi_load_16(filePath.string().c_str(), &width, &height, &channels, requestedChannels);
            break;
        case 4:
            bytes = (uint8_t*)stbi_loadf(filePath.string().c_str(), &width, &height, &channels, requestedChannels);
            break;
        default:
            throw std::runtime_error("Unsupported channel bits per pixel: " + std::to_string(channelBitsPerPixel));
        }

        auto size = width * height * requestedChannels * channelBitsPerPixel;

        std::span<uint8_t> pixels(bytes, size);

        if (size <= 0 || bytes == nullptr) {
            free(bytes);
            throw std::runtime_error("failed to load image from file: " + filePath.string());
        }

        imageInfo.width = static_cast<uint32_t>(width);
        imageInfo.height = static_cast<uint32_t>(height);
        imageInfo.channels = static_cast<uint16_t>(requestedChannels);

        switch (imageInfo.type) {
        case ImageType::CUBE_MAP:
            // TODO: implement cube map import
            return nullptr;
        case ImageType::TEXTURE_2D:
        default:
            return Texture2D::create(imageInfo, pixels);
        }
    }

    void TextureImporter::updateUi(ResourceInfo* resourceInfo) {
        ImGui::SeparatorText("Texture Importer Settings");

        ImageInfo* imageInfo = dynamic_cast<ImageInfo*>(resourceInfo);
        if (!imageInfo) {
            ImGui::Text("Invalid resource info for TextureImporter");
            return;
        }

        int currentType = static_cast<int>(imageInfo->type);
        ui::Dropdown::render("Image Type", currentType, std::span(s_imageTypeNames));
        imageInfo->type = static_cast<ImageType>(currentType);

        int currentFormat = static_cast<int>(imageInfo->format);
        ui::Dropdown::render("Image Format", currentFormat, std::span(s_imageFormatNames));
        imageInfo->format = static_cast<ImageFormat>(currentFormat);
    }
} // namespace pxt