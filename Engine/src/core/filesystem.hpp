#pragma once

#include "tinyfiledialogs.h"

namespace PXTEngine {
	class FileSystem {
	public:
		static std::string openFileDialog();
		static void openErrorModal(const std::string& message);
	};
}