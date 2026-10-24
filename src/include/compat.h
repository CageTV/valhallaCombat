#pragma once

/* The two CommonLibSSE-NG trees Valhalla Combat is built with differ in a few game API calls. The current build (alandtse's tree, Skyrim 1.6.1170 and newer) and the legacy
build (CharmedBaryon's 3.7.0, Skyrim SE 1.5.97 and AE before 1.6.1170, VC_LEGACY_BUILD) call the same helpers here, so the plugin code is written once. */
#ifdef VC_LEGACY_BUILD
// The legacy tree has no fmt formatter for BSFixedString (the log lines print them directly).
template <>
struct fmt::formatter<RE::BSFixedString> : fmt::formatter<std::string_view>
{
	auto format(const RE::BSFixedString& a_string, fmt::format_context& a_ctx) const
	{
		return fmt::formatter<std::string_view>::format(a_string.c_str() ? std::string_view(a_string.c_str()) : std::string_view(), a_ctx);
	}
};
#endif

namespace Compat
{
	inline bool IsStaggered(RE::ActorState* a_state)
	{
#ifdef VC_LEGACY_BUILD
		return a_state->actorState2.staggered;
#else
		return a_state->IsStaggered();
#endif
	}

	inline void AddSkillExperience(RE::PlayerCharacter* a_player, RE::ActorValue a_skill, float a_experience)
	{
#ifdef VC_LEGACY_BUILD
		a_player->AddSkillExperience(a_skill, a_experience);
#else
		a_player->AddSkillExperience(a_skill, a_experience, nullptr, 0);  // four arguments on 1.7; the two-argument call leaves garbage in the others
#endif
	}

	inline void SetBool(RE::Setting* a_setting, bool a_value)
	{
#ifdef VC_LEGACY_BUILD
		a_setting->data.b = a_value;
#else
		a_setting->SetBool(a_value);
#endif
	}

	/*The difficulty setting, compared with the RE::DIFFICULTY values of offsets.h.*/
	inline auto GetDifficulty()
	{
#ifdef VC_LEGACY_BUILD
		return RE::PlayerCharacter::GetSingleton()->GetGameStatsData().difficulty;
#else
		return RE::PlayerCharacter::GetSingleton()->GetPlayerRuntimeData().difficulty;
#endif
	}

	/*Damages (negative) or restores (positive) the damage modifier of an actor value.*/
	inline void ModDamageModifier(RE::Actor* a_actor, RE::ActorValue a_value, float a_delta)
	{
#ifdef VC_LEGACY_BUILD
		a_actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, a_value, a_delta);
#else
		a_actor->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, a_value, a_delta);
#endif
	}

	inline void SetProjectileCollisionOwner(RE::Actor* a_actor, RE::hkpCollidable* a_collidable)
	{
#ifdef VC_LEGACY_BUILD
		std::uint32_t filter = 0;
		a_actor->GetCollisionFilterInfo(filter);
		a_collidable->broadPhaseHandle.collisionFilterInfo &= (0x0000FFFF);
		a_collidable->broadPhaseHandle.collisionFilterInfo |= (filter << 16);
#else
		RE::CFilter filter;  // GetCollisionFilterInfo takes RE::CFilter& in this tree
		a_actor->GetCollisionFilterInfo(filter);
		a_collidable->broadPhaseHandle.collisionFilterInfo.filter &= (0x0000FFFF);
		a_collidable->broadPhaseHandle.collisionFilterInfo.filter |= (filter.filter << 16);
#endif
	}

	inline void ShowMessageBox(RE::MessageBoxData* a_box)
	{
#ifdef VC_LEGACY_BUILD
		a_box->unk4C = 4;
		a_box->unk38 = 10;
		a_box->QueueMessage();
#else
		a_box->buttonPressOffset = 4;  // was unk4C (offset 0x4C)
		a_box->warningType = 10;       // was unk38 (offset 0x38)
		RE::MessageBoxMenu::QueueMessage(a_box);
#endif
	}
}
