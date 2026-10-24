#include "include/hitProcessor.h"
#include "include/attackHandler.h"
#include "include/stunHandler.h"
#include "include/balanceHandler.h"
#include "include/executionHandler.h"
#include "include/settings.h"
#include "include/reactionHandler.h"
#include "include/AI.h"
#include "include/Utils.h"
void hitProcessor::processHit(RE::Actor* a_aggressor, RE::Actor* a_victim, RE::HitData& a_hitData) {
	//offset damage from hitdata, based on player difficulty setting.
	float realDamage = a_hitData.totalDamage;
	inlineUtils::offsetRealDamage(realDamage, a_aggressor, a_victim);
	auto hitFlag = a_hitData.flags;
	using HITFLAG = RE::HitData::Flag;
	if (settings::bAttackStaminaToggle) {
		if (debuffHandler::GetSingleton()->isInDebuff(a_victim)) {
			reactionHandler::triggerStagger(a_aggressor, a_victim, reactionHandler::kLargest);
		}
	}
	if (hitFlag.any(HITFLAG::kBlocked)) {
		blockHandler::GetSingleton()->processPhysicalBlock(a_victim, a_aggressor, hitFlag, a_hitData);
		if (settings::bBlockedHitRegenStamina && settings::bAttackStaminaToggle) {
			if (!hitFlag.any(HITFLAG::kPowerAttack)) {
				attackHandler::GetSingleton()->OnLightHit(a_aggressor);
			}
		}
		return;
	}
	//from this point on the hit is not blocked/

	//bash hit
	if (hitFlag.any(HITFLAG::kBash)) {
		if (hitFlag.any(HITFLAG::kPowerAttack)) {
			stunHandler::GetSingleton()->processStunDamage(stunHandler::STUNSOURCE::powerBash, nullptr, a_aggressor, a_victim, 0);
		}
		else {
			stunHandler::GetSingleton()->processStunDamage(stunHandler::STUNSOURCE::bash, nullptr, a_aggressor, a_victim, 0);
		}
		return;
	}

	//from this point on the hit can only be unblocked melee hit.
	if (settings::bAttackStaminaToggle) {
		if (!hitFlag.any(HITFLAG::kPowerAttack)) {
			attackHandler::GetSingleton()->OnLightHit(a_aggressor);
		}
		if (debuffHandler::GetSingleton()->isInDebuff(a_victim)) {
			Utils::Actor::restoreav(a_victim, RE::ActorValue::kStamina, a_victim->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina) * settings::fMeleeRewardLightHit_Percent);
		}
	}

	if (stunHandler::GetSingleton()->getIsStunBroken(a_victim) && a_hitData.weapon && a_hitData.weapon->IsMelee()) {
		if (a_aggressor->IsPlayerRef()) {
			if (settings::bAutoExecution) {//player only auto-execute if auto execution is on
				executionHandler::GetSingleton()->attemptExecute(a_aggressor, a_victim);
			}
		}
		else { //NPC auto-execute always.
			executionHandler::GetSingleton()->attemptExecute(a_aggressor, a_victim);
		}
	}
	//DEBUG("test execution");
	//executionHandler::GetSingleton()->playExecutionIdle(aggressor, victim, data::testIdle);
	if (hitFlag.any(HITFLAG::kPowerAttack)) {
		bool b;
		//TODO:redo stun damage for elden counter
		if (a_aggressor->GetGraphVariableBool(data::GraphBool_IsGuardCountering, b) && b) {
			stunHandler::GetSingleton()->processStunDamage(stunHandler::STUNSOURCE::powerAttack, a_hitData.weapon, a_aggressor, a_victim, realDamage);
		} else {
			stunHandler::GetSingleton()->processStunDamage(stunHandler::STUNSOURCE::powerAttack, a_hitData.weapon, a_aggressor, a_victim, realDamage);
		}
	}
	else {
		stunHandler::GetSingleton()->processStunDamage(stunHandler::STUNSOURCE::lightAttack, a_hitData.weapon, a_aggressor, a_victim, realDamage);
	}

}

void hitProcessor::precisionPosthitCallbackFunc(const PRECISION_API::PrecisionHitData& a_precisionHitData, const RE::HitData& a_hitData)
{
	auto aggressor = a_precisionHitData.attacker;
	auto target = a_precisionHitData.target;
	if (!aggressor || !target || target->GetFormType() != RE::FormType::ActorCharacter) {
		return;
	}
	auto victim = target->As<RE::Actor>();
	if (!victim || Utils::Actor::isDead(victim)) {
		return;
	}
	// A normal block of the player's catches a hit: the guard counter window opens (Elden Counter, built in). Blocks that Valhalla Combat handles itself
	// (timed blocks) open it from blockHandler.
	if (victim->IsPlayerRef() && blockHandler::EldenCounterCompatibility::on && a_hitData.flags.any(RE::HitData::Flag::kBlocked)) {
		blockHandler::EldenCounterCompatibility::triggerCounter(victim);
	}
	static std::atomic<int> logged{ 0 };
	if (logged.fetch_add(1) < 5) {
		logger::info("Precision post-hit: {} hit {}, flags {:#x}, weapon {}", aggressor->GetName(), victim->GetName(), static_cast<std::uint32_t>(a_hitData.flags.underlying()),
			a_hitData.weapon ? a_hitData.weapon->GetName() : "none");
	}
	hitProcessor::GetSingleton()->processHit(aggressor, victim, const_cast<RE::HitData&>(a_hitData));
}
