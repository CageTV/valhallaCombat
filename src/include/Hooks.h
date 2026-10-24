#pragma once

#include "SKSE/Trampoline.h"
#include "include/blockHandler.h"
#include "include/Utils.h"
#include "include/hitstop.h"
#include "include/parry.h"
namespace Hooks
{
	/* ng build: the call offsets below were measured on one game version. A write_call over anything but a CALL (E8) would corrupt the code, so a hook is
	installed only when the byte at the site really is a call; otherwise it is skipped and logged. The target is logged to verify the site against the
	Address Library. */
	inline bool CheckCallSite(std::uintptr_t a_site, std::string_view a_name)
	{
		const auto op = *reinterpret_cast<const std::uint8_t*>(a_site);
		if (op != 0xE8) {
			logger::error("hook:{} SKIPPED: the byte at the call site is {:#04x}, not a call (E8). This game version needs a new offset", a_name, op);
			return false;
		}
		const auto rel = *reinterpret_cast<const std::int32_t*>(a_site + 1);
		const auto target = a_site + 5 + rel;
		const auto text = REL::Module::get().segment(REL::Segment::textx);
		if (target < text.address() || target >= text.address() + text.size()) {
			logger::error("hook:{} SKIPPED: the E8 at the call site points outside the code section (target RVA {:#x}), so it is the middle of another instruction. This game version needs a new offset",
				a_name, static_cast<std::int64_t>(target - REL::Module::get().base()));
			return false;
		}
		logger::info("hook:{} site ok, calls RVA {:#x}", a_name, target - REL::Module::get().base());
		return true;
	}
	/*Every call inside [a_base, a_base + a_len) that goes to the function with the given Address Library id. The offset of a call site moves between game
	versions (on 1.7.104 the old fixed offsets land in the middle of instructions), the function it calls does not, so the site is found by what it calls.*/
	inline std::vector<std::uintptr_t> FindCallsTo(std::uintptr_t a_base, std::size_t a_len, std::uint64_t a_targetId)
	{
		std::vector<std::uintptr_t> sites;
		const auto                  want = REL::ID(a_targetId).address();
		for (std::size_t i = 0; i + 5 <= a_len; ++i) {
			const auto p = a_base + i;
			if (*reinterpret_cast<const std::uint8_t*>(p) != 0xE8) {
				continue;
			}
			if (p + 5 + *reinterpret_cast<const std::int32_t*>(p + 1) == want) {
				sites.push_back(p);
			}
		}
		return sites;
	}
	class Hook_OnGetAttackStaminaCost  //Actor__sub_140627930+16E	call ActorValueOwner__sub_1403BEC90
	{
		/*to cancel out vanilla power attack stamina consumption.*/
	public:
		static void install()
		{
			auto& trampoline = SKSE::GetTrampoline();

			REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37650, 38603) };  //SE:627930 + 16E => 3BEC90 AE:64D350 + 171 => 3D6720

			const auto site = hook.address() + RELOCATION_OFFSET(0x16E, 0x171);
			if (!CheckCallSite(site, "getAttackStaminaCost")) {
				return;
			}
			_getAttackStaminaCost = trampoline.write_call<5>(site, getAttackStaminaCost);
			
			logger::info("hook:OnGetAttackStaminaCost");
		}

	private:
		static float getAttackStaminaCost(RE::ActorValueOwner* avOwner, RE::BGSAttackData* atkData);
		static inline REL::Relocation<decltype(getAttackStaminaCost)> _getAttackStaminaCost;
	};


	class Hook_OnCheckStaminaRegenCondition  //block stamina regen during weapon swing
	{
	public:
		static void install()
		{
			REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37510, 38452) };  // 140620690       140645AA0
			auto& trampoline = SKSE::GetTrampoline();
			const auto site = hook.address() + RELOCATION_OFFSET(0x62, 0x6F);
			if (!CheckCallSite(site, "HasFlags1")) {
				return;
			}
			_HasFlags1 = trampoline.write_call<5>(site, HasFlags1);
			logger::info("hook:CheckStaminaRegenCondition");
		}

	private:
		static bool HasFlags1(RE::ActorState* a_this, uint16_t a_flags);
		static inline REL::Relocation<decltype(HasFlags1)> _HasFlags1;  //14063C330       140662930
	};

	class Hook_OnRestoreActorValue
	{
	public:
		static void install()
		{
			REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37510, 38452) };// 140620690       140645AA0
			auto& trampoline = SKSE::GetTrampoline();
			const auto site = hook.address() + RELOCATION_OFFSET(0x176, 0xE1);
			if (!CheckCallSite(site, "RestoreActorValue")) {
				return;
			}
			_RestoreActorValue = trampoline.write_call<5>(site, RestoreActorValue);
			logger::info("hook:OnRestoresActorValue");
		}

	private:
		static void RestoreActorValue(RE::Actor* a, RE::ActorValue av, float val);
		static inline REL::Relocation<decltype(RestoreActorValue)> _RestoreActorValue; //140620900		140645d30
	};

	class Hook_OnMeleeHit
	{
	public:
		static void install()
		{
			REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37673, 38627) };  //140628C20       14064E760
			auto& trampoline = SKSE::GetTrampoline();
			std::uintptr_t site = hook.address() + RELOCATION_OFFSET(0x3C0, 0x4A8);
			if (REL::Module::IsAtLeast(SKSE::RUNTIME_SSE_1_7_99)) {
				// 1.7: find the call to the hit function (AE id 38586) inside the collision function instead of trusting the 1.6 offset
				const auto sites = FindCallsTo(hook.address(), 0x900, 38586);
				for (const auto s : sites) {
					logger::info("hook:OnMeleeHit processHit: call to id 38586 at +{:#x}", s - hook.address());
				}
				if (sites.size() != 1) {
					logger::error("hook:OnMeleeHit processHit SKIPPED: found {} calls to id 38586 in id 38627, expected exactly one", sites.size());
					return;
				}
				site = sites.front();
			}
			if (!CheckCallSite(site, "OnMeleeHit processHit")) {
				return;
			}
			_ProcessHit = trampoline.write_call<5>(site, processHit);
			logger::info("hook:OnMeleeHit");
		}

	private:
		static void processHit(RE::Actor* victim, RE::HitData& hitData);
		static inline REL::Relocation<decltype(processHit)> _ProcessHit;  //140626400       14064BAB0
	};

	/*Remove vanilla stagger by setting return value of all vanilla stagger magnitude calculations to 0.*/
	//class Hook_OnGetStaggerMagnitude
	//{
	//public:
	//	static void install()
	//	{
	//		auto& trampoline = SKSE::GetTrampoline();
	//		REL::Relocation<uintptr_t> hook1{ REL::ID(42839) };  //Down	p	Character__sub_1407431D0+81	call    ActorValueOwner__sub_1403BEB10
	//		_getStaggerMagnitude_Weapon = trampoline.write_call<5>(hook1.address() + 0x81, getStaggerMagnitude_Weapon);

	//		REL::Relocation<uintptr_t> hook2{ REL::ID(42839) };  //Down	p	Character__sub_1407431D0+5B	call    sub_1403BE760
	//		_getStaggerManitude_Bash = trampoline.write_call<5>(hook2.address() + 0x5B, getStaggerManitude_Bash);
	//		logger::info("hook:OnGetStaggerMagnitude");
	//	}

	//private:
	//	static float getStaggerMagnitude_Weapon(RE::ActorValueOwner* a1, RE::ActorValueOwner* a2, RE::TESObjectWEAP* a3, float a4);
	//	static inline REL::Relocation<decltype(getStaggerMagnitude_Weapon)> _getStaggerMagnitude_Weapon;

	//	static float getStaggerManitude_Bash(uintptr_t a1, uintptr_t a2);
	//	static inline REL::Relocation<decltype(getStaggerManitude_Bash)> _getStaggerManitude_Bash;
	//};

	//class Hook_MagicHit
	//{  //not used now
	//public:
	//	static void install()
	//	{
	//		auto& trampoline = SKSE::GetTrampoline();
	//		REL::Relocation<uintptr_t> hook{ REL::ID(43015) };  // magic hit process
	//		_processMagicHit = trampoline.write_call<5>(hook.address() + 0x216, processMagicHit);
	//		logger::debug("hook:OnMagicHit");
	//	}

	//private:
	//	static void __fastcall processMagicHit([[maybe_unused]] RE::ActorMagicCaster* attacker, [[maybe_unused]] RE::NiPoint3* rdx0, [[maybe_unused]] RE::Projectile* a_projectile, [[maybe_unused]] RE::TESObjectREFR* victim, [[maybe_unused]] float a5, [[maybe_unused]] float a6, [[maybe_unused]] char a7, [[maybe_unused]] char a8)
	//	{
	//		if (!attacker || !victim) {
	//			_processMagicHit(attacker, rdx0, a_projectile, victim, a5, a6, a7, a8);
	//		}
	//		if (victim && victim->IsPlayerRef()) {
	//		}
	//		_processMagicHit(attacker, rdx0, a_projectile, victim, a5, a6, a7, a8);
	//	}
	//	static inline REL::Relocation<decltype(processMagicHit)> _processMagicHit;
	//};

//	class Hook_MainUpdate
//	{
//	public:
//		static void install()
//		{
//			auto& trampoline = SKSE::GetTrampoline();
//			REL::Relocation<uintptr_t> hook{ RELOCATION_ID(35551, 36544) };  // 5AF3D0, 5D29F0, main loop
//
//#ifdef SKYRIM_SUPPORT_AE
//			_Update = trampoline.write_call<5>(hook.address() + 0x160, Update);
//#else
//			_Update = trampoline.write_call<5>(hook.address() + 0x11F, Update);
//#endif
//			logger::info("hook:OnMainUpdate");
//		}
//
//	private:
//		static void Update(RE::Main* a_this, float a2);
//		static inline REL::Relocation<decltype(Update)> _Update;
//	};
	class Hook_AttackBlockHandler_OnProcessButton //not used
	{
	public:
		static void install() 
		{
			REL::Relocation<std::uintptr_t> atkbckVtbl{ RE::VTABLE_AttackBlockHandler[0] };

			// AttackBlockHandler::ProcessButton is at a DIFFERENT vtable slot depending
			// on the game version: slot 0x4 on older AE / SE, but slot 0x6 on AE 1.7.99+.
			// (Confirmed in CommonLibSSE-NG: "ProcessButton ... // 04, 06 (AE 1.7.99)".)
			// The mod previously hardcoded 0x4, so on 1.7.99 / 1.7.104 it hooked the wrong
			// function -- the player's block never reached this hook, which is why timed/
			// perfect block silently did nothing (no [BlockDiag] AUTO-DETECT lines fired).
			// Pick the correct slot at runtime so block detection works on every version.
			const std::size_t processButtonVtblIndex =
				REL::Module::IsAtLeast(SKSE::RUNTIME_SSE_1_7_99) ? 0x6 : 0x4;

			_ProcessButton = atkbckVtbl.write_vfunc(processButtonVtblIndex, ProcessButton);
			logger::info("hook:AttackBlockHandler_OnProcessButton (ProcessButton vtable slot = {:#x})", processButtonVtblIndex);
			
		}

	private:
		static void ProcessButton(RE::AttackBlockHandler* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data);
		static inline REL::Relocation<decltype(ProcessButton)> _ProcessButton;
	};

	class Hook_OnPlayerUpdate  //no longer used
	{
	public:
		static void install()
		{
			REL::Relocation<std::uintptr_t> PlayerCharacterVtbl{ RE::VTABLE_PlayerCharacter[0] };

			_Update = PlayerCharacterVtbl.write_vfunc(0xAD, Update);
			logger::info("hook:OnPlayerUpdate");
		}

	private:
		static void Update(RE::PlayerCharacter* a_this, float a_delta)
		{
			ValhallaCombat::GetSingleton()->update();
			HitStop::Update();
			Parry::Update();
			_Update(a_this, a_delta);
		}
		static inline REL::Relocation<decltype(Update)> _Update;
	};

	class Hook_OnProjectileCollision
	{
	public:
		static void install()
		{
			REL::Relocation<std::uintptr_t> arrowProjectileVtbl{ RE::VTABLE_ArrowProjectile[0] };
			REL::Relocation<std::uintptr_t> missileProjectileVtbl{ RE::VTABLE_MissileProjectile[0] };

			_arrowCollission = arrowProjectileVtbl.write_vfunc(190, OnArrowCollision);
			_missileCollission = missileProjectileVtbl.write_vfunc(190, OnMissileCollision);
			logger::info("hook:OnProjectileCollision");
		};

	private:
		static void OnArrowCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector);

		static void OnMissileCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector);
		static inline REL::Relocation<decltype(OnArrowCollision)> _arrowCollission;
		static inline REL::Relocation<decltype(OnMissileCollision)> _missileCollission;
	};

	class Hook_OnMeleeCollision
	{
	public:
		static void install()
		{
			REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37650, 38603) };  //SE:627930 + 38B AE:64D350 +  45A
			auto& trampoline = SKSE::GetTrampoline();

			const auto site = hook.address() + RELOCATION_OFFSET(0x38B, 0x45A);
			if (!CheckCallSite(site, "OnMeleeCollision processHit")) {
				return;
			}
			_ProcessHit = trampoline.write_call<5>(site, processHit);

			logger::info("hook:OnMeleeCollision");
		}
	private:
		static void processHit(RE::Actor* a_aggressor, RE::Actor* a_victim, std::int64_t a_int1, bool a_bool, void* a_unkptr);
		
		static inline REL::Relocation<decltype(processHit)> _ProcessHit;
	};

	class Hook_GetWantBlock
	{
	public:
		static void install()
		{
			// This hook was only ever given an SE address id; its AE id is 00000 (invalid).
			// On AE/1.7.99 that resolves to a garbage address, and installing a call hook
			// there corrupts memory -- which tends to crash the game a few minutes later
			// rather than immediately. Since the author never provided a valid AE target,
			// skip this hook on AE entirely (it becomes a harmless no-op) instead of
			// writing into an invalid address. On SE it installs exactly as before.
			if (REL::Module::IsAE()) {
				logger::info("hook:OnGetWantBlock skipped on AE (no valid AE address for id 37376)");
				return;
			}

			REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37376, 00000) };
			auto& trampoline = SKSE::GetTrampoline();

			const auto site = hook.address() + RELOCATION_OFFSET(0x23, 0x00);
			if (!CheckCallSite(site, "GetWantBlock")) {
				return;
			}
			_GetWantBlock = trampoline.write_call<5>(site, GetWantBlock);

			logger::info("hook:OnGetWantBlock");
		}

	private:
		static std::int32_t& GetWantBlock(void* unk_ptr, const RE::BSFixedString& a_channelName, std::uint8_t unk_int, RE::Actor* a_actor, std::int32_t& a_result);

		static inline REL::Relocation<decltype(GetWantBlock)> _GetWantBlock;

	};

	class Hook_OnAttackAction
	{
	public:
		static void install()
		{
			auto& trampoline = SKSE::GetTrampoline();
			REL::Relocation<std::uintptr_t> AttackActionBase{ RELOCATION_ID(48139, 49170) };
			const auto site = AttackActionBase.address() + RELOCATION_OFFSET(0x4D7, 0x435);
			if (!CheckCallSite(site, "PerformAttackAction")) {
				return;
			}
			_PerformAttackAction = trampoline.write_call<5>(site, PerformAttackAction);
			installed = true;
			logger::info("hook:OnAttackAction");
		}
		static bool IsInstalled() { return installed; }

	private:
		static bool PerformAttackAction(RE::TESActionData* a_actionData);

		static inline REL::Relocation<decltype(PerformAttackAction)> _PerformAttackAction;
		static inline bool installed = false;
	};


	static void install()
	{
		logger::info("Installing hooks...");
		SKSE::AllocTrampoline(1 << 8);
		Hook_OnGetAttackStaminaCost::install();
		//Hook_OnCheckStaminaRegenCondition::install(); //todo: fix this hook
		Hook_OnRestoreActorValue::install();
		Hook_OnMeleeHit::install();
		Hook_OnPlayerUpdate::install();
		Hook_OnProjectileCollision::install();
		Hook_OnMeleeCollision::install();
		Hook_OnAttackAction::install();
		Hook_GetWantBlock::install();
		Hook_AttackBlockHandler_OnProcessButton::install();
		logger::info("...done");
	}
}

