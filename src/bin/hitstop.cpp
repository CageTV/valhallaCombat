#include "include/hitstop.h"
#include "include/offsets.h"
#include "include/Hooks.h"

namespace HitStop
{
	namespace
	{
		constexpr const char* kSettingsPath = "Data\\SKSE\\Plugins\\ValhallaCombat\\Settings.ini";
		constexpr const char* kSection = "HitStop";

		Settings            g_settings;
		std::vector<Item>   g_items;
		std::once_flag      g_itemsOnce;

		/* ---------- settings table ---------- */

		void BuildItems()
		{
			auto& s = g_settings;
			const auto B = [](const char* key, const char* label, const char* group, bool* v) { g_items.push_back({ key, label, group, nullptr, v, nullptr, 0, 0, 0 }); };
			const auto F = [](const char* key, const char* label, const char* group, float* v, float mn, float mx, int dec) {
				g_items.push_back({ key, label, group, v, nullptr, nullptr, mn, mx, dec });
			};

			HitStop::Item method{ "iFrameWork", "Method", "General", nullptr, nullptr, &s.framework, 0, 1, 0 };
			method.options = "Per-actor animation speed\0Global time multiplier (player only)\0";
			g_items.push_back(method);
			B("bPChitStop", "Hit stop when the player hits", "General", &s.pcHitStop);
			B("bNPChitStop", "Hit stop when NPCs hit", "General", &s.npcHitStop);
			B("bStopOnCreature", "Stop on living creatures and people", "General", &s.stopOnCreature);
			B("bStopOnDead", "Stop on dead bodies", "General", &s.stopOnDead);
			B("bStopOnObject", "Stop on objects and walls", "General", &s.stopOnObject);
			B("bStopOnBash", "Stop on bashes", "General", &s.stopOnBash);
			B("bStopOnBlocked", "Stop on blocked hits", "General", &s.stopOnBlocked);
			B("bpcShake", "Screen shake when the player hits", "Screen shake", &s.pcShake);
			B("bshakeOnLight", "Shake on light attacks too (otherwise power attacks only)", "Screen shake", &s.shakeOnLight);
			B("bshakeOnCreature", "Shake on living creatures and people", "Screen shake", &s.shakeOnCreature);
			B("bshakeOnDead", "Shake on dead bodies", "Screen shake", &s.shakeOnDead);
			B("bshakeOnObject", "Shake on objects and walls", "Screen shake", &s.shakeOnObject);
			B("bshakeOnBash", "Shake on bashes", "Screen shake", &s.shakeOnBash);
			B("bshakeOnBlocked", "Shake on blocked hits", "Screen shake", &s.shakeOnBlocked);

			// base values: [Kind][StopType]
			static const char* const baseKeys[4][4] = {
				{ "fstopOnObject", "fstopOnBash", "fstopOnBlocked", "fstopOnCreature" },
				{ "fstopOnObjectSpd", "fstopOnBashSpd", "fstopOnBlockedSpd", "fstopOnCreatureSpd" },
				{ "fShakeOnObject", "fShakeOnBash", "fShakeOnBlocked", "fShakeOnCreature" },
				{ "fobjectShakeMagnitude", "fbashShakeMagnitude", "fblockedShakeMagnitude", "fcreatureShakeMagnitude" },
			};
			static const char* const typeName[4] = { "objects", "bashes", "blocked hits", "creatures" };
			static const char* const kindGroup[4] = { "Stop time (seconds)", "Stop speed (1 = no slowdown, 0 = frozen)", "Shake time (seconds)", "Shake strength" };
			static const char* const kindLabel[4] = { "Stop time on ", "Stop speed on ", "Shake time on ", "Shake strength on " };
			static std::string labels[4][4];
			for (int k = 0; k < 4; ++k) {
				for (int t = 0; t < 4; ++t) {
					labels[k][t] = std::string(kindLabel[k]) + typeName[t];
					F(baseKeys[k][t], labels[k][t].c_str(), kindGroup[k], &s.base[k][t], 0.0f, k == 3 ? 2.0f : (k == 1 ? 1.0f : 1.0f), 2);
				}
			}

			static const char* const powerKeys[4] = { "fstopTimePowerMult", "fstopSpeedPowerMult", "fShakeTimePowerMult", "fShakeMagnitudePowerMult" };
			static const char* const kindShort[4] = { "stop time", "stop speed", "shake time", "shake strength" };
			static std::string powerLabels[4];
			for (int k = 0; k < 4; ++k) {
				powerLabels[k] = std::string("Power attack multiplier, ") + kindShort[k];
				F(powerKeys[k], powerLabels[k].c_str(), "Multipliers", &s.power[k], 0.0f, 5.0f, 2);
			}

			static const char* const weaponKeys[4][wCount] = {
				{ "fstopTimeHandToHandMult", "fstopTimeDaggerMult", "fstopTimeSwordMult", "fstopTimeGreatSwordMult", "fstopTimeAxeMult", "fstopTimeMaceMult", "fstopTime2hwMult" },
				{ "fstopSpeedHandToHandMult", "fstopSpeedDaggerMult", "fstopSpeedSwordMult", "fstopSpeedGreatSwordMult", "fstopSpeedAxeMult", "fstopSpeedMaceMult", "fstopSpeed2hwMult" },
				{ "fShakeTimeHandToHandMult", "fShakeTimeDaggerMult", "fShakeTimeSwordMult", "fShakeTimeGreatSwordMult", "fShakeTimeAxeMult", "fShakeTimeMaceMult", "fShakeTime2hwMult" },
				{ "fShakeMagnitudeHandToHandMult", "fShakeMagnitudeDaggerMult", "fShakeMagnitudeSwordMult", "fShakeMagnitudeGreatSwordMult", "fShakeMagnitudeAxeMult", "fShakeMagnitudeMaceMult", "fShakeMagnitude2hwMult" },
			};
			static const char* const weaponName[wCount] = { "unarmed", "daggers", "swords", "greatswords", "axes", "maces", "two-handed axes and hammers" };
			static std::string weaponLabels[4][wCount];
			for (int k = 0; k < 4; ++k) {
				for (int w = 0; w < wCount; ++w) {
					weaponLabels[k][w] = std::string(kindShort[k]) + ", " + weaponName[w];
					F(weaponKeys[k][w], weaponLabels[k][w].c_str(), "Multipliers", &s.weapon[k][w], 0.0f, 5.0f, 2);
				}
			}
		}

		/* ---------- animation speed ---------- */

		struct AnimSpeedData
		{
			float speedMult;
			float dur;
		};
		std::unordered_map<RE::ActorHandle, AnimSpeedData> g_animSpeeds;
		std::mutex                                         g_animLock;

		void SetAnimSpeed(RE::ActorHandle a_handle, float a_speed, float a_dur)
		{
			std::lock_guard lock(g_animLock);
			g_animSpeeds[a_handle] = { a_speed, a_dur };
		}

		/* Scales this frame's animation delta of the actor while its hit stop lasts. */
		void ScaleDelta(RE::ActorHandle a_handle, float& a_delta)
		{
			if (a_delta <= 0.0f) {
				return;
			}
			std::lock_guard lock(g_animLock);
			const auto it = g_animSpeeds.find(a_handle);
			if (it == g_animSpeeds.end()) {
				return;
			}
			const float speed = it->second.speedMult;
			const float remaining = it->second.dur - a_delta;
			if (remaining <= 0.0f) {
				const float inside = (a_delta + remaining) / a_delta;  // the part of this frame that was still inside the stop
				g_animSpeeds.erase(it);
				a_delta *= speed * inside + (1.0f - inside);
			} else {
				it->second.dur = remaining;
				a_delta *= speed;
			}
		}

		struct PlayerAnimHook
		{
			static void thunk(RE::PlayerCharacter* a_this, float a_delta)
			{
				ScaleDelta(a_this->GetHandle(), a_delta);
				func(a_this, a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct NpcAnimHook
		{
			static void thunk(RE::Actor* a_this, float a_delta)
			{
				ScaleDelta(a_this->GetHandle(), a_delta);
				func(a_this, a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		/* ---------- global time multiplier method ---------- */

		float g_sgtmTime = 0.0f;
		bool  g_sgtmOngoing = false;

		/* ---------- values ---------- */

		float Value(Kind a_kind, StopType a_type, RE::WEAPON_TYPE a_weapon, bool a_power)
		{
			const auto& s = g_settings;
			float       v = s.base[a_kind][a_type];
			switch (a_weapon) {
			case RE::WEAPON_TYPE::kHandToHandMelee: v *= s.weapon[a_kind][wHandToHand]; break;
			case RE::WEAPON_TYPE::kOneHandDagger: v *= s.weapon[a_kind][wDagger]; break;
			case RE::WEAPON_TYPE::kOneHandSword: v *= s.weapon[a_kind][wSword]; break;
			case RE::WEAPON_TYPE::kOneHandMace: v *= s.weapon[a_kind][wMace]; break;
			case RE::WEAPON_TYPE::kOneHandAxe: v *= s.weapon[a_kind][wAxe]; break;
			case RE::WEAPON_TYPE::kTwoHandAxe: v *= s.weapon[a_kind][w2hw]; break;
			case RE::WEAPON_TYPE::kTwoHandSword: v *= s.weapon[a_kind][wGreatSword]; break;
			default: break;
			}
			if (a_power) {
				v *= s.power[a_kind];
			}
			return v;
		}

		void Stop(float a_time, float a_speed, RE::Actor* a_actor)
		{
			if (g_settings.framework == 0) {
				SetAnimSpeed(a_actor->GetHandle(), a_speed, a_time);
			} else if (a_actor->IsPlayerRef()) {
				if (a_actor->AsMagicTarget()->HasEffectWithArchetype(RE::MagicTarget::Archetype::kSlowTime)) {
					return;  // do not fight a slow time spell over the global multiplier
				}
				g_sgtmTime = a_time;
				g_sgtmOngoing = true;
				RE::Offset::SGTM(a_speed);
			}
		}

		bool ShakeAllowed(StopType a_type, bool a_dead)
		{
			const auto& s = g_settings;
			if (a_dead) {
				return s.shakeOnDead;
			}
			switch (a_type) {
			case objectStop: return s.shakeOnObject;
			case bashStop: return s.shakeOnBash;
			case blockedStop: return s.shakeOnBlocked;
			default: return s.shakeOnCreature;
			}
		}

		void CalculateStop(bool a_power, RE::Actor* a_hitter, RE::TESObjectWEAP* a_weapon, StopType a_type, bool a_dead)
		{
			const auto wpn = a_weapon->GetWeaponType();
			Stop(Value(kStopTime, a_type, wpn, a_power), Value(kStopSpeed, a_type, wpn, a_power), a_hitter);
			if (a_hitter->IsPlayerRef() && g_settings.pcShake && (a_power || g_settings.shakeOnLight) && ShakeAllowed(a_type, a_dead)) {
				RE::Offset::shakeCamera(Value(kShakeMagnitude, a_type, wpn, a_power), a_hitter->GetPosition(), Value(kShakeTime, a_type, wpn, a_power));
			}
		}

		bool IsPowerAttacking(RE::Actor* a_actor)
		{
			const auto process = a_actor->GetActorRuntimeData().currentProcess;
			if (process && process->high && process->high->attackData) {
				const auto flags = process->high->attackData->data.flags;
				return flags.any(RE::AttackData::AttackFlag::kPowerAttack) && !flags.any(RE::AttackData::AttackFlag::kBashAttack);
			}
			return false;
		}

		/* ---------- hit event ---------- */

		class HitSink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (!a_event || !a_event->cause || !a_event->target || !a_event->source) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(a_event->source);
				if (!weapon || !weapon->IsWeapon() || !weapon->IsMelee()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto cause = a_event->cause.get();
				if (!cause) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (cause->IsPlayerRef()) {
					if (!g_settings.pcHitStop) {
						return RE::BSEventNotifyControl::kContinue;
					}
				} else if (cause->GetFormType() == RE::FormType::ActorCharacter) {
					if (!g_settings.npcHitStop || g_settings.framework == 1) {
						return RE::BSEventNotifyControl::kContinue;  // the global multiplier would stop the player instead
					}
				} else {
					return RE::BSEventNotifyControl::kContinue;
				}
				Process(a_event, cause->As<RE::Actor>(), weapon);
				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			static void Process(const RE::TESHitEvent* a_event, RE::Actor* a_hitter, RE::TESObjectWEAP* a_weapon)
			{
				const auto target = a_event->target.get();
				if (!target || !a_hitter || !a_hitter->Is3DLoaded()) {
					return;
				}
				const bool power = a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack) || IsPowerAttacking(a_hitter);
				const auto targetActor = target->As<RE::Actor>();
				const auto& s = g_settings;
				if (!targetActor) {  // an object
					if (s.stopOnObject) {
						CalculateStop(power, a_hitter, a_weapon, objectStop, false);
					}
					return;
				}
				if (!s.stopOnCreature) {
					return;
				}
				if (targetActor->IsDead()) {
					if (s.stopOnDead) {
						CalculateStop(power, a_hitter, a_weapon, creatureStop, true);
					}
					return;
				}
				if (a_event->flags.any(RE::TESHitEvent::Flag::kBashAttack)) {
					if (s.stopOnBash) {
						CalculateStop(power, a_hitter, a_weapon, bashStop, false);
					}
					return;
				}
				if (a_event->flags.any(RE::TESHitEvent::Flag::kHitBlocked)) {
					if (s.stopOnBlocked) {
						CalculateStop(power, a_hitter, a_weapon, blockedStop, false);
					}
					return;
				}
				CalculateStop(power, a_hitter, a_weapon, creatureStop, false);
			}
		};
	}

	Settings& Get() { return g_settings; }

	const std::vector<Item>& Items()
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
		if (g_settings.framework != 0 && g_settings.framework != 1) {
			g_settings.framework = 0;
		}
		if (!g_sgtmOngoing && g_settings.framework == 0) {
			// nothing to undo
		}
	}

	void Update()
	{
		if (g_settings.framework != 1 || !g_sgtmOngoing) {
			return;
		}
		if (g_sgtmTime <= 0.0f) {
			RE::Offset::SGTM(1.0f);
			g_sgtmOngoing = false;
			return;
		}
		g_sgtmTime -= *RE::Offset::g_deltaTime;
	}

	void Install()
	{
		ReadSettings();
		static HitSink sink;
		if (const auto holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink(&sink);
			logger::info("hit stop: registered the hit event sink");
		}
		// Player: the virtual UpdateAnimation (slot 0x7D, the same on SE and AE including 1.7.x).
		REL::Relocation<std::uintptr_t> playerVtbl{ RE::VTABLE_PlayerCharacter[0] };
		PlayerAnimHook::func = playerVtbl.write_vfunc(0x7D, PlayerAnimHook::thunk);
		logger::info("hook:HitStop player animation update");
		// Everyone else: the call in RunOneActorAnimationUpdateJob (checked: the byte must be a call into the code section).
		REL::Relocation<std::uintptr_t> job{ RELOCATION_ID(40436, 41453) };
		const auto site = job.address() + RELOCATION_OFFSET(0x74, 0x74);
		if (Hooks::CheckCallSite(site, "HitStop UpdateAnimationInternal")) {
			NpcAnimHook::func = SKSE::GetTrampoline().write_call<5>(site, NpcAnimHook::thunk);
			logger::info("hook:HitStop NPC animation update");
		} else {
			logger::info("hit stop: NPCs will not get a hit stop on this game version (the player still does)");
		}
	}
}
