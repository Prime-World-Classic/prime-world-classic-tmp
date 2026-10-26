/** Observe delivery time without executing game-specific commands in the mock world. */
class LinuxReplayTimelineWorld : public LinuxBootstrapReplayProbeWorld
{
	OBJECT_BASIC_METHODS(LinuxReplayTimelineWorld);
public:
	explicit LinuxReplayTimelineWorld(bool nativeOrigin = false) : originOffset(nativeOrigin ? 1 : 0) {}
	int GetStepNumber() const override { return LinuxBootstrapReplayProbeWorld::GetStepNumber() - originOffset; }
	std::vector<int> commandTicks, statusTicks;
	void ExecuteCommands(const NCore::TPackedCommands& commands) override
	{
		for (int i = 0; i < commands.size(); ++i) commandTicks.push_back(GetStepNumber());
		LinuxBootstrapReplayProbeWorld::ExecuteCommands(commands);
	}
	void UpdatePlayerStatuses(const NCore::TStatuses& statuses) override
	{
		for (int i = 0; i < statuses.size(); ++i) statusTicks.push_back(GetStepNumber());
		LinuxBootstrapReplayProbeWorld::UpdatePlayerStatuses(statuses);
	}
private:
	int originOffset;
};

/** Own a compact native-format fixture, including real serialized keepalive commands. */
class LinuxTimelineReplayFile
{
public:
	struct Record { int step; bool command; bool status; };
	char path[64] = "/tmp/pw-replay-timeline-XXXXXX";
	bool ready = false;
	LinuxTimelineReplayFile(int start, const std::vector<Record>& records)
	{
		const int fd = mkstemp(path);
		if (fd < 0) { path[0] = 0; return; }
		close(fd);
		std::ofstream out(path, std::ios::binary);
		const char magic[] = "PWLXREPLAY1\n";
		out.write(magic, sizeof(magic) - 1);
		auto write = [&](const auto& value) { out.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
		const long long session = 0;
		write(session); write(start);
		CObj<NCore::WorldCommand> command = NWorld::CreateCmdKeepAlive();
		CObj<NCore::PackedWorldCommand> packed = new NCore::PackedWorldCommand(command, nullptr, 1984, 0);
		MemoryStream stream;
		NCore::WriteCommandToStream(packed, &stream, nullptr);
		for (const auto& record : records)
		{
			write(record.step);
			const unsigned short commands = record.command ? 1 : 0, statuses = record.status ? 1 : 0;
			write(commands); write(statuses);
			if (record.command)
			{
				const unsigned short size = static_cast<unsigned short>(stream.GetPosition());
				write(size); out.write(stream.GetBuffer(), size);
			}
			if (record.status)
			{
				Peered::BriefClientInfo status;
				status.clientId = 1984; status.status = Peered::Connecting; status.step = record.step;
				const unsigned short size = sizeof(status);
				write(size); write(status);
			}
		}
		out.flush(); ready = out.good();
	}
	~LinuxTimelineReplayFile() { if (path[0]) unlink(path); }
	LinuxTimelineReplayFile(const LinuxTimelineReplayFile&) = delete;
	LinuxTimelineReplayFile& operator=(const LinuxTimelineReplayFile&) = delete;
};

/** Sparse native timestamps must preserve empty ticks and payload delivery, not just totals. */
static bool RunLinuxReplayTimelineProbe()
{
	int checks = 0, failures = 0;
	auto check = [&](bool ok, const char* message) {
		++checks;
		if (!ok) { ++failures; fprintf(stderr, "Replay timeline FAIL: %s\n", message); }
	};
	for (int start : {0, 7}) for (bool duplicateFinish : {false, true})
	{
		std::vector<LinuxTimelineReplayFile::Record> records = {
			{start + 1, true, false}, {start + 4, true, true}, {start + 7, true, false}};
		if (duplicateFinish) records.push_back({start + 7, false, false});
		LinuxTimelineReplayFile file(start, records);
		check(file.ready, "binary fixture is written");
		if (!file.ready) continue;
		for (bool server : {false, true})
		{
			CObj<NWorld::ReplayStorage2> storage = new NWorld::ReplayStorage2(NCore::REPLAY_BUFFER_READ, file.path, nullptr, nullptr);
			check(storage->IsOk(), "sparse records and optional empty finish record load");
			CObj<LinuxReplayTimelineWorld> world = new LinuxReplayTimelineWorld;
			Strong<NWorld::ReplayTransceiver> transceiver = new NWorld::ReplayTransceiver(storage, 100);
			transceiver->SetUseServerReplay(server); transceiver->SetWorld(world);
			for (int i = 0; i < 9; ++i) transceiver->Step(100);
			check(world->GetStepNumber() == 7, "seven timeline ticks survive sparse recording and repeated EOF");
			check(world->commandTicks == std::vector<int>({0, 3, 6}), "commands execute on their original scheduler ticks");
			check(world->statusTicks == (server ? std::vector<int>({3}) : std::vector<int>()),
				"server status arrives with its command; client mode leaves status handling unchanged");
			check(world->GetCommands() == 3, "timeline expansion neither duplicates nor drops commands");
		}
	}
	for (const auto& records : std::vector<std::vector<LinuxTimelineReplayFile::Record>>{
		{{-1, false, false}}, {{0, true, false}}, {{3, true, false}, {2, true, false}},
		{{3, true, false}, {3, true, false}}})
	{
		LinuxTimelineReplayFile file(0, records);
		NWorld::ReplayStorage2 storage(NCore::REPLAY_BUFFER_READ, file.path, nullptr, nullptr);
		check(file.ready && !storage.IsOk(), "invalid or ambiguous record timestamps fail closed");
		NCore::ReplaySegment segment;
		check(!storage.GetNextSegment(segment), "invalid streams cannot expose an accepted prefix");
	}
	{
		LinuxTimelineReplayFile file(0, {{1000000, true, false}});
		NWorld::ReplayStorage2 storage(NCore::REPLAY_BUFFER_READ, file.path, nullptr, nullptr);
		check(storage.IsOk() && storage.GetLinuxReplayLoadedSegments() == 1 &&
			storage.GetLinuxReplayTimelineSteps() == 1000000, "large idle spans stay sparse in memory");
		NCore::ReplaySegment client;
		NCore::SyncSegment server;
		for (int i = 0; i < 3; ++i)
		{
			check(storage.GetNextSegment(client) && client.seg.empty(), "leading client idle ticks are preserved");
			check(storage.GetNextSegment(server) && server.commands.empty() && server.step == static_cast<uint>(i),
				"server cursor is independent and reports relative simulation ticks");
		}
		check(storage.GetNextLinuxRecord(client) && client.seg.size() == 1 && !storage.GetNextLinuxRecord(client),
			"raw capture inspection remains independent of timeline cursors");
	}
	for (bool interactive : {false, true})
	{
		// ReplayWriter currently uses a fixed relative filename; isolate it from real recordings.
		struct Directory
		{
			fs::path previous = fs::current_path();
			char path[64] = "/tmp/pw-replay-capture-XXXXXX";
			bool ready = mkdtemp(path) != nullptr;
			Directory() { if (ready) fs::current_path(path); }
			~Directory() { if (ready) { fs::current_path(previous); fs::remove_all(path); } }
		} directory;
		check(directory.ready, "writer fixture has an isolated working directory");
		if (!directory.ready) continue;
		StrongMT<NCore::ReplayWriter> writer = new NCore::ReplayWriter;
		writer->WriteStartGame(0, 0);
		LinuxClientLaunchSettings settings; settings.runSeconds = 10;
		settings.bootstrapInteractiveWorld = interactive;
		LinuxSelectedMapPreview preview;
		LinuxBootstrapScreenRuntime runtime;
		runtime.localScheduler = new Game::LocalCmdScheduler(1984);
		runtime.localScheduler->StartGame(); runtime.localScheduler->SetReplayWriter(writer);
		runtime.transceiver = new NCore::Transceiver(runtime.localScheduler, 100, false);
		CObj<LinuxReplayTimelineWorld> live = new LinuxReplayTimelineWorld(true);
		runtime.transceiverWorld = live; runtime.transceiver->SetWorld(live);
		runtime.transceiverWorldAttached = runtime.mapLoadingJobCompleted = true;
		runtime.transceiverWorldStep = live->GetStepNumber();
		for (int i = 0; i < 32; ++i)
		{
			if (i == 0 || i == 3)
			{
				CObj<NCore::WorldCommand> command = NWorld::CreateCmdKeepAlive();
				runtime.transceiver->SendCommand(command, false);
			}
			DriveLinuxBootstrapGameScheduler(settings, preview, &runtime, i * .1);
		}
		const int barrier = runtime.localScheduler->CloseLinuxInput();
		for (int i = 0; i < 30; ++i)
			DriveLinuxBootstrapGameScheduler(settings, preview, &runtime, 3.2 + i * .1);
		check(live->GetStepNumber() == barrier && live->GetCommands() == 2, "live fixture drains two sparse commands");
		CObj<NWorld::ReplayStorage2> storage = new NWorld::ReplayStorage2(NCore::REPLAY_BUFFER_READ,
			writer->GetLinuxReplayFilePath().c_str(), nullptr, nullptr);
		check(storage->IsOk() && storage->GetLinuxReplayLoadedSegments() == writer->GetLinuxReplayStepWrites(),
			"actual scheduler recording retains exact physical record accounting");
		CObj<LinuxReplayTimelineWorld> replay = new LinuxReplayTimelineWorld(true);
		Strong<NWorld::ReplayTransceiver> player = new NWorld::ReplayTransceiver(storage, 100);
		player->SetWorld(replay);
		for (int i = 0; i < barrier + 5; ++i) player->Step(100);
		check(replay->commandTicks == live->commandTicks, "recorded commands retain live scheduler delivery ticks");
		check(replay->GetStepNumber() == live->GetStepNumber(), "quiet tail and final world tick survive actual recording");
	}
	fprintf(stdout, "Replay timeline: %d checks, %d failures\n", checks, failures);
	return failures == 0;
}
