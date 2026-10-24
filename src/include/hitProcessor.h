#pragma once
#include "PCH.h"
#include "data.h"
#include "attackHandler.h"
#include "blockHandler.h"
#include "include/lib/PrecisionAPI.h"
using HITFLAG = RE::HitData::Flag;
class hitProcessor {
public:
	static hitProcessor* GetSingleton()
	{
		static hitProcessor singleton;
		return  std::addressof(singleton);
	}

	/*Process a valid hit passed from hook. Offset the raw damage based on game setting.*/
	void processHit(RE::Actor* aggressor, RE::Actor* victim, RE::HitData& hitData);

	/*Precision's post-hit callback: on 1.7.x a weapon hit no longer reaches the vanilla hit handler call this mod used to hook, so the hits Precision
	handles are processed from here, with the final hit data.*/
	static void precisionPosthitCallbackFunc(const PRECISION_API::PrecisionHitData& a_precisionHitData, const RE::HitData& a_hitData);


};

namespace inlineUtils
{


}
