/** Exercise the real session setup before map-owned objects consume registry IDs. */
static bool RunLinuxReplayIdentityProbe()
{
	int checks = 0, failures = 0;
	auto check = [&](bool ok, const char* message) {
		++checks;
		if (!ok) { ++failures; fprintf(stderr, "Replay identity FAIL: %s\n", message); }
	};
	struct Directory
	{
		fs::path previous = fs::current_path();
		char path[64] = "/tmp/pw-replay-identity-XXXXXX";
		bool ready = mkdtemp(path) != nullptr;
		Directory()
		{
			if (ready) fs::current_path(path);
			NDb::SetResourceCache(NDb::CreateGameResourceCache(RootFileSystem::GetRootFileSystem(),
				&RootFileSystem::GetChangesProcessor()));
		}
		~Directory()
		{
			NDb::SetResourceCache(nullptr);
			if (ready) { fs::current_path(previous); fs::remove_all(path); }
		}
	} directory;
	check(directory.ready, "setup has an isolated recording directory");
	if (!directory.ready) return false;
	LinuxClientLaunchSettings settings;
	LinuxBootstrapScreenRuntime live, replay;
	for (auto* runtime : {&live, &replay})
	{
		runtime->loadingInitialized = true;
		runtime->loadingScreen = new Game::LoadingScreen("", "", false, false);
		runtime->loadingMapStartInfo.mapDescName = "mock-map";
		NCore::PlayerStartInfo player;
		player.playerID = 0; player.userID = 1984; player.playerType = NCore::EPlayerType::Human;
		runtime->loadingMapStartInfo.playersInfo.push_back(player);
	}
	EnsureLinuxBootstrapGameScheduler(settings, &live);
	live.localScheduler->Step(.1f);
	settings.replayFile = fs::absolute(live.replayWriterPath);
	replay.replayFileInputActive = true;
	EnsureLinuxBootstrapGameScheduler(settings, &replay);
	auto* liveWorld = dynamic_cast<NWorld::PFWorld*>(live.transceiverWorld.GetPtr());
	auto* replayWorld = dynamic_cast<NWorld::PFWorld*>(replay.transceiverWorld.GetPtr());
	check(liveWorld && replayWorld && replay.replayInputHeaderReady, "both actual setup paths attach a real world");
	if (!liveWorld || !replayWorld) return false;
	check(liveWorld->GetAIContainer() && replayWorld->GetAIContainer(), "both sessions allocate the AI manager before map objects");
	check(liveWorld->IsLinuxAutoAIEnabled() && !replayWorld->IsLinuxAutoAIEnabled(), "identity parity does not enable autonomous replay input");
	EnsureLinuxBootstrapGameScheduler(settings, &replay);
	check(replay.transceiverWorld.GetPtr() == replayWorld, "repeated replay setup retains the original world");
	CObj<NCore::IWorldBase> offline = CreateLinuxReplayWorld();
	auto* offlineWorld = dynamic_cast<NWorld::PFWorld*>(offline.GetPtr());
	check(offlineWorld && offlineWorld->GetAIContainer() && !offlineWorld->IsLinuxAutoAIEnabled(),
		"offline playback shares the same isolated pre-map setup");
	if (!offlineWorld) return false;
	CObj<NWorld::PFBaseMaleHero> heroes[3];
	CObj<NWorld::PFPlayer> players[3];
	int index = 0;
	for (auto* world : {liveWorld, replayWorld, offlineWorld})
	{
		NWorld::PFBaseHero::SpawnInfo spawn;
		spawn.pHero = new NDb::Hero;
		spawn.placement = Placement(CVec3(5, 5, 0), QNULL, CVec3(1, 1, 1));
		heroes[index] = new NWorld::PFBaseMaleHero(world, spawn, NDb::FACTION_FREEZE, NDb::FACTION_FREEZE);
		players[index] = new NWorld::PFPlayer(world, 0, NCore::ETeam::Team1, NCore::ETeam::Team1,
			1984, NCore::ESex::Male, false, 0, false, false);
		heroes[index]->AttachToPlayer(players[index].GetPtr());
		++index;
	}
	check(heroes[0]->GetObjectId() == heroes[1]->GetObjectId(), "authored hero allocation order yields identical object IDs");
	check(players[0]->GetObjectId() == players[1]->GetObjectId(), "player allocation order yields identical object IDs");
	CObj<NCore::WorldCommand> stop = NWorld::CreateCmdStopHero(heroes[0]);
	CObj<NCore::PackedWorldCommand> packed = new NCore::PackedWorldCommand(stop,
		liveWorld->GetPointerSerialization(), 1984, 0);
	CObj<NCore::WorldCommand> decoded = packed->GetWorldCommand(replayWorld->GetPointerSerialization());
	check(decoded && decoded->CanExecute(), "a live packed hero command is executable against its replay identity");
	check(heroes[0]->GetObjectId() == heroes[2]->GetObjectId() && players[0]->GetObjectId() == players[2]->GetObjectId(),
		"offline playback preserves hero and player registry identity too");
	CObj<NCore::WorldCommand> offlineCommand = packed->GetWorldCommand(offlineWorld->GetPointerSerialization());
	check(offlineCommand && offlineCommand->CanExecute(), "offline playback admits the same packed targeted command");
	check(replayWorld->GetLinuxAICommandsSent() == 0, "replay setup generated no autonomous commands");
	LinuxBootstrapReplayFileProof proof;
	ValidateLinuxBootstrapReplayFileProof(settings.replayFile, &proof, false);
	check(proof.validated && !proof.liveWorldCreated && proof.liveWorldValidationError == "disabled",
		"ordinary validation retains record/timeline checks without creating a second gameplay world");
	ValidateLinuxBootstrapReplayFileProof(settings.replayFile, &proof, true);
	check(proof.validated && !proof.liveWorldCreated && proof.liveWorldValidationError == "map-desc-missing",
		"explicit diagnostics still attempt the offline map validation path");
	fprintf(stdout, "Replay identity: %d checks, %d failures\n", checks, failures);
	return failures == 0;
}
