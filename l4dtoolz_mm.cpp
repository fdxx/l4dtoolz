#include "l4dtoolz_mm.h"
#include "signature.h"
#include "game_signature.h"
#include "icommandline.h"

l4dtoolz g_l4dtoolz;
IVEngineServer* engine = NULL;
ICvar* g_pCVar = NULL;

#if SOURCE_ENGINE == SE_LEFT4DEAD
void* l4dtoolz::max_players_friend_lobby = NULL;
void* l4dtoolz::chuman_limit = NULL;
#endif

void* l4dtoolz::max_players_connect = NULL;
void* l4dtoolz::max_players_server_browser = NULL;
void* l4dtoolz::lobby_sux_ptr = NULL;
void* l4dtoolz::tmp_player = NULL;
void* l4dtoolz::unreserved_ptr = NULL;
void* l4dtoolz::lobby_match_ptr = NULL;

ConVar sv_maxplayers("sv_maxplayers", "-1", 0, "Max Human Players", true, -1, true, 32, l4dtoolz::OnChangeMaxplayers);
#if SOURCE_ENGINE == SE_LEFT4DEAD
ConVar sv_removehumanlimit("sv_removehumanlimit", "0", 0, "Remove Human limit reached kick", true, 0, true, 1, l4dtoolz::OnChangeRemovehumanlimit);
#endif
ConVar sv_force_unreserved("sv_force_unreserved", "0", 0, "Disallow lobby reservation cookie", true, 0, true, 1, l4dtoolz::OnChangeUnreserved);

void l4dtoolz::OnChangeMaxplayers ( IConVar *var, const char *pOldValue, float flOldValue )
{
	int new_value = ((ConVar*)var)->GetInt();
	int old_value = atoi(pOldValue);
#if SOURCE_ENGINE == SE_LEFT4DEAD
	if (max_players_friend_lobby == NULL || max_players_connect == NULL || max_players_server_browser == NULL || lobby_sux_ptr == NULL) {
#else
	if (max_players_connect == NULL || max_players_server_browser == NULL || lobby_sux_ptr == NULL) {
#endif
		Msg("sv_maxplayers init error\n");
		return;
	}
	if(new_value != old_value) {
		if(new_value >= 0) {
#if SOURCE_ENGINE == SE_LEFT4DEAD
			max_players_new[4] = friends_lobby_new[3] = server_bplayers_new[3] = (unsigned char)new_value;
#else
			max_players_new[4] = server_bplayers_new[3] = (unsigned char)new_value;
#endif
			if(lobby_match_ptr) {
				lobby_match_new[2] = (unsigned char)new_value;
				write_signature(lobby_match_ptr, lobby_match_new);
			} else {
				Msg("sv_maxplayers MS init error\n");
			}
#if SOURCE_ENGINE == SE_LEFT4DEAD
			write_signature(max_players_friend_lobby, friends_lobby_new);
#endif
			write_signature(max_players_connect, max_players_new);
			write_signature(lobby_sux_ptr, lobby_sux_new);
			write_signature(max_players_server_browser, server_bplayers_new);
		} else {
#if SOURCE_ENGINE == SE_LEFT4DEAD
			write_signature(max_players_friend_lobby, friends_lobby_org);
#endif
			write_signature(max_players_connect, max_players_org);
			write_signature(lobby_sux_ptr, lobby_sux_org);
			write_signature(max_players_server_browser, server_bplayers_org);
		
			if(lobby_match_ptr)
				write_signature(lobby_match_ptr, lobby_match_org);
		}
	}
}

#if SOURCE_ENGINE == SE_LEFT4DEAD
void l4dtoolz::OnChangeRemovehumanlimit ( IConVar *var, const char *pOldValue, float flOldValue )
{
	int new_value = ((ConVar*)var)->GetInt();
	int old_value = atoi(pOldValue);
	if(chuman_limit == NULL) {
		Msg( "sv_removehumanlimit init error\n");
		return;
	}
	if(new_value != old_value) {
		if(new_value == 1) {
			write_signature(chuman_limit, human_limit_new);
		}else{
			write_signature(chuman_limit, human_limit_org);
		}
	}
}
#endif

void l4dtoolz::OnChangeUnreserved ( IConVar *var, const char *pOldValue, float flOldValue )
{
	int new_value = ((ConVar*)var)->GetInt();
	int old_value = atoi(pOldValue);
	if(unreserved_ptr == NULL ) {
		Msg("unreserved_ptr init error\n");
		return;
	}
	if(new_value != old_value) {
		if(new_value == 1) {
			write_signature(unreserved_ptr, unreserved_new);
			engine->ServerCommand("sv_allow_lobby_connect_only 0\n");
		} else {
			write_signature(unreserved_ptr, unreserved_org);
		}
	}
}

PLUGIN_EXPOSE(l4dtoolz, g_l4dtoolz);

bool l4dtoolz::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);

	ConVar_Register(0, this);

	struct base_addr_t base_addr;
	base_addr.addr = NULL;
	base_addr.len = 0;

	find_base_from_list(matchmaking_dll, &base_addr);

	if(!lobby_match_ptr) {
		lobby_match_ptr = find_signature(lobby_match, &base_addr, 1);
		get_original_signature(lobby_match_ptr, lobby_match_new, lobby_match_org);
	}

	find_base_from_list(engine_dll, &base_addr);
#if SOURCE_ENGINE == SE_LEFT4DEAD
	if(!max_players_friend_lobby) {
		max_players_friend_lobby = find_signature(friends_lobby, &base_addr, 0);
		get_original_signature(max_players_friend_lobby, friends_lobby_new, friends_lobby_org);
	}
#endif
	if(!max_players_connect) {
		max_players_connect = find_signature(max_players, &base_addr, 0);
		get_original_signature(max_players_connect, max_players_new, max_players_org);
	}
	if(!lobby_sux_ptr) {
#ifdef WIN32
		lobby_sux_ptr = max_players_connect;
#else
		lobby_sux_ptr = find_signature(lobby_sux, &base_addr, 0);
#endif
		get_original_signature(lobby_sux_ptr, lobby_sux_new, lobby_sux_org);
	}
#if SOURCE_ENGINE == SE_LEFT4DEAD
#ifdef WIN32
	if(!max_players_server_browser) {
		max_players_server_browser = find_signature(server_bplayers, &base_addr, 0);
		get_original_signature(max_players_server_browser, server_bplayers_new, server_bplayers_org);
	}
#endif
#endif
	if(!tmp_player) {
		tmp_player = find_signature(players, &base_addr, 0);
		if(tmp_player) {
			get_original_signature(tmp_player, players_new, players_org);
			write_signature(tmp_player, players_new);
			const char *pszCmdLineMax;
			if(CommandLine()->CheckParm("-maxplayers", &pszCmdLineMax) || CommandLine()->CheckParm("+maxplayers", &pszCmdLineMax)) {
				char command[32];
				UTIL_Format(command, sizeof(command), "maxplayers %d\n", clamp(atoi(pszCmdLineMax), 1, 32));
				engine->ServerCommand(command);
			} else {
				engine->ServerCommand("maxplayers 31\n");
			}
			engine->ServerExecute();
			write_signature(tmp_player, players_org);
			free(players_org);
			players_org = NULL;
		}
	}
	if(!unreserved_ptr) {
		unreserved_ptr = find_signature(unreserved, &base_addr, 0);
		get_original_signature(unreserved_ptr, unreserved_new, unreserved_org);
	}

	find_base_from_list(server_dll, &base_addr);
#if SOURCE_ENGINE == SE_LEFT4DEAD
	if(!chuman_limit) {
		chuman_limit = find_signature(human_limit, &base_addr, 0);
		get_original_signature(chuman_limit, human_limit_new, human_limit_org);
	}
#ifndef WIN32
	if(!max_players_server_browser) {
		max_players_server_browser = find_signature(server_bplayers, &base_addr, 0);
		get_original_signature(max_players_server_browser, server_bplayers_new, server_bplayers_org);
	}
#endif
#else
	if(!max_players_server_browser) {
		max_players_server_browser = find_signature(server_bplayers, &base_addr, 0);
		get_original_signature(max_players_server_browser, server_bplayers_new, server_bplayers_org);
	}
#endif

	return true;
}

bool l4dtoolz::Unload(char *error, size_t maxlen)
{
#if SOURCE_ENGINE == SE_LEFT4DEAD
	write_signature(max_players_friend_lobby, friends_lobby_org);
	write_signature(chuman_limit, human_limit_org);
	free(friends_lobby_org);
	free(human_limit_org);
#endif

	write_signature(max_players_connect, max_players_org);
	write_signature(lobby_sux_ptr, lobby_sux_org);
	write_signature(max_players_server_browser, server_bplayers_org);
	write_signature(unreserved_ptr, unreserved_org);
	write_signature(lobby_match_ptr, lobby_match_org);

	free(max_players_org);
	free(lobby_sux_org);
	free(server_bplayers_org);
	free(unreserved_org);
	free(lobby_match_org);

	return true;
}

size_t UTIL_Format(char *buffer, size_t maxlength, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	size_t len = vsnprintf(buffer, maxlength, fmt, ap);
	va_end(ap);

	if (len >= maxlength)
	{
		len = maxlength - 1;
		buffer[len] = '\0';
	}

	return len;
}

const char *l4dtoolz::GetLicense()
{
	return "GPLv3";
}

const char *l4dtoolz::GetVersion()
{
	return "1.1.0.2";
}

const char *l4dtoolz::GetDate()
{
	return __DATE__;
}

const char *l4dtoolz::GetLogTag()
{
	return "L4DToolZ";
}

const char *l4dtoolz::GetAuthor()
{
	return "Accelerator, Ivailosp";
}

const char *l4dtoolz::GetDescription()
{
	return "Unlock the max player limit on L4D and L4D2";
}

const char *l4dtoolz::GetName()
{
	return "L4DToolZ";
}

const char *l4dtoolz::GetURL()
{
	return "https://github.com/Accelerator74/l4dtoolz";
}

bool l4dtoolz::RegisterConCommandBase(ConCommandBase *pVar)
{
	return META_REGCVAR(pVar);
}
