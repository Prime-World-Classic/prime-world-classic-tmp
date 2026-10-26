/** Mock map loader: expose only authored team capacity, never load or simulate a world. */
class LobbyAdmissionMapLoader : public NWorld::IMapLoader, public BaseObjectMT
{
	NI_DECLARE_REFCOUNT_CLASS_2(LobbyAdmissionMapLoader, NWorld::IMapLoader, BaseObjectMT);
public:
	explicit LobbyAdmissionMapLoader(int capacity) : teamCapacity(capacity) {}
	const char* GetMapDbId() const override { return "valid-map"; }
	const char* GetMapDescName() override { return "valid-map"; }
	int GetMaxPlayersPerTeam() override { return teamCapacity; }
	bool FillMapStartInfo(NCore::MapStartInfo&, const lobby::TGameLineUp&, const lobby::SGameParameters&) override { return false; }
	bool FillPlayersInfo(NCore::MapStartInfo&, const vector<Peered::ClientInfo>&, bool) override { return false; }
private:
	int teamCapacity;
};

/** Count resolver calls, including forbidden empty IDs, at the real context's dependency boundary. */
class LobbyAdmissionMaps : public NWorld::IMapCollection, public BaseObjectMT
{
	NI_DECLARE_REFCOUNT_CLASS_2(LobbyAdmissionMaps, NWorld::IMapCollection, BaseObjectMT);
public:
	int calls = 0, teamCapacity = 5;
	bool available = true;
	int MapsListSize() const override { return 1; }
	const char* MapId(int) const override { return "valid-map"; }
	const char* MapDescId(int) const override { return "valid-map"; }
	const wchar_t* MapTitle(int) const override { return L"Map"; }
	const wchar_t* MapDescription(int) const override { return L"Map"; }
	const char* CustomDescId(int) const override { return "valid-map"; }
	const wchar_t* CustomTitle(int) const override { return L"Map"; }
	const wchar_t* CustomDescription(int) const override { return L"Map"; }
	void InitCustomList(const NDb::Ptr<NDb::MapList>) override {}
	int FindMapById(const char* id) const override { return id && strcmp(id, "valid-map") == 0 ? 0 : -1; }
	NWorld::IMapLoader* CreateMapLoader(const char* id) override
	{
		++calls;
		return available && FindMapById(id) == 0 ? new LobbyAdmissionMapLoader(teamCapacity) : nullptr;
	}
};

/** Exercise actual native CreateGame admission, retry and state preservation without a display. */
static bool RunLinuxLobbyAdmissionProbe()
{
	int checks = 0, failures = 0;
	auto check = [&](bool ok, const char* message) {
		++checks;
		if (!ok) { ++failures; fprintf(stderr, "Lobby admission FAIL: %s\n", message); }
	};
	for (const char* id : {static_cast<const char*>(nullptr), "", "missing-map"})
	{
		StrongMT<LobbyAdmissionMaps> maps = new LobbyAdmissionMaps;
		StrongMT<LinuxBootstrapGameContextUi> context = new LinuxBootstrapGameContextUi(maps);
		context->CreateGame(id, 10);
		check(context->GetLobbyStatus() == lobby::EClientStatus::Connected, "invalid map stays in the menu");
		check(context->GetCreatedMapId().empty() && context->GetMaxPlayers() == 0, "invalid map commits no session fields");
		check(context->GetCreateGameRejectedCalls() == 1 && context->GetLastLobbyAction() == "none",
			"rejection is reported separately from accepted actions");
		check(maps->calls == (id && *id ? 1 : 0), "empty map never enters the resource loader");
		context->CreateGame("valid-map", 10);
		check(context->GetLobbyStatus() == lobby::EClientStatus::InCustomLobby
			&& context->GetCreatedMapId() == "valid-map" && context->GetMaxPlayers() == 10,
			"valid retry after rejection creates the selected map");
		check(context->GetCreateGameCalls() == 2 && context->GetMultiplayerCreateCalls() == 2,
			"callback counters include attempts without implying successful transitions");
		check(context->GetCreateGameRejectedCalls() == 1 && context->GetLastLobbyAction() == "create-multiplayer",
			"valid retry retains rejection history and publishes the accepted action");
	}
	for (int players : {-1, 0, 1, 6, 10, 15})
	{
		StrongMT<LobbyAdmissionMaps> maps = new LobbyAdmissionMaps;
		StrongMT<LinuxBootstrapGameContextUi> context = new LinuxBootstrapGameContextUi(maps);
		context->CreateGame("valid-map", players);
		if (players <= 0)
		{
			check(context->GetLobbyStatus() == lobby::EClientStatus::Connected && maps->calls == 0,
				"nonpositive lineup is rejected before resolving a map");
			check(context->GetCreatedMapId().empty() && context->GetMaxPlayers() == 0, "invalid lineup preserves session fields");
		}
		else
		{
			check(context->GetLobbyStatus() == lobby::EClientStatus::InCustomLobby && maps->calls == 1,
				"valid single or multiplayer session resolves one loader");
			check(context->GetMaxPlayers() == std::min(players, 10), "lineup is capped by original two-team map capacity");
		}
	}
	for (int capacity : {-1, 0, std::numeric_limits<int>::max()})
	{
		StrongMT<LobbyAdmissionMaps> maps = new LobbyAdmissionMaps;
		maps->teamCapacity = capacity;
		StrongMT<LinuxBootstrapGameContextUi> context = new LinuxBootstrapGameContextUi(maps);
		context->CreateGame("valid-map", 10);
		check(context->GetLobbyStatus() == lobby::EClientStatus::Connected
			&& context->GetCreatedMapId().empty() && context->GetMaxPlayers() == 0,
			"invalid authored capacity cannot create an empty or overflowing lineup");
	}
	{
		StrongMT<LobbyAdmissionMaps> maps = new LobbyAdmissionMaps;
		maps->available = false;
		StrongMT<LinuxBootstrapGameContextUi> context = new LinuxBootstrapGameContextUi(maps);
		context->CreateGame("valid-map", 10);
		check(context->GetLobbyStatus() == lobby::EClientStatus::Connected
			&& context->GetCreatedMapId().empty(), "unavailable map cannot transition");
		maps->available = true;
		context->CreateGame("valid-map", 6);
		check(context->GetLobbyStatus() == lobby::EClientStatus::InCustomLobby && context->GetMaxPlayers() == 6,
			"recovered loader admits a later retry");
		const int calls = maps->calls;
		context->CreateGame("missing-map", 10);
		check(context->GetCreatedMapId() == "valid-map" && context->GetMaxPlayers() == 6 && maps->calls == calls,
			"late Create callback cannot overwrite an accepted session");
		check(context->GetLastLobbyAction() == "create-multiplayer" && context->GetCreateGameRejectedCalls() == 2,
			"late callback cannot overwrite transition provenance");
	}
	fprintf(stdout, "Lobby admission: %d checks, %d failures\n", checks, failures);
	return failures == 0;
}
