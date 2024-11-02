#define private public
#include <tier1/convar.h>
#undef private
#include <tier1/tier1.h>
#include <tier0/icommandline.h>
#include "l4dtoolz.h"
#include "memorypatch.h"
#include "gamedata.h"
#include "memutil.h"

#define GAMENAME "left4dead2"
#define GAMEDATA_FILE "addons/l4dtoolz/l4dtoolz.txt"
#define MAX_MEMPATCH 13

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

fakeGlobals g_FakeGlobals = { {0.0f, 0.0f, 0.0f, 0.0f}, 0.03333333f };
fakeGlobals *g_pFakeGlobals = &g_FakeGlobals;


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

	GameData gamedata(GAMENAME);
	if (!gamedata.LoadFile(filesystem, GAMEDATA_FILE))
	{
		Warning("Failed to LoadFile %s\n", GAMEDATA_FILE);
		return false;
	}

	// ------------------- unlock max players limit -------------------

	// SV_InitGameDLL -> CGameServer::InitMaxClients -> CServerGameClients::GetPlayerLimits -> CGameServer::SetMaxClients
	// Replace PlayerLimits with our value.
	const char *buffer = "CServerGameClients::GetPlayerLimits";
	g_memPatch[0] = new MemoryPatch(&gamedata);
	if (!g_memPatch[0]->CreatePatch(buffer) || !g_memPatch[0]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	int maxplayers = 31;
	if (CommandLine()->CheckParm("-maxplayers", &buffer) || CommandLine()->CheckParm("+maxplayers", &buffer))
		maxplayers = clamp(atoi(buffer), 1, 32);

	memutil::WriteAddress<int>(g_memPatch[0]->GetAddress(), 2, maxplayers, false); // minplayers
	memutil::WriteAddress<int>(g_memPatch[0]->GetAddress(), 8, maxplayers, false); // maxplayers


	// Since m_numGameSlots is dynamically modified in the CBaseServer::ReplyReservationRequest function: m_numGameSlots = pKV->GetInt( "members/numSlots", 0 )
	// Patch the CBaseServer::ConnectClient to always use our value.
	// linux: cmp ebx, [edi+17Ch] -> cmp ebx, [0x00000000]
	// windows: cmp eax, [esi+180h] -> cmp eax, [0x00000000]
	// Change the original memory comparison behavior of relative address to direct comparison with g_iMaxHumanPlayers
	// In OnChangeMaxHumanPlayers, replace [0x00000000] with &g_iMaxHumanPlayers.
	buffer = "CBaseServer::ConnectClient::m_numGameSlots";
	g_memPatch[1] = new MemoryPatch(&gamedata);
	if (!g_memPatch[1]->CreatePatch(buffer) || !g_memPatch[1]->VerifyPatch())
	{
		Warning("Failed to create or verify patch: %s\n", buffer);
		return false;
	}

	// CBaseServer::GetMaxHumanPlayers -> CServerGameClients::GetMaxHumanPlayers -> CTerrorGameRules::GetMaxHumanPlayers -> 
	// return CTerrorGameRules::HasPlayerControlledZombies(v1) == 0 ? 4 : 8;
	// Modified to: directly return g_iMaxHumanPlayers
	buffer = "CTerrorGameRules::GetMaxHumanPlayers";
	g_memPatch[2] = new MemoryPatch(&gamedata);
	if (!g_memPatch[2]->CreatePatch(buffer) || !g_memPatch[2]->VerifyPatch())
	{
		Warning("Failed to create or verify patch: %s\n", buffer);
		return false;
	}

	// CMatchNetworkMsgControllerBase::GetActiveServerGameDetails -> CMatchTitle::GetTotalNumPlayersSupported() -> return 8;
	// Modified to: directly return g_iMaxHumanPlayers
	buffer = "CMatchTitle::GetTotalNumPlayersSupported";
	g_memPatch[3] = new MemoryPatch(&gamedata);
	if (!g_memPatch[3]->CreatePatch(buffer) || !g_memPatch[3]->VerifyPatch())
	{
		Warning("Failed to create or verify patch: %s\n", buffer);
		return false;
	}


	buffer = "CBaseServer::m_nReservationCookie";
	g_pReservationCookie = (uint64_t*)gamedata.GetAddress(buffer);
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
	// Modified to: directly return g_fTickInterval
	buffer = "CServerGameDLL::GetTickInterval";
	g_memPatch[4] = new MemoryPatch(&gamedata);
	if (!g_memPatch[4]->CreatePatch(buffer) || !g_memPatch[4]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	int tickrate = 30;
	if (CommandLine()->CheckParm("-tickrate", &buffer) || CommandLine()->CheckParm("+tickrate", &buffer))
		tickrate = clamp(atoi(buffer), 30, 200);
	g_fTickInterval = 1.0f / tickrate;

	memutil::WriteAddress<float>(g_memPatch[4]->GetAddress(), 0, g_fTickInterval, false);

	// ------------------- Fix boomer Vomit. -------------------
	//
	// Replace gpGlobals with our g_pFakeGlobals. to keep the frametime at 0.03333f.
	// uintptr_t ppGlobals = memutil::ReadAddress<uintptr_t>(g_memPatch[5]->m_pAddress, 1);
	// fakeGlobals *pGlobals = *(fakeGlobals**)ppGlobals;
	// float frametime = pGlobals->frametime;
	buffer = "CVomit::UpdateAbility::patch1";
	g_memPatch[5] = new MemoryPatch(&gamedata);
	if (!g_memPatch[5]->CreatePatch(buffer) || !g_memPatch[5]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}
	memutil::WriteAddress<uintptr_t>(g_memPatch[5]->GetAddress(), 1, (uintptr_t)&g_pFakeGlobals, false);

	buffer = "CVomit::UpdateAbility::patch2";
	g_memPatch[6] = new MemoryPatch(&gamedata);
	if (!g_memPatch[6]->CreatePatch(buffer) || !g_memPatch[6]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}
	memutil::WriteAddress<uintptr_t>(g_memPatch[6]->GetAddress(), 1, (uintptr_t)&g_pFakeGlobals, false);

#ifdef _WIN32
	buffer = "CVomit::UpdateAbility::patch3";
	g_memPatch[7] = new MemoryPatch(&gamedata);
	if (!g_memPatch[7]->CreatePatch(buffer) || !g_memPatch[7]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}
	memutil::WriteAddress<uintptr_t>(g_memPatch[7]->GetAddress(), 1, (uintptr_t)&g_pFakeGlobals, false);
#endif

	// ------------------- unlock cl_rate limit -------------------
	// Linux: CGameClient::SetRate -> ClampClientRate -> CBaseClient::SetRate -> CNetChan::SetDataRate
	// windows: CGameClient::SetRate (inline ClampClientRate) -> CBaseClient::SetRate -> CNetChan::SetDataRate
	// windows: CBoundedCvar_Rate.GetFloat() -> ClampClientRate

#ifdef _WIN32
	// Skip if ( v2 > 30000 )
	buffer = "CGameClient::SetRate";
	g_memPatch[8] = new MemoryPatch(&gamedata);
	if (!g_memPatch[8]->CreatePatch(buffer) || !g_memPatch[8]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}
#endif

	/*
	// Used on the server and on the client to bound its cl_rate cvar.
	int ClampClientRate( int nRate )
	{
		// Apply mod specific clamps
		if ( sv_maxrate.GetInt() > 0 )
		{
			nRate = MIN( nRate, sv_maxrate.GetInt() );
		}

		if ( sv_minrate.GetInt() > 0 )
		{
			nRate = MAX( nRate, sv_minrate.GetInt() );
		}

		// Apply overall clamp
		nRate = clamp( nRate, MIN_RATE, MAX_RATE );

		return nRate;
	}
	*/
	// linux: Patch MAX_RATE(30000) to 100000000. Make it clamp between MIN_RATE and sv_maxrate.
	// windows: Skip if ( result > 30000 )
	buffer = "ClampClientRate";
	g_memPatch[9] = new MemoryPatch(&gamedata);
	if (!g_memPatch[9]->CreatePatch(buffer) || !g_memPatch[9]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	// linux: Always jump fmaxf(rate, 1000.0f);
	// windows: Skip if ( a2 > 30000.0 )
	buffer = "CNetChan::SetDataRate";
	g_memPatch[10] = new MemoryPatch(&gamedata);
	if (!g_memPatch[10]->CreatePatch(buffer) || !g_memPatch[10]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	// Patch hardcoded limit of UpdateRate 100 on server side.

	buffer = "CGameClient::SetUpdateRate";
	g_memPatch[11] = new MemoryPatch(&gamedata);
	if (!g_memPatch[11]->CreatePatch(buffer) || !g_memPatch[11]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	
	/*
	// linux: 
	0F 4E D0  cmovle edx, eax ; Conditional move, if less than or equal.
	->
	89 C2 90  mov edx, eax ; Unconditional move.
	*/
	buffer = "CBaseClient::SetUpdateRate";
	g_memPatch[12] = new MemoryPatch(&gamedata);
	if (!g_memPatch[12]->CreatePatch(buffer) || !g_memPatch[12]->EnablePatch())
	{
		Warning("Failed to create or enable patch: %s\n", buffer);
		return false;
	}

	// In order to set the value over the boundary directly in server.cfg.
	ICvar *icvar = (ICvar *)interfaceFactory(CVAR_INTERFACE_VERSION, nullptr);
	SetCvarBound(icvar, "sv_minrate", false);
	SetCvarBound(icvar, "sv_maxrate", false);
	SetCvarBound(icvar, "net_splitpacket_maxrate", false);

	Msg("[l4dtoolz] unlock the max players limit and tickrate limit.\n");
	return true;
}

static void OnChangeMaxHumanPlayers( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVarRef convar(var);
	g_iMaxHumanPlayers = convar.GetInt();

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
		memutil::WriteAddress<uintptr_t>(g_memPatch[1]->GetAddress(), 2, (uintptr_t)&g_iMaxHumanPlayers, false);
		memutil::WriteAddress<uintptr_t>(g_memPatch[2]->GetAddress(), 1, (uintptr_t)&g_iMaxHumanPlayers, false);
		memutil::WriteAddress<uintptr_t>(g_memPatch[3]->GetAddress(), 1, (uintptr_t)&g_iMaxHumanPlayers, false);
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

	// The global variable g_memPatch[i] is initialized to nullptr,
	// and it is safe to delete nullptr.
	for (int i = 0; i < MAX_MEMPATCH; i++)
		delete g_memPatch[i];
}
