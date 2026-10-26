// Included only inside the native PFAIWorld implementation's NWorld namespace.
namespace
{
/** Bound nested death rewards and reject rebroadcast of the same in-flight victim. */
struct LinuxNonheroRewardScope
{
	static thread_local std::vector<const PFBaseUnit*> active;
	bool entered;
	explicit LinuxNonheroRewardScope(const PFBaseUnit* victim)
		: entered(active.size() < 128 && std::find(active.begin(), active.end(), victim) == active.end())
	{ if (entered) active.push_back(victim); }
	~LinuxNonheroRewardScope() { if (entered) active.pop_back(); }
};
thread_local std::vector<const PFBaseUnit*> LinuxNonheroRewardScope::active;
}

/** Preserve the original nonhero role order and share arithmetic, without invented reward defaults.
 * Hero-victim formulas, assists, statistics and neutral level modifiers remain separate native work.
 */
void PFAIWorld::AwardLinuxNonheroKill(CPtr<PFBaseUnit> victimRef, CPtr<PFBaseUnit> killerRef) const
{
	PFWorld* world = GetWorld();
	auto registered = [&](const PFBaseUnit* unit) {
		return unit && world && unit->GetWorld() == world && !unit->IsObjectDead() &&
			world->FindLinuxUnitByObjectId(unit->GetObjectId()) == unit;
	};
	auto reject = [&](int code) { ++linuxAwardKillersRejected; linuxLastAwardRejectCode = code; };
	if (!registered(victimRef) || !registered(killerRef)) { reject(4); return; }
	CObj<PFBaseUnit> victim(victimRef.GetPtr()), actual(killerRef.GetPtr());
	LinuxNonheroRewardScope scope(victim);
	if (!scope.entered) { reject(6); return; }
	GetKiller(victimRef, killerRef);
	if (!registered(killerRef) || killerRef == victimRef || victim->GetMasterUnit() == killerRef) { reject(4); return; }
	CObj<PFBaseUnit> killer(killerRef.GetPtr());
	linuxLastAwardKillerObjectId = killer->GetObjectId();
	killer->KilledTarget(victimRef);
	if (!registered(victim) || !registered(killer)) { reject(4); return; }
	const auto settings = primeSettings;
	const float party = GetAIParameters().killAwardPartyMultiplier;
	if (!std::isfinite(settings.awardSpecRange) || settings.awardSpecRange <= 0 ||
		!std::isfinite(settings.awardTeamRange) || settings.awardTeamRange <= 0 || !std::isfinite(party))
	{ reject(5); return; }
	const auto faction = killer->GetFaction();
	if (faction < NDb::FACTION_NEUTRAL || faction > NDb::FACTION_BURN) { reject(5); return; }
	std::vector<CObj<PFBaseHero>> spectators, representatives;
	const int mask = NDb::SPELLTARGET_HEROMALE | NDb::SPELLTARGET_HEROFEMALE |
		NDb::SPELLTARGET_AFFECTMOUNTED | NDb::SPELLTARGET_FLYING;
	const UnitMaskingPredicate predicate(1 << faction, mask);
	auto nearby = [&](float range, std::vector<CObj<PFBaseHero>>& result) {
		auto collect = [&](PFBaseUnit& unit) {
			if (unit.IsTrueHero() && IsUnitValid(&unit) && registered(&unit))
				if (auto* hero = dynamic_cast<PFBaseHero*>(&unit)) result.push_back(CObj<PFBaseHero>(hero));
		};
		ForAllUnitsInRange(victim->GetPosition(), range, collect, predicate);
	};
	nearby(settings.awardSpecRange, spectators); nearby(settings.awardTeamRange, representatives);
	struct Recipient {
		CObj<PFBaseHero> hero;
		CObj<PFPlayer> player;
		NDb::EFaction faction;
		bool killer, spectator, active;
	};
	std::vector<Recipient> recipients;
	auto add = [&](PFBaseHero* hero, bool lastHit) {
		if (!registered(hero) || !hero->IsTrueHero()) return;
		for (const auto& entry : recipients) if (entry.hero == hero) return;
		bool spectator = false;
		for (const auto& nearbyHero : spectators) if (nearbyHero == hero) spectator = true;
		PFPlayer* player = hero->GetPlayer();
		if (player && (player->GetWorld() != world || player->GetHero() != hero ||
			!world->IsSubscribed(player))) return;
		recipients.push_back({CObj<PFBaseHero>(hero), CObj<PFPlayer>(player), hero->GetFaction(),
			lastHit, spectator, player && (player->IsBot() || player->IsActive())});
	};
	if (killer->IsTrueHero()) add(dynamic_cast<PFBaseHero*>(killer.GetPtr()), true);
	const int team = killer->GetTeamId();
	for (int i = 0; team != NCore::ETeam::None && i < world->GetPlayersCount(); ++i) {
		PFPlayer* player = world->GetPlayer(i);
		if (player && player->GetTeamID() == team && player->GetHero() && player->GetHero()->GetPlayer() == player)
			add(player->GetHero(), false);
	}
	int active = 0;
	for (const auto& entry : recipients) if (entry.active) ++active;
	NaftaAward award;
	award.toKiller = victim->GetNaftaForKill(); award.toAssister = victim->GetNaftaForAssist();
	award.toSpectator = victim->GetNaftaSpecForKill(); award.toKillerTeam = victim->GetTeamNaftaForKill();
	award.toPresentTeam = victim->GetTeamNaftaForPresence();
	CObj<PFBaseBehaviour> behaviour(victim->HasBehaviour() ? victim->Behaviour() : nullptr);
	if (behaviour) behaviour->GetKillerAward(killerRef, award);
	if (!registered(victim) || !registered(killer)) { reject(4); return; }
	if (spectators.size() > 1) award.toSpectator /= float(spectators.size());
	if (active > 1) award.toPresentTeam /= float(active);
	float spectatorAward = award.toSpectator;
	if (!spectators.empty() && !settings.multipliers.empty())
		spectatorAward *= settings.multipliers[std::min<size_t>(spectators.size(), settings.multipliers.size()) - 1];
	spectatorAward *= party;
	if (!std::isfinite(award.toKiller) || !std::isfinite(spectatorAward) ||
		!std::isfinite(award.toKillerTeam) || !std::isfinite(award.toPresentTeam)) { reject(5); return; }
	linuxLastAwardVictimNaftaForKill = award.toKiller;
	bool changed = false;
	for (const auto& entry : recipients) {
		PFBaseHero* hero = entry.hero;
		auto eligible = [&]() {
			return registered(victim) && registered(killer) && registered(hero) && hero->GetFaction() == entry.faction &&
				hero->GetPlayer() == entry.player && (!entry.player || (entry.player->GetHero() == hero &&
					world->IsSubscribed(entry.player.GetPtr())));
		};
		if (!eligible()) continue;
		const int before = hero->GetGold();
		float paid = 0;
		auto give = [&](float amount, bool fromVictim, const char* source) {
			if (!(amount > 0) || !std::isfinite(amount) || !eligible()) return;
			const int previous = hero->GetGold();
			hero->AwardForKill(victimRef, amount, fromVictim);
			const int after = hero->GetGold();
			if (after != previous) {
				paid += amount;
				fprintf(stdout, "Native nonhero prime: step=%d victim=%d killer=%d hero=%d player=%d user=%d source=%s amount=%.3f gold=%d->%d\n",
					world->GetStepNumber(), victim->GetObjectId(), killer->GetObjectId(), hero->GetObjectId(),
					hero->GetPlayerId(), entry.player ? entry.player->GetUserID() : -1, source, amount, previous, after);
			}
		};
		if (entry.killer) {
			hero->OnKill(victimRef, award.toKiller > 0);
			const float amount = award.toKiller * (1 + hero->GetVariable(UnitVariables::szPrimeForKillMultiplier)) +
				hero->GetVariable(UnitVariables::szLastHitPrimeBonus);
			linuxLastAwardHeroObjectId = hero->GetObjectId(); linuxLastAwardKillerAmount = amount;
			linuxLastAwardTeamAmount = award.toKillerTeam; linuxLastAwardHeroGoldBefore = before;
			give(amount, true, "kill");
		}
		if (entry.spectator) give(spectatorAward * (1 + hero->GetVariable(UnitVariables::szPrimeSpecForKillMultiplier)), true, "spectator");
		give(award.toKillerTeam, false, "team");
		if (!representatives.empty() && entry.active) give(award.toPresentTeam, false, "presence");
		if (entry.killer) linuxLastAwardHeroGoldAfter = hero->GetGold();
		if (hero->GetGold() != before) {
			changed = true;
			linuxLastAppliedAwardVictimObjectId = victim->GetObjectId(); linuxLastAppliedAwardKillerObjectId = killer->GetObjectId();
			linuxLastAppliedAwardHeroObjectId = hero->GetObjectId(); linuxLastAppliedAwardAmount = paid;
			linuxLastAppliedAwardHeroGoldBefore = before; linuxLastAppliedAwardHeroGoldAfter = hero->GetGold();
		}
	}
	if (changed) ++linuxAwardKillersApplied;
}
