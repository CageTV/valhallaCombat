#pragma once

/* Hit stop and camera shake on melee hits: the Stop On Slash mod by dTry (Nexus 66155, open permissions from the author) as part of Valhalla Combat.
Changes from the original: no MCM Helper or Papyrus (the settings are in the SKSE Menu Framework page and in Settings.ini, section [HitStop], with the
original key names), the per-actor animation speed map is locked and no longer reads an erased element, the call-site hook is checked before it is written,
and the shake follows the setting of the type of hit that was made. */
namespace HitStop
{
	enum StopType : int
	{
		objectStop = 0,
		bashStop = 1,
		blockedStop = 2,
		creatureStop = 3
	};

	enum Kind : int  // which of the four tables a value belongs to
	{
		kStopTime = 0,
		kStopSpeed = 1,
		kShakeTime = 2,
		kShakeMagnitude = 3
	};

	enum Weapon : int  // index into the per-weapon multipliers
	{
		wHandToHand = 0,
		wDagger,
		wSword,
		wGreatSword,
		wAxe,
		wMace,
		w2hw,
		wCount
	};

	struct Settings
	{
		int  framework = 0;  // 0 = per-actor animation speed, 1 = global time multiplier (player only)
		bool pcHitStop = true;
		bool npcHitStop = true;
		bool stopOnCreature = true;
		bool stopOnObject = true;
		bool stopOnDead = true;
		bool stopOnBash = true;
		bool stopOnBlocked = true;
		bool pcShake = true;
		bool shakeOnLight = false;
		bool shakeOnCreature = true;
		bool shakeOnObject = true;
		bool shakeOnDead = true;
		bool shakeOnBash = true;
		bool shakeOnBlocked = true;
		float base[4][4] = { { 0.1f, 0.1f, 0.1f, 0.1f }, { 0.1f, 0.1f, 0.1f, 0.1f }, { 0.3f, 0.3f, 0.3f, 0.3f }, { 0.3f, 0.3f, 0.3f, 0.3f } };  // [Kind][StopType]
		float power[4] = { 1, 1, 1, 1 };                                                                                                       // [Kind]
		float weapon[4][wCount] = { { 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1 } };      // [Kind][Weapon]
	};

	/* One setting in the menu and in the settings file. Exactly one of f / b / i points at the value. */
	struct Item
	{
		const char* key;
		const char* label;
		const char* group;
		float*      f;
		bool*       b;
		int*        i;
		float       min;
		float       max;
		int         decimals;
		const char* options = nullptr;  // for an int shown as a list: the entries separated by zeros
	};

	Settings&                  Get();
	const std::vector<Item>&   Items();

	/* Registers the hit event sink and the animation speed hooks. Call at kDataLoaded, after the settings were read. */
	void Install();

	/* Re-reads Data/SKSE/Plugins/ValhallaCombat/Settings.ini, section [HitStop]. */
	void ReadSettings();

	/* Per frame (player update), for the global time multiplier method. */
	void Update();
}
