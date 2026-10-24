#include "include/parry.h"
#include "include/offsets.h"
#include "include/blockHandler.h"
#include "include/stunHandler.h"
#include "include/reactionHandler.h"
#include "include/data.h"
#include "include/Utils.h"
#include "include/lib/BlockSpark.h"

namespace Parry
{
	namespace
	{
		constexpr const char* kSettingsPath = "Data\\SKSE\\Plugins\\ValhallaCombat\\Settings.ini";
		constexpr const char* kSection = "Parry";

		Settings          g_settings;
		std::vector<HitStop::Item> g_items;
		std::once_flag    g_itemsOnce;

		std::shared_mutex                        g_timerLock;
		std::unordered_map<RE::FormID, float>    g_timers;   // seconds since the parry window opened
		std::atomic<bool>                        g_updating{ false };

		std::mutex                               g_costLock;
		std::unordered_map<RE::FormID, float>    g_costQueue;
		std::unordered_set<RE::FormID>           g_costSkip;

		float g_parryAngle = 0.0f;

		void BuildItems()
		{
			auto& s = g_settings;
			const auto B = [](const char* key, const char* label, const char* group, bool* v) { g_items.push_back({ key, label, group, nullptr, v, nullptr, 0, 0, 0 }); };
			const auto F = [](const char* key, const char* label, const char* group, float* v, float mn, float mx, int dec) {
				g_items.push_back({ key, label, group, v, nullptr, nullptr, mn, mx, dec });
			};
			HitStop::Item mode{ "iParryMode", "Parry system", "General", nullptr, nullptr, &s.mode, 0, 2, 0 };
			mode.options = "Off\0Elden Parry (built in)\0Parry for All\0";
			g_items.push_back(mode);
			B("bEnableWeaponParry", "Parry with a weapon bash", "General", &s.enableWeaponParry);
			B("bEnableShieldParry", "Parry with a shield bash", "General", &s.enableShieldParry);
			B("bEnableNPCParry", "NPCs parry too", "General", &s.enableNPCParry);
			B("bSuccessfulParryNoCost", "A successful parry costs no stamina", "General", &s.successfulParryNoCost);
			F("fParryWindow_Start", "Parry window opens after (s)", "General", &s.parryWindowStart, 0.0f, 1.0f, 2);
			F("fParryWindow_End", "Parry window closes after (s)", "General", &s.parryWindowEnd, 0.05f, 1.5f, 2);
			B("bEnableParrySoundEffect", "Parry sound", "Effects", &s.soundEffect);
			B("bEnableParrySparkEffect", "Parry sparks", "Effects", &s.sparkEffect);
			B("bEnableScreenShakeEffect", "Screen shake", "Effects", &s.screenShakeEffect);
			B("bEnableSlowTimeEffect", "Slow time on a parry", "Effects", &s.slowTimeEffect);
			B("bEnableShieldGuardBash", "Shield power bash is a guard bash", "Guard bash", &s.shieldGuardBash);
			B("bEnableWeaponGuardBash", "Weapon power bash is a guard bash", "Guard bash", &s.weaponGuardBash);
			B("bEnableArrowProjectileDeflection", "Deflect arrows with a bash", "Projectiles", &s.arrowDeflection);
			B("bEnableMagicProjectileDeflection", "Deflect spells with a bash", "Projectiles", &s.magicDeflection);
			F("fMeleeParryExp", "Block experience for a melee parry", "Experience", &s.meleeParryExp, 0.0f, 100.0f, 1);
			F("fProjectileParryExp", "Block experience for a projectile parry", "Experience", &s.projectileParryExp, 0.0f, 100.0f, 1);
			F("fGuardBashExp", "Block experience for a guard bash", "Experience", &s.guardBashExp, 0.0f, 100.0f, 1);
		}

		bool InBlockAngle(RE::Actor* a_blocker, RE::TESObjectREFR* a_obj)
		{
			const auto angle = a_blocker->GetHeadingAngle(a_obj->GetPosition(), false);
			return angle <= g_parryAngle && angle >= -g_parryAngle;
		}

		bool InParryState(RE::Actor* a_actor)
		{
			std::shared_lock lock(g_timerLock);
			const auto it = g_timers.find(a_actor->GetFormID());
			return it != g_timers.end() && it->second >= g_settings.parryWindowStart;
		}

		bool CanParry(RE::Actor* a_parrier, RE::TESObjectREFR* a_obj)
		{
			return InParryState(a_parrier) && InBlockAngle(a_parrier, a_obj);
		}

		RE::ATTACK_STATE_ENUM AttackState(RE::Actor* a_actor) { return a_actor->AsActorState()->GetAttackState(); }

		void NegateCost(RE::Actor* a_actor)
		{
			std::lock_guard lock(g_costLock);
			g_costSkip.insert(a_actor->GetFormID());
		}

		void PlayEffects(RE::Actor* a_actor)
		{
			const auto& s = g_settings;
			if (s.soundEffect) {
				ValhallaUtils::playSound(a_actor, Utils::Actor::isEquippedShield(a_actor) ? data::soundParryShield_perfect : data::soundParryWeapon_perfect);
			}
			if (s.sparkEffect) {
				MaxsuBlockSpark::blockSpark::playPerfectBlockSpark(a_actor);
			}
			if (a_actor->IsPlayerRef()) {
				if (s.slowTimeEffect) {
					inlineUtils::slowTime(0.2f, 0.3f);
				}
				if (s.screenShakeEffect) {
					RE::Offset::shakeCamera(1.5f, a_actor->GetPosition(), 0.4f);
				}
			}
		}

		void SendEvent(const char* a_name, RE::TESForm* a_sender)
		{
			SKSE::ModCallbackEvent modEvent{ RE::BSFixedString(a_name), RE::BSFixedString(), 0.0f, a_sender };
			SKSE::GetModCallbackEventSource()->SendEvent(&modEvent);
		}

		bool ProcessMeleeParry(RE::Actor* a_attacker, RE::Actor* a_parrier)
		{
			if (!a_attacker || !a_parrier || !CanParry(a_parrier, a_attacker)) {
				return false;
			}
			PlayEffects(a_parrier);
			reactionHandler::triggerStagger(a_parrier, a_attacker, reactionHandler::kLarge);
			stunHandler::GetSingleton()->processStunDamage(stunHandler::STUNSOURCE::parry, nullptr, a_parrier, a_attacker, 0);
			if (a_parrier->IsPlayerRef()) {
				RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, g_settings.meleeParryExp, nullptr, 0);
			}
			if (g_settings.successfulParryNoCost) {
				NegateCost(a_parrier);
			}
			SendEvent("EP_MeleeParryEvent", a_attacker);
			return true;
		}

		bool ProcessProjectileParry(RE::Actor* a_parrier, RE::Projectile* a_projectile, RE::hkpCollidable* a_collidable)
		{
			if (!CanParry(a_parrier, a_projectile)) {
				return false;
			}
			blockHandler::GetSingleton()->deflectForParry(a_parrier, a_projectile, a_collidable);
			PlayEffects(a_parrier);
			if (a_parrier->IsPlayerRef()) {
				RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, g_settings.projectileParryExp, nullptr, 0);
			}
			if (g_settings.successfulParryNoCost) {
				NegateCost(a_parrier);
			}
			SendEvent("EP_RangedParryEvent", nullptr);
			return true;
		}

		void ProcessGuardBash(RE::Actor* a_basher, RE::Actor* a_blocker)
		{
			if (!a_blocker->IsBlocking() || !InBlockAngle(a_blocker, a_basher) || AttackState(a_blocker) == RE::ATTACK_STATE_ENUM::kBash) {
				return;
			}
			reactionHandler::triggerStagger(a_basher, a_blocker, reactionHandler::kMedium);
			PlayEffects(a_basher);
			RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, g_settings.guardBashExp, nullptr, 0);
		}
	}

	Settings& Get() { return g_settings; }

	const std::vector<HitStop::Item>& Items()
	{
		std::call_once(g_itemsOnce, BuildItems);
		return g_items;
	}

	void ReadSettings()
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.LoadFile(kSettingsPath);
		for (const auto& item : Items()) {
			const char* v = ini.GetValue(kSection, item.key);
			if (!v) {
				continue;
			}
			if (item.f) {
				*item.f = static_cast<float>(std::atof(v));
			} else if (item.b) {
				*item.b = ini.GetBoolValue(kSection, item.key, *item.b);
			} else if (item.i) {
				*item.i = static_cast<int>(std::atof(v));
			}
		}
		if (g_settings.mode < kOff || g_settings.mode > kParryForAll) {
			g_settings.mode = kEldenParry;
		}
		if (g_settings.parryWindowEnd <= g_settings.parryWindowStart) {
			g_settings.parryWindowEnd = g_settings.parryWindowStart + 0.05f;  // an empty window would never parry
		}
		if (const auto gsc = RE::GameSettingCollection::GetSingleton()) {
			if (const auto setting = gsc->GetSetting("fCombatHitConeAngle")) {
				g_parryAngle = setting->GetFloat();
			}
		}
	}

	void Update()
	{
		if (!g_updating.load()) {
			return;
		}
		std::unique_lock lock(g_timerLock);
		for (auto it = g_timers.begin(); it != g_timers.end();) {
			if (it->second > g_settings.parryWindowEnd) {
				it = g_timers.erase(it);
			} else {
				it->second += *RE::Offset::g_deltaTime;
				++it;
			}
		}
		if (g_timers.empty()) {
			g_updating = false;
		}
	}

	void OnBlockStop(RE::Actor* a_actor)
	{
		if (!EldenParryActive() || !a_actor || AttackState(a_actor) != RE::ATTACK_STATE_ENUM::kBash) {
			return;
		}
		std::unique_lock lock(g_timerLock);
		g_timers[a_actor->GetFormID()] = 0.0f;
		g_updating = true;
	}

	void OnBashStop(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		const auto id = a_actor->GetFormID();
		{
			std::lock_guard lock(g_costLock);
			const auto it = g_costQueue.find(id);
			if (it != g_costQueue.end()) {
				if (!g_costSkip.contains(id)) {
					Utils::Actor::damageav(a_actor, RE::ActorValue::kStamina, it->second);  // the bash was free only if the parry worked
				}
				g_costQueue.erase(it);
			}
			g_costSkip.erase(id);
		}
		std::unique_lock lock(g_timerLock);
		g_timers.erase(id);
	}

	bool TakeBashCost(RE::Actor* a_actor, RE::BGSAttackData* a_attackData, float a_originalCost)
	{
		if (!EldenParryActive() || !g_settings.successfulParryNoCost || !a_actor || !a_attackData || !a_attackData->data.flags.any(RE::AttackData::AttackFlag::kBashAttack)) {
			return false;
		}
		std::lock_guard lock(g_costLock);
		g_costQueue[a_actor->GetFormID()] = a_originalCost;
		return true;
	}

	bool ShouldIgnoreMeleeHit(RE::Actor* a_aggressor, RE::Actor* a_victim)
	{
		if (!EldenParryActive() || !a_aggressor || !a_victim) {
			return false;
		}
		const auto& s = g_settings;
		if (AttackState(a_aggressor) == RE::ATTACK_STATE_ENUM::kBash) {
			const bool shield = Utils::Actor::isEquippedShield(a_aggressor);
			if (!Utils::Actor::isPowerAttacking(a_aggressor)) {
				// a light bash has no hit frame
				if ((a_aggressor->IsPlayerRef() || s.enableNPCParry) && ((shield && s.enableShieldParry) || s.enableWeaponParry)) {
					return true;
				}
			} else if ((a_aggressor->IsPlayerRef() || s.enableNPCParry) && ((shield && s.shieldGuardBash) || s.weaponGuardBash)) {
				ProcessGuardBash(a_aggressor, a_victim);
			}
		} else if (AttackState(a_victim) == RE::ATTACK_STATE_ENUM::kBash) {
			if (a_victim->IsPlayerRef() || s.enableNPCParry) {
				const bool shield = Utils::Actor::isEquippedShield(a_victim);
				if ((shield && s.enableShieldParry) || s.enableWeaponParry) {
					return ProcessMeleeParry(a_aggressor, a_victim);
				}
			}
		}
		return false;
	}

	bool ShouldIgnoreProjectile(RE::Projectile* a_projectile, RE::hkpAllCdPointCollector* a_collector)
	{
		if (!EldenParryActive() || !a_collector || !a_projectile) {
			return false;
		}
		const auto& s = g_settings;
		const bool  allowed = (a_projectile->GetProjectileRuntimeData().spell && s.magicDeflection) || s.arrowDeflection;
		if (!allowed) {
			return false;
		}
		for (auto& hit : a_collector->hits) {
			const auto refrA = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableA);
			const auto refrB = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableB);
			if (refrA && refrA->GetFormType() == RE::FormType::ActorCharacter) {
				const auto actor = refrA->As<RE::Actor>();
				if (AttackState(actor) == RE::ATTACK_STATE_ENUM::kBash && (actor->IsPlayerRef() || s.enableNPCParry)) {
					return ProcessProjectileParry(actor, a_projectile, const_cast<RE::hkpCollidable*>(hit.rootCollidableB));
				}
			}
			if (refrB && refrB->GetFormType() == RE::FormType::ActorCharacter) {
				const auto actor = refrB->As<RE::Actor>();
				if (AttackState(actor) == RE::ATTACK_STATE_ENUM::kBash && (actor->IsPlayerRef() || s.enableNPCParry)) {
					return ProcessProjectileParry(actor, a_projectile, const_cast<RE::hkpCollidable*>(hit.rootCollidableA));
				}
			}
		}
		return false;
	}
}
