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
	// Camera presentation must use the recorded human without granting command ownership.
	LinuxBootstrapScreenRuntime camera;
	camera.loadingInitialized = true;
	camera.loadingScreen = replay.loadingScreen;
	camera.sessionPresentation = LinuxBootstrap::SessionPresentation::World;
	camera.transceiverWorld = CreateLinuxReplayWorld();
	auto* cameraWorld = dynamic_cast<NWorld::PFWorld*>(camera.transceiverWorld.GetPtr());
	auto* terrain = new NDb::Terrain;
	terrain->elemXCount = terrain->elemYCount = 20; terrain->tilesPerElement = 2;
	auto* dbMap = new NDb::AdvMap; dbMap->terrain = terrain;
	auto* desc = new NDb::AdvMapDescription; desc->map = dbMap;
	NDb::Ptr<NDb::AdvMapDescription> description = desc;
	check(cameraWorld->LoadMap(description, nullptr, live.loadingMapStartInfo.playersInfo, nullptr,
		false, NWorld::PFResourcesCollection::TalentMap()), "camera fixture loads real map and player registry");
	auto* owner = cameraWorld->GetPlayerByUID(1984);
	check(owner != nullptr, "camera fixture contains the recorded human");
	if (!owner) return false;
	NWorld::PFBaseHero::SpawnInfo spawn;
	spawn.pHero = new NDb::Hero;
	spawn.placement = Placement(CVec3(50, 60, 0), QNULL, CVec3(1, 1, 1));
	CObj<NWorld::PFBaseMaleHero> hero = new NWorld::PFBaseMaleHero(cameraWorld, spawn,
		NDb::FACTION_FREEZE, NDb::FACTION_FREEZE);
	hero->AttachToPlayer(owner);
	camera.localScheduler = new Game::LocalCmdScheduler(1984);
	LinuxSelectedMapPreview map;
	map.descriptor = "mock-camera-map";
	map.tactical.ready = true;
	map.tactical.minX = map.tactical.minY = 0;
	map.tactical.maxX = map.tactical.maxY = 200;
	UpdateLinuxLocalMapCamera(settings, map, &camera);
	const float liveZoom = camera.mapPreviewZoom, liveX = camera.mapPreviewPanX, liveY = camera.mapPreviewPanZ;
	check(camera.localMapCamera && liveZoom > 1.18f, "live camera starts in a close human view");
	camera.replayFileInputActive = true;
	camera.replayInputStorage = replay.replayInputStorage;
	camera.replayInputHeaderReady = replay.replayInputHeaderReady;
	camera.replayInputClientId = replay.replayInputClientId;
	check(camera.replayInputClientId == 1984, "actual replay setup retains the recorded client ID");
	camera.localScheduler = new Game::LocalCmdScheduler(999);
	camera.localMapCamera = false;
	ResetLinuxMapPreviewCamera(&camera, "probe-replay");
	UpdateLinuxLocalMapCamera(settings, map, &camera);
	check(camera.localMapCamera && camera.localMapFollow && camera.mapPreviewZoom == liveZoom &&
		camera.mapPreviewPanX == liveX && camera.mapPreviewPanZ == liveY,
		"replay follows the recorded human, not the placeholder scheduler identity");
	check(!ResolveLinuxLocalPlayerHero(&camera, cameraWorld), "replay camera grants no gameplay command ownership");
	PanLinuxMapPreview(&camera, 1, 1, "probe-pan");
	const float panX = camera.mapPreviewPanX, panY = camera.mapPreviewPanZ;
	const auto updates = camera.localMapFocusUpdates;
	UpdateLinuxLocalMapCamera(settings, map, &camera);
	check(camera.localMapCamera && !camera.localMapFollow && camera.localMapFocusUpdates == updates &&
		camera.mapPreviewPanX == panX && camera.mapPreviewPanZ == panY,
		"manual replay pan detaches follow without reinitializing the view");
	ResetLinuxMapPreviewCamera(&camera, "probe-reset");
	UpdateLinuxLocalMapCamera(settings, map, &camera);
	check(camera.localMapCamera && camera.localMapFollow && camera.mapPreviewPanX == liveX &&
		camera.mapPreviewPanZ == liveY && camera.mapPreviewZoom == liveZoom, "reset restores recorded-human follow");
	for (int mode : {0, 1, 2, 3, 4})
	{
		const auto before = camera.localMapFocusUpdates;
		settings.spectator = mode == 0;
		camera.replayInputHeaderReady = mode != 1;
		owner->SetIsPlaying(mode != 2);
		owner->SetUserID(mode == 3 ? -1 : 1984);
		camera.replayInputClientId = mode == 4 ? 999 : 1984;
		UpdateLinuxLocalMapCamera(settings, map, &camera);
		check(camera.localMapFocusUpdates == before, "spectator, missing header/owner, inactive or bot owner cannot retarget camera");
	}
	settings.spectator = false;
	camera.replayInputHeaderReady = true;
	camera.replayInputClientId = 1984;
	owner->SetIsPlaying(true); owner->SetUserID(1984);
	const auto beforeDetach = camera.localMapFocusUpdates;
	hero->DetachFromPlayer();
	UpdateLinuxLocalMapCamera(settings, map, &camera);
	check(camera.localMapFocusUpdates == beforeDetach, "detached hero cannot update camera");
	hero->AttachToPlayer(owner);
	camera.mapPreviewYawDegrees = 18; camera.mapPreviewPitchDegrees = 40; camera.mapPreviewZoom = 4;
	UpdateLinuxLocalMapCamera(settings, map, &camera);
	check(camera.localMapFocusUpdates > beforeDetach && camera.mapPreviewRenderedYawDegrees == 18 &&
		camera.mapPreviewRenderedPitchDegrees == 40 && camera.mapPreviewZoom == 4,
		"follow resumes without overriding user rotation, tilt or zoom");
	camera.sessionPresentation = LinuxBootstrap::SessionPresentation::Loading;
	UpdateLinuxLocalMapCamera(settings, map, &camera);
	check(!camera.localMapCamera, "leaving replay world presentation restores overview");
	fprintf(stdout, "Replay identity: %d checks, %d failures\n", checks, failures);
	return failures == 0;
}
