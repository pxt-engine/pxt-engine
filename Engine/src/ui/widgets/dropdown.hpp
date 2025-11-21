#pragma once

#include "core/pch.hpp"

namespace PXTEngine::UI {
	class Dropdown {
	public:
		static void render(const char* label, int& currentItem, std::span<const char*> itemsName);
	};
}