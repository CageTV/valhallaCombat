#pragma once

#include <string_view>

// Stands in for the Plugin.h that cmake/Plugin.h.in generates in the original build.
namespace Plugin
{
	using namespace std::literals;

	inline constexpr REL::Version VERSION{ 1u, 0u, 0u };

	inline constexpr auto NAME = "ValhallaCombat"sv;
}
