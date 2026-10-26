/** One deterministic autonomous command per tick, through the real packed-command sender.
 * The inert default constructor preserves the fixture's weak-shell lifetime contract.
 */
class LinuxDrainAIController : public NWorld::PFBaseAIController
{
	OBJECT_BASIC_METHODS(LinuxDrainAIController);
public:
	LinuxDrainAIController() = default;
	LinuxDrainAIController(NWorld::PFWorld* world, NCore::ITransceiver* transceiver)
		: owner(world), sender(transceiver) {}
	void Step(float) override
	{
		++ticks;
		CObj<NCore::WorldCommand> command = NWorld::CreateCmdKeepAlive();
		sender->SendCommand(command, false);
		owner->RecordLinuxAICommand(NWorld::PFWorld::LinuxAICommandUnknown, nullptr, nullptr, true);
	}
	void StopHero() override { ++stops; }
	int ticks = 0, stops = 0;
private:
	CPtr<NWorld::PFWorld> owner;
	Weak<NCore::ITransceiver> sender;
};

/** Exercise finite wall-time, direct closure and diagnostic step-cap boundaries without sleeping. */
static bool RunLinuxAIInputDrainProbe()
{
	int checks = 0, failures = 0;
	auto check = [&](bool ok, const char* message) {
		++checks;
		if (!ok) { ++failures; fprintf(stderr, "AI input drain FAIL: %s\n", message); }
	};
	for (int mode : {0, 1, 2, 3, 4}) for (int prefix : {5, 9, 17})
	{
		LinuxClientLaunchSettings settings;
		settings.runSeconds = mode == 3 ? 0 : (mode == 2 ? 1 : 3);
		settings.bootstrapInteractiveWorld = mode != 2;
		LinuxSelectedMapPreview preview;
		LinuxBootstrapScreenRuntime runtime;
		runtime.localScheduler = new Game::LocalCmdScheduler(1984);
		runtime.localScheduler->StartGame();
		runtime.transceiver = new NCore::Transceiver(runtime.localScheduler, DEFAULT_GAME_STEP_LENGTH, false);
		runtime.transceiverWorld = NWorld::PFWorld::CreatePFWorld();
		auto* world = dynamic_cast<NWorld::PFWorld*>(runtime.transceiverWorld.GetPtr());
		check(world != nullptr, "real world factory succeeds");
		if (!world) continue;
		auto* terrain = new NDb::Terrain;
		terrain->elemXCount = 4; terrain->elemYCount = 3; terrain->tilesPerElement = 4;
		auto* map = new NDb::AdvMap; map->terrain = terrain;
		auto* desc = new NDb::AdvMapDescription; desc->map = map;
		NDb::Ptr<NDb::AdvMapDescription> description = desc;
		const bool loaded = world->LoadMap(description, 0, NCore::TPlayersStartInfo(), 0,
			false, NWorld::PFResourcesCollection::TalentMap());
		check(loaded, "mock authored map loads through the real world");
		if (!loaded) continue;
		runtime.transceiver->SetWorld(world);
		runtime.transceiverWorldAttached = true;
		runtime.transceiverWorldStep = world->GetStepNumber();
		runtime.mapLoadingJobCompleted = true;
		world->SetAIContainer(new NWorld::PFAIContainer(world, runtime.transceiver));
		check(!world->GetAIContainer()->IsLinuxInputClosed(), "a new session starts with autonomous input open");
		CObj<LinuxDrainAIController> actor = new LinuxDrainAIController(world, runtime.transceiver);
		world->GetAIContainer()->controllers.push_back(actor.GetPtr());
		for (int i = 0; i < prefix; ++i)
			DriveLinuxBootstrapGameScheduler(settings, preview, &runtime, i * .1);
		check(actor->ticks > 0 && world->GetLinuxAICommandsSent() == actor->ticks,
			"open live world generates real autonomous commands");
		int generatedAtSeal = actor->ticks;
		LinuxTimedDrain drain;
		if (mode >= 3)
		{
			runtime.replayFileInputActive = mode == 4;
			check(PollLinuxTimedExit(settings, runtime, 100, drain) ==
				(mode == 3 ? LinuxTimedExit::Continue : LinuxTimedExit::Complete),
				"unbounded and replay sessions bypass local timed closure");
			DriveLinuxBootstrapGameScheduler(settings, preview, &runtime, 100);
			check(!world->GetAIContainer()->IsLinuxInputClosed() &&
				runtime.localScheduler->GetLinuxInputFinishStep() < 0,
				"unbounded and replay driving leave local autonomous input open");
			check(mode == 4 ? actor->ticks == generatedAtSeal : actor->ticks > generatedAtSeal,
				"unbounded live AI continues; replay driver does not step the live world");
			continue;
		}
		if (mode == 0)
			runtime.localScheduler->CloseLinuxInput();
		else if (mode == 1)
			check(PollLinuxTimedExit(settings, runtime, 3, drain) == LinuxTimedExit::Draining,
				"wall deadline seals pending AI commands before draining");
		else
		{
			for (int i = prefix; i < 100 && runtime.localScheduler->GetLinuxInputFinishStep() < 0; ++i)
			{
				generatedAtSeal = actor->ticks;
				DriveLinuxBootstrapGameScheduler(settings, preview, &runtime, i * .1);
			}
		}
		const int barrier = runtime.localScheduler->GetLinuxInputFinishStep();
		check(barrier >= 0, "all finite closure paths establish a barrier");
		const int beforeStep = world->GetStepNumber();
		for (int i = 0; i < 24 && runtime.transceiverWorldStep < barrier; ++i)
		{
			if (mode == 1)
				ServiceLinuxTimedExit(settings, preview, runtime, 3.01 + i * .016, drain, (prefix + i) * .1);
			else
				DriveLinuxBootstrapGameScheduler(settings, preview, &runtime, (prefix + i) * .1);
		}
		check(runtime.transceiverWorldStep == barrier && world->GetStepNumber() == barrier,
			"real simulation drains to the exact sealed replay boundary");
		check(world->GetStepNumber() >= beforeStep, "closing AI does not rewind the world clock");
		check(actor->ticks == generatedAtSeal && world->GetLinuxAICommandsSent() == generatedAtSeal,
			"draining never generates autonomous commands behind the closed scheduler");
		check(world->GetLinuxExecutedPackedWorldCommandsCount() == generatedAtSeal,
			"every pre-close autonomous command executes exactly once");
		check(world->GetAIContainer()->GetLinuxControllerCount() == 1 && actor->stops == 0,
			"closing preserves controller ownership and does not issue stop orders");
		check(world->GetAIContainer()->IsLinuxInputClosed(), "every local closure path quiesces autonomous input");
		world->GetAIContainer()->CloseLinuxInput();
		world->GetAIContainer()->Step(.1f);
		check(actor->ticks == generatedAtSeal, "repeated closure and direct container ticks remain quiescent");
		const int finalStep = world->GetStepNumber();
		DriveLinuxBootstrapGameScheduler(settings, preview, &runtime, 100);
		check(world->GetStepNumber() == finalStep && actor->ticks == generatedAtSeal,
			"repeated drain service cannot reopen AI or advance beyond the barrier");
	}
	fprintf(stdout, "AI input drain: %d checks, %d failures\n", checks, failures);
	return failures == 0;
}
