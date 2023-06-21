#define private public
#include "tier1/convar.h"
#undef private

#include "l4dtoolz.h"
#include "memoryutils.h"

#include "tier1/tier1.h"
#include "icommandline.h"

#define MAX_MEMPATCH 11

l4dtoolz g_l4dtoolz;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(l4dtoolz, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_l4dtoolz );

static int g_iMaxHumanPlayers;
static MemoryPatch *g_memPatch[MAX_MEMPATCH];
static float g_fTickInterval;
static uint64_t *g_pReservationCookie;

struct fakeGlobals
{
	float padding[4];
	float frametime;
};

fakeGlobals g_FakeGlobals = { {0.0f, 0.0f, 0.0f, 0.0f}, 0.03333333f};
fakeGlobals *gp_FakeGlobals = &g_FakeGlobals;


static void SetCvarBound(ICvar *icvar, const char *name, bool bHasBound)
{
	ConVar *convar = icvar->FindVar(name);
	if (!convar) return;
	convar->m_bHasMin = bHasBound;
	convar->m_bHasMax = bHasBound;
}

bool l4dtoolz::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	IFileSystem *filesystem = (IFileSystem *)interfaceFactory(FILESYSTEM_INTERFACE_VERSION, nullptr);
	if (!filesystem)
	{
		Warning("Failed to get filesystem\n");
		return false;
	}

	const char *buffer = "addons/l4dtoolz/l4dtoolz.txt";
	GameConfig gameconf;
	if (!gameconf.LoadFile(filesystem, buffer))
	{
		Warning("Failed to LoadFile %s\n", buffer);
		return false;
	}

	// ------------------- unlock max players limit -------------------

	// SV_InitGameDLL -> CGameServer::InitMaxClients -> CServerGameClients::GetPlayerLimits -> CGameServer::SetMaxClients
	buffer = "CServerGameClients::GetPlayerLimits";
	g_memPatch[0] = new MemoryPatch();
	if (!g_memPatch[0]->CreateFromConf(&gameconf, buffer) || !g_memPatch[0]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	int maxplayers = 31;
	if (CommandLine()->CheckParm("-maxplayers", &buffer) || CommandLine()->CheckParm("+maxplayers", &buffer))
		maxplayers = clamp(atoi(buffer), 1, 32);

	WriteAddress<int>(g_memPatch[0]->GetAddress(), 2, maxplayers, false); // minplayers
	WriteAddress<int>(g_memPatch[0]->GetAddress(), 8, maxplayers, false); // maxplayers


	// Since m_numGameSlots is dynamically modified in the CBaseServer::ReplyReservationRequest function: m_numGameSlots = pKV->GetInt( "members/numSlots", 0 )
	// Patch the CBaseServer::ConnectClient to always use our value.
	buffer = "CBaseServer::ConnectClient::m_numGameSlots";
	g_memPatch[1] = new MemoryPatch();
	if (!g_memPatch[1]->CreateFromConf(&gameconf, buffer) || !g_memPatch[1]->VerifyPatch())
	{
		Warning("Failed to create or verify patch: %s\n", buffer);
		return false;
	}

	// CBaseServer::GetMaxHumanPlayers -> CServerGameClients::GetMaxHumanPlayers -> CTerrorGameRules::GetMaxHumanPlayers -> 
	// return CTerrorGameRules::HasPlayerControlledZombies(v1) == 0 ? 4 : 8;
	buffer = "CTerrorGameRules::GetMaxHumanPlayers";
	g_memPatch[2] = new MemoryPatch();
	if (!g_memPatch[2]->CreateFromConf(&gameconf, buffer) || !g_memPatch[2]->VerifyPatch())
	{
		Warning("Failed to create or verify patch: %s\n", buffer);
		return false;
	}

	// CMatchNetworkMsgControllerBase::GetActiveServerGameDetails -> CMatchTitle::GetTotalNumPlayersSupported() -> return 8;
	buffer = "CMatchTitle::GetTotalNumPlayersSupported";
	g_memPatch[3] = new MemoryPatch();
	if (!g_memPatch[3]->CreateFromConf(&gameconf, buffer) || !g_memPatch[3]->VerifyPatch())
	{
		Warning("Failed to create or verify patch: %s\n", buffer);
		return false;
	}


	buffer = "CBaseServer::m_nReservationCookie";
	g_pReservationCookie = (uint64_t*)gameconf.GetAddress(buffer);
	if (!g_pReservationCookie)
	{
		Warning("Failed to GetAddress %s\n", buffer);
		return false;
	}

	// Register sv_maxplayers ConVar
	ConnectTier1Libraries(&interfaceFactory, 1);
	ConVar_Register(0);

	// ------------------- unlock tick limit -------------------

	//	void Host_Init( bool bIsDedicated )
	//	{
	//		host_state.interval_per_tick = 0.0166667 (0x3C888889);
	//		SV_InitGameDLL();
	//	}
	//
	//	void SV_InitGameDLL()
	//	{
	//		host_state.interval_per_tick = serverGameDLL->GetTickInterval(); // 0.033333335 (0x3D088889)
	//	}
	//
	//	CGameServer::SpawnServer()
	//	{
	//		m_flTickInterval = host_state.interval_per_tick;
	//	}
	buffer = "CServerGameDLL::GetTickInterval";
	g_memPatch[4] = new MemoryPatch();
	if (!g_memPatch[4]->CreateFromConf(&gameconf, buffer) || !g_memPatch[4]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	int tickrate = 30;
	if (CommandLine()->CheckParm("-tickrate", &buffer) || CommandLine()->CheckParm("+tickrate", &buffer))
		tickrate = clamp(atoi(buffer), 30, 128);
	g_fTickInterval = 1.0f / tickrate;

	WriteAddress<float>(g_memPatch[4]->GetAddress(), 0, g_fTickInterval, false);

	// Fix boomer Vomit
	buffer = "CVomit::UpdateAbility::patch1";
	g_memPatch[5] = new MemoryPatch();
	if (!g_memPatch[5]->CreateFromConf(&gameconf, buffer) || !g_memPatch[5]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}
	WriteAddress<uint32_t>(g_memPatch[5]->GetAddress(), 1, (uint32_t)&gp_FakeGlobals, false);

	buffer = "CVomit::UpdateAbility::patch2";
	g_memPatch[6] = new MemoryPatch();
	if (!g_memPatch[6]->CreateFromConf(&gameconf, buffer) || !g_memPatch[6]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}
	WriteAddress<uint32_t>(g_memPatch[6]->GetAddress(), 1, (uint32_t)&gp_FakeGlobals, false);

#ifdef _WIN32
	buffer = "CVomit::UpdateAbility::patch3";
	g_memPatch[7] = new MemoryPatch();
	if (!g_memPatch[7]->CreateFromConf(&gameconf, buffer) || !g_memPatch[7]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}
	WriteAddress<uint32_t>(g_memPatch[7]->GetAddress(), 1, (uint32_t)&gp_FakeGlobals, false);


	// Linux: CGameClient::SetRate -> ClampClientRate -> CBaseClient::SetRate -> CNetChan::SetDataRate
	// windows: CGameClient::SetRate (inline ClampClientRate) -> CBaseClient::SetRate -> CNetChan::SetDataRate
	// windows: CBoundedCvar_Rate.GetFloat() -> ClampClientRate

	// Skip if ( v2 > 30000 )
	buffer = "CGameClient::SetRate";
	g_memPatch[8] = new MemoryPatch();
	if (!g_memPatch[8]->CreateFromConf(&gameconf, buffer) || !g_memPatch[8]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}
#endif


	// linux: Patch max clamp 30000 to 100000000. this should be big enough.
	// windows: Skip if ( result > 30000 )
	buffer = "ClampClientRate";
	g_memPatch[9] = new MemoryPatch();
	if (!g_memPatch[9]->CreateFromConf(&gameconf, buffer) || !g_memPatch[9]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	// linux: Always jump fmaxf(rate, 1000.0f);
	// windows: Skip if ( a2 > 30000.0 )
	buffer = "CNetChan::SetDataRate";
	g_memPatch[10] = new MemoryPatch();
	if (!g_memPatch[10]->CreateFromConf(&gameconf, buffer) || !g_memPatch[10]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	// In order to set the value over the boundary directly in server.cfg.
	ICvar *icvar = (ICvar *)interfaceFactory(CVAR_INTERFACE_VERSION, nullptr);
	SetCvarBound(icvar, "sv_minrate", false);
	SetCvarBound(icvar, "sv_maxrate", false);
	SetCvarBound(icvar, "net_splitpacket_maxrate", false);

	return true;
}

static void OnChangeMaxHumanPlayers( IConVar *var, const char *pOldValue, float flOldValue )
{
	g_iMaxHumanPlayers = ((ConVar*)var)->GetInt();
	if (g_iMaxHumanPlayers == -1)
	{
		g_memPatch[1]->DisablePatch();
		g_memPatch[2]->DisablePatch();
		g_memPatch[3]->DisablePatch();
		return;
	}

	if (!g_memPatch[1]->IsEnabled())
	{
		g_memPatch[1]->EnablePatch(); // CBaseServer::ConnectClient::m_numGameSlots
		g_memPatch[2]->EnablePatch(); // CTerrorGameRules::GetMaxHumanPlayers
		g_memPatch[3]->EnablePatch(); // CMatchTitle::GetTotalNumPlayersSupported
		WriteAddress<uint32_t>(g_memPatch[1]->GetAddress(), 2, (uint32_t)&g_iMaxHumanPlayers, false);
		WriteAddress<uint32_t>(g_memPatch[2]->GetAddress(), 1, (uint32_t)&g_iMaxHumanPlayers, false);
		WriteAddress<uint32_t>(g_memPatch[3]->GetAddress(), 1, (uint32_t)&g_iMaxHumanPlayers, false);
	}
}
static ConVar g_cvMaxHumanPlayer("sv_maxplayers", "-1", 0, "Max Human Players", true, -1, true, 32, OnChangeMaxHumanPlayers);


CON_COMMAND( sv_lobby_unreserve, "Set lobby reservation cookie to 0" )
{
	*g_pReservationCookie = 0ull;
	ConMsg("[l4dtoolz] Set lobby reservation cookie to 0\n");
}

CON_COMMAND( l4dtoolz_version, "Print version" )
{
	ConMsg("%s\n", g_l4dtoolz.GetPluginDescription());
}

// Auto call when l4dtoolz::Load fails
void l4dtoolz::Unload()
{
	ConVar_Unregister();
	DisconnectTier1Libraries();
	for (int i = 0; i < MAX_MEMPATCH; i++)
		delete g_memPatch[i];
}
