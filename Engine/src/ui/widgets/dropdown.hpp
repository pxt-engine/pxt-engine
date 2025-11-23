#pragma once

#include "core/pch.hpp"

namespace pxt::ui {
	class Dropdown {
	public:
		static void render(const char* label, int& currentItem, std::span<const char*> itemsName);
	};
}