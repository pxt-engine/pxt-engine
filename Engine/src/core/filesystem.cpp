#include "core/filesystem.hpp"

namespace PXTEngine {
	std::string FileSystem::openFileDialog() {
		const char* path = tinyfd_openFileDialog(
			"Select File to Import",
			"", // default path / file
			0, // num filters
			nullptr, // filters
			nullptr,
			0 // allow multiple select
		);

		if (path) {
			std::string fullPath(path);

			return fullPath;
		}

		return "";
	}
}