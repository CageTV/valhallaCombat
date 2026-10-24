#pragma once

#include <string_view>
#include "FormIdMap.h"

namespace VCIds
{
	/* The light ValhallaCombat.esp renumbered its records (FormIdMap.h). Files that name the old FormIDs (Killmoves.ini and other plugins' configs) keep working:
	a FormID of ValhallaCombat.esp is translated when the loaded plugin is the light one, and left alone when it is the original. */
	inline RE::FormID Remap(std::string_view a_plugin, RE::FormID a_id)
	{
		if (a_plugin.size() != 18) {
			return a_id;
		}
		constexpr std::string_view name = "ValhallaCombat.esp";
		for (std::size_t i = 0; i < name.size(); ++i) {
			if (std::tolower(static_cast<unsigned char>(a_plugin[i])) != std::tolower(static_cast<unsigned char>(name[i]))) {
				return a_id;
			}
		}
		static const bool light = [] {
			const auto dh = RE::TESDataHandler::GetSingleton();
			const auto file = dh ? dh->LookupModByName(name) : nullptr;
			return file && file->IsLight();
		}();
		if (!light) {
			return a_id;
		}
		for (const auto& p : kMap) {
			if (p.oldId == (a_id & 0xFFFFFF)) {
				return p.newId;
			}
		}
		return a_id;
	}
}
