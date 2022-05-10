#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

#include <ISmmPlugin.h>

class l4dtoolz : public ISmmPlugin, public IConCommandBaseAccessor
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
public:    //IConCommandBaseAccessor
	bool RegisterConCommandBase(ConCommandBase *pVar);
public:

	static void OnChangeMaxplayers ( IConVar *var, const char *pOldValue, float flOldValue );
	static void OnChangeUnreserved ( IConVar *var, const char *pOldValue, float flOldValue );

#if SOURCE_ENGINE == SE_LEFT4DEAD
	static void OnChangeRemovehumanlimit ( IConVar *var, const char *pOldValue, float flOldValue );
	static void* max_players_friend_lobby;
	static void* chuman_limit;
#endif

	static void* max_players_connect;
	static void* max_players_server_browser;
	static void* lobby_sux_ptr;
	static void* tmp_player;
	static void* unreserved_ptr;
	static void* lobby_match_ptr;
};

size_t UTIL_Format(char *buffer, size_t maxlength, const char *fmt, ...);

extern l4dtoolz g_l4dtoolz;

PLUGIN_GLOBALVARS();

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
