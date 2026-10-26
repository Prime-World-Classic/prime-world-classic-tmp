/** Capture the previous independent queries as an oracle for snapshot optimization. */
static LinuxCreepDiagnostics LegacyCreepDiagnosticsB13(PFWorld* world)
{
	LinuxCreepDiagnostics result;
	result.spawnedNeutralCreeps = world->GetLinuxSpawnedNeutralCreepObjectsCount();
	result.movingCommonCreeps = world->GetLinuxMovingCommonCreepObjectsCount();
	result.movedCommonCreeps = world->GetLinuxMovedCommonCreepObjectsCount();
	result.commonCreepMovementDistance = world->GetLinuxCommonCreepMovementDistance();
	result.creepWaves = world->GetLinuxCreepSpawnerWavesCount();
	result.neutralWaves = world->GetLinuxNeutralCreepSpawnerWavesCount();
	result.readyCreepSpawners = world->GetLinuxReadyCreepSpawnerObjectsCount();
	result.readyNeutralSpawners = world->GetLinuxReadyNeutralCreepSpawnerObjectsCount();
	result.enabledCreepSpawners = world->GetLinuxEnabledCreepSpawnerObjectsCount();
	result.enabledNeutralSpawners = world->GetLinuxEnabledNeutralCreepSpawnerObjectsCount();
	result.contentCreepSpawners = world->GetLinuxContentCreepSpawnerObjectsCount();
	result.contentNeutralSpawners = world->GetLinuxContentNeutralCreepSpawnerObjectsCount();
	result.minCreepSpawnDelay = world->GetLinuxMinCreepSpawnerSpawnDelay();
	result.minNeutralSpawnDelay = world->GetLinuxMinNeutralCreepSpawnerSpawnDelay();
	return result;
}

/** Compare every field exactly, including float accumulation order, without padding reads. */
static LinuxCreepDiagnostics CheckCreepDiagnosticsB13(Checks& check, PFWorld* world)
{
	const int step = world->GetStepNumber();
	const float elapsed = world->GetTimeElapsed();
	const auto expected = LegacyCreepDiagnosticsB13(world);
	const auto actual = world->GetLinuxCreepDiagnostics();
	check(actual.spawnedNeutralCreeps == expected.spawnedNeutralCreeps, "diagnostics: spawned neutral creeps");
	check(actual.movingCommonCreeps == expected.movingCommonCreeps, "diagnostics: moving common creeps");
	check(actual.movedCommonCreeps == expected.movedCommonCreeps, "diagnostics: moved common creeps");
	check(actual.commonCreepMovementDistance == expected.commonCreepMovementDistance, "diagnostics: exact distance sum");
	check(actual.creepWaves == expected.creepWaves, "diagnostics: lane waves");
	check(actual.neutralWaves == expected.neutralWaves, "diagnostics: neutral waves");
	check(actual.readyCreepSpawners == expected.readyCreepSpawners, "diagnostics: ready lane spawners");
	check(actual.readyNeutralSpawners == expected.readyNeutralSpawners, "diagnostics: ready neutral spawners");
	check(actual.enabledCreepSpawners == expected.enabledCreepSpawners, "diagnostics: enabled lane spawners");
	check(actual.enabledNeutralSpawners == expected.enabledNeutralSpawners, "diagnostics: enabled neutral spawners");
	check(actual.contentCreepSpawners == expected.contentCreepSpawners, "diagnostics: authored lane contents");
	check(actual.contentNeutralSpawners == expected.contentNeutralSpawners, "diagnostics: authored neutral contents");
	check(actual.minCreepSpawnDelay == expected.minCreepSpawnDelay, "diagnostics: minimum lane delay");
	check(actual.minNeutralSpawnDelay == expected.minNeutralSpawnDelay, "diagnostics: minimum neutral delay");
	check(world->GetStepNumber() == step && world->GetTimeElapsed() == elapsed,
		"diagnostics: sampling never advances the simulation clock");
	return actual;
}

/** Real constructors consume mock authored data; no subclass changes ownership or RTTI. */
static CObj<PFBaseSpawner> DiagnosticSpawnerB13(PFWorld* world, bool neutral, float delay, bool content)
{
	NDb::AdvMapObject object;
	object.offset = CPlacement(CVec3(5, 5, 0), QNULL, CVec3(1, 1, 1));
	if (neutral)
	{
		auto* db = new NDb::AdvMapNeutralCreepSpawner;
		db->firstSpawnTimeOffset = delay;
		if (content) db->groups.resize(1);
		object.gameObject = db;
		return new PFNeutralCreepSpawner(world, object);
	}
	auto* db = new NDb::AdvMapCreepSpawner;
	db->firstSpawnTimeOffset = delay;
	db->mustRegisterInAIWorld = false;
	db->faction = NDb::FACTION_FREEZE;
	if (content) db->creeps.resize(1);
	object.gameObject = db;
	return new PFCreepSpawner(world, object);
}

/** Observational queries stay fresh across movement, waves, hiding and registry removal. */
static void CreepDiagnosticsB13(Checks& check)
{
	const int before = check.count;
	B8BehaviourForeignWorld f(check); if (!f.world) return;
	const auto empty = CheckCreepDiagnosticsB13(check, f.world);
	check(empty.minCreepSpawnDelay == 0 && empty.minNeutralSpawnDelay == 0
		&& empty.movedCommonCreeps == 0 && empty.commonCreepMovementDistance == 0,
		"diagnostics: empty world produces zero defaults");
	auto lane = DiagnosticSpawnerB13(f.world, false, 3, true);
	auto neutral = DiagnosticSpawnerB13(f.world, true, 5, true);
	f.world->ProcessAddRemove();
	const auto positive = CheckCreepDiagnosticsB13(check, f.world);
	check(positive.minCreepSpawnDelay == lane->GetLinuxSpawnDelay()
		&& positive.minCreepSpawnDelay > 0 && positive.minNeutralSpawnDelay > 0,
		"diagnostics: positive minima are not clamped to zero");
	auto negativeLane = DiagnosticSpawnerB13(f.world, false, -3, false);
	auto negativeNeutral = DiagnosticSpawnerB13(f.world, true, -5, false);
	f.world->ProcessAddRemove();
	const auto negative = CheckCreepDiagnosticsB13(check, f.world);
	check(negative.minCreepSpawnDelay == negativeLane->GetLinuxSpawnDelay()
		&& negative.minNeutralSpawnDelay == negativeNeutral->GetLinuxSpawnDelay()
		&& negative.contentCreepSpawners == 1 && negative.contentNeutralSpawners == 1,
		"diagnostics: empty authored spawners still contribute signed minima");
	lane->Hide(true); neutral->Hide(true);
	negativeLane->SpawnWave(); negativeNeutral->SpawnWave();
	const auto hidden = CheckCreepDiagnosticsB13(check, f.world);
	check(hidden.enabledCreepSpawners == 1 && hidden.enabledNeutralSpawners == 1
		&& hidden.creepWaves == 1 && hidden.neutralWaves == 1,
		"diagnostics: hidden and empty-wave spawners retain original counters");
	auto first = CommonCreepB11(f.world);
	auto second = CommonCreepB11(f.world);
	f.world->ProcessAddRemove();
	first->TeleportTo(CVec2(5.5f, 5), false, false);
	second->TeleportTo(CVec2(5.75f, 5), false, false);
	const auto boundary = CheckCreepDiagnosticsB13(check, f.world);
	check(boundary.movedCommonCreeps == 1 && boundary.commonCreepMovementDistance == 1.25f,
		"diagnostics: moved threshold is strictly greater than half a unit");
	first->MoveToSpecial(CVec2(9, 5));
	const auto moving = CheckCreepDiagnosticsB13(check, f.world);
	check(moving.movingCommonCreeps == 1, "diagnostics: movement changes are visible without a world tick");
	first->Stop();
	second->KillUnit(nullptr, PFBaseUnit::UNITDIEFLAGS_FORBIDREWARDS);
	const auto dead = CheckCreepDiagnosticsB13(check, f.world);
	check(dead.movingCommonCreeps == 0 && dead.movedCommonCreeps == 1,
		"diagnostics: dead but registered common creeps still contribute");
	f.world->UnregisterObject(second.GetPtr(), false); f.world->ProcessAddRemove();
	const auto removed = CheckCreepDiagnosticsB13(check, f.world);
	check(removed.movedCommonCreeps == 0 && removed.commonCreepMovementDistance == 0.5f,
		"diagnostics: unregister is visible even while a strong owner survives");
	check(boundary.movedCommonCreeps == 1 && boundary.commonCreepMovementDistance == 1.25f,
		"diagnostics: earlier snapshot is a detached value");
	auto readyLane = DiagnosticSpawnerB13(f.world, false, -20, false);
	auto readyNeutral = DiagnosticSpawnerB13(f.world, true, -20, true);
	f.world->ProcessAddRemove();
	f.world->GetAIWorld()->SetSpawnCreeps(true);
	f.world->GetAIWorld()->SetSpawnNeutralCreeps(true);
	const auto ready = CheckCreepDiagnosticsB13(check, f.world);
	check(ready.readyCreepSpawners > 0 && ready.readyNeutralSpawners == 1,
		"diagnostics: ready spawners are counted through their real policies");
	f.world->GetAIWorld()->SetSpawnCreeps(false);
	f.world->GetAIWorld()->SetSpawnNeutralCreeps(false);
	const auto disabled = CheckCreepDiagnosticsB13(check, f.world);
	check(disabled.readyCreepSpawners == 0 && disabled.readyNeutralSpawners == 0,
		"diagnostics: AI spawn toggles invalidate readiness immediately");
	NDb::Ptr<NDb::Creature> seed = B8BehaviourCreatureDb(f.world, NDb::FACTION_NEUTRAL);
	auto* raw = new NDb::AdvMapCreep;
	raw->stats = seed->stats; raw->targetingParams = seed->targetingParams;
	NDb::Ptr<NDb::AdvMapCreep> db = raw;
	auto* neutralSpawner = dynamic_cast<PFNeutralCreepSpawner*>(readyNeutral.GetPtr());
	CObj<PFNeutralCreep> neutralCreep = new PFNeutralCreep(f.world, *db,
		Placement(CVec3(6, 6, 0), QNULL, CVec3(1, 1, 1)), false, 5, neutralSpawner, vector<CVec2>());
	neutralSpawner->InitializeCreep(neutralCreep, db->recolor);
	f.world->ProcessAddRemove();
	check(CheckCreepDiagnosticsB13(check, f.world).spawnedNeutralCreeps == 1,
		"diagnostics: neutral population comes from spawner ownership");
	neutralSpawner->DropCreep(neutralCreep);
	check(CheckCreepDiagnosticsB13(check, f.world).spawnedNeutralCreeps == 0,
		"diagnostics: detached neutral is not counted merely because it remains registered");
	std::printf("Creep diagnostics: %d checks\n", check.count - before);
}

/** Optional CPU microbenchmark, not a timing-dependent test or a claim about GPU FPS. */
static void BenchmarkCreepDiagnosticsB13(PFWorld* world)
{
	if (!std::getenv("PW_BENCH_CREEP_DIAGNOSTICS")) return;
	constexpr int iterations = 1000;
	volatile float checksum = 0;
	for (int repeat = 0; repeat < 3; ++repeat)
	{
		auto measure = [&](bool legacy) {
			const auto start = std::chrono::steady_clock::now();
			for (int i = 0; i < iterations; ++i)
			{
				const auto result = legacy ? LegacyCreepDiagnosticsB13(world) : world->GetLinuxCreepDiagnostics();
				checksum = checksum + result.commonCreepMovementDistance + result.creepWaves;
			}
			return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / iterations;
		};
		const double legacy = measure(true), snapshot = measure(false);
		std::printf("Creep diagnostic benchmark: legacyUs=%.3f snapshotUs=%.3f ratio=%.3f checksum=%.3f\n",
			legacy, snapshot, legacy / snapshot, static_cast<float>(checksum));
	}
}
