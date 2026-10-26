/** Real player/hero ownership and mock rewards; no salary ticks or alternate reward model. */
struct NonheroRewardsFixtureB13
{
	static NCore::TPlayersStartInfo Players()
	{
		NCore::TPlayersStartInfo players;
		for (int i = 0; i < 4; ++i) {
			const auto team = i == 3 ? NCore::ETeam::Team2 : NCore::ETeam::Team1;
			players.push_back(NCore::PlayerStartInfo(i, team, team, NCore::EPlayerType::Human,
				1984 + i, NCore::ESex::Male));
		}
		return players;
	}
	B8BehaviourForeignWorld map;
	std::array<CObj<SalaryHeroB13>, 4> heroes;
	CObj<B8BehaviourCreep> killer, victim;
	float savedParty = 1;
	NDb::PrimeSettings& Prime() const { return const_cast<NDb::PrimeSettings&>(map.world->GetAIWorld()->GetPrimeSettings()); }
	explicit NonheroRewardsFixtureB13(Checks& check) : map(check, Players())
	{
		if (!map.world) return;
		auto& logic = const_cast<NDb::AILogicParameters&>(map.world->GetAIWorld()->GetAIParameters());
		savedParty = logic.killAwardPartyMultiplier; logic.killAwardPartyMultiplier = 2;
		NDb::Ptr<NDb::AdvMapSettings> settings = new NDb::AdvMapSettings;
		auto& prime = const_cast<NDb::AdvMapSettings*>(settings.GetPtr())->primeSettings;
		prime.awardSpecRange = 10; prime.awardTeamRange = 3;
		prime.multipliers.push_back(1); prime.multipliers.push_back(1.5f);
		map.world->GetAIWorld()->SetMapData(nullptr, settings);
		for (int i = 0; i < 4; ++i) {
			heroes[i] = SalaryCreateB13(map.world);
			heroes[i]->AttachToPlayer(map.world->GetPlayer(i));
			map.world->GetPlayer(i)->SetIsActive(true);
			heroes[i]->SetMoveFlags(MOVE_FLAG_NO_COLLIDE);
			check(heroes[i]->TeleportTo(CVec2(i == 2 ? 30 : i == 1 ? 9 : 6, 5)),
				"B13 reward fixture places actual heroes through engine movement");
		}
		killer = B8BehaviourMoving(map.world, NDb::FACTION_FREEZE);
		victim = B8BehaviourMoving(map.world, NDb::FACTION_BURN);
		killer->AddFlag(NDb::UNITFLAG_FORBIDSELECTTARGET | NDb::UNITFLAG_FORBIDATTACK);
		victim->AddFlag(NDb::UNITFLAG_FORBIDSELECTTARGET | NDb::UNITFLAG_FORBIDATTACK);
		for (const auto entry : {std::pair<NDb::EStat, float>{NDb::STAT_NAFTAFORKILL, 50},
			{NDb::STAT_NAFTASPECFORKILL, 20}, {NDb::STAT_TEAMNAFTAFORKILL, 8},
			{NDb::STAT_TEAMNAFTAFORPRESENCE, 12}})
			victim->GetStat(entry.first)->SetBaseValue(entry.second);
		map.world->ProcessAddRemove();
		check(victim->GetNaftaForKill() == 50 && victim->GetNaftaSpecForKill() == 20,
			"B13 reward fixture uses initialized engine stats");
	}
	~NonheroRewardsFixtureB13()
	{
		if (!map.world) return;
		for (auto& hero : heroes) if (hero) { hero->DetachFromPlayer(); hero->Cleanup(); }
		if (killer) killer->Cleanup(); if (victim) victim->Cleanup();
		const_cast<NDb::AILogicParameters&>(map.world->GetAIWorld()->GetAIParameters()).killAwardPartyMultiplier = savedParty;
	}
};

/** Nonhero deaths distribute original spectator, whole-team and present-team income independently. */
static void NonheroRewardsB13(Checks& check)
{
	const int before = check.count, failed = check.failed;
	for (bool heroKiller : {false, true}) for (bool inactive : {false, true}) {
		NonheroRewardsFixtureB13 f(check); if (!f.map.world) continue;
		f.map.world->GetPlayer(1)->SetIsActive(!inactive);
		f.victim->KillUnit(CPtr<PFBaseUnit>(heroKiller ? f.heroes[0].GetPtr() : static_cast<PFBaseUnit*>(f.killer.GetPtr())));
		check(f.victim->IsDead(), "B13 reward regression traverses actual unit death");
		const float present = inactive ? 6 : 4;
		check(f.heroes[0]->Balance() == 30 + 8 + present + (heroKiller ? 50 : 0),
			"B13 nearby allied hero earns spectator and team prime even when a creep lands the kill");
		check(f.heroes[1]->Balance() == 30 + 8 + (inactive ? 0 : present),
			"B13 inactive players keep spectator/team shares but not active presence income");
		check(f.heroes[2]->Balance() == 8 + present && f.heroes[3]->Balance() == 0,
			"B13 distant teammates receive team/presence income while opponents receive none");
	}
	std::printf("B13 nonhero rewards checks=%d failed=%d\n", check.count - before, check.failed - failed);
}

/** Invoke original behaviour hooks, including removal/reentry, without replacing the payout API. */
struct RewardBehaviourB13 : PFBaseBehaviour
{
	std::function<void(NaftaAward&)> action;
	explicit RewardBehaviourB13(PFBaseMovingUnit* owner) : PFBaseBehaviour(owner) {}
	void GetKillerAward(CPtr<PFBaseUnit>, NaftaAward& award) override { if (action) action(award); }
};

/** Match original spatial/team roles, zero overrides and callback-stable award ownership. */
static void NonheroRewardBoundariesB13(Checks& check)
{
	const int before = check.count, failed = check.failed;
	for (int mode = 0; mode < 4; ++mode) {
		NonheroRewardsFixtureB13 f(check); if (!f.map.world) continue;
		if (mode == 0) f.Prime().awardSpecRange = f.Prime().awardTeamRange = .25f;
		if (mode == 1) f.Prime().awardSpecRange = 2;
		if (mode == 2) f.Prime().multipliers.clear();
		if (mode == 3) { f.Prime().multipliers.clear(); f.Prime().multipliers.push_back(1.5f); }
		f.victim->KillUnit(CPtr<PFBaseUnit>(f.killer.GetPtr()));
		const float spectator = mode == 0 ? 0 : mode == 1 ? 40 : mode == 2 ? 20 : 30;
		const float presence = mode == 0 ? 0 : 4;
		check(f.heroes[0]->Balance() == spectator + 8 + presence,
			"B13 original spectator count division and empty/saturated multiplier-table rules");
		check(f.heroes[1]->Balance() == (mode == 1 ? 0 : spectator) + 8 + presence &&
			f.heroes[2]->Balance() == 8 + presence, "B13 team presence is independent of spectator membership");
	}
	{
		NonheroRewardsFixtureB13 f(check); if (f.map.world) {
			for (const auto stat : {NDb::STAT_NAFTAFORKILL, NDb::STAT_NAFTASPECFORKILL,
				NDb::STAT_TEAMNAFTAFORKILL, NDb::STAT_TEAMNAFTAFORPRESENCE}) f.victim->GetStat(stat)->SetBaseValue(0);
			f.Prime().naftaForKill = 999; f.Prime().naftaSpecForKill = 999; f.Prime().teamNaftaForPresence = 999;
			f.victim->KillUnit(CPtr<PFBaseUnit>(f.heroes[0].GetPtr()));
			for (auto& hero : f.heroes) check(hero->Balance() == 0,
				"B13 authored zero nonhero rewards cannot acquire hero-only map overrides or fallback income");
		}
	}
	{
		NonheroRewardsFixtureB13 f(check); if (f.map.world) {
			f.heroes[0]->GetVariableVWM(UnitVariables::szPrimeForKillMultiplier)->SetBaseValue(.5f);
			f.heroes[0]->GetVariableVWM(UnitVariables::szLastHitPrimeBonus)->SetBaseValue(2);
			f.heroes[0]->GetVariableVWM(UnitVariables::szPrimeSpecForKillMultiplier)->SetBaseValue(1);
			f.victim->KillUnit(CPtr<PFBaseUnit>(f.heroes[0].GetPtr()));
			check(f.heroes[0]->Balance() == 149 && f.heroes[1]->Balance() == 42,
				"B13 per-recipient kill/last-hit and spectator variables do not multiply team shares");
		}
	}
	for (bool history : {false, true}) {
		NonheroRewardsFixtureB13 f(check); if (!f.map.world) continue;
		KillAttributionFixtureB13 attribution(check, f.map.world);
		if (history) attribution.Hit(f.heroes[0], f.victim, .25f);
		else f.killer->SetMaster(f.heroes[0].GetPtr());
		f.victim->KillUnit(CPtr<PFBaseUnit>(f.killer.GetPtr()));
		check(f.heroes[0]->Balance() == 92 && f.heroes[1]->Balance() == 42,
			"B13 original summon ownership and recent actual damage select the qualified last-hit hero");
		f.killer->SetMaster(nullptr);
	}
	{
		NonheroRewardsFixtureB13 f(check); if (f.map.world) {
			f.heroes[0]->KillUnit(nullptr, PFBaseUnit::UNITDIEFLAGS_FORBIDREWARDS);
			check(f.heroes[1]->TeleportTo(CVec2(7, 5)), "B13 living representative stays inside the independent team-presence radius");
			f.victim->KillUnit(CPtr<PFBaseUnit>(f.killer.GetPtr()));
			check(f.heroes[0]->Balance() == 12 && f.heroes[1]->Balance() == 52 && f.heroes[2]->Balance() == 12,
				"B13 dead heroes remain team recipients but cannot dilute the live spatial spectator pool");
		}
	}
	for (bool forbid : {false, true}) {
		NonheroRewardsFixtureB13 f(check); if (!f.map.world) continue;
		auto unattached = SalaryCreateB13(f.map.world);
		f.victim->KillUnit(CPtr<PFBaseUnit>(f.killer.GetPtr()), forbid ? PFBaseUnit::UNITDIEFLAGS_FORBIDREWARDS : 0);
		check(unattached->Balance() == 0 && f.heroes[0]->Balance() == (forbid ? 0 : 32) &&
			f.heroes[1]->Balance() == (forbid ? 0 : 32),
			"B13 unassigned nearby heroes count spatially but are not team payees; forbidden deaths pay nobody");
		unattached->Cleanup();
	}
	for (int mode = 0; mode < 6; ++mode) {
		NonheroRewardsFixtureB13 f(check); if (!f.map.world) continue;
		CObj<RewardBehaviourB13> behaviour = new RewardBehaviourB13(f.victim);
		int calls = 0;
		behaviour->action = [&](NaftaAward& award) {
			++calls;
			if (mode == 0) { f.map.world->UnregisterObject(f.heroes[1], false); f.map.world->ProcessAddRemove(); }
			if (mode == 1) f.heroes[1]->DetachFromPlayer();
			if (mode == 2) f.map.world->GetAIWorld()->AwardKillers(CPtr<PFBaseUnit>(f.victim.GetPtr()), CPtr<PFBaseUnit>(f.killer.GetPtr()));
			if (mode == 3) award.toSpectator = 4;
			if (mode == 4) { f.map.world->UnregisterObject(f.map.world->GetPlayer(1), false); f.map.world->ProcessAddRemove(); }
			if (mode == 5) f.victim->RemoveBehaviour(behaviour.GetPtr());
		};
		f.victim->AddBehaviourOnTop(behaviour);
		f.victim->KillUnit(CPtr<PFBaseUnit>(f.killer.GetPtr()));
		check(calls == 1 && f.heroes[0]->Balance() == (mode == 3 ? 18 : 42) &&
			f.heroes[1]->Balance() == (mode < 2 || mode == 4 ? 0 : mode == 3 ? 18 : 42) && f.heroes[2]->Balance() == 12,
			"B13 behaviour reward hooks retain ownership and reject removed/reassigned recipients or same-victim recursion");
		behaviour->action = nullptr;
	}
	for (int mode = 0; mode < 4; ++mode) {
		NonheroRewardsFixtureB13 f(check); if (!f.map.world) continue;
		const float nan = std::numeric_limits<float>::quiet_NaN();
		if (mode == 0) f.Prime().awardSpecRange = nan;
		if (mode == 1) f.Prime().awardTeamRange = -1;
		if (mode == 2) f.Prime().multipliers[1] = nan;
		if (mode == 3) const_cast<NDb::AILogicParameters&>(f.map.world->GetAIWorld()->GetAIParameters()).killAwardPartyMultiplier = nan;
		f.victim->KillUnit(CPtr<PFBaseUnit>(f.killer.GetPtr()));
		for (auto& hero : f.heroes) check(hero->Balance() == 0,
			"B13 malformed shared reward inputs reject before any participant receives prime");
	}
	std::printf("B13 nonhero reward boundaries checks=%d failed=%d\n", check.count - before, check.failed - failed);
}

/** Constant-only reward initialization shares parser limits without enabling general live-stat formulas. */
static void NonheroRewardFormulasB13(Checks& check)
{
	const int before = check.count, failed = check.failed;
	NonheroRewardsFixtureB13 f(check); if (!f.map.world) return;
	auto& logic = const_cast<NDb::AILogicParameters&>(f.map.world->GetAIWorld()->GetAIParameters());
	const auto saved = logic.globalConstants;
	auto* constants = new NDb::UnitConstantsContainer; logic.globalConstants = constants;
	auto add = [&](const char* name, const char* expression) {
		auto* value = new NDb::UnitConstant; value->name = name; value->var.sString = expression;
		constants->vars.push_back(value); return value;
	};
	auto* reward = add("Reward", "17.5"); add("Nested", "cReward * 2"); add("Cycle", "cCycle");
	add("Reward", "999");
	using LinuxBootstrap::EvaluateGlobalConstantNumericFormula;
	check(EvaluateGlobalConstantNumericFormula("cNested + cReward", constants).value == 52.5f,
		"B13 constant-only context preserves original first-match lookup and recursive float results");
	reward->var.sString = "20";
	check(EvaluateGlobalConstantNumericFormula("cNested + cReward", constants).value == 60,
		"B13 constant descriptors are read again rather than retaining stale values across evaluations");
	check(EvaluateGlobalConstantNumericFormula("true ? 7 : cMissing", nullptr).value == 7,
		"B13 unused global constants need no borrowed context");
	for (const char* expression : {"cMissing", "cCycle", "sLife", "random(1,2)", "1/0", "1e39", ""})
		check(!EvaluateGlobalConstantNumericFormula(expression, constants).Succeeded(),
			"B13 missing/cyclic globals, selected live context and malformed arithmetic cannot become valid reward values");
	for (int i = 0; i < 18; ++i) add(NStr::StrFmt("Chain%d", i), i == 17 ? "1" : NStr::StrFmt("cChain%d", i + 1));
	check(!EvaluateGlobalConstantNumericFormula("cChain0", constants).Succeeded(),
		"B13 constant-only reward formulas retain the existing recursion bound");
	for (const auto stat : {NDb::STAT_NAFTAFORKILL, NDb::STAT_NAFTAFORASSIST, NDb::STAT_NAFTASPECFORKILL,
		NDb::STAT_TEAMNAFTAFORKILL, NDb::STAT_TEAMNAFTAFORPRESENCE}) {
		for (const char* expression : {"cNested+2.5", "cMissing", "cCycle", "1e30", "compiled-only"}) {
			auto source = B8BehaviourCreatureDb(f.map.world, NDb::FACTION_BURN);
			auto* stats = new NDb::StatsContainer; stats->stats = source->stats->stats;
			NDb::UnitStat item; item.statId = stat; item.value.sString = expression; item.increment.sString = "0";
			if (std::strcmp(expression, "compiled-only") == 0) { item.value.sString = ""; item.value.compiledString = "unsupported"; }
			stats->stats.push_back(item);
			item.statId = NDb::STAT_STRENGTH; item.value.sString = "cReward"; item.value.compiledString = "";
			stats->stats.push_back(item);
			const_cast<NDb::Creature*>(source.GetPtr())->stats = stats;
			CObj<B8BehaviourCreep> unit = new B8BehaviourCreep(f.map.world, *source, NDb::FACTION_BURN);
			check(unit->GetStatValue(stat) == (std::strcmp(expression, "cNested+2.5") == 0 ? 42.5f : 0),
				"B13 all five nonhero reward stats use checked global expressions and reject unsafe integer-facing amounts");
			check(unit->GetStatValue(NDb::STAT_STRENGTH) == 0,
				"B13 unrelated stat initialization remains on its existing literal-only path");
			unit->Cleanup();
		}
	}
	logic.globalConstants = saved;
	std::printf("B13 nonhero reward formulas checks=%d failed=%d\n", check.count - before, check.failed - failed);
}

/** Shipped MOBA rules and PvP melee stat formulas, with explicit ordinary/event constant settings. */
static void NonheroRewardsAuthoredB13(Checks& check, const char* dataRoot)
{
	const int before = check.count, failed = check.failed;
	CObj<WinFileSystem> files = new WinFileSystem(dataRoot, false); RootFileSystem::RegisterFileSystem(files);
	NDb::SetResourceCache(NDb::CreateGameResourceCache(RootFileSystem::GetRootFileSystem(), &RootFileSystem::GetChangesProcessor()));
	{
		const auto settings = NDb::Get<NDb::AdvMapSettings>(NDb::DBID("/Maps/Multiplayer/MOBA/_.ADMPSTTN.xdb"));
		const auto stats = NDb::Get<NDb::StatsContainer>(NDb::DBID("/Creeps/DefaultStats/PvP/Faction_Melee.STCN.xdb"));
		check(settings && stats && settings->primeSettings.awardTeamRange == 25,
			"B13 original MOBA prime rules and PvP melee stat descriptors load unchanged");
		if (settings && stats) for (bool event : {false, true}) for (bool lastHit : {false, true}) {
			NonheroRewardsFixtureB13 f(check); if (!f.map.world) continue;
			auto& logic = const_cast<NDb::AILogicParameters&>(f.map.world->GetAIWorld()->GetAIParameters());
			const auto savedConstants = logic.globalConstants;
			auto* globals = new NDb::UnitConstantsContainer; auto* constant = new NDb::UnitConstant;
			constant->name = "IventNaftaOnlyMe"; constant->var.sString = event ? "1" : "0"; globals->vars.push_back(constant);
			logic.globalConstants = globals; logic.killAwardPartyMultiplier = 1;
			f.map.world->GetAIWorld()->SetMapData(nullptr, settings);
			auto* raw = new NDb::Creature; raw->stats = stats; raw->targetingParams = f.victim->DbUnitDesc()->targetingParams;
			NDb::Ptr<NDb::Creature> db = raw;
			CObj<B8BehaviourCreep> creep = new B8BehaviourCreep(f.map.world, *db, NDb::FACTION_BURN);
			check(creep->GetNaftaForKill() == (event ? 69 : 36) && creep->GetTeamNaftaForPresence() == (event ? 0 : 33),
				"B13 native stat initialization evaluates original event-dependent melee rewards");
			auto damage = DirectDamage(); damage.amount = 10000; damage.damageType = NDb::APPLICATORDAMAGETYPE_PURE;
			damage.ignoreDefences = true; damage.pSender = lastHit ? f.heroes[0].GetPtr() : static_cast<PFBaseUnit*>(f.killer.GetPtr());
			creep->OnDamage(damage);
			check(creep->IsDead() && f.heroes[0]->Balance() == (event ? (lastHit ? 69 : 0) : (lastHit ? 47 : 11)) &&
				f.heroes[1]->Balance() == (event ? 0 : 11) && f.heroes[2]->Balance() == (event ? 0 : 11) && f.heroes[3]->Balance() == 0,
				"B13 lethal damage pays original presence/last-hit split or the configured last-hit-only event");
			creep->Cleanup(); logic.globalConstants = savedConstants;
		}
	}
	NDb::SetResourceCache(nullptr); RootFileSystem::UnregisterFileSystem(files);
	std::printf("B13 authored nonhero rewards checks=%d failed=%d\n", check.count - before, check.failed - failed);
}
