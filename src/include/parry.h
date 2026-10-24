#pragma once
#include "include/hitstop.h"

/* Elden Parry by dTry (Nexus 70240, https://github.com/D7ry/EldenParry, open permissions from the author) as part of Valhalla Combat.
A light bash has no hit frame; a bash made while an attack lands is a parry: the attacker is staggered, takes Valhalla Combat's parry stun, and projectiles are
deflected. Changes from the original: it lives inside the Valhalla Combat hooks (one set of hooks, one Precision callback), the stun is an internal call instead of
the plugin API, the parry sounds and sparks are Valhalla Combat's own (no separate ESL), the actor is taken from the AV owner through RTTI (the old fixed offset is wrong
on Anniversary Edition), skill experience uses the four-argument call of 1.7, timers are keyed by FormID, and the settings are in the menu.

"Parry mode" picks which parry system is active, so two never fight over the same hit: off, this one, or Parry for All (Nexus 167873, whose parries feed Valhalla Combat
through blockHandler::onParryForAll). */
namespace Parry
{
	enum Mode : int
	{
		kOff = 0,
		kEldenParry = 1,
		kParryForAll = 2
	};

	struct Settings
	{
		int   mode = kEldenParry;
		float parryWindowStart = 0.0f;
		float parryWindowEnd = 0.3f;
		bool  enableWeaponParry = true;
		bool  enableShieldParry = true;
		bool  enableNPCParry = true;
		bool  successfulParryNoCost = true;
		bool  slowTimeEffect = false;
		bool  screenShakeEffect = true;
		bool  sparkEffect = true;
		bool  soundEffect = true;
		bool  weaponGuardBash = true;
		bool  shieldGuardBash = true;
		bool  arrowDeflection = true;
		bool  magicDeflection = true;
		float projectileParryExp = 20.0f;
		float meleeParryExp = 10.0f;
		float guardBashExp = 10.0f;
	};

	Settings&                          Get();
	const std::vector<HitStop::Item>&  Items();
	void                               ReadSettings();

	inline bool EldenParryActive() { return Get().mode == kEldenParry; }
	inline bool ParryForAllActive() { return Get().mode == kParryForAll; }

	/* Per frame (player update): advances the parry windows. */
	void Update();

	/* Animation events: "blockStop" opens the window of a bashing actor, "bashStop" closes it (and charges the bash if the parry did not succeed). */
	void OnBlockStop(RE::Actor* a_actor);
	void OnBashStop(RE::Actor* a_actor);

	/* A melee hit is about to land (from the vanilla collision hook or from Precision's pre-hit callback). Returns true when the hit must be ignored:
	a light bash has no hit frame, a power bash is a guard bash, and a hit that lands in a bashing victim's parry window is parried. */
	bool ShouldIgnoreMeleeHit(RE::Actor* a_aggressor, RE::Actor* a_victim);

	/* A projectile collided: returns true when an actor's bash deflected it. */
	bool ShouldIgnoreProjectile(RE::Projectile* a_projectile, RE::hkpAllCdPointCollector* a_collector);

	/* The bash stamina cost is cached and zeroed so a successful parry is free (called from the attack stamina cost hook). Returns true when the cost was taken over. */
	bool TakeBashCost(RE::Actor* a_actor, RE::BGSAttackData* a_attackData, float a_originalCost);
}
